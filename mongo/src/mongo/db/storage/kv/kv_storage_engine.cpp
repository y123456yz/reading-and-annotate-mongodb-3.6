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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kStorage

#include "mongo/db/storage/kv/kv_storage_engine.h"

#include <algorithm>

#include "mongo/db/operation_context_noop.h"
#include "mongo/db/storage/kv/kv_database_catalog_entry.h"
#include "mongo/db/storage/kv/kv_engine.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {

using std::string;
using std::vector;

/*
_mdb_catalog.wt里存储了所有集合的元数据，包括集合对应的WT table名字，集合的创建选项，集合的索引信息等，
WT存储引擎初始化时，会从_mdb_catalog.wt里读取所有的集合信息，并加载元信息到内存。
集合名与WT table名的对应关系可以通过db.collection.stats()获取

mongo-9552:PRIMARY> db.system.users.stats().wiredTiger.uri
statistics:table:admin/collection-10--1436312956560417970
也可以直接dump出_mdb_catalog.wt里的内容查看，dump出的内容为BSON格式，阅读起来不是很方便。

wt -C "extensions=[/usr/local/lib/libwiredtiger_snappy.so]" -h . dump table:_mdb_catalog

*/
namespace {
//KVStorageEngine::KVStorageEngine中创建对应的_mdb_catalog.wt元数据文件，例如第一次创建集群，实例启动的时候需要创建
const std::string catalogInfo = "_mdb_catalog";
}

//KVEngine(WiredTigerKVEngine)和StorageEngine(KVStorageEngine)的关系: KVStorageEngine._engine类型为WiredTigerKVEngine
//也就是KVStorageEngine类包含有WiredTigerKVEngine类成员

//KVStorageEngine._engine为WiredTigerKVEngine，通过KVStorageEngine._engine和WiredTigerKVEngine关联起来
class KVStorageEngine::RemoveDBChange : public RecoveryUnit::Change {
public:
	//删库操作通过这里记录下来
    RemoveDBChange(KVStorageEngine* engine, StringData db, KVDatabaseCatalogEntryBase* entry)
        : _engine(engine), _db(db.toString()), _entry(entry) {}
	
    virtual void commit() {
        delete _entry;
    }

    virtual void rollback() {
        stdx::lock_guard<stdx::mutex> lk(_engine->_dbsLock);
        _engine->_dbs[_db] = _entry;
    }

    KVStorageEngine* const _engine;
    const std::string _db;
    KVDatabaseCatalogEntryBase* const _entry;
};

//wiredtiger对应WiredTigerKVEngine  KVStorageEngine._engine为WiredTigerKVEngine

//mongod实例重启后首先需要加载_mdb_catalog.wt文件获取元数据信息

//WiredTigerFactory::create中new该类 
KVStorageEngine::KVStorageEngine(
	//对应WiredTigerKVEngine
    KVEngine* engine,
    const KVStorageEngineOptions& options,
    //默认为defaultDatabaseCatalogEntryFactory
    stdx::function<KVDatabaseCatalogEntryFactory> databaseCatalogEntryFactory)
    : _databaseCatalogEntryFactory(std::move(databaseCatalogEntryFactory)),
      _options(options),
      //WiredTigerKVEngine  KVStorageEngine._engine为WiredTigerKVEngine
      _engine(engine), 
      //wiredtiger是支持的，见 WiredTigerKVEngine::supportsDocLocking
      _supportsDocLocking(_engine->supportsDocLocking()),
      _supportsDBLocking(_engine->supportsDBLocking()) {
    uassert(28601,
            "Storage engine does not support --directoryperdb",
            !(options.directoryPerDB && !engine->supportsDirectoryPerDB()));

    OperationContextNoop opCtx(_engine->newRecoveryUnit()); //WiredTigerKVEngine::newRecoveryUnit
	
    bool catalogExists = engine->hasIdent(&opCtx, catalogInfo);

    if (options.forRepair && catalogExists) {
        log() << "Repairing catalog metadata";
        // TODO should also validate all BSON in the catalog.
        engine->repairIdent(&opCtx, catalogInfo).transitional_ignore();
    }

    if (!catalogExists) {
        WriteUnitOfWork uow(&opCtx);

		//WiredTigerKVEngine::createGroupedRecordStore
		//_mdb_catalog.wt元数据文件不存在，则创建对应的_mdb_catalog.wt元数据文件，例如第一次创建集群，实例启动的时候需要创建
        Status status = _engine->createGroupedRecordStore(
        
            &opCtx, catalogInfo, catalogInfo, CollectionOptions(), KVPrefix::kNotPrefixed);
        // BadValue is usually caused by invalid configuration string.
        // We still fassert() but without a stack trace.
        if (status.code() == ErrorCodes::BadValue) {
            fassertFailedNoTrace(28562);
        }
        fassert(28520, status);
        uow.commit();
    }

	//WiredTigerKVEngine::getGroupedRecordStore，默认返回StandardWiredTigerRecordStore类
    _catalogRecordStore = _engine->getGroupedRecordStore(
    //const std::string catalogInfo = "_mdb_catalog"; 也就是该StandardWiredTigerRecordStore对应_mdb_catalog文件
        &opCtx, catalogInfo, catalogInfo, CollectionOptions(), KVPrefix::kNotPrefixed);
    _catalog.reset(new KVCatalog(
		//StandardWiredTigerRecordStore
        _catalogRecordStore.get(), _options.directoryPerDB, _options.directoryForIndexes));
	//KVCatalog::init
	//KVStorageEngine::KVStorageEngine->KVCatalog::init初始化构造的时候就从元数据
	//文件_mdb_catalog.wt中获取元数据信息
	_catalog->init(&opCtx);

    std::vector<std::string> collections;
	//KVCatalog::getAllCollections 获取表信息，例如实例重启，需要通过_mdb_catalog.wt获取表元数据信息
    _catalog->getAllCollections(&collections);

    KVPrefix maxSeenPrefix = KVPrefix::kNotPrefixed;
	//从_mdb_catalog.wt中解析出库表元数据信息
	//mongod实例重启后首先需要加载_mdb_catalog.wt文件获取元数据信息
    for (size_t i = 0; i < collections.size(); i++) {
		//从_mdb_catalog.wt文件中获取库名和表名
        std::string coll = collections[i];
        NamespaceString nss(coll);
        string dbName = nss.db().toString();

        // No rollback since this is only for committed dbs.

		//这里对_dbs赋值，可以看出一个dbname对应一个db KVDatabaseCatalogEntryBase
        KVDatabaseCatalogEntryBase*& db = _dbs[dbName];
        if (!db) {
            db = _databaseCatalogEntryFactory(dbName, this).release();
        }

		//KVDatabaseCatalogEntryBase::initCollection调用
		//这样保障了同一个db下面的所有的表都存储在了KVDatabaseCatalogEntryBase._collections[]数组中
        db->initCollection(&opCtx, coll, options.forRepair);
        auto maxPrefixForCollection = _catalog->getMetaData(&opCtx, coll).getMaxPrefix();
        maxSeenPrefix = std::max(maxSeenPrefix, maxPrefixForCollection);
    }
	
    KVPrefix::setLargestPrefix(maxSeenPrefix);
    opCtx.recoveryUnit()->abandonSnapshot();
}

/**
 * This method reconciles differences between idents the KVEngine is aware of and the
 * KVCatalog. There are three differences to consider:
 *
 * First, a KVEngine may know of an ident that the KVCatalog does not. This method will drop
 * the ident from the KVEngine.
 *
 * Second, a KVCatalog may have a collection ident that the KVEngine does not. This is an
 * illegal state and this method fasserts.
 *
 * Third, a KVCatalog may have an index ident that the KVEngine does not. This method will
 * rebuild the index.
 */
/*
对MongoDB层可见的所有数据表，在_mdb_catalog表中维护了MongoDB需要的元数据，同样在WiredTiger层中，
会有一份对应的WiredTiger需要的元数据维护在WiredTiger.wt表中。因此，事实上这里有两份数据表的列表，
并且在某些情况下可能会存在不一致，比如，异常宕机的场景。因此MongoDB在启动过程中，会对这两份数据
进行一致性检查，如果是异常宕机启动过程，会以WiredTiger.wt表中的数据为准，对_mdb_catalog表中的记录进行修正。这个过程会需要遍历WiredTiger.wt表得到所有数据表的列表。

综上，可以看到，在MongoDB启动过程中，有多处涉及到需要从WiredTiger.wt表中读取数据表的元数据。
对这种需求，WiredTiger专门提供了一类特殊的『metadata』类型的cursor。
*/

//repairDatabasesAndCheckVersion中调用
StatusWith<std::vector<StorageEngine::CollectionIndexNamePair>>
	KVStorageEngine::reconcileCatalogAndIdents(OperationContext* opCtx) {
    // Gather all tables known to the storage engine and drop those that aren't cross-referenced
    // in the _mdb_catalog. This can happen for two reasons.
    //
    // First, collection creation and deletion happen in two steps. First the storage engine
    // creates/deletes the table, followed by the change to the _mdb_catalog. It's not assumed a
    // storage engine can make these steps atomic.
    //
    // Second, a replica set node in 3.6+ on supported storage engines will only persist "stable"
    // data to disk. That is data which replication guarantees won't be rolled back. The
    // _mdb_catalog will reflect the "stable" set of collections/indexes. However, it's not
    // expected for a storage engine's ability to persist stable data to extend to "stable
    // tables".
    
	//WiredTigerKVEngine::getAllIdents和KVCatalog::getAllIdents区别：
	// 1. WiredTigerKVEngine::getAllIdents对应WiredTiger.wt元数据文件，由wiredtiger存储引擎自己维护
	// 2. KVCatalog::getAllIdents对应_mdb_catalog.wt，由mongodb server层storage模块维护
	// 3. 这两个元数据相比较，冲突的时候collection以_mdb_catalog.wt为准，该表下面的索引以WiredTiger.wt为准
	//    参考KVStorageEngine::reconcileCatalogAndIdents

    std::set<std::string> engineIdents;
    {   //对应WiredTiger.wt
    	//WiredTigerKVEngine::getAllIdents
        std::vector<std::string> vec = _engine->getAllIdents(opCtx);
        engineIdents.insert(vec.begin(), vec.end());
        engineIdents.erase(catalogInfo);
    }

    std::set<std::string> catalogIdents;
    {   //对应_mdb_catalog.wt
    	//KVCatalog::getAllIdents
        std::vector<std::string> vec = _catalog->getAllIdents(opCtx);
        catalogIdents.insert(vec.begin(), vec.end());
    }

    // Drop all idents in the storage engine that are not known to the catalog. This can happen in
    // the case of a collection or index creation being rolled back.
    
    //把WiredTiger.wt中有，但是_mdb_catalog.wt中没有的元数据信息清除 
    for (const auto& it : engineIdents) {
		log() << "yang test ....reconcileCatalogAndIdents...... ident: " << it;
		//找到了相同的ident数据目录文件，继续下一个
        if (catalogIdents.find(it) != catalogIdents.end()) {
            continue;
        }

		//是否普通数据集合或者索引集合
        if (!_catalog->isUserDataIdent(it)) {
            continue;
        }

        const auto& toRemove = it;
        log() << "Dropping unknown ident: " << toRemove;
        WriteUnitOfWork wuow(opCtx);
		//WiredTigerKVEngine::dropIdent 删除对应ident文件
        fassertStatusOK(40591, _engine->dropIdent(opCtx, toRemove));
        wuow.commit();
    }

    // Scan all collections in the catalog and make sure their ident is known to the storage
    // engine. An omission here is fatal. A missing ident could mean a collection drop was rolled
    // back. Note that startup already attempts to open tables; this should only catch errors in
    // other contexts such as `recoverToStableTimestamp`.
    std::vector<std::string> collections;
    _catalog->getAllCollections(&collections);
	//如果_mdb_catalog.wt中有，但是WiredTiger.wt中没有对应元数据信息，则直接抛出异常
    for (const auto& coll : collections) {
        const auto& identForColl = _catalog->getCollectionIdent(coll);
        if (engineIdents.find(identForColl) == engineIdents.end()) {
            return {ErrorCodes::UnrecoverableRollbackError,
                    str::stream() << "Expected collection does not exist. NS: " << coll
                                  << " Ident: "
                                  << identForColl};
        }
    }

    // Scan all indexes and return those in the catalog where the storage engine does not have the
    // corresponding ident. The caller is expected to rebuild these indexes.
    std::vector<CollectionIndexNamePair> ret;
	//遍历_mdb_catalog.wt中的元数据集合信息
    for (const auto& coll : collections) {
        const BSONCollectionCatalogEntry::MetaData metaData = _catalog->getMetaData(opCtx, coll);
        for (const auto& indexMetaData : metaData.indexes) {
            const std::string& indexName = indexMetaData.name();
            std::string indexIdent = _catalog->getIndexIdent(opCtx, coll, indexName);
            if (engineIdents.find(indexIdent) != engineIdents.end()) {
                continue;
            }

			//_mdb_catalog.wt元数据有该索引，但是WiredTiger.wt元数据中却没用该索引，则说明需要重做该索引
            log() << "Expected index data is missing, rebuilding. NS: " << coll
                  << " Index: " << indexName << " Ident: " << indexIdent;

			//这些索引添加到ret返回
            ret.push_back(CollectionIndexNamePair(coll, indexName));
        }
    }

    return ret;
}

//ServiceContextMongoD::shutdownGlobalStorageEngineCleanly()调用
//shutdown回收处理
void KVStorageEngine::cleanShutdown() {
    for (DBMap::const_iterator it = _dbs.begin(); it != _dbs.end(); ++it) {
        delete it->second;
    }
    _dbs.clear();

    _catalog.reset(NULL);
    _catalogRecordStore.reset(NULL);

    _engine->cleanShutdown();
    // intentionally not deleting _engine
}

KVStorageEngine::~KVStorageEngine() {}

void KVStorageEngine::finishInit() {}

// ServiceContextMongoD::_newOpCtx->KVStorageEngine::newRecoveryUnit
RecoveryUnit* KVStorageEngine::newRecoveryUnit() {
    if (!_engine) {
        // shutdown
        return NULL;
    }
    return _engine->newRecoveryUnit(); //WiredTigerKVEngine::newRecoveryUnit
}

void KVStorageEngine::listDatabases(std::vector<std::string>* out) const {
    stdx::lock_guard<stdx::mutex> lk(_dbsLock);
    for (DBMap::const_iterator it = _dbs.begin(); it != _dbs.end(); ++it) {
        if (it->second->isEmpty())
            continue;
        out->push_back(it->first);
    }
}


KVDatabaseCatalogEntryBase* KVStorageEngine::getDatabaseCatalogEntry(OperationContext* opCtx,
                                                                     StringData dbName) {
    stdx::lock_guard<stdx::mutex> lk(_dbsLock);
    KVDatabaseCatalogEntryBase*& db = _dbs[dbName.toString()];
    if (!db) {
        // Not registering change since db creation is implicit and never rolled back.
        //defaultDatabaseCatalogEntryFactory
        //生成一个KVDatabaseCatalogEntry类
        db = _databaseCatalogEntryFactory(dbName, this).release();
    }
    return db;
}

Status KVStorageEngine::closeDatabase(OperationContext* opCtx, StringData db) {
    // This is ok to be a no-op as there is no database layer in kv.
    return Status::OK();
}

//DatabaseImpl::dropDatabase调用，先删除所有的表，然后再从_dbs数组中清除该db
Status KVStorageEngine::dropDatabase(OperationContext* opCtx, StringData db) {
    KVDatabaseCatalogEntryBase* entry;
	//找到对应的DB,没有直接返回
    {
        stdx::lock_guard<stdx::mutex> lk(_dbsLock);
        DBMap::const_iterator it = _dbs.find(db.toString());
        if (it == _dbs.end())
            return Status(ErrorCodes::NamespaceNotFound, "db not found to drop");
        entry = it->second;
    }

    // This is called outside of a WUOW since MMAPv1 has unfortunate behavior around dropping
    // databases. We need to create one here since we want db dropping to all-or-nothing
    // wherever possible. Eventually we want to move this up so that it can include the logOp
    // inside of the WUOW, but that would require making DB dropping happen inside the Dur
    // system for MMAPv1.
    //所有操作放入一个事务中
    WriteUnitOfWork wuow(opCtx);

    std::list<std::string> toDrop;
	//获取要删除的表信息
    entry->getCollectionNamespaces(&toDrop);

    for (std::list<std::string>::iterator it = toDrop.begin(); it != toDrop.end(); ++it) {
        string coll = *it;
		//KVDatabaseCatalogEntry::dropCollection
		//这里面会对数据表做真正的删除操作
        entry->dropCollection(opCtx, coll).transitional_ignore();
    }
    toDrop.clear();
    entry->getCollectionNamespaces(&toDrop);
    invariant(toDrop.empty());

    {
        stdx::lock_guard<stdx::mutex> lk(_dbsLock);
		//WiredTigerRecoveryUnit::registerChange 注册到WiredTigerRecoveryUnit，
		//最终在下面的commit中会调用WiredTigerRecoveryUnit::_commit()和WiredTigerRecoveryUnit::_abort()
		//执行RemoveDBChange的rollback或者commit
        opCtx->recoveryUnit()->registerChange(new RemoveDBChange(this, db, entry));
		//从_dbs数组清除该db信息
        _dbs.erase(db.toString());
    }

	//这里面最终执行registerChange注册的RemoveDBChange::commit释放内存
    wuow.commit();
    return Status::OK();
}

//FSyncLockThread::run()调用  db.adminCommand( { fsync: 1, lock: true } )
int KVStorageEngine::flushAllFiles(OperationContext* opCtx, bool sync) {
    return _engine->flushAllFiles(opCtx, sync);
}

Status KVStorageEngine::beginBackup(OperationContext* opCtx) {
    // We should not proceed if we are already in backup mode
    if (_inBackupMode)
        return Status(ErrorCodes::BadValue, "Already in Backup Mode");
    Status status = _engine->beginBackup(opCtx);
    if (status.isOK())
        _inBackupMode = true;
    return status;
}

void KVStorageEngine::endBackup(OperationContext* opCtx) {
    // We should never reach here if we aren't already in backup mode
    invariant(_inBackupMode);
    _engine->endBackup(opCtx);
    _inBackupMode = false;
}

bool KVStorageEngine::isDurable() const {
    return _engine->isDurable();
}

/*
ephemeralForTest存储引擎（ephemeralForTest Storage Engine）

MongoDB 3.2提供了一个新的用于测试的存储引擎。而不是一些元数据，用于测试的存储引擎不维护
任何磁盘数据，不需要在测试运行期间做清理。用于测试的存储引擎是无支持的。
*/
bool KVStorageEngine::isEphemeral() const {
    return _engine->isEphemeral();
}

SnapshotManager* KVStorageEngine::getSnapshotManager() const {
    return _engine->getSnapshotManager();
}

////CmdRepairDatabase::errmsgRun
Status KVStorageEngine::repairRecordStore(OperationContext* opCtx, const std::string& ns) {
    Status status = _engine->repairIdent(opCtx, _catalog->getCollectionIdent(ns));
    if (!status.isOK())
        return status;

    _dbs[nsToDatabase(ns)]->reinitCollectionAfterRepair(opCtx, ns);
    return Status::OK();
}

//ReplicationCoordinatorExternalStateImpl::startThreads
void KVStorageEngine::setJournalListener(JournalListener* jl) {
    _engine->setJournalListener(jl);
}

//StorageInterfaceImpl::setStableTimestamp调用
void KVStorageEngine::setStableTimestamp(Timestamp stableTimestamp) {
	//WiredTigerKVEngine::setStableTimestamp
    _engine->setStableTimestamp(stableTimestamp);
}


void KVStorageEngine::setInitialDataTimestamp(Timestamp initialDataTimestamp) {
	//WiredTigerKVEngine::setInitialDataTimestamp
    _engine->setInitialDataTimestamp(initialDataTimestamp);
}

/*
WiredTiger 提供设置 oldest timestamp 的功能，允许由 MongoDB 来设置该时间戳，含义是Read as of a timestamp
不会提供更小的时间戳来进行一致性读，也就是说，WiredTiger 无需维护 oldest timestamp 之前的所有历史版本。
MongoDB 层需要频繁（及时）更新 oldest timestamp，避免让 WT cache 压力太大。
参考https://mongoing.com/%3Fp%3D6084
*/
void KVStorageEngine::setOldestTimestamp(Timestamp oldestTimestamp) {
	//WiredTigerKVEngine::setOldestTimestamp
    _engine->setOldestTimestamp(oldestTimestamp);
}

bool KVStorageEngine::supportsRecoverToStableTimestamp() const {
    return _engine->supportsRecoverToStableTimestamp();
}

void KVStorageEngine::replicationBatchIsComplete() const {
    return _engine->replicationBatchIsComplete();
}
}  // namespace mongo
