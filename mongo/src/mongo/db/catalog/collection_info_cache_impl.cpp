/**
 *    Copyright (C) 2017 10gen Inc.
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

#include "mongo/platform/basic.h"

#include "mongo/db/catalog/collection_info_cache_impl.h"

#include "mongo/base/init.h"
#include "mongo/db/catalog/collection.h"
#include "mongo/db/catalog/index_catalog.h"
#include "mongo/db/concurrency/d_concurrency.h"
#include "mongo/db/fts/fts_spec.h"
#include "mongo/db/index/index_descriptor.h"
#include "mongo/db/index_legacy.h"
#include "mongo/db/query/plan_cache.h"
#include "mongo/db/query/planner_ixselect.h"
#include "mongo/db/service_context.h"
#include "mongo/db/ttl_collection_cache.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/clock_source.h"
#include "mongo/util/debug_util.h"
#include "mongo/util/log.h"

namespace mongo {
namespace {
MONGO_INITIALIZER(InitializeCollectionInfoCacheFactory)(InitializerContext* const) {
    CollectionInfoCache::registerFactory(
        [](Collection* const collection, const NamespaceString& ns) {
            return stdx::make_unique<CollectionInfoCacheImpl>(collection, ns);
        });
    return Status::OK();
}
}  // namespace

CollectionInfoCacheImpl::CollectionInfoCacheImpl(Collection* collection, const NamespaceString& ns)
    : _collection(collection),
      _ns(ns),
      _keysComputed(false),
      _planCache(stdx::make_unique<PlanCache>(ns.ns())),
      _querySettings(stdx::make_unique<QuerySettings>()),
      _indexUsageTracker(getGlobalServiceContext()->getPreciseClockSource()) {}

CollectionInfoCacheImpl::~CollectionInfoCacheImpl() {
    // Necessary because the collection cache will not explicitly get updated upon database drop.
    if (_hasTTLIndex) {
        TTLCollectionCache& ttlCollectionCache = TTLCollectionCache::get(getGlobalServiceContext());
        ttlCollectionCache.unregisterCollection(_ns);
    }
}

//获取_indexedPaths
const UpdateIndexData& CollectionInfoCacheImpl::getIndexKeys(OperationContext* opCtx) const {
    // This requires "some" lock, and MODE_IS is an expression for that, for now.
    dassert(opCtx->lockState()->isCollectionLockedForMode(_collection->ns().ns(), MODE_IS));
    invariant(_keysComputed);
    return _indexedPaths;
}

//生成最新的_indexedPaths
void CollectionInfoCacheImpl::computeIndexKeys(OperationContext* opCtx) {
    _indexedPaths.clear();

    bool hadTTLIndex = _hasTTLIndex;
    _hasTTLIndex = false;

	//遍历该表下面的所有index信息
    IndexCatalog::IndexIterator i = _collection->getIndexCatalog()->getIndexIterator(opCtx, true);
    while (i.more()) {
        IndexDescriptor* descriptor = i.next();

		//非TEXT索引
        if (descriptor->getAccessMethodName() != IndexNames::TEXT) {
            BSONObj key = descriptor->keyPattern();
            const BSONObj& infoObj = descriptor->infoObj();
			//过期索引
            if (infoObj.hasField("expireAfterSeconds")) {
                _hasTTLIndex = true;
            }
            BSONObjIterator j(key);
            while (j.more()) {
                BSONElement e = j.next();
				//索引信息添加到_indexedPaths
                _indexedPaths.addPath(e.fieldName());
            }
        } else { //text索引
            fts::FTSSpec ftsSpec(descriptor->infoObj());

            if (ftsSpec.wildcard()) {
                _indexedPaths.allPathsIndexed();
            } else {
                for (size_t i = 0; i < ftsSpec.numExtraBefore(); ++i) {
                    _indexedPaths.addPath(ftsSpec.extraBefore(i));
                }
                for (fts::Weights::const_iterator it = ftsSpec.weights().begin();
                     it != ftsSpec.weights().end();
                     ++it) {
                    _indexedPaths.addPath(it->first);
                }
                for (size_t i = 0; i < ftsSpec.numExtraAfter(); ++i) {
                    _indexedPaths.addPath(ftsSpec.extraAfter(i));
                }
                // Any update to a path containing "language" as a component could change the
                // language of a subdocument.  Add the override field as a path component.
                _indexedPaths.addPathComponent(ftsSpec.languageOverrideField());
            }
        }

        // handle partial indexes
        const IndexCatalogEntry* entry = i.catalogEntry(descriptor);
        const MatchExpression* filter = entry->getFilterExpression();
        if (filter) {
            unordered_set<std::string> paths;
            QueryPlannerIXSelect::getFields(filter, "", &paths);
            for (auto it = paths.begin(); it != paths.end(); ++it) {
                _indexedPaths.addPath(*it);
            }
        }
    }

    TTLCollectionCache& ttlCollectionCache = TTLCollectionCache::get(getGlobalServiceContext());

	//以前没有TTL索引，现在有了，则需要registerCollection
	//以前有TTL索引，现在没有了，则需要注销
    if (_hasTTLIndex != hadTTLIndex) {
        if (_hasTTLIndex) {
            ttlCollectionCache.registerCollection(_collection->ns());
        } else {
            ttlCollectionCache.unregisterCollection(_collection->ns());
        }
    }

    _keysComputed = true;
}

/*
调用的地方比较多:
Count_cmd.cpp (mongo\src\mongo\db\commands):            collection->infoCache()->notifyOfQuery(opCtx, summaryStats.indexesUsed);
Distinct.cpp (mongo\src\mongo\db\commands):            collection->infoCache()->notifyOfQuery(opCtx, stats.indexesUsed);
Document_source_cursor.cpp (mongo\src\mongo\db\pipeline):        collection->infoCache()->notifyOfQuery(pExpCtx->opCtx, _planSummaryStats.indexesUsed);
Find.cpp (mongo\src\mongo\db\query):        collection->infoCache()->notifyOfQuery(opCtx, summaryStats.indexesUsed);
Find_and_modify.cpp (mongo\src\mongo\db\commands):                    collection->infoCache()->notifyOfQuery(opCtx, summaryStats.indexesUsed);
Find_and_modify.cpp (mongo\src\mongo\db\commands):                    collection->infoCache()->notifyOfQuery(opCtx, summaryStats.indexesUsed);
Geo_near_cmd.cpp (mongo\src\mongo\db\commands):        collection->infoCache()->notifyOfQuery(opCtx, summary.indexesUsed);
Group_cmd.cpp (mongo\src\mongo\db\commands):            coll->infoCache()->notifyOfQuery(opCtx, summaryStats.indexesUsed);
Mr.cpp (mongo\src\mongo\db\commands):                coll->infoCache()->notifyOfQuery(opCtx, stats.indexesUsed);
Write_ops_exec.cpp (mongo\src\mongo\db\ops):        collection->getCollection()->infoCache()->notifyOfQuery(opCtx, summary.indexesUsed);
Write_ops_exec.cpp (mongo\src\mongo\db\ops):        collection.getCollection()->infoCache()->notifyOfQuery(opCtx, summary.indexesUsed);
*/
//db.collection.aggregate({"$indexStats":{}})里面有对索引的统计
//当前真在使用的索引统计
void CollectionInfoCacheImpl::notifyOfQuery(OperationContext* opCtx,
                                            const std::set<std::string>& indexesUsed) {
    // Record indexes used to fulfill query.
    for (auto it = indexesUsed.begin(); it != indexesUsed.end(); ++it) {
        // This index should still exist, since the PlanExecutor would have been killed if the
        // index was dropped (and we would not get here).
        dassert(NULL != _collection->getIndexCatalog()->findIndexByName(opCtx, *it));

		//CollectionIndexUsageTracker::recordIndexAccess
        _indexUsageTracker.recordIndexAccess(*it);
    }
}

//清除缓存的_planCache  CollectionInfoCacheImpl::rebuildIndexData调用
void CollectionInfoCacheImpl::clearQueryCache() {
    LOG(1) << _collection->ns().ns() << ": clearing plan cache - collection info cache reset";
    if (NULL != _planCache.get()) {
        _planCache->clear();
    }
}

PlanCache* CollectionInfoCacheImpl::getPlanCache() const {
    return _planCache.get();
}

QuerySettings* CollectionInfoCacheImpl::getQuerySettings() const {
    return _querySettings.get();
}

//CollectionInfoCacheImpl::rebuildIndexData中调用
//CollectionInfoCacheImpl::updatePlanCacheIndexEntries中完成IndexEntry和IndexDescriptor的转换
void CollectionInfoCacheImpl::updatePlanCacheIndexEntries(OperationContext* opCtx) {
    std::vector<IndexEntry> indexEntries;

    // TODO We shouldn't need to include unfinished indexes, but we must here because the index
    // catalog may be in an inconsistent state.  SERVER-18346.
    const bool includeUnfinishedIndexes = true;
    IndexCatalog::IndexIterator ii =
        _collection->getIndexCatalog()->getIndexIterator(opCtx, includeUnfinishedIndexes);
    while (ii.more()) {
		//获取索引信息
        const IndexDescriptor* desc = ii.next();
        const IndexCatalogEntry* ice = ii.catalogEntry(desc);
		//完成IndexEntry和IndexDescriptor的转换
        indexEntries.emplace_back(desc->keyPattern(),
                                  desc->getAccessMethodName(),
                                  desc->isMultikey(opCtx),
                                  ice->getMultikeyPaths(opCtx),
                                  desc->isSparse(),
                                  desc->unique(),
                                  desc->indexName(),
                                  ice->getFilterExpression(),
                                  desc->infoObj(),
                                  ice->getCollator());
    }

	//PlanCache::notifyOfIndexEntries
    _planCache->notifyOfIndexEntries(indexEntries);
}

//CollectionImpl::init中调用
//初始化该集合的planCache IndexEntry
void CollectionInfoCacheImpl::init(OperationContext* opCtx) {
    // Requires exclusive collection lock.
    invariant(opCtx->lockState()->isCollectionLockedForMode(_collection->ns().ns(), MODE_X));

    const bool includeUnfinishedIndexes = false;
    IndexCatalog::IndexIterator ii =
        _collection->getIndexCatalog()->getIndexIterator(opCtx, includeUnfinishedIndexes);
	//遍历该表的所有索引信息注册到_indexUsageTracker
	while (ii.more()) {
        const IndexDescriptor* desc = ii.next();
        _indexUsageTracker.registerIndex(desc->indexName(), desc->keyPattern());
    }

	//重新生产索引data 重新生成planCache的IndexEntry
    rebuildIndexData(opCtx);
}

//通过类似_collection->infoCache()->addedIndex(_opCtx, descriptorPtr);方式调用，参考IndexCatalogImpl::IndexBuildBlock::init()
//IndexCatalogImpl::IndexBuildBlock::init()调用
//新索引注册到_indexUsageTracker，表示该表新增了索引信息
void CollectionInfoCacheImpl::addedIndex(OperationContext* opCtx, const IndexDescriptor* desc) {
    // Requires exclusive collection lock.
    invariant(opCtx->lockState()->isCollectionLockedForMode(_collection->ns().ns(), MODE_X));
    invariant(desc);

    rebuildIndexData(opCtx);

    _indexUsageTracker.registerIndex(desc->indexName(), desc->keyPattern());
}

//IndexCatalogImpl::_dropIndex调用
//从_indexUsageTracker中注销索引
void CollectionInfoCacheImpl::droppedIndex(OperationContext* opCtx, StringData indexName) {
    // Requires exclusive collection lock.
    invariant(opCtx->lockState()->isCollectionLockedForMode(_collection->ns().ns(), MODE_X));

    rebuildIndexData(opCtx);
    _indexUsageTracker.unregisterIndex(indexName);
}

//CollectionInfoCacheImpl::init   CollectionInfoCacheImpl::addedIndex   
//CollectionInfoCacheImpl::droppedIndex调用
//重新生成planCache的IndexEntry
void CollectionInfoCacheImpl::rebuildIndexData(OperationContext* opCtx) {
	//清除缓存的querycache
    clearQueryCache();

    _keysComputed = false;
    computeIndexKeys(opCtx);
    updatePlanCacheIndexEntries(opCtx);
}

//db.collection.aggregate({"$indexStats":{}})会用到
//getIndexStats调用
CollectionIndexUsageMap CollectionInfoCacheImpl::getIndexUsageStats() const {
	//CollectionIndexUsageTracker::getUsageStats()
    return _indexUsageTracker.getUsageStats();
}
}  // namespace mongo
