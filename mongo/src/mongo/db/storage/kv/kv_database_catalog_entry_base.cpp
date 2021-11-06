/**
 *    Copyright (C) 2016 MongoDB Inc.
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
/*  _mdb_catalog.wt内容
{
	ns: "test.test1",
	md: {
		ns: "test.test1",
		options: {
			uuid: UUID("520904ec-0432-4c00-a15d-788e2f5d707b")
		},
		indexes: [{
			spec: {
				v: 2,
				key: {
					_id: 1
				},
				name: "_id_",
				ns: "test.test1"
			},
			ready: true,
			multikey: false,
			multikeyPaths: {
				_id: BinData(0, 00)
			},
			head: 0,
			prefix: -1
		}, {
			spec: {
				v: 2,
				key: {
					name: 1.0,
					age: 1.0
				},
				name: "name_1_age_1",
				ns: "test.test1",
				background: true
			},
			ready: true,
			multikey: false,
			multikeyPaths: {
				name: BinData(0, 00),
				age: BinData(0, 00)
			},
			head: 0,
			prefix: -1
		}, {
			spec: {
				v: 2,
				key: {
					zipcode: 1.0
				},
				name: "zipcode_1",
				ns: "test.test1",
				background: true
			},
			ready: true,
			multikey: false,
			multikeyPaths: {
				zipcode: BinData(0, 00)
			},
			head: 0,
			prefix: -1
		}],
		prefix: -1
	},
	idxIdent: {
		_id_: "test/index/8-380857198902467499",
		name_1_age_1: "test/index/0--6948813758302814892",
		zipcode_1: "test/index/3--6948813758302814892"
	},
	ident: "test/collection/7-380857198902467499"
}

*/
#include "mongo/platform/basic.h"

#include <memory>

#include "mongo/db/storage/kv/kv_database_catalog_entry.h"

#include "mongo/db/namespace_string.h"
#include "mongo/db/operation_context.h"
#include "mongo/db/storage/kv/kv_catalog_feature_tracker.h"
#include "mongo/db/storage/kv/kv_collection_catalog_entry.h"
#include "mongo/db/storage/kv/kv_engine.h"
#include "mongo/db/storage/kv/kv_storage_engine.h"
#include "mongo/db/storage/recovery_unit.h"

namespace mongo {

using std::string;
using std::vector;

//KVDatabaseCatalogEntryBase::createCollection中构造使用
class KVDatabaseCatalogEntryBase::AddCollectionChange : public RecoveryUnit::Change {
public:
    AddCollectionChange(OperationContext* opCtx,
                        KVDatabaseCatalogEntryBase* dce,
                        StringData collection,
                        StringData ident,
                        bool dropOnRollback)
        : _opCtx(opCtx),
          _dce(dce),
          _collection(collection.toString()),
          _ident(ident.toString()),
          _dropOnRollback(dropOnRollback) {}

	
	////KVDatabaseCatalogEntryBase::commit->WiredTigerKVEngine::dropIdent删表中调用，真正的表删除
	//KVCollectionCatalogEntry::RemoveIndexChange::commit()->WiredTigerKVEngine::dropIdent 删索引，中调用，真正删除索引在这里
    virtual void commit() {}
    virtual void rollback() {
        if (_dropOnRollback) {
            // Intentionally ignoring failure
            //真正的删表操作在这里
            _dce->_engine->getEngine()->dropIdent(_opCtx, _ident).transitional_ignore();
        }

        const CollectionMap::iterator it = _dce->_collections.find(_collection);
        if (it != _dce->_collections.end()) {
            delete it->second;
            _dce->_collections.erase(it);
        }
    }

    OperationContext* const _opCtx;
    KVDatabaseCatalogEntryBase* const _dce;
    const std::string _collection;
    const std::string _ident;
    const bool _dropOnRollback;
};

class KVDatabaseCatalogEntryBase::RemoveCollectionChange : public RecoveryUnit::Change {
public:
	////真正的表删除在这里，通过这里触发，参考KVDatabaseCatalogEntryBase::dropCollection
    RemoveCollectionChange(OperationContext* opCtx,
                           KVDatabaseCatalogEntryBase* dce,
                           StringData collection,
                           StringData ident,
                           KVCollectionCatalogEntry* entry,
                           bool dropOnCommit)
        : _opCtx(opCtx),
          _dce(dce),
          _collection(collection.toString()),
          _ident(ident.toString()),
          _entry(entry),
          _dropOnCommit(dropOnCommit) {}

	
    virtual void commit() {
        delete _entry;

        // Intentionally ignoring failure here. Since we've removed the metadata pointing to the
        // collection, we should never see it again anyway.
        if (_dropOnCommit)
			//表真正清理在这里
			//WiredTigerKVEngine::dropIdent
            _dce->_engine->getEngine()->dropIdent(_opCtx, _ident).transitional_ignore();
    }

    virtual void rollback() {
        _dce->_collections[_collection] = _entry;
    }

    OperationContext* const _opCtx;
    KVDatabaseCatalogEntryBase* const _dce;
    const std::string _collection;
    const std::string _ident;
    KVCollectionCatalogEntry* const _entry;
    const bool _dropOnCommit;
};

KVDatabaseCatalogEntryBase::KVDatabaseCatalogEntryBase(StringData db, KVStorageEngine* engine)
    : DatabaseCatalogEntry(db), _engine(engine) {}

KVDatabaseCatalogEntryBase::~KVDatabaseCatalogEntryBase() {
    for (CollectionMap::const_iterator it = _collections.begin(); it != _collections.end(); ++it) {
        delete it->second;
    }
    _collections.clear();
}

//检查该DB下面是否有可用表信息
bool KVDatabaseCatalogEntryBase::exists() const {
    return !isEmpty();
}

//是否有表存在
bool KVDatabaseCatalogEntryBase::isEmpty() const {
    return _collections.empty();
}

//有表信息则说明有数据存在
bool KVDatabaseCatalogEntryBase::hasUserData() const {
    return !isEmpty();
}

//db.runCommand({ listDatabases : 1 })获取所有库的磁盘信息
/*
> db.runCommand({ listDatabases : 1 })
{
        "databases" : [
                {
                        "name" : "admin",
                        "sizeOnDisk" : 32768,
                        "empty" : false
                },
                {
                        "name" : "config",
                        "sizeOnDisk" : 73728,
                        "empty" : false
                },
                {
                        "name" : "local",
                        "sizeOnDisk" : 77824,
                        "empty" : false
                },
                {
                        "name" : "test",
                        "sizeOnDisk" : 90112,
                        "empty" : false
                }
        ],
        "totalSize" : 274432,
        "ok" : 1
}
*/
//磁盘数据大小=所有表的数据+所有表的索引总和
int64_t KVDatabaseCatalogEntryBase::sizeOnDisk(OperationContext* opCtx) const {
    int64_t size = 0;

    for (CollectionMap::const_iterator it = _collections.begin(); it != _collections.end(); ++it) {
        const KVCollectionCatalogEntry* coll = it->second;
        if (!coll)
            continue;
        size += coll->getRecordStore()->storageSize(opCtx);

        vector<string> indexNames;
        coll->getAllIndexes(opCtx, &indexNames);

        for (size_t i = 0; i < indexNames.size(); i++) {
            string ident =
                _engine->getCatalog()->getIndexIdent(opCtx, coll->ns().ns(), indexNames[i]);
            size += _engine->getEngine()->getIdentSize(opCtx, ident);
        }
    }

    return size;
}

void KVDatabaseCatalogEntryBase::appendExtraStats(OperationContext* opCtx,
                                                  BSONObjBuilder* out,
                                                  double scale) const {}

Status KVDatabaseCatalogEntryBase::currentFilesCompatible(OperationContext* opCtx) const {
    // Delegate to the FeatureTracker as to whether the data files are compatible or not.
    return _engine->getCatalog()->getFeatureTracker()->isCompatibleWithCurrentCode(opCtx);
}

//获取所有的表名存入out数组
void KVDatabaseCatalogEntryBase::getCollectionNamespaces(std::list<std::string>* out) const {
    for (CollectionMap::const_iterator it = _collections.begin(); it != _collections.end(); ++it) {
        out->push_back(it->first);
    }
}

//获取表对应的KVCollectionCatalogEntry，一个表对应一个KVCollectionCatalogEntry
CollectionCatalogEntry* KVDatabaseCatalogEntryBase::getCollectionCatalogEntry(StringData ns) const {
    CollectionMap::const_iterator it = _collections.find(ns.toString());
    if (it == _collections.end()) {
        return NULL;
    }

    return it->second;
}


//WiredTigerIndexUnique(唯一索引文件操作)、WiredTigerIndexStandard(普通索引文件操作)
//WiredTigerRecordStore(表数据文件操作)

//获取对该表进行底层数据KV操作的WiredTigerRecordStore
RecordStore* KVDatabaseCatalogEntryBase::getRecordStore(StringData ns) const {
    CollectionMap::const_iterator it = _collections.find(ns.toString());
    if (it == _collections.end()) {
        return NULL;
    }

	//KVCollectionCatalogEntry::getRecordStore,也就是获取KVCollectionCatalogEntry._recordStore成员
	//默认为默认为StandardWiredTigerRecordStore类型
	
	//KVCollectionCatalogEntry::getRecordStore, 也就是获取KVCollectionCatalogEntry._recordStore成员信息
    return it->second->getRecordStore();
}


//insertBatchAndHandleErrors->makeCollection->mongo::userCreateNS->mongo::userCreateNSImpl
//->DatabaseImpl::createCollection->KVDatabaseCatalogEntryBase::createCollection
// Collection* createCollection调用
//开始调用底层WT存储引擎相关接口建表，同时生成一个KVCollectionCatalogEntry存入_collections数组

//KVDatabaseCatalogEntryBase::createCollection和KVDatabaseCatalogEntryBase::initCollection的区别如下:
// 1. KVDatabaseCatalogEntryBase::createCollection对应的是新键的表
// 2. KVDatabaseCatalogEntryBase::initCollection对应的是mongod重启，从_mdb_catalog.wt元数据文件中加载的表


//注意，这里面只有空表对应数据KV ident操作，空表对应id索引对应索引文件idxident在外层DatabaseImpl::createCollection中实现
Status KVDatabaseCatalogEntryBase::createCollection(OperationContext* opCtx,
                                                    StringData ns,
                                                    const CollectionOptions& options,
                                                    bool allocateDefaultSpace) {
    invariant(opCtx->lockState()->isDbLockedForMode(name(), MODE_X));

    if (ns.empty()) {
        return Status(ErrorCodes::BadValue, "Collection namespace cannot be empty");
    }

    if (_collections.count(ns.toString())) {
        invariant(_collections[ns.toString()]);
        return Status(ErrorCodes::NamespaceExists, "collection already exists");
    }

    KVPrefix prefix = KVPrefix::getNextPrefix(NamespaceString(ns));

	//将collection的ns、ident存储到元数据文件_mdb_catalog中。
    // need to create it  调用KVCatalog::newCollection 创建wiredtiger 数据文件
    //更新_idents，记录下集合对应元数据信息，也就是集合路径  集合uuid 集合索引，以及在元数据_mdb_catalog.wt中的位置
	//KVCatalog::newCollection，跟新_mdb_catalog.wt文件元数据，有新表了
	Status status = _engine->getCatalog()->newCollection(opCtx, ns, options, prefix);
    if (!status.isOK())
        return status;

	//也就是newCollection中生成的集合ident，也就是元数据元数据_mdb_catalog.wt文件路径
	//获取对应表的元数据信息
    string ident = _engine->getCatalog()->getCollectionIdent(ns);  
	
	//WiredTigerKVEngine::createGroupedRecordStore(数据文件相关)  
	//WiredTigerKVEngine::createGroupedSortedDataInterface(索引文件相关)
	//调用WT存储引擎的create接口建表，底层建索引表
    status = _engine->getEngine()->createGroupedRecordStore(opCtx, ns, ident, options, prefix);
    if (!status.isOK())
        return status;

    // Mark collation feature as in use if the collection has a non-simple default collation.
    if (!options.collation.isEmpty()) {
        const auto feature = KVCatalog::FeatureTracker::NonRepairableFeature::kCollation;
        if (_engine->getCatalog()->getFeatureTracker()->isNonRepairableFeatureInUse(opCtx,
                                                                                    feature)) {
            _engine->getCatalog()->getFeatureTracker()->markNonRepairableFeatureAsInUse(opCtx,
                                                                                        feature);
        }
    }


	//新建collection这个事件记录下来
    opCtx->recoveryUnit()->registerChange(new AddCollectionChange(opCtx, this, ns, ident, true));
	//WiredTigerKVEngine::getGroupedRecordStore
	//生成StandardWiredTigerRecordStore类,该类和表实际上关联起来，对该类的相关接口操作实际上就是对表的KV操作
	//一个表和一个StandardWiredTigerRecordStore对应，以后对该表的底层存储引擎KV操作将由该类实现
	auto rs = _engine->getEngine()->getGroupedRecordStore(opCtx, ns, ident, options, prefix);
    invariant(rs);

	//存到map表中，把WiredTigerKVEngine  
	//最终一个表对应一个KVCollectionCatalogEntry，存储到_collections数组中
    _collections[ns.toString()] = new KVCollectionCatalogEntry(
       //WiredTigerKVEngine--存储引擎    
       //     KVStorageEngine::getCatalog(KVStorageEngine._catalog(KVCatalog类型))---"_mdb_catalog.wt"元数据接口
       //                                                        ident-----表对应数据文件目录
       //                                                            StandardWiredTigerRecordStore--底层WT存储引擎KV操作--类似表底层存储引擎KV接口
        _engine->getEngine(), _engine->getCatalog(), ns, ident, std::move(rs));

    return Status::OK();
}

//KVStorageEngine::KVStorageEngine->KVDatabaseCatalogEntryBase::initCollection从元数据_mdb_catalog.wt中加载表信息
//当mongod重启的时候会，会调用KVStorageEngine::KVStorageEngine调用本接口

//KVDatabaseCatalogEntryBase::createCollection和KVDatabaseCatalogEntryBase::initCollection的区别如下:
// 1. KVDatabaseCatalogEntryBase::createCollection对应的是新键的表
// 2. KVDatabaseCatalogEntryBase::initCollection对应的是mongod重启，从_mdb_catalog.wt元数据文件中加载的表
void KVDatabaseCatalogEntryBase::initCollection(OperationContext* opCtx,
                                                const std::string& ns,
                                                bool forRepair) {
    invariant(!_collections.count(ns));
	
	//获取ns对应wt文件名，也就是磁盘路径名
    const std::string ident = _engine->getCatalog()->getCollectionIdent(ns);

    std::unique_ptr<RecordStore> rs;
    if (forRepair) {
        // Using a NULL rs since we don't want to open this record store before it has been
        // repaired. This also ensures that if we try to use it, it will blow up.
        rs = nullptr;
    } else {
    	//获取该表的元数据信息
        BSONCollectionCatalogEntry::MetaData md = _engine->getCatalog()->getMetaData(opCtx, ns);
		//WiredTigerKVEngine::getGroupedRecordStore
		//获取对该表进行底层KV操作的RecordStore
		rs = _engine->getEngine()->getGroupedRecordStore(opCtx, ns, ident, md.options, md.prefix);
        invariant(rs);
    }

    // No change registration since this is only for committed collections
   //WiredTigerKVEngine--存储引擎    
   //     KVStorageEngine::getCatalog(KVStorageEngine._catalog(KVCatalog类型))---"_mdb_catalog.wt"元数据接口
   //                                           StandardWiredTigerRecordStore--底层WT存储引擎KV操作--类似表底层存储引擎KV接口
    _collections[ns] = new KVCollectionCatalogEntry(
   //WiredTigerKVEngine--存储引擎    
   //     KVStorageEngine::getCatalog(KVStorageEngine._catalog(KVCatalog类型))---"_mdb_catalog.wt"元数据接口
   //                                                        ident-----表对应数据文件目录
   //                                                            StandardWiredTigerRecordStore--底层WT存储引擎KV操作--类似表底层存储引擎KV接口
        _engine->getEngine(), _engine->getCatalog(), ns, ident, std::move(rs));
}

void KVDatabaseCatalogEntryBase::reinitCollectionAfterRepair(OperationContext* opCtx,
                                                             const std::string& ns) {
    // Get rid of the old entry.
    CollectionMap::iterator it = _collections.find(ns);
    invariant(it != _collections.end());
    delete it->second;
    _collections.erase(it);

    // Now reopen fully initialized.
    initCollection(opCtx, ns, false);
}

//DatabaseImpl::renameCollection调用，集合重命名
//从该接口可以看出，一个表操作需要包含以下信息：
// 1. 更新sizeStorer.wt size元数据文件中对应的表，因为表名已经改变了
// 2. 表名修改后，_mdb_catalog.wt元数据也需要更新，包括表名和ident等，ident是通过表名生成的，表名改了，因此ident也需要修改
// 3. 表名改了后，ident也变了，因此操作该表的WiredTigerRecordStore也需要改变，需要重新生成
// 4. 在cache中根据上面新的表名、新的ident、新的WiredTigerRecordStore生成新的KVCollectionCatalogEntry，该entry在内存cache中缓存起来
//  疑问？为何没有对idxIdent(name_1_age_1: "test/index/0--6948813758302814892")改名，原因是idxIdent中对应的test是库，没有表信息
Status KVDatabaseCatalogEntryBase::renameCollection(OperationContext* opCtx,
                                                    StringData fromNS,
                                                    StringData toNS,
                                                    bool stayTemp) {
    invariant(opCtx->lockState()->isDbLockedForMode(name(), MODE_X));

    RecordStore* originalRS = NULL;

	//获取表信息
    CollectionMap::const_iterator it = _collections.find(fromNS.toString());
    if (it == _collections.end()) {
        return Status(ErrorCodes::NamespaceNotFound, "rename cannot find collection");
    }

	//获取操作该表的WiredTigerRecordStore
    originalRS = it->second->getRecordStore();

	//目的表明是否已经存在，存在说明重复了，直接报错
    it = _collections.find(toNS.toString());
    if (it != _collections.end()) {
        return Status(ErrorCodes::NamespaceExists, "for rename to already exists");
    }

	//原表对应ident
    const std::string identFrom = _engine->getCatalog()->getCollectionIdent(fromNS);

	//WiredTigerKVEngine::okToRename
	//cache中记录的表数据大小，表重命名后，记录数元数据文件sizeStorer.wt也需要对应修改
    Status status = _engine->getEngine()->okToRename(opCtx, fromNS, toNS, identFrom, originalRS);
    if (!status.isOK())
        return status;

	//_mdb_catalog.wt元数据文件中的表名需要更新，元数据也需要更新，现在_mdb_catalog.wt中是新表的元数据信息了
	//KVCatalog::renameCollection
	status = _engine->getCatalog()->renameCollection(opCtx, fromNS, toNS, stayTemp);
    if (!status.isOK())
        return status;

	//源表名对应的ident也需要改名，因为表名对应ident必须根据指定算法生成，表名改了，indent肯定也就不一样了
    const std::string identTo = _engine->getCatalog()->getCollectionIdent(toNS);

    invariant(identFrom == identTo);

	//获取新表的元数据信息文件_mdb_catalog.wt中的元数据信息md
    BSONCollectionCatalogEntry::MetaData md = _engine->getCatalog()->getMetaData(opCtx, toNS);

	//清除原表的内存cache信息
    const CollectionMap::iterator itFrom = _collections.find(fromNS.toString());
    invariant(itFrom != _collections.end());
    opCtx->recoveryUnit()->registerChange(
        new RemoveCollectionChange(opCtx, this, fromNS, identFrom, itFrom->second, false));
    _collections.erase(itFrom);

    opCtx->recoveryUnit()->registerChange(
        new AddCollectionChange(opCtx, this, toNS, identTo, false));

	//获取操作新表的WiredTigerRecordStore
    auto rs =
        _engine->getEngine()->getGroupedRecordStore(opCtx, toNS, identTo, md.options, md.prefix);

	//新表生成对应新的KVCollectionCatalogEntry cache信息
    _collections[toNS.toString()] = new KVCollectionCatalogEntry(
        _engine->getEngine(), _engine->getCatalog(), toNS, identTo, std::move(rs));

    return Status::OK();
}

//drop删表CmdDrop::errmsgRun->dropCollection->DatabaseImpl::dropCollectionEvenIfSystem->DatabaseImpl::_finishDropCollection
//    ->DatabaseImpl::_finishDropCollection->KVDatabaseCatalogEntryBase::dropCollection->KVCatalog::dropCollection
Status KVDatabaseCatalogEntryBase::dropCollection(OperationContext* opCtx, StringData ns) {
    invariant(opCtx->lockState()->isDbLockedForMode(name(), MODE_X));

	//找到该表对应的KVCollectionCatalogEntry
    CollectionMap::const_iterator it = _collections.find(ns.toString());
    if (it == _collections.end()) {
        return Status(ErrorCodes::NamespaceNotFound, "cannnot find collection to drop");
    }

    KVCollectionCatalogEntry* const entry = it->second;

    invariant(entry->getTotalIndexCount(opCtx) == entry->getCompletedIndexCount(opCtx));
	//先清除该表的所有索引
    {
        std::vector<std::string> indexNames;
        entry->getAllIndexes(opCtx, &indexNames);
        for (size_t i = 0; i < indexNames.size(); i++) {
			//KVCollectionCatalogEntry::removeIndex
			//从该表MetaData元数据中清除该index，并清除索引文件
            entry->removeIndex(opCtx, indexNames[i]).transitional_ignore();
        }
    }

    invariant(entry->getTotalIndexCount(opCtx) == 0);

    const std::string ident = _engine->getCatalog()->getCollectionIdent(ns);

	//KVStorageEngine::getCatalog获取KVDatabaseCatalogEntry   KVStorageEngine::getCatalog获取KVCatalog
	//KVCatalog::dropCollection 
	//删除表后需要从元数据中删除该表
    Status status = _engine->getCatalog()->dropCollection(opCtx, ns);
    if (!status.isOK()) {
        return status;
    }

    // This will lazily delete the KVCollectionCatalogEntry and notify the storageEngine to
    // drop the collection only on WUOW::commit().
    //真正的表删除在这里，通过这里触发，在外层的KVStorageEngine::dropDatabase中调用WUOW::commit()触发真正的删除
    opCtx->recoveryUnit()->registerChange(
    //最终外层调用触发RemoveCollectionChange::commit真正进行删除
        new RemoveCollectionChange(opCtx, this, ns, ident, it->second, true));

	//从cache中清除该表
    _collections.erase(ns.toString());

    return Status::OK();
}
}  // namespace mongo
