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

    virtual void commit() {}
    virtual void rollback() {
        if (_dropOnRollback) {
            // Intentionally ignoring failure
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

bool KVDatabaseCatalogEntryBase::exists() const {
    return !isEmpty();
}

bool KVDatabaseCatalogEntryBase::isEmpty() const {
    return _collections.empty();
}

bool KVDatabaseCatalogEntryBase::hasUserData() const {
    return !isEmpty();
}

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

void KVDatabaseCatalogEntryBase::getCollectionNamespaces(std::list<std::string>* out) const {
    for (CollectionMap::const_iterator it = _collections.begin(); it != _collections.end(); ++it) {
        out->push_back(it->first);
    }
}

CollectionCatalogEntry* KVDatabaseCatalogEntryBase::getCollectionCatalogEntry(StringData ns) const {
    CollectionMap::const_iterator it = _collections.find(ns.toString());
    if (it == _collections.end()) {
        return NULL;
    }

    return it->second;
}

RecordStore* KVDatabaseCatalogEntryBase::getRecordStore(StringData ns) const {
    CollectionMap::const_iterator it = _collections.find(ns.toString());
    if (it == _collections.end()) {
        return NULL;
    }

	//KVCollectionCatalogEntry::getRecordStore,也就是获取KVCollectionCatalogEntry._recordStore成员
	//默认为默认为StandardWiredTigerRecordStore类型
    return it->second->getRecordStore();
}


//insertBatchAndHandleErrors->makeCollection->mongo::userCreateNS->mongo::userCreateNSImpl
//->DatabaseImpl::createCollection->Collection* createCollection->KVDatabaseCatalogEntryBase::createCollection
// Collection* createCollection调用
//开始调用底层WT存储引擎相关接口建表
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
	//KVCatalog::newCollection
	Status status = _engine->getCatalog()->newCollection(opCtx, ns, options, prefix);
    if (!status.isOK())
        return status;

	//也就是newCollection中生成的集合ident
    string ident = _engine->getCatalog()->getCollectionIdent(ns); //获取文件名
	
	//WiredTigerKVEngine::createGroupedRecordStore(数据文件相关)  
	//WiredTigerKVEngine::createGroupedSortedDataInterface(索引文件相关)
	//调用WT存储引擎的create接口建表
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

    opCtx->recoveryUnit()->registerChange(new AddCollectionChange(opCtx, this, ns, ident, true));
	//WiredTigerKVEngine::getGroupedRecordStore
	//生成StandardWiredTigerRecordStore类
    auto rs = _engine->getEngine()->getGroupedRecordStore(opCtx, ns, ident, options, prefix);
    invariant(rs);

	//存到map表中，把WiredTigerKVEngine  
	//最终一个表对应一个KVCollectionCatalogEntry，存储到_collections数组中
    _collections[ns.toString()] = new KVCollectionCatalogEntry(
       //WiredTigerKVEngine--存储引擎   
       //              KVStorageEngine::getCatalog(默认KVDatabaseCatalogEntryBase)---库接口
       //                                           StandardWiredTigerRecordStore--底层WT存储引擎
        _engine->getEngine(), _engine->getCatalog(), ns, ident, std::move(rs));

    return Status::OK();
}

void KVDatabaseCatalogEntryBase::initCollection(OperationContext* opCtx,
                                                const std::string& ns,
                                                bool forRepair) {
    invariant(!_collections.count(ns));

    const std::string ident = _engine->getCatalog()->getCollectionIdent(ns);

    std::unique_ptr<RecordStore> rs;
    if (forRepair) {
        // Using a NULL rs since we don't want to open this record store before it has been
        // repaired. This also ensures that if we try to use it, it will blow up.
        rs = nullptr;
    } else {
        BSONCollectionCatalogEntry::MetaData md = _engine->getCatalog()->getMetaData(opCtx, ns);
        rs = _engine->getEngine()->getGroupedRecordStore(opCtx, ns, ident, md.options, md.prefix);
        invariant(rs);
    }

    // No change registration since this is only for committed collections
    _collections[ns] = new KVCollectionCatalogEntry(
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

Status KVDatabaseCatalogEntryBase::renameCollection(OperationContext* opCtx,
                                                    StringData fromNS,
                                                    StringData toNS,
                                                    bool stayTemp) {
    invariant(opCtx->lockState()->isDbLockedForMode(name(), MODE_X));

    RecordStore* originalRS = NULL;

    CollectionMap::const_iterator it = _collections.find(fromNS.toString());
    if (it == _collections.end()) {
        return Status(ErrorCodes::NamespaceNotFound, "rename cannot find collection");
    }

    originalRS = it->second->getRecordStore();

    it = _collections.find(toNS.toString());
    if (it != _collections.end()) {
        return Status(ErrorCodes::NamespaceExists, "for rename to already exists");
    }

    const std::string identFrom = _engine->getCatalog()->getCollectionIdent(fromNS);

    Status status = _engine->getEngine()->okToRename(opCtx, fromNS, toNS, identFrom, originalRS);
    if (!status.isOK())
        return status;

    status = _engine->getCatalog()->renameCollection(opCtx, fromNS, toNS, stayTemp);
    if (!status.isOK())
        return status;

    const std::string identTo = _engine->getCatalog()->getCollectionIdent(toNS);

    invariant(identFrom == identTo);

    BSONCollectionCatalogEntry::MetaData md = _engine->getCatalog()->getMetaData(opCtx, toNS);

    const CollectionMap::iterator itFrom = _collections.find(fromNS.toString());
    invariant(itFrom != _collections.end());
    opCtx->recoveryUnit()->registerChange(
        new RemoveCollectionChange(opCtx, this, fromNS, identFrom, itFrom->second, false));
    _collections.erase(itFrom);

    opCtx->recoveryUnit()->registerChange(
        new AddCollectionChange(opCtx, this, toNS, identTo, false));

    auto rs =
        _engine->getEngine()->getGroupedRecordStore(opCtx, toNS, identTo, md.options, md.prefix);

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

    {
        std::vector<std::string> indexNames;
        entry->getAllIndexes(opCtx, &indexNames);
        for (size_t i = 0; i < indexNames.size(); i++) {
			//KVCollectionCatalogEntry::removeIndex
            entry->removeIndex(opCtx, indexNames[i]).transitional_ignore();
        }
    }

    invariant(entry->getTotalIndexCount(opCtx) == 0);

    const std::string ident = _engine->getCatalog()->getCollectionIdent(ns);

	//KVStorageEngine::getCatalog获取KVDatabaseCatalogEntry   KVStorageEngine::getCatalog获取KVCatalog
	//KVCatalog::dropCollection
    Status status = _engine->getCatalog()->dropCollection(opCtx, ns);
    if (!status.isOK()) {
        return status;
    }

    // This will lazily delete the KVCollectionCatalogEntry and notify the storageEngine to
    // drop the collection only on WUOW::commit().
    opCtx->recoveryUnit()->registerChange(
        new RemoveCollectionChange(opCtx, this, ns, ident, it->second, true));

    _collections.erase(ns.toString());

    return Status::OK();
}
}  // namespace mongo
