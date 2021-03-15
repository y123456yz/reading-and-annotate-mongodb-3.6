/**
 *    Copyright (C) 2017 MongoDB Inc.
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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kIndex

#include "mongo/platform/basic.h"

#include "mongo/db/catalog/index_catalog_impl.h"

#include <vector>

#include "mongo/base/init.h"
#include "mongo/bson/simple_bsonelement_comparator.h"
#include "mongo/bson/simple_bsonobj_comparator.h"
#include "mongo/db/audit.h"
#include "mongo/db/background.h"
#include "mongo/db/catalog/collection.h"
#include "mongo/db/catalog/collection_catalog_entry.h"
#include "mongo/db/catalog/database_catalog_entry.h"
#include "mongo/db/catalog/index_create.h"
#include "mongo/db/catalog/index_key_validate.h"
#include "mongo/db/client.h"
#include "mongo/db/clientcursor.h"
#include "mongo/db/curop.h"
#include "mongo/db/field_ref.h"
#include "mongo/db/index/index_access_method.h"
#include "mongo/db/index/index_descriptor.h"
#include "mongo/db/index_legacy.h"
#include "mongo/db/index_names.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/keypattern.h"
#include "mongo/db/matcher/expression.h"
#include "mongo/db/operation_context.h"
#include "mongo/db/ops/delete.h"
#include "mongo/db/query/collation/collation_spec.h"
#include "mongo/db/query/collation/collator_factory_interface.h"
#include "mongo/db/query/internal_plans.h"
#include "mongo/db/repl/replication_coordinator_global.h"
#include "mongo/db/server_options.h"
#include "mongo/db/service_context.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"
#include "mongo/util/represent_as.h"

namespace mongo {
namespace {
MONGO_INITIALIZER(InitializeIndexCatalogFactory)(InitializerContext* const) {
    IndexCatalog::registerFactory([](
        IndexCatalog* const this_, Collection* const collection, const int maxNumIndexesAllowed) {
        return stdx::make_unique<IndexCatalogImpl>(this_, collection, maxNumIndexesAllowed);
    });
    return Status::OK();
}

MONGO_INITIALIZER(InitializeIndexCatalogIndexIteratorFactory)(InitializerContext* const) {
    IndexCatalog::IndexIterator::registerFactory([](OperationContext* const opCtx,
                                                    const IndexCatalog* const cat,
                                                    const bool includeUnfinishedIndexes) {
        return stdx::make_unique<IndexCatalogImpl::IndexIteratorImpl>(
            opCtx, cat, includeUnfinishedIndexes);
    });
    return Status::OK();
}

MONGO_INITIALIZER(InitializeFixIndexKeyImpl)(InitializerContext* const) {
    IndexCatalog::registerFixIndexKeyImpl(&IndexCatalogImpl::fixIndexKey);
    return Status::OK();
}

MONGO_INITIALIZER(InitializePrepareInsertDeleteOptionsImpl)(InitializerContext* const) {
    IndexCatalog::registerPrepareInsertDeleteOptionsImpl(
        &IndexCatalogImpl::prepareInsertDeleteOptions);
    return Status::OK();
}

}  // namespace

using std::unique_ptr;
using std::endl;
using std::string;
using std::vector;

using IndexVersion = IndexDescriptor::IndexVersion;

static const int INDEX_CATALOG_INIT = 283711;
static const int INDEX_CATALOG_UNINIT = 654321;

const BSONObj IndexCatalogImpl::_idObj = BSON("_id" << 1);

// -------------

IndexCatalogImpl::IndexCatalogImpl(IndexCatalog* const this_,
                                   Collection* collection,
                                   int maxNumIndexesAllowed)
    : _magic(INDEX_CATALOG_UNINIT),
      _collection(collection),
      _maxNumIndexesAllowed(maxNumIndexesAllowed),
      _this(this_) {}

IndexCatalogImpl::~IndexCatalogImpl() {
    if (_magic != INDEX_CATALOG_UNINIT) {
        // only do this check if we haven't been initialized
        _checkMagic();
    }
    _magic = 123456;
}

Status IndexCatalogImpl::init(OperationContext* opCtx) {
    vector<string> indexNames;
	//BSONCollectionCatalogEntry::getAllIndexes 
	//获取所有的索引名
    _collection->getCatalogEntry()->getAllIndexes(opCtx, &indexNames);

    for (size_t i = 0; i < indexNames.size(); i++) {
        const string& indexName = indexNames[i];
        BSONObj spec = _collection->getCatalogEntry()->getIndexSpec(opCtx, indexName).getOwned();

		//BSONCollectionCatalogEntry::isIndexReady 索引是否已经构建完成
        if (!_collection->getCatalogEntry()->isIndexReady(opCtx, indexName)) {
			// These are the index specs of indexes that were "leftover".
		    // "Leftover" means they were unfinished when a mongod shut down.
		    // Certain operations are prohibited until someone fixes.
		    // Retrieve by calling getAndClearUnfinishedIndexes().
			//索引没有执行完成，加到_unfinishedIndexes
            _unfinishedIndexes.push_back(spec);
            continue;
        }

        BSONObj keyPattern = spec.getObjectField("key");
		//构造IndexDescriptor
        auto descriptor = stdx::make_unique<IndexDescriptor>(
            _collection, _getAccessMethodName(opCtx, keyPattern), spec);
        const bool initFromDisk = true;
		//获取descriptor该索引对应的IndexCatalogEntryImpl添加到_entries数组
        IndexCatalogEntry* entry =
            _setupInMemoryStructures(opCtx, std::move(descriptor), initFromDisk);

        fassert(17340, entry->isReady(opCtx));
    }

    if (_unfinishedIndexes.size()) {
        // if there are left over indexes, we don't let anyone add/drop indexes
        // until someone goes and fixes them
        log() << "found " << _unfinishedIndexes.size()
              << " index(es) that wasn't finished before shutdown";
    }

    _magic = INDEX_CATALOG_INIT;
    return Status::OK();
}

//IndexCatalogImpl::IndexBuildBlock::init  IndexCatalogImpl::init中调用

//获取descriptor该索引对应的IndexCatalogEntryImpl添加到_entries数组
IndexCatalogEntry* IndexCatalogImpl::_setupInMemoryStructures(
    OperationContext* opCtx, std::unique_ptr<IndexDescriptor> descriptor, bool initFromDisk) {
	//索引检查，索引版本是否正确等
	Status status = _isSpecOk(opCtx, descriptor->infoObj());
    if (!status.isOK() && status != ErrorCodes::IndexAlreadyExists) {
        severe() << "Found an invalid index " << descriptor->infoObj() << " on the "
                 << _collection->ns().ns() << " collection: " << redact(status);
        fassertFailedNoTrace(28782);
    }

    auto* const descriptorPtr = descriptor.get();

	//构造IndexCatalogEntryImpl类
    auto entry = stdx::make_unique<IndexCatalogEntry>(opCtx,
                                                      _collection->ns().ns(),
                                                      //CollectionCatalogEntry
                                                      _collection->getCatalogEntry(),
                                                      std::move(descriptor),
                                                      _collection->infoCache());

	//CollectionImpl::dbce获取KVDatabaseCatalogEntry类，然后调用
	//KVDatabaseCatalogEntry::getIndex获取对应method，betree对应BtreeAccessMethod
	std::unique_ptr<IndexAccessMethod> accessMethod(
        _collection->dbce()->getIndex(opCtx, _collection->getCatalogEntry(), entry.get()));
	//IndexCatalogEntryImpl::init,给IndexCatalogEntryImpl._accessMethod赋值为accessMethod
	entry->init(std::move(accessMethod));

	//获取IndexCatalogEntryImpl添加到_entries数组中，一个索引信息对应一个IndexCatalogEntryImpl
    IndexCatalogEntry* save = entry.get();
    _entries.add(entry.release());

    if (!initFromDisk) {
        opCtx->recoveryUnit()->onRollback([ this, opCtx, descriptor = descriptorPtr ] {
            // Need to preserve indexName as descriptor no longer exists after remove().
            const std::string indexName = descriptor->indexName();
            _entries.remove(descriptor);
            _collection->infoCache()->droppedIndex(opCtx, indexName);
        });
    }

    invariant(save == _entries.find(descriptorPtr));
    invariant(save == _entries.find(descriptorPtr->indexName()));

    return save;
}

bool IndexCatalogImpl::ok() const {
    return (_magic == INDEX_CATALOG_INIT);
}

void IndexCatalogImpl::_checkMagic() const {
    if (ok()) {
        return;
    }
    log() << "IndexCatalog::_magic wrong, is : " << _magic;
    fassertFailed(17198);
}

Status IndexCatalogImpl::checkUnfinished() const {
    if (_unfinishedIndexes.size() == 0)
        return Status::OK();

    return Status(ErrorCodes::InternalError,
                  str::stream() << "IndexCatalog has left over indexes that must be cleared"
                                << " ns: "
                                << _collection->ns().ns());
}

bool IndexCatalogImpl::_shouldOverridePlugin(OperationContext* opCtx,
                                             const BSONObj& keyPattern) const {
    string pluginName = IndexNames::findPluginName(keyPattern);
    bool known = IndexNames::isKnownName(pluginName);

    if (!_collection->dbce()->isOlderThan24(opCtx)) {
        // RulesFor24+
        // This assert will be triggered when downgrading from a future version that
        // supports an index plugin unsupported by this version.
        uassert(17197,
                str::stream() << "Invalid index type '" << pluginName << "' "
                              << "in index "
                              << keyPattern,
                known);
        return false;
    }

    // RulesFor22
    if (!known) {
        log() << "warning: can't find plugin [" << pluginName << "]";
        return true;
    }

    if (!IndexNames::existedBefore24(pluginName)) {
        warning() << "Treating index " << keyPattern << " as ascending since "
                  << "it was created before 2.4 and '" << pluginName << "' "
                  << "was not a valid type at that time.";
        return true;
    }

    return false;
}

//索引类型是btree  text  2d中的哪一种
string IndexCatalogImpl::_getAccessMethodName(OperationContext* opCtx,
                                              const BSONObj& keyPattern) const {
    if (_shouldOverridePlugin(opCtx, keyPattern)) {
        return "";
    }

    return IndexNames::findPluginName(keyPattern);
}


// ---------------------------

Status IndexCatalogImpl::_upgradeDatabaseMinorVersionIfNeeded(OperationContext* opCtx,
                                                              const string& newPluginName) {
    // first check if requested index requires pdfile minor version to be bumped
    if (IndexNames::existedBefore24(newPluginName)) {
        return Status::OK();
    }

    DatabaseCatalogEntry* dbce = _collection->dbce();

    if (!dbce->isOlderThan24(opCtx)) {
        return Status::OK();  // these checks have already been done
    }

    // Everything below is MMAPv1 specific since it was the only storage engine that existed
    // before 2.4. We look at all indexes in this database to make sure that none of them use
    // plugins that didn't exist before 2.4. If that holds, we mark the database as "2.4-clean"
    // which allows creation of indexes using new plugins.

    RecordStore* indexes = dbce->getRecordStore(dbce->name() + ".system.indexes");
    auto cursor = indexes->getCursor(opCtx);
    while (auto record = cursor->next()) {
        const BSONObj index = record->data.releaseToBson();
        const BSONObj key = index.getObjectField("key");
        const string plugin = IndexNames::findPluginName(key);
        if (IndexNames::existedBefore24(plugin))
            continue;

        const string errmsg = str::stream()
            << "Found pre-existing index " << index << " with invalid type '" << plugin << "'. "
            << "Disallowing creation of new index type '" << newPluginName << "'. See "
            << "http://dochub.mongodb.org/core/index-type-changes";

        return Status(ErrorCodes::CannotCreateIndex, errmsg);
    }

    dbce->markIndexSafe24AndUp(opCtx);

    return Status::OK();
}

//IndexCatalogImpl::createIndexOnEmptyCollection  MultiIndexBlockImpl::removeExistingIndexes
//MultiIndexBlockImpl::init  中调用
//索引个数 索引冲突 索引名冲突等检查
StatusWith<BSONObj> IndexCatalogImpl::prepareSpecForCreate(OperationContext* opCtx,
                                                           const BSONObj& original) const {
    Status status = _isSpecOk(opCtx, original);
    if (!status.isOK())
        return StatusWith<BSONObj>(status);

	//根据original原始数据构造fixspec
    auto fixed = _fixIndexSpec(opCtx, _collection, original);
    if (!fixed.isOK()) {
        return fixed;
    }

    // we double check with new index spec
    //做一些检查
    status = _isSpecOk(opCtx, fixed.getValue());
    if (!status.isOK())
        return StatusWith<BSONObj>(status);

	////索引个数 索引冲突 索引名冲突等检查
    status = _doesSpecConflictWithExisting(opCtx, fixed.getValue());
    if (!status.isOK())
        return StatusWith<BSONObj>(status);

    return fixed;
}

/*
db/catalog/collection_impl.cpp:        status = _indexCatalog.createIndexOnEmptyCollection(opCtx, indexSpecs[i]).getStatus();
db/catalog/database_impl.cpp:                fullIdIndexSpec = uassertStatusOK(ic->createIndexOnEmptyCollection(

*/ 
//DatabaseImpl::createCollection   createSystemIndexes  CollectionImpl::truncate中调用执行
//空表上面建索引
StatusWith<BSONObj> IndexCatalogImpl::createIndexOnEmptyCollection(OperationContext* opCtx,
                                                                   BSONObj spec) {
    invariant(opCtx->lockState()->isCollectionLockedForMode(_collection->ns().toString(), MODE_X));
    invariant(_collection->numRecords(opCtx) == 0);

	//log() << "yang test .... IndexCatalogImpl::createIndexOnEmptyCollection";
    _checkMagic();
    Status status = checkUnfinished();
    if (!status.isOK())
        return status;

	//索引个数 索引冲突 索引名冲突等检查失败，例如索引名有冲突  索引达到上限等
    StatusWith<BSONObj> statusWithSpec = prepareSpecForCreate(opCtx, spec);
    status = statusWithSpec.getStatus();
    if (!status.isOK())
        return status;
    spec = statusWithSpec.getValue();

	//索引类型 text  hashed  2d  btree中的哪一种
    string pluginName = IndexNames::findPluginName(spec["key"].Obj());
    if (pluginName.size()) {
        Status s = _upgradeDatabaseMinorVersionIfNeeded(opCtx, pluginName);
        if (!s.isOK())
            return s;
    }

    // now going to touch disk
    IndexBuildBlock indexBuildBlock(opCtx, _collection, spec);
	//IndexCatalogImpl::IndexBuildBlock::init
	status = indexBuildBlock.init(); //建索引
    if (!status.isOK())
        return status;

    // sanity checks, etc...
    IndexCatalogEntry* entry = indexBuildBlock.getEntry();
    invariant(entry);
    IndexDescriptor* descriptor = entry->descriptor();
    invariant(descriptor);
    invariant(entry == _entries.find(descriptor));

    status = entry->accessMethod()->initializeAsEmpty(opCtx);
    if (!status.isOK())
        return status;
    indexBuildBlock.success();

    // sanity check
    invariant(_collection->getCatalogEntry()->isIndexReady(opCtx, descriptor->indexName()));

    return spec;
}

//index_create_impl.cpp中的MultiIndexBlockImpl::init new该类
IndexCatalogImpl::IndexBuildBlock::IndexBuildBlock(OperationContext* opCtx,
                                                   Collection* collection,
                                                   const BSONObj& spec)
    : _collection(collection),
      _catalog(collection->getIndexCatalog()),
      _ns(_catalog->_getCollection()->ns().ns()),
      _spec(spec.getOwned()),
      _entry(nullptr),
      _opCtx(opCtx) {
    invariant(collection);
}

//建索引
//创建集合的时候或者程序重启的时候建索引:DatabaseImpl::createCollection->IndexCatalogImpl::createIndexOnEmptyCollection->IndexCatalogImpl::IndexBuildBlock::init
//MultiIndexBlockImpl::init->IndexCatalogImpl::IndexBuildBlock::init  程序运行过程中，并且集合已经存在的时候建索引

//IndexCatalogImpl::createIndexOnEmptyCollection 调用
//CmdCreateIndex::errmsgRun->MultiIndexBlockImpl::init调用

////获取descriptor该索引对应的IndexCatalogEntryImpl添加到_entries数组
Status IndexCatalogImpl::IndexBuildBlock::init() {
    // need this first for names, etc...
    //获取所有信息的key，可以db.collection.getIndexes()获取
    BSONObj keyPattern = _spec.getObjectField("key");
	//构造索引IndexDescriptor信息
    auto descriptor = stdx::make_unique<IndexDescriptor>(
        _collection, IndexNames::findPluginName(keyPattern), _spec);

	//log() << "yang test ... IndexCatalogImpl::IndexBuildBlock::init";
	//获取索引名信息
	//IndexDescriptor::indexName 
    _indexName = descriptor->indexName();
	//IndexDescriptor::indexNamespace
    _indexNamespace = descriptor->indexNamespace();

    /// ----------   setup on disk structures ----------------

	//KVCollectionCatalogEntry::prepareForIndexBuild
	//准备工作
    Status status = _collection->getCatalogEntry()->prepareForIndexBuild(_opCtx, descriptor.get());
    if (!status.isOK())
        return status;

	//获取IndexDescriptor信息
    auto* const descriptorPtr = descriptor.get();
    /// ----------   setup in memory structures  ----------------
    const bool initFromDisk = false;
	//获取descriptor该索引对应的IndexCatalogEntryImpl添加到_entries数组
    _entry = IndexCatalogImpl::_setupInMemoryStructures(
        _catalog, _opCtx, std::move(descriptor), initFromDisk);

    // Register this index with the CollectionInfoCache to regenerate the cache. This way, updates
    // occurring while an index is being build in the background will be aware of whether or not
    // they need to modify any indexes.
    //CollectionInfoCacheImpl::addedIndex
    _collection->infoCache()->addedIndex(_opCtx, descriptorPtr);

    return Status::OK();
}

IndexCatalogImpl::IndexBuildBlock::~IndexBuildBlock() {
    // Don't need to call fail() here, as rollback will clean everything up for us.
}

void IndexCatalogImpl::IndexBuildBlock::fail() {
    fassert(17204, _catalog->_getCollection()->ok());  // defensive

    IndexCatalogEntry* entry = IndexCatalog::_getEntries(_catalog).find(_indexName);
    invariant(entry == _entry);

    if (entry) {
        IndexCatalogImpl::_dropIndex(_catalog, _opCtx, entry).transitional_ignore();
    } else {
        IndexCatalog::_deleteIndexFromDisk(_catalog, _opCtx, _indexName, _indexNamespace);
    }
}

//IndexCatalogImpl::createIndexOnEmptyCollection调用  检查索引是否创建成功
void IndexCatalogImpl::IndexBuildBlock::success() {
    Collection* collection = _catalog->_getCollection();
    fassert(17207, collection->ok());
    NamespaceString ns(_indexNamespace);
    invariant(_opCtx->lockState()->isDbLockedForMode(ns.db(), MODE_X));

    collection->getCatalogEntry()->indexBuildSuccess(_opCtx, _indexName);

    IndexDescriptor* desc = _catalog->findIndexByName(_opCtx, _indexName, true);
    fassert(17330, desc);
    IndexCatalogEntry* entry = _catalog->_getEntries().find(desc);
    fassert(17331, entry && entry == _entry);

    OperationContext* opCtx = _opCtx;
    LOG(2) << "marking index " << _indexName << " as ready in snapshot id "
           << opCtx->recoveryUnit()->getSnapshotId();
    _opCtx->recoveryUnit()->onCommit([opCtx, entry, collection] {
        // Note: this runs after the WUOW commits but before we release our X lock on the
        // collection. This means that any snapshot created after this must include the full index,
        // and no one can try to read this index before we set the visibility.
        auto replCoord = repl::ReplicationCoordinator::get(opCtx);
        auto snapshotName = replCoord->reserveSnapshotName(opCtx);
        entry->setMinimumVisibleSnapshot(snapshotName);

        // TODO remove this once SERVER-20439 is implemented. It is a stopgap solution for
        // SERVER-20260 to make sure that reads with majority readConcern level can see indexes that
        // are created with w:majority by making the readers block.
        collection->setMinimumVisibleSnapshot(snapshotName);
    });

    entry->setIsReady(true);
}

namespace {
// While technically recursive, only current possible with 2 levels.
Status _checkValidFilterExpressions(MatchExpression* expression, int level = 0) {
    if (!expression)
        return Status::OK();

    switch (expression->matchType()) {
        case MatchExpression::AND:
            if (level > 0)
                return Status(ErrorCodes::CannotCreateIndex,
                              "$and only supported in partialFilterExpression at top level");
            for (size_t i = 0; i < expression->numChildren(); i++) {
                Status status = _checkValidFilterExpressions(expression->getChild(i), level + 1);
                if (!status.isOK())
                    return status;
            }
            return Status::OK();
        case MatchExpression::EQ:
        case MatchExpression::LT:
        case MatchExpression::LTE:
        case MatchExpression::GT:
        case MatchExpression::GTE:
        case MatchExpression::EXISTS:
        case MatchExpression::TYPE_OPERATOR:
            return Status::OK();
        default:
            return Status(ErrorCodes::CannotCreateIndex,
                          str::stream() << "unsupported expression in partial index: "
                                        << expression->toString());
    }
}
}

//索引检查，索引版本是否正确等
Status IndexCatalogImpl::_isSpecOk(OperationContext* opCtx, const BSONObj& spec) const {
    const NamespaceString& nss = _collection->ns();

    BSONElement vElt = spec["v"];
    if (!vElt) {
        return {ErrorCodes::InternalError,
                str::stream()
                    << "An internal operation failed to specify the 'v' field, which is a required "
                       "property of an index specification: "
                    << spec};
    }

    if (!vElt.isNumber()) {
        return Status(ErrorCodes::CannotCreateIndex,
                      str::stream() << "non-numeric value for \"v\" field: " << vElt);
    }

    auto vEltAsInt = representAs<int>(vElt.number());
    if (!vEltAsInt) {
        return {ErrorCodes::CannotCreateIndex,
                str::stream() << "Index version must be representable as a 32-bit integer, but got "
                              << vElt.toString(false, false)};
    }

    auto indexVersion = static_cast<IndexVersion>(*vEltAsInt);

    if (indexVersion >= IndexVersion::kV2) {
        auto status = index_key_validate::validateIndexSpecFieldNames(spec);
        if (!status.isOK()) {
            return status;
        }
    }

    // SERVER-16893 Forbid use of v0 indexes with non-mmapv1 engines
    if (indexVersion == IndexVersion::kV0 &&
        !opCtx->getServiceContext()->getGlobalStorageEngine()->isMmapV1()) {
        return Status(ErrorCodes::CannotCreateIndex,
                      str::stream() << "use of v0 indexes is only allowed with the "
                                    << "mmapv1 storage engine");
    }

    if (!IndexDescriptor::isIndexVersionSupported(indexVersion)) {
        return Status(ErrorCodes::CannotCreateIndex,
                      str::stream() << "this version of mongod cannot build new indexes "
                                    << "of version number "
                                    << static_cast<int>(indexVersion));
    }

    if (nss.isSystemDotIndexes())
        return Status(ErrorCodes::CannotCreateIndex,
                      "cannot have an index on the system.indexes collection");

    if (nss.isOplog())
        return Status(ErrorCodes::CannotCreateIndex, "cannot have an index on the oplog");

    if (nss.coll() == "$freelist") {
        // this isn't really proper, but we never want it and its not an error per se
        return Status(ErrorCodes::IndexAlreadyExists, "cannot index freelist");
    }

    const BSONElement specNamespace = spec["ns"];
    if (specNamespace.type() != String)
        return Status(ErrorCodes::CannotCreateIndex,
                      "the index spec is missing a \"ns\" string field");

    if (nss.ns() != specNamespace.valueStringData())
        return Status(ErrorCodes::CannotCreateIndex,
                      str::stream() << "the \"ns\" field of the index spec '"
                                    << specNamespace.valueStringData()
                                    << "' does not match the collection name '"
                                    << nss.ns()
                                    << "'");

    // logical name of the index
    const BSONElement nameElem = spec["name"];
    if (nameElem.type() != String)
        return Status(ErrorCodes::CannotCreateIndex, "index name must be specified as a string");

    const StringData name = nameElem.valueStringData();
    if (name.find('\0') != std::string::npos)
        return Status(ErrorCodes::CannotCreateIndex, "index name cannot contain NUL bytes");

    if (name.empty())
        return Status(ErrorCodes::CannotCreateIndex, "index name cannot be empty");

    // Drop pending collections are internal to the server and will not be exported to another
    // storage engine. The indexes contained in these collections are not subject to the same
    // namespace length constraints as the ones in created by users.
    if (!nss.isDropPendingNamespace()) {
        auto indexNamespace = IndexDescriptor::makeIndexNamespace(nss.ns(), name);
        if (indexNamespace.length() > NamespaceString::MaxNsLen)
            return Status(ErrorCodes::CannotCreateIndex,
                          str::stream() << "namespace name generated from index name \""
                                        << indexNamespace
                                        << "\" is too long (127 byte max)");
    }

    const BSONObj key = spec.getObjectField("key");
    const Status keyStatus = index_key_validate::validateKeyPattern(key, indexVersion);
    if (!keyStatus.isOK()) {
        return Status(ErrorCodes::CannotCreateIndex,
                      str::stream() << "bad index key pattern " << key << ": "
                                    << keyStatus.reason());
    }

    std::unique_ptr<CollatorInterface> collator;
    BSONElement collationElement = spec.getField("collation");
    if (collationElement) {
        if (collationElement.type() != BSONType::Object) {
            return Status(ErrorCodes::CannotCreateIndex,
                          "\"collation\" for an index must be a document");
        }
        auto statusWithCollator = CollatorFactoryInterface::get(opCtx->getServiceContext())
                                      ->makeFromBSON(collationElement.Obj());
        if (!statusWithCollator.isOK()) {
            return statusWithCollator.getStatus();
        }
        collator = std::move(statusWithCollator.getValue());

        if (!collator) {
            return {ErrorCodes::InternalError,
                    str::stream() << "An internal operation specified the collation "
                                  << CollationSpec::kSimpleSpec
                                  << " explicitly, which should instead be implied by omitting the "
                                     "'collation' field from the index specification"};
        }

        if (static_cast<IndexVersion>(vElt.numberInt()) < IndexVersion::kV2) {
            return {ErrorCodes::CannotCreateIndex,
                    str::stream() << "Index version " << vElt.fieldNameStringData() << "="
                                  << vElt.numberInt()
                                  << " does not support the '"
                                  << collationElement.fieldNameStringData()
                                  << "' option"};
        }

        string pluginName = IndexNames::findPluginName(key);
        if ((pluginName != IndexNames::BTREE) && (pluginName != IndexNames::GEO_2DSPHERE) &&
            (pluginName != IndexNames::HASHED)) {
            return Status(ErrorCodes::CannotCreateIndex,
                          str::stream() << "Index type '" << pluginName
                                        << "' does not support collation: "
                                        << collator->getSpec().toBSON());
        }
    }

    const bool isSparse = spec["sparse"].trueValue();

    // Ensure if there is a filter, its valid.
    BSONElement filterElement = spec.getField("partialFilterExpression");
    if (filterElement) {
        if (isSparse) {
            return Status(ErrorCodes::CannotCreateIndex,
                          "cannot mix \"partialFilterExpression\" and \"sparse\" options");
        }

        if (filterElement.type() != Object) {
            return Status(ErrorCodes::CannotCreateIndex,
                          "\"partialFilterExpression\" for an index must be a document");
        }

        // The collator must outlive the constructed MatchExpression.
        boost::intrusive_ptr<ExpressionContext> expCtx(
            new ExpressionContext(opCtx, collator.get()));

        // Parsing the partial filter expression is not expected to fail here since the
        // expression would have been successfully parsed upstream during index creation. However,
        // filters that were allowed in partial filter expressions prior to 3.6 may be present in
        // the index catalog and must also successfully parse (e.g., partial index filters with the
        // $isolated/$atomic option).
        StatusWithMatchExpression statusWithMatcher =
            MatchExpressionParser::parse(filterElement.Obj(),
                                         std::move(expCtx),
                                         ExtensionsCallbackNoop(),
                                         MatchExpressionParser::kIsolated);
        if (!statusWithMatcher.isOK()) {
            return statusWithMatcher.getStatus();
        }
        const std::unique_ptr<MatchExpression> filterExpr = std::move(statusWithMatcher.getValue());

        Status status = _checkValidFilterExpressions(filterExpr.get());
        if (!status.isOK()) {
            return status;
        }
    }

    if (IndexDescriptor::isIdIndexPattern(key)) {
        BSONElement uniqueElt = spec["unique"];
        if (uniqueElt && !uniqueElt.trueValue()) {
            return Status(ErrorCodes::CannotCreateIndex, "_id index cannot be non-unique");
        }

        if (filterElement) {
            return Status(ErrorCodes::CannotCreateIndex, "_id index cannot be a partial index");
        }

        if (isSparse) {
            return Status(ErrorCodes::CannotCreateIndex, "_id index cannot be sparse");
        }

        if (collationElement &&
            !CollatorInterface::collatorsMatch(collator.get(), _collection->getDefaultCollator())) {
            return Status(ErrorCodes::CannotCreateIndex,
                          "_id index must have the collection default collation");
        }
    } else {
        // for non _id indexes, we check to see if replication has turned off all indexes
        // we _always_ created _id index
        if (!repl::getGlobalReplicationCoordinator()->buildsIndexes()) {
            // this is not exactly the right error code, but I think will make the most sense
            return Status(ErrorCodes::IndexAlreadyExists, "no indexes per repl");
        }
    }

    // --- only storage engine checks allowed below this ----

    BSONElement storageEngineElement = spec.getField("storageEngine");
    if (storageEngineElement.eoo()) {
        return Status::OK();
    }
    if (storageEngineElement.type() != mongo::Object) {
        return Status(ErrorCodes::CannotCreateIndex,
                      "\"storageEngine\" options must be a document if present");
    }
    BSONObj storageEngineOptions = storageEngineElement.Obj();
    if (storageEngineOptions.isEmpty()) {
        return Status(ErrorCodes::CannotCreateIndex,
                      "Empty \"storageEngine\" options are invalid. "
                      "Please remove the field or include valid options.");
    }
    Status storageEngineStatus =
        validateStorageOptions(storageEngineOptions,
                               stdx::bind(&StorageEngine::Factory::validateIndexStorageOptions,
                                          stdx::placeholders::_1,
                                          stdx::placeholders::_2));
    if (!storageEngineStatus.isOK()) {
        return storageEngineStatus;
    }

    return Status::OK();
}

/*
判断是否冲突，例如
> db.test.createIndex({name:1})
{
        "createdCollectionAutomatically" : false,
        "numIndexesBefore" : 2,
        "numIndexesAfter" : 2,
        "note" : "all indexes already exist",
        "ok" : 1
}
> db.test.createIndex({name:1},{background: true, unique:true}) 
{
        "ok" : 0,
        "errmsg" : "Index with name: name_1 already exists with different options",
        "code" : 85,
        "codeName" : "IndexOptionsConflict"
}
*/

//索引个数 索引冲突 索引名冲突等检查
//IndexCatalogImpl::prepareSpecForCreate中调用
Status IndexCatalogImpl::_doesSpecConflictWithExisting(OperationContext* opCtx,
                                                       const BSONObj& spec) const {
    const char* name = spec.getStringField("name");
    invariant(name[0]);

    const BSONObj key = spec.getObjectField("key");
    const BSONObj collation = spec.getObjectField("collation");

    {
        // Check both existing and in-progress indexes (2nd param = true)
        //IndexCatalogImpl::findIndexByName 根据索引名查找索引是否已经存在
        const IndexDescriptor* desc = findIndexByName(opCtx, name, true);
        if (desc) {
            // index already exists with same name

            if (SimpleBSONObjComparator::kInstance.evaluate(desc->keyPattern() == key) &&
                SimpleBSONObjComparator::kInstance.evaluate(
                    desc->infoObj().getObjectField("collation") != collation)) {
                // key patterns are equal but collations differ.
                return Status(ErrorCodes::IndexOptionsConflict,
                              str::stream()
                                  << "An index with the same key pattern, but a different "
                                  << "collation already exists with the same name.  Try again with "
                                  << "a unique name. "
                                  << "Existing index: "
                                  << desc->infoObj()
                                  << " Requested index: "
                                  << spec);
            }

            if (SimpleBSONObjComparator::kInstance.evaluate(desc->keyPattern() != key) ||
                SimpleBSONObjComparator::kInstance.evaluate(
                    desc->infoObj().getObjectField("collation") != collation)) {
                return Status(ErrorCodes::IndexKeySpecsConflict,
                              str::stream() << "Index must have unique name."
                                            << "The existing index: "
                                            << desc->infoObj()
                                            << " has the same name as the requested index: "
                                            << spec);
            }

            IndexDescriptor temp(_collection, _getAccessMethodName(opCtx, key), spec);
            if (!desc->areIndexOptionsEquivalent(&temp))
                return Status(ErrorCodes::IndexOptionsConflict,
                              str::stream() << "Index with name: " << name
                                            << " already exists with different options");

            // Index already exists with the same options, so no need to build a new
            // one (not an error). Most likely requested by a client using ensureIndex.
            return Status(ErrorCodes::IndexAlreadyExists,
                          str::stream() << "Identical index already exists: " << name);
        }
    }

    {
        // Check both existing and in-progress indexes.
        const bool findInProgressIndexes = true;
        const IndexDescriptor* desc =
            findIndexByKeyPatternAndCollationSpec(opCtx, key, collation, findInProgressIndexes);
        if (desc) {
            LOG(2) << "index already exists with diff name " << name << " pattern: " << key
                   << " collation: " << collation;

            IndexDescriptor temp(_collection, _getAccessMethodName(opCtx, key), spec);
            if (!desc->areIndexOptionsEquivalent(&temp))
                return Status(ErrorCodes::IndexOptionsConflict,
                              str::stream() << "Index: " << spec
                                            << " already exists with different options: "
                                            << desc->infoObj());

            return Status(ErrorCodes::IndexAlreadyExists,
                          str::stream() << "index already exists with different name: " << name);
        }
    }

	//索引个数超过限制数
    if (numIndexesTotal(opCtx) >= _maxNumIndexesAllowed) {
        string s = str::stream() << "add index fails, too many indexes for "
                                 << _collection->ns().ns() << " key:" << key;
        log() << s;
        return Status(ErrorCodes::CannotCreateIndex, s);
    }

    // Refuse to build text index if another text index exists or is in progress.
    // Collections should only have one text index.
    string pluginName = IndexNames::findPluginName(key);
    if (pluginName == IndexNames::TEXT) {
        vector<IndexDescriptor*> textIndexes;
        const bool includeUnfinishedIndexes = true;
        findIndexByType(opCtx, IndexNames::TEXT, textIndexes, includeUnfinishedIndexes);
        if (textIndexes.size() > 0) {
            return Status(ErrorCodes::CannotCreateIndex,
                          str::stream() << "only one text index per collection allowed, "
                                        << "found existing text index \""
                                        << textIndexes[0]->indexName()
                                        << "\"");
        }
    }
    return Status::OK();
}

BSONObj IndexCatalogImpl::getDefaultIdIndexSpec(
    ServerGlobalParams::FeatureCompatibility::Version featureCompatibilityVersion) const {
    dassert(_idObj["_id"].type() == NumberInt);

    const auto indexVersion = IndexDescriptor::getDefaultIndexVersion(featureCompatibilityVersion);

    BSONObjBuilder b;
    b.append("v", static_cast<int>(indexVersion));
    b.append("name", "_id_");
    b.append("ns", _collection->ns().ns());
    b.append("key", _idObj);
    if (_collection->getDefaultCollator() && indexVersion >= IndexVersion::kV2) {
        // Creating an index with the "collation" option requires a v=2 index.
        b.append("collation", _collection->getDefaultCollator()->getSpec().toBSON());
    }
    return b.obj();
}

//drop删表CmdDrop::errmsgRun->dropCollection->DatabaseImpl::dropCollectionEvenIfSystem->DatabaseImpl::_finishDropCollection
//删除表下面的所有索引，只是清除内存中缓存的
void IndexCatalogImpl::dropAllIndexes(OperationContext* opCtx,
                                      bool includingIdIndex,
                                      std::map<std::string, BSONObj>* droppedIndexes) {
    invariant(opCtx->lockState()->isCollectionLockedForMode(_collection->ns().toString(), MODE_X));

    BackgroundOperation::assertNoBgOpInProgForNs(_collection->ns().ns());

    // there may be pointers pointing at keys in the btree(s).  kill them.
    // TODO: can this can only clear cursors on this index?
    _collection->getCursorManager()->invalidateAll(
        opCtx, false, "all indexes on collection dropped");

    // make sure nothing in progress
    massert(17348,
            "cannot dropAllIndexes when index builds in progress",
            numIndexesTotal(opCtx) == numIndexesReady(opCtx));

    bool haveIdIndex = false;

    vector<string> indexNamesToDrop;
    {
        int seen = 0;
        IndexIterator ii = _this->getIndexIterator(opCtx, true);
        while (ii.more()) {
            seen++;
            IndexDescriptor* desc = ii.next();
            if (desc->isIdIndex() && includingIdIndex == false) {
                haveIdIndex = true;
                continue;
            }
            indexNamesToDrop.push_back(desc->indexName());
        }
        invariant(seen == numIndexesTotal(opCtx));
    }

    for (size_t i = 0; i < indexNamesToDrop.size(); i++) {
        string indexName = indexNamesToDrop[i];
        IndexDescriptor* desc = findIndexByName(opCtx, indexName, true);
        invariant(desc);
        LOG(1) << "\t dropAllIndexes dropping: " << desc->toString();
        IndexCatalogEntry* entry = _entries.find(desc);
        invariant(entry);
        _dropIndex(opCtx, entry).transitional_ignore();

        if (droppedIndexes != nullptr) {
            droppedIndexes->emplace(desc->indexName(), desc->infoObj());
        }
    }

    // verify state is sane post cleaning

    long long numIndexesInCollectionCatalogEntry =
        _collection->getCatalogEntry()->getTotalIndexCount(opCtx);

    if (haveIdIndex) {
        fassert(17324, numIndexesTotal(opCtx) == 1);
        fassert(17325, numIndexesReady(opCtx) == 1);
        fassert(17326, numIndexesInCollectionCatalogEntry == 1);
        fassert(17336, _entries.size() == 1);
    } else {
        if (numIndexesTotal(opCtx) || numIndexesInCollectionCatalogEntry || _entries.size()) {
            error() << "About to fassert - "
                    << " numIndexesTotal(): " << numIndexesTotal(opCtx)
                    << " numSystemIndexesEntries: " << numIndexesInCollectionCatalogEntry
                    << " _entries.size(): " << _entries.size()
                    << " indexNamesToDrop: " << indexNamesToDrop.size()
                    << " haveIdIndex: " << haveIdIndex;
        }
        fassert(17327, numIndexesTotal(opCtx) == 0);
        fassert(17328, numIndexesInCollectionCatalogEntry == 0);
        fassert(17337, _entries.size() == 0);
    }
}

Status IndexCatalogImpl::dropIndex(OperationContext* opCtx, IndexDescriptor* desc) {
    invariant(opCtx->lockState()->isCollectionLockedForMode(_collection->ns().toString(), MODE_X));
    IndexCatalogEntry* entry = _entries.find(desc);

    if (!entry)
        return Status(ErrorCodes::InternalError, "cannot find index to delete");

    if (!entry->isReady(opCtx))
        return Status(ErrorCodes::InternalError, "cannot delete not ready index");

    BackgroundOperation::assertNoBgOpInProgForNs(_collection->ns().ns());

    return _dropIndex(opCtx, entry);
}

namespace {
class IndexRemoveChange final : public RecoveryUnit::Change {
public:
    IndexRemoveChange(OperationContext* opCtx,
                      Collection* collection,
                      IndexCatalogEntryContainer* entries,
                      IndexCatalogEntry* entry)
        : _opCtx(opCtx), _collection(collection), _entries(entries), _entry(entry) {}

    void commit() final {
        // Ban reading from this collection on committed reads on snapshots before now.
        auto replCoord = repl::ReplicationCoordinator::get(_opCtx);
        auto snapshotName = replCoord->reserveSnapshotName(_opCtx);
        _collection->setMinimumVisibleSnapshot(snapshotName);

        delete _entry;
    }

    void rollback() final {
        _entries->add(_entry);
        _collection->infoCache()->addedIndex(_opCtx, _entry->descriptor());
    }

private:
    OperationContext* _opCtx;
    Collection* _collection;
    IndexCatalogEntryContainer* _entries;
    IndexCatalogEntry* _entry;
};
}  // namespace

Status IndexCatalogImpl::_dropIndex(OperationContext* opCtx, IndexCatalogEntry* entry) {
    /**
     * IndexState in order
     *  <db>.system.indexes
     *    NamespaceDetails
     *      <db>.system.ns
     */

    // ----- SANITY CHECKS -------------
    if (!entry)
        return Status(ErrorCodes::BadValue, "IndexCatalog::_dropIndex passed NULL");

    _checkMagic();
    Status status = checkUnfinished();
    if (!status.isOK())
        return status;

    // Pulling indexName/indexNamespace out as they are needed post descriptor release.
    string indexName = entry->descriptor()->indexName();
    string indexNamespace = entry->descriptor()->indexNamespace();

    // If any cursors could be using this index, invalidate them. Note that we do not use indexes
    // until they are ready, so we do not need to invalidate anything if the index fails while it is
    // being built.
    // TODO only kill cursors that are actually using the index rather than everything on this
    // collection.
    if (entry->isReady(opCtx)) {
        _collection->getCursorManager()->invalidateAll(
            opCtx, false, str::stream() << "index '" << indexName << "' dropped");
    }

    // --------- START REAL WORK ----------
    audit::logDropIndex(&cc(), indexName, _collection->ns().ns());

    invariant(_entries.release(entry->descriptor()) == entry);
    opCtx->recoveryUnit()->registerChange(
        new IndexRemoveChange(opCtx, _collection, &_entries, entry));
    _collection->infoCache()->droppedIndex(opCtx, indexName);
    entry = nullptr;
    _deleteIndexFromDisk(opCtx, indexName, indexNamespace);

    _checkMagic();


    return Status::OK();
}

void IndexCatalogImpl::_deleteIndexFromDisk(OperationContext* opCtx,
                                            const string& indexName,
                                            const string& indexNamespace) {
    Status status = _collection->getCatalogEntry()->removeIndex(opCtx, indexName);
    if (status.code() == ErrorCodes::NamespaceNotFound) {
        // this is ok, as we may be partially through index creation
    } else if (!status.isOK()) {
        warning() << "couldn't drop index " << indexName << " on collection: " << _collection->ns()
                  << " because of " << redact(status);
    }
}

// These are the index specs of indexes that were "leftover".
// "Leftover" means they were unfinished when a mongod shut down.
// Certain operations are prohibited until someone fixes.
// Retrieve by calling getAndClearUnfinishedIndexes().
vector<BSONObj> IndexCatalogImpl::getAndClearUnfinishedIndexes(OperationContext* opCtx) {
    vector<BSONObj> toReturn = _unfinishedIndexes;
    _unfinishedIndexes.clear();
    for (size_t i = 0; i < toReturn.size(); i++) {
        BSONObj spec = toReturn[i];

        BSONObj keyPattern = spec.getObjectField("key");
        IndexDescriptor desc(_collection, _getAccessMethodName(opCtx, keyPattern), spec);

        _deleteIndexFromDisk(opCtx, desc.indexName(), desc.indexNamespace());
    }
    return toReturn;
}

bool IndexCatalogImpl::isMultikey(OperationContext* opCtx, const IndexDescriptor* idx) {
    IndexCatalogEntry* entry = _entries.find(idx);
    invariant(entry);
    return entry->isMultikey();
}

MultikeyPaths IndexCatalogImpl::getMultikeyPaths(OperationContext* opCtx,
                                                 const IndexDescriptor* idx) {
    IndexCatalogEntry* entry = _entries.find(idx);
    invariant(entry);
    return entry->getMultikeyPaths(opCtx);
}

// ---------------------------

bool IndexCatalogImpl::haveAnyIndexes() const {
    return _entries.size() != 0;
}

//该表索引总数，可以参考dropCollection调用方法
int IndexCatalogImpl::numIndexesTotal(OperationContext* opCtx) const {
    int count = _entries.size() + _unfinishedIndexes.size();
    dassert(_collection->getCatalogEntry()->getTotalIndexCount(opCtx) == count);
    return count;
}

int IndexCatalogImpl::numIndexesReady(OperationContext* opCtx) const {
    int count = 0;
    IndexIterator ii = _this->getIndexIterator(opCtx, /*includeUnfinished*/ false);
    while (ii.more()) {
        ii.next();
        count++;
    }
    dassert(_collection->getCatalogEntry()->getCompletedIndexCount(opCtx) == count);
    return count;
}

bool IndexCatalogImpl::haveIdIndex(OperationContext* opCtx) const {
    return findIdIndex(opCtx) != nullptr;
}

IndexCatalogImpl::IndexIteratorImpl::IndexIteratorImpl(OperationContext* opCtx,
                                                       const IndexCatalog* cat,
                                                       bool includeUnfinishedIndexes)
    : _includeUnfinishedIndexes(includeUnfinishedIndexes),
      _opCtx(opCtx),
      _catalog(cat),
      _iterator(cat->_getEntries().begin()),
      _start(true),
      _prev(nullptr),
      _next(nullptr) {}

auto IndexCatalogImpl::IndexIteratorImpl::clone_impl() const -> IndexIteratorImpl* {
    return new IndexIteratorImpl(*this);
}

/*
插入数据的时候调用栈

Breakpoint 1, mongo::KVCollectionCatalogEntry::_getMetaData (this=0x7fe69d14b400, opCtx=0x7fe69d14db80) at src/mongo/db/storage/kv/kv_collection_catalog_entry.cpp:309
309         return _catalog->getMetaData(opCtx, ns().toString());
(gdb) bt
#0  mongo::KVCollectionCatalogEntry::_getMetaData (this=0x7fe69d14b400, opCtx=0x7fe69d14db80) at src/mongo/db/storage/kv/kv_collection_catalog_entry.cpp:309
#1  0x00007fe69567c7e2 in mongo::BSONCollectionCatalogEntry::isIndexReady (this=<optimized out>, opCtx=<optimized out>, indexName=...) at src/mongo/db/storage/bson_collection_catalog_entry.cpp:172
#2  0x00007fe69585e4a6 in _catalogIsReady (opCtx=<optimized out>, this=0x7fe699a36500) at src/mongo/db/catalog/index_catalog_entry_impl.cpp:332
#3  mongo::IndexCatalogEntryImpl::isReady (this=0x7fe699a36500, opCtx=<optimized out>) at src/mongo/db/catalog/index_catalog_entry_impl.cpp:167
#4  0x00007fe695854ee9 in isReady (opCtx=0x7fe69d14db80, this=0x7fe699a5b3b8) at src/mongo/db/catalog/index_catalog_entry.h:224
#5  mongo::IndexCatalogImpl::IndexIteratorImpl::_advance (this=this@entry=0x7fe69d3d3680) at src/mongo/db/catalog/index_catalog_impl.cpp:1170
#6  0x00007fe695855047 in mongo::IndexCatalogImpl::IndexIteratorImpl::more (this=0x7fe69d3d3680) at src/mongo/db/catalog/index_catalog_impl.cpp:1129
#7  0x00007fe69585315d in more (this=<synthetic pointer>) at src/mongo/db/catalog/index_catalog.h:104
#8  mongo::IndexCatalogImpl::findIdIndex (this=<optimized out>, opCtx=<optimized out>) at src/mongo/db/catalog/index_catalog_impl.cpp:1182
#9  0x00007fe69583af61 in findIdIndex (opCtx=0x7fe69d14db80, this=0x7fe69984c038) at src/mongo/db/catalog/index_catalog.h:331
#10 mongo::CollectionImpl::insertDocuments (this=0x7fe69984bfc0, opCtx=0x7fe69d14db80, begin=..., end=..., opDebug=0x7fe69d145938, enforceQuota=true, fromMigrate=false) at src/mongo/db/catalog/collection_impl.cpp:350
#11 0x00007fe6957ce352 in insertDocuments (fromMigrate=false, enforceQuota=true, opDebug=<optimized out>, end=..., begin=..., opCtx=0x7fe69d14db80, this=<optimized out>) at src/mongo/db/catalog/collection.h:498
#12 mongo::(anonymous namespace)::insertDocuments (opCtx=0x7fe69d14db80, collection=<optimized out>, begin=begin@entry=..., end=end@entry=...) at src/mongo/db/ops/write_ops_exec.cpp:329
#13 0x00007fe6957d4026 in operator() (__closure=<optimized out>) at src/mongo/db/ops/write_ops_exec.cpp:406
#14 writeConflictRetry<mongo::(anonymous namespace)::insertBatchAndHandleErrors(mongo::OperationContext*, const mongo::write_ops::Insert&, std::vector<mongo::InsertStatement>&, mongo::(anonymous namespace)::LastOpFixer*, mongo::WriteResult*)::<lambda()> > (f=<optimized out>, ns=..., opStr=..., opCtx=0x7fe69d14db80) at src/mongo/db/concurrency/write_conflict_exception.h:91
*/

bool IndexCatalogImpl::IndexIteratorImpl::more() {
    if (_start) {
        _advance(); //IndexCatalogImpl::IndexIteratorImpl::_advance
        _start = false;
    }
    return _next != nullptr;
}

IndexDescriptor* IndexCatalogImpl::IndexIteratorImpl::next() {
    if (!more())
        return nullptr;
    _prev = _next;
    _advance();
    return _prev->descriptor();
}

IndexAccessMethod* IndexCatalogImpl::IndexIteratorImpl::accessMethod(const IndexDescriptor* desc) {
    invariant(desc == _prev->descriptor());
    return _prev->accessMethod();
}

IndexCatalogEntry* IndexCatalogImpl::IndexIteratorImpl::catalogEntry(const IndexDescriptor* desc) {
    invariant(desc == _prev->descriptor());
    return _prev;
}

void IndexCatalogImpl::IndexIteratorImpl::_advance() {
    _next = nullptr;

    while (_iterator != _catalog->_getEntries().end()) {
        IndexCatalogEntry* entry = _iterator->get();
        ++_iterator;

        if (!_includeUnfinishedIndexes) {
            if (auto minSnapshot = entry->getMinimumVisibleSnapshot()) {
                if (auto mySnapshot = _opCtx->recoveryUnit()->getMajorityCommittedSnapshot()) {
                    if (mySnapshot < minSnapshot) {
                        // This index isn't finished in my snapshot.
                        continue;
                    }
                }
            }

            if (!entry->isReady(_opCtx)) //IndexCatalogEntryImpl::isReady
                continue;
        }

        _next = entry;
        return;
    }
}

/*
插入数据的时候调用栈

Breakpoint 1, mongo::KVCollectionCatalogEntry::_getMetaData (this=0x7fe69d14b400, opCtx=0x7fe69d14db80) at src/mongo/db/storage/kv/kv_collection_catalog_entry.cpp:309
309         return _catalog->getMetaData(opCtx, ns().toString());
(gdb) bt
#0  mongo::KVCollectionCatalogEntry::_getMetaData (this=0x7fe69d14b400, opCtx=0x7fe69d14db80) at src/mongo/db/storage/kv/kv_collection_catalog_entry.cpp:309
#1  0x00007fe69567c7e2 in mongo::BSONCollectionCatalogEntry::isIndexReady (this=<optimized out>, opCtx=<optimized out>, indexName=...) at src/mongo/db/storage/bson_collection_catalog_entry.cpp:172
#2  0x00007fe69585e4a6 in _catalogIsReady (opCtx=<optimized out>, this=0x7fe699a36500) at src/mongo/db/catalog/index_catalog_entry_impl.cpp:332
#3  mongo::IndexCatalogEntryImpl::isReady (this=0x7fe699a36500, opCtx=<optimized out>) at src/mongo/db/catalog/index_catalog_entry_impl.cpp:167
#4  0x00007fe695854ee9 in isReady (opCtx=0x7fe69d14db80, this=0x7fe699a5b3b8) at src/mongo/db/catalog/index_catalog_entry.h:224
#5  mongo::IndexCatalogImpl::IndexIteratorImpl::_advance (this=this@entry=0x7fe69d3d3680) at src/mongo/db/catalog/index_catalog_impl.cpp:1170
#6  0x00007fe695855047 in mongo::IndexCatalogImpl::IndexIteratorImpl::more (this=0x7fe69d3d3680) at src/mongo/db/catalog/index_catalog_impl.cpp:1129
#7  0x00007fe69585315d in more (this=<synthetic pointer>) at src/mongo/db/catalog/index_catalog.h:104
#8  mongo::IndexCatalogImpl::findIdIndex (this=<optimized out>, opCtx=<optimized out>) at src/mongo/db/catalog/index_catalog_impl.cpp:1182
#9  0x00007fe69583af61 in findIdIndex (opCtx=0x7fe69d14db80, this=0x7fe69984c038) at src/mongo/db/catalog/index_catalog.h:331
#10 mongo::CollectionImpl::insertDocuments (this=0x7fe69984bfc0, opCtx=0x7fe69d14db80, begin=..., end=..., opDebug=0x7fe69d145938, enforceQuota=true, fromMigrate=false) at src/mongo/db/catalog/collection_impl.cpp:350
#11 0x00007fe6957ce352 in insertDocuments (fromMigrate=false, enforceQuota=true, opDebug=<optimized out>, end=..., begin=..., opCtx=0x7fe69d14db80, this=<optimized out>) at src/mongo/db/catalog/collection.h:498
#12 mongo::(anonymous namespace)::insertDocuments (opCtx=0x7fe69d14db80, collection=<optimized out>, begin=begin@entry=..., end=end@entry=...) at src/mongo/db/ops/write_ops_exec.cpp:329
#13 0x00007fe6957d4026 in operator() (__closure=<optimized out>) at src/mongo/db/ops/write_ops_exec.cpp:406
#14 writeConflictRetry<mongo::(anonymous namespace)::insertBatchAndHandleErrors(mongo::OperationContext*, const mongo::write_ops::Insert&, std::vector<mongo::InsertStatement>&, mongo::(anonymous namespace)::LastOpFixer*, mongo::WriteResult*)::<lambda()> > (f=<optimized out>, ns=..., opStr=..., opCtx=0x7fe69d14db80) at src/mongo/db/concurrency/write_conflict_exception.h:91
*/

//查找ID索引
//CollectionImpl::insertDocuments中执行
IndexDescriptor* IndexCatalogImpl::findIdIndex(OperationContext* opCtx) const {
    IndexIterator ii = _this->getIndexIterator(opCtx, false); //IndexCatalogImpl::IndexIteratorImpl::more
    while (ii.more()) { //索引遍历
        IndexDescriptor* desc = ii.next();
        if (desc->isIdIndex())
            return desc;
    }
    return nullptr;
}

//根据索引名查找
IndexDescriptor* IndexCatalogImpl::findIndexByName(OperationContext* opCtx,
                                                   StringData name,
                                                   bool includeUnfinishedIndexes) const {
    IndexIterator ii = _this->getIndexIterator(opCtx, includeUnfinishedIndexes);
    while (ii.more()) {
        IndexDescriptor* desc = ii.next();
        if (desc->indexName() == name)
            return desc;
    }
    return nullptr;
}

//索引一样，名字不一样，通过这里判断
//参考IndexCatalogImpl::_doesSpecConflictWithExisting
IndexDescriptor* IndexCatalogImpl::findIndexByKeyPatternAndCollationSpec(
    OperationContext* opCtx,
    const BSONObj& key,
    const BSONObj& collationSpec,
    bool includeUnfinishedIndexes) const {
    IndexIterator ii = _this->getIndexIterator(opCtx, includeUnfinishedIndexes);
    while (ii.more()) {
        IndexDescriptor* desc = ii.next();
        if (SimpleBSONObjComparator::kInstance.evaluate(desc->keyPattern() == key) &&
            SimpleBSONObjComparator::kInstance.evaluate(
                desc->infoObj().getObjectField("collation") == collationSpec)) {
            return desc;
        }
    }
    return nullptr;
}

void IndexCatalogImpl::findIndexesByKeyPattern(OperationContext* opCtx,
                                               const BSONObj& key,
                                               bool includeUnfinishedIndexes,
                                               std::vector<IndexDescriptor*>* matches) const {
    invariant(matches);
    IndexIterator ii = _this->getIndexIterator(opCtx, includeUnfinishedIndexes);
    while (ii.more()) {
        IndexDescriptor* desc = ii.next();
        if (SimpleBSONObjComparator::kInstance.evaluate(desc->keyPattern() == key)) {
            matches->push_back(desc);
        }
    }
}

IndexDescriptor* IndexCatalogImpl::findShardKeyPrefixedIndex(OperationContext* opCtx,
                                                             const BSONObj& shardKey,
                                                             bool requireSingleKey) const {
    IndexDescriptor* best = nullptr;

    IndexIterator ii = _this->getIndexIterator(opCtx, false);
    while (ii.more()) {
        IndexDescriptor* desc = ii.next();
        bool hasSimpleCollation = desc->infoObj().getObjectField("collation").isEmpty();

        if (desc->isPartial())
            continue;

        if (!shardKey.isPrefixOf(desc->keyPattern(), SimpleBSONElementComparator::kInstance))
            continue;

        if (!desc->isMultikey(opCtx) && hasSimpleCollation)
            return desc;

        if (!requireSingleKey && hasSimpleCollation)
            best = desc;
    }

    return best;
}

void IndexCatalogImpl::findIndexByType(OperationContext* opCtx,
                                       const string& type,
                                       vector<IndexDescriptor*>& matches,
                                       bool includeUnfinishedIndexes) const {
    IndexIterator ii = _this->getIndexIterator(opCtx, includeUnfinishedIndexes);
    while (ii.more()) {
        IndexDescriptor* desc = ii.next();
        if (IndexNames::findPluginName(desc->keyPattern()) == type) {
            matches.push_back(desc);
        }
    }
}

IndexAccessMethod* IndexCatalogImpl::getIndex(const IndexDescriptor* desc) {
    IndexCatalogEntry* entry = _entries.find(desc);
    massert(17334, "cannot find index entry", entry);
    return entry->accessMethod();
}

const IndexAccessMethod* IndexCatalogImpl::getIndex(const IndexDescriptor* desc) const {
    return getEntry(desc)->accessMethod();
}

const IndexCatalogEntry* IndexCatalogImpl::getEntry(const IndexDescriptor* desc) const {
    const IndexCatalogEntry* entry = _entries.find(desc);
    massert(17357, "cannot find index entry", entry);
    return entry;
}


const IndexDescriptor* IndexCatalogImpl::refreshEntry(OperationContext* opCtx,
                                                      const IndexDescriptor* oldDesc) {
    invariant(opCtx->lockState()->isCollectionLockedForMode(_collection->ns().ns(), MODE_X));
    invariant(!BackgroundOperation::inProgForNs(_collection->ns()));

    const std::string indexName = oldDesc->indexName();
    invariant(_collection->getCatalogEntry()->isIndexReady(opCtx, indexName));

    // Notify other users of the IndexCatalog that we're about to invalidate 'oldDesc'.
    const bool collectionGoingAway = false;
    _collection->getCursorManager()->invalidateAll(
        opCtx,
        collectionGoingAway,
        str::stream() << "definition of index '" << indexName << "' changed");

    // Delete the IndexCatalogEntry that owns this descriptor.  After deletion, 'oldDesc' is
    // invalid and should not be dereferenced.
    IndexCatalogEntry* oldEntry = _entries.release(oldDesc);
    opCtx->recoveryUnit()->registerChange(
        new IndexRemoveChange(opCtx, _collection, &_entries, oldEntry));

    // Ask the CollectionCatalogEntry for the new index spec.
    BSONObj spec = _collection->getCatalogEntry()->getIndexSpec(opCtx, indexName).getOwned();
    BSONObj keyPattern = spec.getObjectField("key");

    // Re-register this index in the index catalog with the new spec.
    auto newDesc = stdx::make_unique<IndexDescriptor>(
        _collection, _getAccessMethodName(opCtx, keyPattern), spec);
    const bool initFromDisk = false;
    const IndexCatalogEntry* newEntry =
        _setupInMemoryStructures(opCtx, std::move(newDesc), initFromDisk);
    invariant(newEntry->isReady(opCtx));

    // Return the new descriptor.
    return newEntry->descriptor();
}

// ---------------------------
//IndexCatalogImpl::_indexRecords
Status IndexCatalogImpl::_indexFilteredRecords(OperationContext* opCtx,
                                               IndexCatalogEntry* index,
                                               const std::vector<BsonRecord>& bsonRecords,
                                               int64_t* keysInsertedOut) {
    InsertDeleteOptions options;
    prepareInsertDeleteOptions(opCtx, index->descriptor(), &options);

    for (auto bsonRecord : bsonRecords) {
        int64_t inserted;
        invariant(bsonRecord.id != RecordId());
		//IndexCatalogEntryImpl::accessMethod
		//IndexAccessMethod::insert
        Status status = index->accessMethod()->insert(
            opCtx, *bsonRecord.docPtr, bsonRecord.id, options, &inserted);
        if (!status.isOK())
            return status;

        if (keysInsertedOut) {
            *keysInsertedOut += inserted;
        }
    }
    return Status::OK();
}

//IndexCatalogImpl::indexRecords
Status IndexCatalogImpl::_indexRecords(OperationContext* opCtx,
                                       IndexCatalogEntry* index,
                                       const std::vector<BsonRecord>& bsonRecords,
                                       int64_t* keysInsertedOut) {
    const MatchExpression* filter = index->getFilterExpression();
    if (!filter)
        return _indexFilteredRecords(opCtx, index, bsonRecords, keysInsertedOut);

    std::vector<BsonRecord> filteredBsonRecords;
    for (auto bsonRecord : bsonRecords) {
        if (filter->matchesBSON(*(bsonRecord.docPtr)))
            filteredBsonRecords.push_back(bsonRecord);
    }

    return _indexFilteredRecords(opCtx, index, filteredBsonRecords, keysInsertedOut);
}

Status IndexCatalogImpl::_unindexRecord(OperationContext* opCtx,
                                        IndexCatalogEntry* index,
                                        const BSONObj& obj,
                                        const RecordId& loc,
                                        bool logIfError,
                                        int64_t* keysDeletedOut) {
    InsertDeleteOptions options;
    prepareInsertDeleteOptions(opCtx, index->descriptor(), &options);
    options.logIfError = logIfError;

    // On WiredTiger, we do blind unindexing of records for efficiency.  However, when duplicates
    // are allowed in unique indexes, WiredTiger does not do blind unindexing, and instead confirms
    // that the recordid matches the element we are removing.
    // We need to disable blind-deletes for in-progress indexes, in order to force recordid-matching
    // for unindex operations, since initial sync can build an index over a collection with
    // duplicates. See SERVER-17487 for more details.
    options.dupsAllowed = options.dupsAllowed || !index->isReady(opCtx);

    int64_t removed;
    Status status = index->accessMethod()->remove(opCtx, obj, loc, options, &removed);

    if (!status.isOK()) {
        log() << "Couldn't unindex record " << redact(obj) << " from collection "
              << _collection->ns() << ". Status: " << redact(status);
    }

    if (keysDeletedOut) {
        *keysDeletedOut += removed;
    }

    return Status::OK();
}

//CollectionImpl::_insertDocuments中调用执行
//WiredTigerRecordStore::insertRecords 普通插入   IndexCatalogImpl::indexRecords 索引插入
Status IndexCatalogImpl::indexRecords(OperationContext* opCtx,
                                      const std::vector<BsonRecord>& bsonRecords,
                                      int64_t* keysInsertedOut) {
    if (keysInsertedOut) {
        *keysInsertedOut = 0;
    }

    for (IndexCatalogEntryContainer::const_iterator i = _entries.begin(); i != _entries.end();
         ++i) {
        Status s = _indexRecords(opCtx, i->get(), bsonRecords, keysInsertedOut);
        if (!s.isOK())
            return s;
    }

    return Status::OK();
}

void IndexCatalogImpl::unindexRecord(OperationContext* opCtx,
                                     const BSONObj& obj,
                                     const RecordId& loc,
                                     bool noWarn,
                                     int64_t* keysDeletedOut) {
    if (keysDeletedOut) {
        *keysDeletedOut = 0;
    }

    for (IndexCatalogEntryContainer::const_iterator i = _entries.begin(); i != _entries.end();
         ++i) {
        IndexCatalogEntry* entry = i->get();

        // If it's a background index, we DO NOT want to log anything.
        bool logIfError = entry->isReady(opCtx) ? !noWarn : false;
        _unindexRecord(opCtx, entry, obj, loc, logIfError, keysDeletedOut).transitional_ignore();
    }
}

BSONObj IndexCatalogImpl::fixIndexKey(const BSONObj& key) {
    if (IndexDescriptor::isIdIndexPattern(key)) {
        return _idObj;
    }
    if (key["_id"].type() == Bool && key.nFields() == 1) {
        return _idObj;
    }
    return key;
}

void IndexCatalogImpl::prepareInsertDeleteOptions(OperationContext* opCtx,
                                                  const IndexDescriptor* desc,
                                                  InsertDeleteOptions* options) {
    auto replCoord = repl::ReplicationCoordinator::get(opCtx);
    if (replCoord->shouldRelaxIndexConstraints(opCtx, NamespaceString(desc->parentNS()))) {
        options->getKeysMode = IndexAccessMethod::GetKeysMode::kRelaxConstraints;
    } else {
        options->getKeysMode = IndexAccessMethod::GetKeysMode::kEnforceConstraints;
    }

    // Don't allow dups for Id key. Allow dups for non-unique keys or when constraints relaxed.
    if (KeyPattern::isIdKeyPattern(desc->keyPattern())) {
        options->dupsAllowed = false;
    } else {
        options->dupsAllowed = !desc->unique() ||
            options->getKeysMode == IndexAccessMethod::GetKeysMode::kRelaxConstraints;
    }
}

//
StatusWith<BSONObj> IndexCatalogImpl::_fixIndexSpec(OperationContext* opCtx,
                                                    Collection* collection,
                                                    const BSONObj& spec) {
    auto statusWithSpec = IndexLegacy::adjustIndexSpecObject(spec);
    if (!statusWithSpec.isOK()) {
        return statusWithSpec;
    }
    BSONObj o = statusWithSpec.getValue();

    BSONObjBuilder b;

    // We've already verified in IndexCatalog::_isSpecOk() that the index version is present and
    // that it is representable as a 32-bit integer.
    auto vElt = o["v"];
    invariant(vElt);

    b.append("v", vElt.numberInt());

    if (o["unique"].trueValue())
        b.appendBool("unique", true);  // normalize to bool true in case was int 1 or something...

    BSONObj key = fixIndexKey(o["key"].Obj());
    b.append("key", key);

    string name = o["name"].String();
    if (IndexDescriptor::isIdIndexPattern(key)) {
        name = "_id_";
    }
    b.append("name", name);

    {
        BSONObjIterator i(o);
        while (i.more()) {
            BSONElement e = i.next();
            string s = e.fieldName();

            if (s == "_id") {
                // skip
            } else if (s == "dropDups") {
                // dropDups is silently ignored and removed from the spec as of SERVER-14710.
            } else if (s == "v" || s == "unique" || s == "key" || s == "name") {
                // covered above
            } else {
                b.append(e);
            }
        }
    }

    return b.obj();
}
}  // namespace mongo
