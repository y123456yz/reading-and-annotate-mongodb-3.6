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

#pragma once

#include <map>
#include <string>

#include "mongo/base/status_with.h"
#include "mongo/base/string_data.h"
#include "mongo/bson/timestamp.h"
#include "mongo/db/storage/journal_listener.h"
#include "mongo/db/storage/kv/kv_catalog.h"
#include "mongo/db/storage/kv/kv_database_catalog_entry_base.h"
#include "mongo/db/storage/record_store.h"
#include "mongo/db/storage/storage_engine.h"
#include "mongo/stdx/functional.h"
#include "mongo/stdx/memory.h"
#include "mongo/stdx/mutex.h"

namespace mongo {

class KVCatalog;
class KVEngine;

struct KVStorageEngineOptions {
    bool directoryPerDB = false;
    bool directoryForIndexes = false;
    bool forRepair = false;
};

/*
 * The actual definition for this function is in
 * `src/mongo/db/storage/kv/kv_database_catalog_entry.cpp` This unusual forward declaration is to
 * facilitate better linker error messages.  Tests need to pass a mock construction factory, whereas
 * main implementations should pass the `default...` factory which is linked in with the main
 * `KVDatabaseCatalogEntry` code.
 */
std::unique_ptr<KVDatabaseCatalogEntryBase> defaultDatabaseCatalogEntryFactory(
    const StringData name, KVStorageEngine* const engine);

using KVDatabaseCatalogEntryFactory = decltype(defaultDatabaseCatalogEntryFactory);


/*
DatabaseHolder
DatabaseHolder是Mongodb数据库操作的入口，提供了打开、关闭数据库的接口，其中openDb接口会创建一个Database对象。

C#

class DatabaseHoler {
public:
Database* openDb(string dbname);
void close(string dbname);
Database* get(string dbname);
pirate:
map<string, Database*> dbs;
};

class DatabaseHoler {
public:
Database* openDb(string dbname);
void close(string dbname);
Database* get(string dbname);
pirate:
map<string, Database*> dbs;
};
Database
Database对象代表Mongodb里的一个db，其提供关于集合操作的所有接口，包括创建、删除、重命名集合，创建
Database时会根据mongod进程的storageEngine配置来决定使用哪个存储引擎。

C#

class Database {
public:
Collection* createCollection(string& coll_name);
void dropCollection(string& coll_name);
Collection* getCollection(string& coll_name);
private:
map<string, Collection*> _collections;
};

class Database {
public:
Collection* createCollection(string& coll_name);
void dropCollection(string& coll_name);
Collection* getCollection(string& coll_name);
private:
map<string, Collection*> _collections;
};
Collection
Collection代表Mongodb里的一个集合，其提供关于文档增删改查的所有接口，这些接口最终会调用RecordStore里
的相应接口实现。

C#

class Collection {
public:
insertDocument();
deleteDocument();
updateDocument();
findDoc();
private:
RecordStore* _recordStore;
};


class Collection {
public:
insertDocument();
deleteDocument();
updateDocument();
findDoc();
private:
RecordStore* _recordStore;
};
GlobalEnvironmentMongoD
GlobalEnvironmentMongoD是mongod的全局运行环境信息，所有的存储引擎在启动时会先注册，mongd根据配置设置
当前使用的存储引擎; 注册引擎时，提供引擎的名字（如mmapv1、wiredTiger）及用于创建引擎对象的工厂方法
（工厂实现create的接口，用于创建StorageEngine对象）。

class GlobalEnvironmentMongoD {
pubic:
void registerStorageEngine(const std::string& name,
const StorageEngine::Factory* factory);
void setGlobalStorageEngine(const std::string& name);
StorageEngine* getGlobalStorageEngine();
private:
StorageEngine* _storageEngine; // 当前存储引擎
FactoryMap _storageFactories;
}；

class GlobalEnvironmentMongoD {
pubic:
void registerStorageEngine(const std::string& name,
const StorageEngine::Factory* factory);
void setGlobalStorageEngine(const std::string& name);
StorageEngine* getGlobalStorageEngine();
private:
StorageEngine* _storageEngine; // 当前存储引擎
FactoryMap _storageFactories;
}；
StorageEngine
StorageEngine定义了一系列Mongdb存储引擎需要实现的接口，是一个接口类，所有的存储引擎需继承这个类，实现
自身的存储逻辑。 getDatabaseCatalogEntry接口用于获取一个DatabaseCatalogEntry对象，该对象实现了关于集合、
文档操作的接口。

C#

class StorageEngine {
    public:
    DatabaseCatalogEntry* getDatabaseCatalogEntry(string& ns);
    void listDatabases( std::vector<std::string>* out );
};

class DatabaseCatalogEntry {
    public:
    createCollection();
    dropCollection();
    getRecordStore(); / * 实现文档操作接口 * /
};

class StorageEngine {
public:
DatabaseCatalogEntry* getDatabaseCatalogEntry(string& ns);
void listDatabases( std::vector<std::string>* out );
};
 
class DatabaseCatalogEntry {
public:
createCollection();
dropCollection();
getRecordStore(); / * 实现文档操作接口 * /
};
MMAPV1StorageEngine
MMAPV1StorageEngine包含了mmapv1存储引擎的所有实现逻辑。

KVStorageEngine
KVStorageEngine实际上不是一个真正存储引擎的实现，只是为了方便接入wiredTiger、rocks等KV类型的存储引擎
而增加的一个抽象层。 KVStorageEngine实现了StorageEngine的接口，但其实现由KVEngine类代理，wiredTiger等
KV存储引擎接入mongdb时，只需实现KVEngine定义的接口即可。

WiredTigerKVEngine
WiredTigerKVEngine继承KVEngine，实现KVEngine的接口，其他的引擎如RocksEngine类似。

见http://blog.jobbole.com/89351/
*/

/*
KVStorageEngine
KVStorageEngine实际上不是一个真正存储引擎的实现，只是为了方便接入wiredTiger、rocks等KV类型的存储引擎而增加
的一个抽象层。 KVStorageEngine实现了StorageEngine的接口，但其实现由KVEngine类代理，wiredTiger等KV存储引擎
接入mongdb时，只需实现KVEngine定义的接口即可。
*/
//WiredTigerFactory::create中new改类
class KVStorageEngine final : public StorageEngine {
public:
    /**
     * @param engine - ownership passes to me
     */
    KVStorageEngine(KVEngine* engine,
                    const KVStorageEngineOptions& options = KVStorageEngineOptions(),
                    stdx::function<KVDatabaseCatalogEntryFactory> databaseCatalogEntryFactory =
                        defaultDatabaseCatalogEntryFactory);

    virtual ~KVStorageEngine();

    virtual void finishInit();

    virtual RecoveryUnit* newRecoveryUnit();

    virtual void listDatabases(std::vector<std::string>* out) const;

    KVDatabaseCatalogEntryBase* getDatabaseCatalogEntry(OperationContext* opCtx,
                                                        StringData db) override;

    virtual bool supportsDocLocking() const {
        return _supportsDocLocking;
    }

    virtual bool supportsDBLocking() const {
        return _supportsDBLocking;
    }

    virtual Status closeDatabase(OperationContext* opCtx, StringData db);

    virtual Status dropDatabase(OperationContext* opCtx, StringData db);

    virtual int flushAllFiles(OperationContext* opCtx, bool sync);

    virtual Status beginBackup(OperationContext* opCtx);

    virtual void endBackup(OperationContext* opCtx);

    virtual bool isDurable() const;

    virtual bool isEphemeral() const;

    virtual Status repairRecordStore(OperationContext* opCtx, const std::string& ns);

    virtual void cleanShutdown();

    virtual void setStableTimestamp(Timestamp stableTimestamp) override;

    virtual void setInitialDataTimestamp(Timestamp initialDataTimestamp) override;

    virtual void setOldestTimestamp(Timestamp oldestTimestamp) override;

    virtual bool supportsRecoverToStableTimestamp() const override;

    virtual void replicationBatchIsComplete() const override;

    SnapshotManager* getSnapshotManager() const final;

    void setJournalListener(JournalListener* jl) final;

    // ------ kv ------

    KVEngine* getEngine() {
        return _engine.get();
    }
    const KVEngine* getEngine() const {
        return _engine.get();
    }

    KVCatalog* getCatalog() {
        return _catalog.get();
    }
    const KVCatalog* getCatalog() const {
        return _catalog.get();
    }

    /**
     * Drop abandoned idents. Returns a parallel list of index name, index spec pairs to rebuild.
     */
    StatusWith<std::vector<StorageEngine::CollectionIndexNamePair>> reconcileCatalogAndIdents(
        OperationContext* opCtx) override;

private:
    class RemoveDBChange;

    stdx::function<KVDatabaseCatalogEntryFactory> _databaseCatalogEntryFactory;

    KVStorageEngineOptions _options;

    // This must be the first member so it is destroyed last.
    //WiredTigerFactory::create->new KVStorageEngine(kv, options);中调用赋值
    std::unique_ptr<KVEngine> _engine; //WiredTigerKVEngine类型

    const bool _supportsDocLocking;
    const bool _supportsDBLocking;

    std::unique_ptr<RecordStore> _catalogRecordStore;
    std::unique_ptr<KVCatalog> _catalog;

    typedef std::map<std::string, KVDatabaseCatalogEntryBase*> DBMap;
    DBMap _dbs;
    mutable stdx::mutex _dbsLock;

    // Flag variable that states if the storage engine is in backup mode.
    bool _inBackupMode = false;
};
}  // namespace mongo
