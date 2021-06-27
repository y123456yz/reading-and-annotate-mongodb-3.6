/**
 *    Copyright (C) 2015 MongoDB Inc.
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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kSharding

#include "mongo/platform/basic.h"

#include "mongo/s/catalog_cache.h"

#include "mongo/base/status.h"
#include "mongo/base/status_with.h"
#include "mongo/db/query/collation/collator_factory_interface.h"
#include "mongo/db/repl/optime_with.h"
#include "mongo/platform/unordered_set.h"
#include "mongo/s/catalog/sharding_catalog_client.h"
#include "mongo/s/catalog/type_collection.h"
#include "mongo/s/catalog/type_database.h"
#include "mongo/s/client/shard_registry.h"
#include "mongo/s/grid.h"
#include "mongo/util/concurrency/with_lock.h"
#include "mongo/util/log.h"
#include "mongo/util/timer.h"

namespace mongo {
namespace {

// How many times to try refreshing the routing info if the set of chunks loaded from the config
// server is found to be inconsistent.
const int kMaxInconsistentRoutingInfoRefreshAttempts = 3;

/**
 * Given an (optional) initial routing table and a set of changed chunks returned by the catalog
 * cache loader, produces a new routing table with the changes applied.
 *
 * If the collection is no longer sharded returns nullptr. If the epoch has changed, expects that
 * the 'collectionChunksList' contains the full contents of the chunks collection for that namespace
 * so that the routing table can be built from scratch.
 *
 * Throws ConflictingOperationInProgress if the chunk metadata was found to be inconsistent (not
 * containing all the necessary chunks, contains overlaps or chunks' epoch values are not the same
 * as that of the collection). Since this situation may be transient, due to the collection being
 * dropped or recreated concurrently, the caller must retry the reload up to some configurable
 * number of attempts.
 */ 

//跟新新的chunk信息swCollectionAndChangedChunks到nss对应的existingRoutingInfo
//返回nss对应的ChunkManager   

//CatalogCache::_scheduleCollectionRefresh调用
std::shared_ptr<ChunkManager> 
 refreshCollectionRoutingInfo(
    OperationContext* opCtx,
    const NamespaceString& nss,
    //跟新新的chunk信息swCollectionAndChangedChunks到nss对应的existingRoutingInfo
    std::shared_ptr<ChunkManager> existingRoutingInfo,
    //swCollectionAndChangedChunks参考赋值ConfigServerCatalogCacheLoader::getChunksSince
    StatusWith<CatalogCacheLoader::CollectionAndChangedChunks> swCollectionAndChangedChunks) {
    if (swCollectionAndChangedChunks == ErrorCodes::NamespaceNotFound) {
        return nullptr;
    }

	//对应ConfigServerCatalogCacheLoader::getChunksSince中的swCollAndChunks
	//cfg中config.chunks表对应版本大于本地缓存lastmod的所有增量变化的chunk
    const auto collectionAndChunks = uassertStatusOK(std::move(swCollectionAndChangedChunks));

	
    auto chunkManager = [&] {
        // If we have routing info already and it's for the same collection epoch, we're updating.
        // Otherwise, we're making a whole new routing table.
        //配合CatalogCache::_scheduleCollectionRefresh->ConfigServerCatalogCacheLoader::getChunksSince  
        //阅读，如果启动后有获取过表路由信息，则满足该条件
        //做更新操作
        if (existingRoutingInfo &&
            existingRoutingInfo->getVersion().epoch() == collectionAndChunks.epoch) {
			 //跟新新的chunk信息swCollectionAndChangedChunks到nss对应的existingRoutingInfo
			 //ChunkManager::makeUpdated,collectionAndChunks.changedChunks代表变化的chunks信息，也就是cfg中版本比本地缓存高的

			 //ChunkManager::makeUpdated
			 return existingRoutingInfo->makeUpdated(collectionAndChunks.changedChunks);
        }

		//表对应epoll发生了变化，说明rename了表名，则需要重新构造一个ChunkManager类
        auto defaultCollator = [&]() -> std::unique_ptr<CollatorInterface> {
            if (!collectionAndChunks.defaultCollation.isEmpty()) {
                // The collation should have been validated upon collection creation
                return uassertStatusOK(CollatorFactoryInterface::get(opCtx->getServiceContext())
                                           ->makeFromBSON(collectionAndChunks.defaultCollation));
            }
            return nullptr;
        }();

		//构造一个ChunkManager类
        return ChunkManager::makeNew(nss,
                                     collectionAndChunks.uuid,
                                     KeyPattern(collectionAndChunks.shardKeyPattern),
                                     std::move(defaultCollator),
                                     collectionAndChunks.shardKeyIsUnique,
                                     collectionAndChunks.epoch,
                                     collectionAndChunks.changedChunks);
    }();

    std::set<ShardId> shardIds;
	//ChunkManager::getAllShardIds  //返回所有包含chunk块的分片id列表存入shardIds
    chunkManager->getAllShardIds(&shardIds);
    for (const auto& shardId : shardIds) {
		//根据shardId获取对应的Shard信息  Grid::shardRegistry->ShardRegistry::getShard
        uassertStatusOK(Grid::get(opCtx)->shardRegistry()->getShard(opCtx, shardId));
    }
    return chunkManager;
}

}  // namespace

CatalogCache::CatalogCache(CatalogCacheLoader& cacheLoader) : _cacheLoader(cacheLoader) {}

CatalogCache::~CatalogCache() = default;

//从cfg获取dbName库的路由信息
StatusWith<CachedDatabaseInfo> CatalogCache::getDatabase(OperationContext* opCtx,
                                                         StringData dbName) {
    try {
        return {CachedDatabaseInfo(_getDatabase(opCtx, dbName))};
    } catch (const DBException& ex) {
        return ex.toStatus();
    }
}

//可以参考https://mongoing.com/archives/75945
//https://mongoing.com/archives/77370

//(元数据刷新，通过这里把chunks路由信息和metadata元数据信息关联起来)流程： 
//    ShardingState::_refreshMetadata->CatalogCache::getShardedCollectionRoutingInfoWithRefresh

//其他调用接口如下:
//moveChunk  checkShardVersion   ChunkSplitter::_runAutosplit splitIfNeeded  
//DropCmd::run RenameCollectionCmd  ClusterFind::runQuery  FindAndModifyCmd::run  CollectionStats::run
//ClusterAggregate::runAggregate    getExecutionNsRoutingInfo  dispatchShardPipeline
//ChunkManagerTargeter::init调用，获取集合路由缓存信息
StatusWith<CachedCollectionRoutingInfo> 
//CatalogCache::onStaleConfigError和CatalogCache::invalidateShardedCollection对needsRefresh设置为true
//这时候才会从cfg获取最新路由信息
  CatalogCache::getCollectionRoutingInfo(//注意这里只有needsRefresh为true的才需要路由刷新
    OperationContext* opCtx, const NamespaceString& nss) {
    while (true) {
        std::shared_ptr<DatabaseInfoEntry> dbEntry;
        try {
			//从cfg复制集的config.database和config.collections中获取dbName库及其下面的表信息
            dbEntry = _getDatabase(opCtx, nss.db());
        } catch (const DBException& ex) {
            return ex.toStatus();
        }

		//注意这里有加锁
        stdx::unique_lock<stdx::mutex> ul(_mutex);

        auto& collections = dbEntry->collections;

		//查找db下面所有的表，获取nss表
        auto it = collections.find(nss.ns());
		//该nss锁在DB下面没找到该nss
        if (it == collections.end()) { 
            auto shardStatus =
				//获取primaryShardId这个分片对应的shard信息   ShardRegistry::getShard返回Shard类型
				//，直接从本地缓存中获取shard信息
                Grid::get(opCtx)->shardRegistry()->getShard(opCtx, dbEntry->primaryShardId);
            if (!shardStatus.isOK()) {
                return {ErrorCodes::Error(40371),
                        str::stream() << "The primary shard for collection " << nss.ns()
                                      << " could not be loaded due to error "
                                      << shardStatus.getStatus().toString()};
            }

			//构造CachedCollectionRoutingInfo返回
            return {CachedCollectionRoutingInfo(
                dbEntry->primaryShardId, nss, std::move(shardStatus.getValue()))};
        }
		
		//本地缓存中有该表信息

		//获取nss对应的CollectionRoutingInfoEntry，也就是该集合对应的chunk分布
        auto& collEntry = it->second;

		//注意这里只有needsRefresh为true的才需要路由刷新
        if (collEntry.needsRefresh) { //该集合的路由信息需要重新刷新
            auto refreshNotification = collEntry.refreshCompletionNotification;
            if (!refreshNotification) {
                refreshNotification = (collEntry.refreshCompletionNotification =
                                           std::make_shared<Notification<Status>>());
				//获取dbEntry库下对应的nss集合的chunks路由信息
                _scheduleCollectionRefresh(ul, dbEntry, std::move(collEntry.routingInfo), nss, 1);
            }

            // Wait on the notification outside of the mutex
            ul.unlock();

            auto refreshStatus = [&]() {
                try {
                    return refreshNotification->get(opCtx);
                } catch (const DBException& ex) {
                    return ex.toStatus();
                }
            }();

            if (!refreshStatus.isOK()) {
                return refreshStatus;
            }

            // Once the refresh is complete, loop around to get the latest value
            continue;
        }

		//记录该集合的主分片和chunk路由信息
        return {CachedCollectionRoutingInfo(dbEntry->primaryShardId, collEntry.routingInfo)};
    }
}

//ChunkManagerTargeter::init调用，获取路由信息
StatusWith<CachedCollectionRoutingInfo> CatalogCache::getCollectionRoutingInfo(
    OperationContext* opCtx, StringData ns) {
    return getCollectionRoutingInfo(opCtx, NamespaceString(ns));
}

StatusWith<CachedCollectionRoutingInfo> CatalogCache::getCollectionRoutingInfoWithRefresh(
    OperationContext* opCtx, const NamespaceString& nss) {
    //设置needsRefresh为true，这样getCollectionRoutingInfo就需要路由刷新
    invalidateShardedCollection(nss);
    return getCollectionRoutingInfo(opCtx, nss);
}


//updateChunkWriteStatsAndSplitIfNeeded   ShardingState::_refreshMetadata中调用
//SessionsCollectionSharded::_checkCacheForSessionsCollection
//获取指定nss表对应得路由信息
StatusWith<CachedCollectionRoutingInfo> CatalogCache::getShardedCollectionRoutingInfoWithRefresh(
    OperationContext* opCtx, const NamespaceString& nss) {
    //设置needsRefresh为true，这样getCollectionRoutingInfo就需要路由刷新
    invalidateShardedCollection(nss);

    auto routingInfoStatus = getCollectionRoutingInfo(opCtx, nss);
    if (routingInfoStatus.isOK() && !routingInfoStatus.getValue().cm()) {
        return {ErrorCodes::NamespaceNotSharded,
                str::stream() << "Collection " << nss.ns() << " is not sharded."};
    }

    return routingInfoStatus;
}

StatusWith<CachedCollectionRoutingInfo> CatalogCache::getShardedCollectionRoutingInfoWithRefresh(
    OperationContext* opCtx, StringData ns) {
    return getShardedCollectionRoutingInfoWithRefresh(opCtx, NamespaceString(ns));
}

//CatalogCache::onStaleConfigError和CatalogCache::invalidateShardedCollection对needsRefresh设置为true
//这时候才会从cfg获取最新路由信息


//ClusterFind::runQuery  
//MoveChunkCmd::errmsgRun
//SplitCollectionCmd::errmsgRun
//ChunkManagerTargeter::refreshNow
//强制ccriToInvalidate这对于的表进行路由刷新
void CatalogCache::onStaleConfigError(CachedCollectionRoutingInfo&& ccriToInvalidate) {
    // Ensure the move constructor of CachedCollectionRoutingInfo is invoked in order to clear the
    // input argument so it can't be used anymore
    auto ccri(ccriToInvalidate);

	//该CachedCollectionRoutingInfo没有cm，说明异常，每个routeinfo都是collections路由信息，其chunk通过cm管理
    if (!ccri._cm) {
        // Here we received a stale config error for a collection which we previously thought was
        // unsharded.
        //设置needsRefresh为true，这样getCollectionRoutingInfo就需要路由刷新
        invalidateShardedCollection(ccri._nss);
        return;
    }

    // Here we received a stale config error for a collection which we previously though was sharded
    stdx::lock_guard<stdx::mutex> lg(_mutex);

	//获取ccri对应得DB，DB没找到说明该collection可能删除了
    auto it = _databases.find(NamespaceString(ccri._cm->getns()).db());
    if (it == _databases.end()) {
        // If the database does not exist, the collection must have been dropped so there is
        // nothing to invalidate. The getCollectionRoutingInfo will handle the reload of the
        // entire database and its collections.
        return;
    }

	//获取DB下面所有得collection
    auto& collections = it->second->collections;

	//ccri._cm chunk manager中查找是否有该表
    auto itColl = collections.find(ccri._cm->getns());
    if (itColl == collections.end()) {
        // If the collection does not exist, this means it must have been dropped since the last
        // time we retrieved a cache entry for it. Doing nothing in this case will cause the
        // next call to getCollectionRoutingInfo to return an unsharded collection.
        return;
    } else if (itColl->second.needsRefresh) {
        // Refresh has been scheduled for the collection already
        return;
    } else if (itColl->second.routingInfo->getVersion() == ccri._cm->getVersion()) {
    	//我们需要获取最新得路由信息
        // If the versions match, the last version of the routing information that we used is no
        // longer valid, so trigger a refresh.
        itColl->second.needsRefresh = true;
    }
}


//CatalogCache::onStaleConfigError和CatalogCache::invalidateShardedCollection对needsRefresh设置为true
//这时候才会从cfg获取最新路由信息

//updateChunkWriteStatsAndSplitIfNeeded CatalogCache::getShardedCollectionRoutingInfoWithRefresh调用
//getCollectionRoutingInfoWithRefresh   CatalogCache::onStaleConfigError
//updateChunkWriteStatsAndSplitIfNeeded
//检查缓存中是否有该collection对应得库，如果没有，则说明没有该collection路由信息，需要从新从cfg获取
//强制刷新路由
void CatalogCache::invalidateShardedCollection(const NamespaceString& nss) {
    stdx::lock_guard<stdx::mutex> lg(_mutex);

    auto it = _databases.find(nss.db());
    if (it == _databases.end()) {
        return;
    }

    it->second->collections[nss.ns()].needsRefresh = true;
}

void CatalogCache::invalidateShardedCollection(StringData ns) {
    invalidateShardedCollection(NamespaceString(ns));
}

/**
 * Non-blocking method, which removes the entire specified database (including its collections)
 * from the cache.
 */
void CatalogCache::purgeDatabase(StringData dbName) {
    stdx::lock_guard<stdx::mutex> lg(_mutex);
    _databases.erase(dbName);
}


/**
 * Non-blocking method, which removes all databases (including their collections) from the
 * cache.
 */
void CatalogCache::purgeAllDatabases() {
    stdx::lock_guard<stdx::mutex> lg(_mutex);
    _databases.clear();
}

//CatalogCache::getCollectionRoutingInfo  CatalogCache::getDatabase调用
//首先从cachez中获取，如果cache没有则从cfg复制集的config.database和config.collections中获取dbName库及其下面的表信息
std::shared_ptr<CatalogCache::DatabaseInfoEntry> CatalogCache::_getDatabase(OperationContext* opCtx,
                                                                            StringData dbName) {
    stdx::lock_guard<stdx::mutex> lg(_mutex);

	//如果该db的信息缓存中已经存在，则直接返回
    auto it = _databases.find(dbName);
    if (it != _databases.end()) {
        return it->second;
    }

	//如果没有缓存，则从cfg中获取


	//Grid::catalogClient获取ShardingCatalogClient   
    const auto catalogClient = Grid::get(opCtx)->catalogClient();

    const auto dbNameCopy = dbName.toString();

    // Load the database entry
    //sharding_catalog_client_impl::getDatabase从cfg复制集config.database表中获取dbName库对应的DatabaseType数据信息
    const auto opTimeWithDb = uassertStatusOK(catalogClient->getDatabase(
        opCtx, dbNameCopy, repl::ReadConcernLevel::kMajorityReadConcern));
	//DatabaseType类型
    const auto& dbDesc = opTimeWithDb.value;

    // Load the sharded collections entries
    std::vector<CollectionType> collections;
    repl::OpTime collLoadConfigOptime;
    uassertStatusOK(
		//sharding_catalog_client_impl::getCollections 
		//获取config.collections表_id中存储的表信息
        catalogClient->getCollections(opCtx, &dbNameCopy, &collections, &collLoadConfigOptime));

    StringMap<CollectionRoutingInfoEntry> collectionEntries;
    for (const auto& coll : collections) {
		//表已经删除了
        if (coll.getDropped()) {
            continue;
        }

		//需要刷新集合路由信息，重新获取DB信息得适合需要刷新该DB下得所有collection路由信息
        collectionEntries[coll.getNs().ns()].needsRefresh = true;
    }

	//db库及其对应的collections(config.collections)表信息全部存放在该_databases中
    return _databases[dbName] = std::shared_ptr<DatabaseInfoEntry>(new DatabaseInfoEntry{
               dbDesc.getPrimary(), dbDesc.getSharded(), std::move(collectionEntries)});
}

//获取dbEntry库下对应的nss集合的chunks路由信息 
//CatalogCache::getCollectionRoutingInfo中调用
void CatalogCache::_scheduleCollectionRefresh(WithLock lk,
                                              std::shared_ptr<DatabaseInfoEntry> dbEntry,
                                              std::shared_ptr<ChunkManager> existingRoutingInfo,
                                              NamespaceString const& nss,
                                              int refreshAttempt) {
    Timer t;

	//本地缓存的ChunkVersion信息
    const ChunkVersion startingCollectionVersion =
        (existingRoutingInfo ? existingRoutingInfo->getVersion() : ChunkVersion::UNSHARDED());

	//刷新失败，则走该{}
    const auto refreshFailed =
        [ this, t, dbEntry, nss, refreshAttempt ](WithLock lk, const Status& status) noexcept 
    {
        log() << "Refresh for collection " << nss << " took " << t.millis() << " ms and failed"
              << causedBy(redact(status));

        auto& collections = dbEntry->collections;
        auto it = collections.find(nss.ns());
        invariant(it != collections.end());
        auto& collEntry = it->second;

        // It is possible that the metadata is being changed concurrently, so retry the
        // refresh again
        //cfg中元数据发生了变化，递归调用最多kMaxInconsistentRoutingInfoRefreshAttempts次
        if (status == ErrorCodes::ConflictingOperationInProgress &&
            refreshAttempt < kMaxInconsistentRoutingInfoRefreshAttempts) {
            //CatalogCache::_scheduleCollectionRefresh 递归调用
            _scheduleCollectionRefresh(lk, dbEntry, nullptr, nss, refreshAttempt + 1);
        } else {
            // Leave needsRefresh to true so that any subsequent get attempts will kick off
            // another round of refresh
            collEntry.refreshCompletionNotification->set(status);
            collEntry.refreshCompletionNotification = nullptr;
        }
    };

	//下面的_cacheLoader.getChunksSince接口对应的回调处理
	//真正在下面的getChunksSince异步执行
    const auto refreshCallback = [ this, t, dbEntry, nss, existingRoutingInfo, refreshFailed ](
        OperationContext * opCtx,
        //swCollAndChunks参考赋值ConfigServerCatalogCacheLoader::getChunksSince
        StatusWith<CatalogCacheLoader::CollectionAndChangedChunks> swCollAndChunks) noexcept {
        std::shared_ptr<ChunkManager> newRoutingInfo;
        try {
			//从cfg获取最新的路由信息
            newRoutingInfo = refreshCollectionRoutingInfo(
                opCtx, nss, std::move(existingRoutingInfo), std::move(swCollAndChunks));
        } catch (const DBException& ex) {
            stdx::lock_guard<stdx::mutex> lg(_mutex);
            refreshFailed(lg, ex.toStatus());
            return;
        }

		//注意这里有加锁
        stdx::lock_guard<stdx::mutex> lg(_mutex);
        auto& collections = dbEntry->collections;
        auto it = collections.find(nss.ns());
        invariant(it != collections.end());
        auto& collEntry = it->second;

		//刷新到最新路由信息了，needsRefresh置为false，例如多个请求过来，则第一个请求刷新路由后，后面的请求就无需刷路由了
        collEntry.needsRefresh = false;
        collEntry.refreshCompletionNotification->set(Status::OK());
        collEntry.refreshCompletionNotification = nullptr;

		//路由信息只会再分片集群有效
        if (!newRoutingInfo) {
            log() << "Refresh for collection " << nss << " took " << t.millis()
                  << " and found the collection is not sharded";

            collections.erase(it);
        } else {
        /*
        新版本对应日志2021-05-31T12:00:39.106+0800 I SH_REFR  [ConfigServerCatalogCacheLoader-27703] Refresh for collection sporthealth.stepsDetail from version 33477|350351||5f9aa6ec3af7fbacfbc99a27 to version 33477|350426||5f9aa6ec3af7fbacfbc99a27 took 364 ms
		新版本这里对应代码如下：
		            const int logLevel = (!existingRoutingInfo || (existingRoutingInfo &&
                                                           routingInfoAfterRefresh->getVersion() !=
                                                               existingRoutingInfo->getVersion()))
                ? 0
                : 1;
            LOG_CATALOG_REFRESH(logLevel)
                << "Refresh for collection " << nss.toString()
                << (existingRoutingInfo
                        ? (" from version " + existingRoutingInfo->getVersion().toString())
                        : "")
                << " to version " << routingInfoAfterRefresh->getVersion().toString() << " took "
                << t.millis() << " ms";
		*/
            log() << "Refresh for collection " << nss << " took " << t.millis()
                  << " ms and found version " << newRoutingInfo->getVersion();

			//刷新该集合得路由信息，routingInfo指向新的路由newRoutingInfo
            collEntry.routingInfo = std::move(newRoutingInfo);
        }
    };

    log() << "Refreshing chunks for collection " << nss << " based on version "
          << startingCollectionVersion;

    try {
		//ConfigServerCatalogCacheLoader::getChunksSince  
		//异步获取集合chunk信息，也就是把refreshCallback丢到线程池中执行
        _cacheLoader.getChunksSince(nss, startingCollectionVersion, refreshCallback);
    } catch (const DBException& ex) {
        const auto status = ex.toStatus();

        // ConflictingOperationInProgress errors trigger retry of the catalog cache reload logic. If
        // we failed to schedule the asynchronous reload, there is no point in doing another
        // attempt.
        invariant(status != ErrorCodes::ConflictingOperationInProgress);

        stdx::lock_guard<stdx::mutex> lg(_mutex);
        refreshFailed(lg, status);
    }
}

//CachedDatabaseInfo相关成员赋值
CachedDatabaseInfo::CachedDatabaseInfo(std::shared_ptr<CatalogCache::DatabaseInfoEntry> db)
    : _db(std::move(db)) {}

const ShardId& CachedDatabaseInfo::primaryId() const {
    return _db->primaryShardId;
}

bool CachedDatabaseInfo::shardingEnabled() const {
    return _db->shardingEnabled;
}

//CachedCollectionRoutingInfo初始化
CachedCollectionRoutingInfo::CachedCollectionRoutingInfo(ShardId primaryId,
                                                         std::shared_ptr<ChunkManager> cm)
    : _primaryId(std::move(primaryId)), _cm(std::move(cm)) {}

//CatalogCache::getCollectionRoutingInfo中构造使用 //CachedCollectionRoutingInfo初始化
CachedCollectionRoutingInfo::CachedCollectionRoutingInfo(ShardId primaryId,
                                                         NamespaceString nss,
                                                         std::shared_ptr<Shard> primary)
    : _primaryId(std::move(primaryId)), _nss(std::move(nss)), _primary(std::move(primary)) {}

}  // namespace mongo
