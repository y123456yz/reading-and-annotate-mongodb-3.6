/**
 *    Copyright (C) 2014 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kStorage

#if defined(__linux__)
#include <sys/vfs.h>
#endif

#include "mongo/platform/basic.h"

#include "mongo/base/init.h"
#include "mongo/db/catalog/collection_options.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/service_context.h"
#include "mongo/db/service_context_d.h"
#include "mongo/db/storage/kv/kv_storage_engine.h"
#include "mongo/db/storage/storage_engine_lock_file.h"
#include "mongo/db/storage/storage_engine_metadata.h"
#include "mongo/db/storage/storage_options.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_global_options.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_index.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_kv_engine.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_parameters.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_record_store.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_server_status.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_util.h"
#include "mongo/util/log.h"

namespace mongo {

//下面的WiredTigerEngineInit初始化注册
//确定对应的存储引擎
namespace {
class WiredTigerFactory : public StorageEngine::Factory {
public:
    virtual ~WiredTigerFactory() {}
	//ServiceContextMongoD::initializeGlobalStorageEngine()中调用执行，执行该WiredTigerFactory::create
	//根据params参数构造KVStorageEngine类  
    virtual StorageEngine* create(const StorageGlobalParams& params,
                                  const StorageEngineLockFile* lockFile) const {
        if (lockFile && lockFile->createdByUncleanShutdown()) {
            warning() << "Recovering data from the last clean checkpoint.";
        }

#if defined(__linux__)
// This is from <linux/magic.h> but that isn't available on all systems.
// Note that the magic number for ext4 is the same as ext2 and ext3.
#define EXT4_SUPER_MAGIC 0xEF53
        {
            struct statfs fs_stats;
			//statfs获得硬盘使用情况
            int ret = statfs(params.dbpath.c_str(), &fs_stats);

            if (ret == 0 && fs_stats.f_type == EXT4_SUPER_MAGIC) {
                log() << startupWarningsLog;
				//WiredTiger存储引擎最好使用XFS文件系统
                log() << "** WARNING: Using the XFS filesystem is strongly recommended with the "
                         "WiredTiger storage engine"
                      << startupWarningsLog;
                log() << "**          See "
                         "http://dochub.mongodb.org/core/prodnotes-filesystem"
                      << startupWarningsLog;
            }
        }
#endif
		//wiredTigerGlobalOptions.cacheSizeGB GB转换为MB
        size_t cacheMB = WiredTigerUtil::getCacheSizeMB(wiredTigerGlobalOptions.cacheSizeGB);
        const bool ephemeral = false;
        WiredTigerKVEngine* kv =
            new WiredTigerKVEngine(getCanonicalName().toString(),
                                   params.dbpath,
                                   getGlobalServiceContext()->getFastClockSource(),
                                   wiredTigerGlobalOptions.engineConfig,
                                   cacheMB,
                                   //mongod --journal 
                                   params.dur,
                                   ephemeral,
                                   params.repair,
                                   params.readOnly);
        kv->setRecordStoreExtraOptions(wiredTigerGlobalOptions.collectionConfig);
        kv->setSortedDataInterfaceExtraOptions(wiredTigerGlobalOptions.indexConfig);
        // Intentionally leaked.
        new WiredTigerServerStatusSection(kv);
        new WiredTigerEngineRuntimeConfigParameter(kv);

        KVStorageEngineOptions options;
        options.directoryPerDB = params.directoryperdb;
        options.directoryForIndexes = wiredTigerGlobalOptions.directoryForIndexes;
        options.forRepair = params.repair;

		//WiredTigerFactory::create->new KVStorageEngine(kv, options);
        return new KVStorageEngine(kv, options);
    }

    virtual StringData getCanonicalName() const {
        return kWiredTigerEngineName;
    }

	//wiredtiger存储引擎的配置参数检查，见wiredtiger_config_validate   普通collections数据文件对应的wiredtiger参数做检查
    virtual Status validateCollectionStorageOptions(const BSONObj& options) const {
        return WiredTigerRecordStore::parseOptionsField(options).getStatus();
    }

	//索引index的配置参数做检查
    virtual Status validateIndexStorageOptions(const BSONObj& options) const {
        return WiredTigerIndex::parseIndexOptions(options).getStatus();
    }

	//检查storage.bson的目的是为了保障每次用的存储引擎是一致的，例如如果重启mongod，之前用wiredtiger，现在改为mmap，则会报错
    virtual Status validateMetadata(const StorageEngineMetadata& metadata,
                                    const StorageGlobalParams& params) const {
		//StorageEngineMetadata::validateStorageEngineOption，检查storage.bson中的directoryPerDB字段的内容是否和预期的params.directoryperdb相同
		Status status =
            metadata.validateStorageEngineOption("directoryPerDB", params.directoryperdb);
        if (!status.isOK()) {
            return status;
        }

	//StorageEngineMetadata::validateStorageEngineOption，检查storage.bson中的directoryForIndexes字段的内容是否和预期的wiredTigerGlobalOptions.directoryForIndexes相同
        status = metadata.validateStorageEngineOption("directoryForIndexes",
                                                      wiredTigerGlobalOptions.directoryForIndexes);
        if (!status.isOK()) {
            return status;
        }

        // If the 'groupCollections' field does not exist in the 'storage.bson' file, the
        // data-format of existing tables is as if 'groupCollections' is false. Passing this in
        // prevents validation from accepting 'params.groupCollections' being true when a "group
        // collections" aware mongod is launched on an 3.4- dbpath.
        const bool kDefaultGroupCollections = false;
        status =
            metadata.validateStorageEngineOption("groupCollections",
                                                 params.groupCollections,
                                                 boost::optional<bool>(kDefaultGroupCollections));
        if (!status.isOK()) {
            return status;
        }

        return Status::OK();
    }

	//initializeGlobalStorageEngine中调用执行，第一次启动的时候创建storage.bson文件，避免重启mongod并且修改存储引擎等配置参数，从而进行报错提示
    virtual BSONObj createMetadataOptions(const StorageGlobalParams& params) const {
        BSONObjBuilder builder;
        builder.appendBool("directoryPerDB", params.directoryperdb);
        builder.appendBool("directoryForIndexes", wiredTigerGlobalOptions.directoryForIndexes);
        builder.appendBool("groupCollections", params.groupCollections);
        return builder.obj();
    }

    bool supportsReadOnly() const final {
        return true;
    }
};
}  // namespace

MONGO_INITIALIZER_WITH_PREREQUISITES(WiredTigerEngineInit, ("SetGlobalEnvironment"))
(InitializerContext* context) {
    getGlobalServiceContext()->registerStorageEngine(kWiredTigerEngineName,
                                                     new WiredTigerFactory());

    return Status::OK();
}
}

