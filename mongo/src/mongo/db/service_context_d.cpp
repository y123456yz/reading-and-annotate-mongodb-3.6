/**
 *    Copyright (C) 2014 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kDefault

#include "mongo/platform/basic.h"

#include "mongo/db/service_context_d.h"

#include "mongo/base/init.h"
#include "mongo/base/initializer.h"
#include "mongo/db/client.h"
#include "mongo/db/concurrency/lock_state.h"
#include "mongo/db/service_entry_point_mongod.h"
#include "mongo/db/storage/storage_engine.h"
#include "mongo/db/storage/storage_engine_lock_file.h"
#include "mongo/db/storage/storage_engine_metadata.h"
#include "mongo/db/storage/storage_options.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/log.h"
#include "mongo/util/map_util.h"
#include "mongo/util/mongoutils/str.h"
#include "mongo/util/scopeguard.h"
#include "mongo/util/system_clock_source.h"
#include "mongo/util/system_tick_source.h"

namespace mongo {
namespace {
//class ServiceContextMongoD final : public ServiceContext {  生成一个ServiceContextMongoD类
auto makeMongoDServiceContext() {
    auto service = stdx::make_unique<ServiceContextMongoD>();
    service->setServiceEntryPoint(stdx::make_unique<ServiceEntryPointMongod>(service.get()));
    service->setTickSource(stdx::make_unique<SystemTickSource>());
    service->setFastClockSource(stdx::make_unique<SystemClockSource>());
    service->setPreciseClockSource(stdx::make_unique<SystemClockSource>());
    return service;
}

MONGO_INITIALIZER(SetGlobalEnvironment)(InitializerContext* context) {
    setGlobalServiceContext(makeMongoDServiceContext());
    return Status::OK();
}
}  // namespace

extern bool _supportsDocLocking;

ServiceContextMongoD::ServiceContextMongoD() = default;

ServiceContextMongoD::~ServiceContextMongoD() = default;

StorageEngine* ServiceContextMongoD::getGlobalStorageEngine() {
    // We don't check that globalStorageEngine is not-NULL here intentionally.  We can encounter
    // an error before it's initialized and proceed to exitCleanly which is equipped to deal
    // with a NULL storage engine.
    //初始化见initializeGlobalStorageEngine，例如mongod对应的是ServiceContextMongoD._storageEngine
    //也就是KVStorageEngine
    return _storageEngine;
}

//_initAndListen调用，创建lockfile
void ServiceContextMongoD::createLockFile() {
    try {
        _lockFile.reset(new StorageEngineLockFile(storageGlobalParams.dbpath));
    } catch (const std::exception& ex) {
        uassert(28596,
                str::stream() << "Unable to determine status of lock file in the data directory "
                              << storageGlobalParams.dbpath
                              << ": "
                              << ex.what(),
                false);
    }
    bool wasUnclean = _lockFile->createdByUncleanShutdown();
    auto openStatus = _lockFile->open();
	//#define ENOTDIR  20 /* Not a directory */    error_code("IllegalOperation", 20)
    if (storageGlobalParams.readOnly && openStatus == ErrorCodes::IllegalOperation) {
        _lockFile.reset(); //智能指针包含了 reset() 方法，如果不传递参数（或者传递 NULL），则智能指针会释放当前管理的内存。
    } else {
        uassertStatusOK(openStatus);
    }

    if (wasUnclean) {
        if (storageGlobalParams.readOnly) {
            severe() << "Attempted to open dbpath in readOnly mode, but the server was "
                        "previously not shut down cleanly.";
            fassertFailedNoTrace(34416);
        }
        warning() << "Detected unclean shutdown - " << _lockFile->getFilespec() << " is not empty.";
    }
}

//初始化存储引擎  _initAndListen中调用执行  主要是根据params参数构造KVStorageEngine类，存到ServiceContextMongoD::_storageEngine
void ServiceContextMongoD::initializeGlobalStorageEngine() {
    // This should be set once.
    invariant(!_storageEngine);

    // We should have a _lockFile or be in read-only mode. Confusingly, we can still have a lockFile
    // if we are in read-only mode. This can happen if the server is started in read-only mode on a
    // writable dbpath.
    invariant(_lockFile || storageGlobalParams.readOnly);

    const std::string dbpath = storageGlobalParams.dbpath;
	//根据storage.bson构造StorageEngineMetadata类
    if (auto existingStorageEngine = StorageEngineMetadata::getStorageEngineForPath(dbpath)) {
		//如果配置文件中有storage.engine配置，则为true
        if (storageGlobalParams.engineSetByUser) {
            // Verify that the name of the user-supplied storage engine matches the contents of
            // the metadata file.   获取存储引擎，默认的WiredTiger存储引擎对应WiredTigerFactory
            //检查该存储引擎是否支持
            const StorageEngine::Factory* factory =
                mapFindWithDefault(_storageFactories,
                                   storageGlobalParams.engine,
                                   static_cast<const StorageEngine::Factory*>(nullptr));

            if (factory) {
                uassert(28662,
                        str::stream() << "Cannot start server. Detected data files in " << dbpath
                                      << " created by"
                                      << " the '"
                                      << *existingStorageEngine
                                      << "' storage engine, but the"
                                      << " specified storage engine was '"
                                      << factory->getCanonicalName()
                                      << "'.",
                        factory->getCanonicalName() == *existingStorageEngine); //storage.bson文件中记录的存储引擎信息必须和配置一致
            }
        } else {
            // Otherwise set the active storage engine as the contents of the metadata file.
            log() << "Detected data files in " << dbpath << " created by the '"
                  << *existingStorageEngine << "' storage engine, so setting the active"
                  << " storage engine to '" << *existingStorageEngine << "'.";
            storageGlobalParams.engine = *existingStorageEngine;
        }
    } else if (!storageGlobalParams.engineSetByUser) {
        // Ensure the default storage engine is available with this build of mongod.
        uassert(28663,
                str::stream()
                    << "Cannot start server. The default storage engine '"
                    << storageGlobalParams.engine
                    << "' is not available with this build of mongod. Please specify a different"
                    << " storage engine explicitly, e.g. --storageEngine=mmapv1.",
                isRegisteredStorageEngine(storageGlobalParams.engine));
    }

    const std::string repairpath = storageGlobalParams.repairpath;
    uassert(40311,
            str::stream() << "Cannot start server. The command line option '--repairpath'"
                          << " is only supported by the mmapv1 storage engine",
            repairpath.empty() || repairpath == dbpath || storageGlobalParams.engine == "mmapv1");

	//获取存储引擎，默认的WiredTiger存储引擎对应WiredTigerFactory
    const StorageEngine::Factory* factory = _storageFactories[storageGlobalParams.engine];

	//如果 --storageEngine xx配置错误，这里直接退出，不支持
    uassert(18656,
            str::stream() << "Cannot start server with an unknown storage engine: "
                          << storageGlobalParams.engine,
            factory);

    if (storageGlobalParams.readOnly) {
        uassert(34368,
                str::stream()
                    << "Server was started in read-only mode, but the configured storage engine, "
                    << storageGlobalParams.engine
                    << ", does not support read-only operation",
                factory->supportsReadOnly());
    }

	//根据storage.bson构造StorageEngineMetadata类
    std::unique_ptr<StorageEngineMetadata> metadata = StorageEngineMetadata::forPath(dbpath);

    if (storageGlobalParams.readOnly) {
        uassert(34415,
                "Server was started in read-only mode, but the storage metadata file was not"
                " found.",
                metadata.get());
    }

    // Validate options in metadata against current startup options.
    if (metadata.get()) {
		//对storage.bson中的文件内容和storageGlobalParams配置做检查，看是否一致
        uassertStatusOK(factory->validateMetadata(*metadata, storageGlobalParams));
    }

    ScopeGuard guard = MakeGuard([&] {
        if (_lockFile) {
            _lockFile->close();
        }
    });

	//WiredTigerFactory::create  //根据params参数构造KVStorageEngine类
    _storageEngine = factory->create(storageGlobalParams, _lockFile.get());
    _storageEngine->finishInit(); //void KVStorageEngine::finishInit() {}

    if (_lockFile) {//把PID写入磁盘lockfile文件中，保证落盘
		//StorageEngineLockFile::writePid()
        uassertStatusOK(_lockFile->writePid());
    }

    // Write a new metadata file if it is not present.
    if (!metadata.get()) { //根据配置重新构建storage.bson文件  
    //没有storage.bson文件则创建，表示第一次使用mongod进程，记录下使用的存储引擎等信息，避免下次配置文件修改新的存储引擎等配置的时候进行报错提示
        invariant(!storageGlobalParams.readOnly);
        metadata.reset(new StorageEngineMetadata(storageGlobalParams.dbpath));
        metadata->setStorageEngine(factory->getCanonicalName().toString());
        metadata->setStorageEngineOptions(factory->createMetadataOptions(storageGlobalParams));
        uassertStatusOK(metadata->write());
    }

    guard.Dismiss();

    _supportsDocLocking = _storageEngine->supportsDocLocking();
}

void ServiceContextMongoD::shutdownGlobalStorageEngineCleanly() {
	//_storageEngine为StorageEngine类型
    invariant(_storageEngine);
    _storageEngine->cleanShutdown();
    if (_lockFile) {
        _lockFile->clearPidAndUnlock();
    }
}

/*
存储引擎注册识别区分流程：
ServiceContextMongoD._storageFactories

void ServiceContextMongoD::registerStorageEngine(const std::string& name,
                                                 const StorageEngine::Factory* factory) {
    // No double-registering.
    invariant(0 == _storageFactories.count(name));

    // Some sanity checks: the factory must exist,
    invariant(factory);

    // and all factories should be added before we pick a storage engine.
    invariant(NULL == _storageEngine);

    _storageFactories[name] = factory;
}


MONGO_INITIALIZER_WITH_PREREQUISITES(DevNullEngineInit, ("SetGlobalEnvironment"))
(InitializerContext* context) {
    getGlobalServiceContext()->registerStorageEngine("devnull", new DevNullStorageEngineFactory());------对应DevNullKVEngine
    return Status::OK();
}
}


MONGO_INITIALIZER_WITH_PREREQUISITES(MMAPV1EngineInit, ("SetGlobalEnvironment"))
(InitializerContext* context) {
    getGlobalServiceContext()->registerStorageEngine("mmapv1", new MMAPV1Factory());   --------对应MMAPV1Engine
    return Status::OK();
}

MONGO_INITIALIZER_WITH_PREREQUISITES(WiredTigerEngineInit, ("SetGlobalEnvironment"))  ---------WiredTigerKVEngine
(InitializerContext* context) {
    getGlobalServiceContext()->registerStorageEngine(kWiredTigerEngineName,
                                                     new WiredTigerFactory());

    return Status::OK();
}

ServiceContextMongoD::initializeGlobalStorageEngine() {
	//获取存储引擎，默认的WiredTiger存储引擎对应WiredTigerFactory
    const StorageEngine::Factory* factory = _storageFactories[storageGlobalParams.engine];
		//WiredTigerFactory::create  //根据params参数构造KVStorageEngine类
    _storageEngine = factory->create(storageGlobalParams, _lockFile.get());
    _storageEngine->finishInit(); //void KVStorageEngine::finishInit() {}
}
*/

void ServiceContextMongoD::registerStorageEngine(const std::string& name,
                                                 const StorageEngine::Factory* factory) {
    // No double-registering.
    invariant(0 == _storageFactories.count(name));

    // Some sanity checks: the factory must exist,
    invariant(factory);

    // and all factories should be added before we pick a storage engine.
    invariant(NULL == _storageEngine);

    _storageFactories[name] = factory;
}

bool ServiceContextMongoD::isRegisteredStorageEngine(const std::string& name) {
    return _storageFactories.count(name);
}

StorageFactoriesIterator* ServiceContextMongoD::makeStorageFactoriesIterator() {
    return new StorageFactoriesIteratorMongoD(_storageFactories.begin(), _storageFactories.end());
}

StorageFactoriesIteratorMongoD::StorageFactoriesIteratorMongoD(const FactoryMapIterator& begin,
                                                               const FactoryMapIterator& end)
    : _curr(begin), _end(end) {}

bool StorageFactoriesIteratorMongoD::more() const {
    return _curr != _end;
}

const StorageEngine::Factory* StorageFactoriesIteratorMongoD::next() {
    return _curr++->second;
}

//客户端的每个请求（insert/update/delete/find/getmore），会生成一个唯一的OperationContext记录执行的上下文，OperationContext从请求解析时创建，到请求执行完成时释放
//生成一个OperationContext类
//ServiceContext::makeOperationContext
std::unique_ptr<OperationContext> ServiceContextMongoD::_newOpCtx(Client* client, unsigned opId) {
    invariant(&cc() == client);
    auto opCtx = stdx::make_unique<OperationContext>(client, opId);

    if (isMMAPV1()) {
        opCtx->setLockState(stdx::make_unique<MMAPV1LockerImpl>());
    } else {
        opCtx->setLockState(stdx::make_unique<DefaultLockerImpl>());
    }

	//OperationContext:setRecoveryUnit  WriteUnitOfWork   
	//newRecoveryUnit()，也就是wiredtiger对应WiredTigerKVEngine::newRecoveryUnit，该recoverUnit在WriteUnitOfWork构造中会用到
    opCtx->setRecoveryUnit(getGlobalStorageEngine()->newRecoveryUnit(), //wiredtiger对应WiredTigerKVEngine::newRecoveryUnit
                           OperationContext::kNotInUnitOfWork);
    return opCtx;
}

}  // namespace mongo
