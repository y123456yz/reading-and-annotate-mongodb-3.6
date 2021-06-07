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

#include "mongo/platform/basic.h"

#include "mongo/s/config_server_catalog_cache_loader.h"

#include "mongo/db/client.h"
#include "mongo/db/operation_context.h"
#include "mongo/s/catalog/sharding_catalog_client.h"
#include "mongo/s/grid.h"
#include "mongo/stdx/memory.h"

namespace mongo {

using CollectionAndChangedChunks = CatalogCacheLoader::CollectionAndChangedChunks;

namespace {

/**
 * Constructs the default options for the thread pool used by the cache loader.
 */
//路由刷新过程由该现成实现，参考https://developer.aliyun.com/article/778536?spm=a2c6h.17698244.wenzhang.9.7b934d126DdIOU
//获取路由信息，mongos是异步调用实现的，因此用专门的线程池
ThreadPool::Options makeDefaultThreadPoolOptions() {
    ThreadPool::Options options;
    options.poolName = "ConfigServerCatalogCacheLoader";
    options.minThreads = 0;
    options.maxThreads = 6;

    // Ensure all threads have a client
    options.onCreateThread = [](const std::string& threadName) {
        Client::initThread(threadName.c_str());
    };

    return options;
}

/**
 * Structure repsenting the generated query and sort order for a chunk diffing operation.
 */
struct QueryAndSort {
    const BSONObj query;
    const BSONObj sort;
};

/**
 * Returns the query needed to find incremental changes to a collection from the config server.
 *
 * The query has to find all the chunks $gte the current max version. Currently, any splits and
 * merges will increment the current max version.
 *
 * The sort needs to be by ascending version in order to pick up the chunks which changed most
 * recent and also in order to handle cursor yields between chunks being migrated/split/merged. This
 * ensures that changes to chunk version (which will always be higher) will always come *after* our
 * current position in the chunk cursor.
 从config节点拉取权威的路由信息，并进行CatalogCache路由信息刷新。
 实际最终是通过 ConfigServerCatalogCacheLoader 线程来进行的，构造一个

{
    "ns": namespace,
  "lastmod": { $gte: sinceVersion}
}
 */
//只拉取版本号超过本地缓存lastmod版本的所有chunk信息
QueryAndSort createConfigDiffQuery(const NamespaceString& nss, ChunkVersion collectionVersion) {
    return {BSON(ChunkType::ns() << nss.ns() << ChunkType::lastmod() << GTE
                                 << Timestamp(collectionVersion.toLong())),
            BSON(ChunkType::lastmod() << 1)};
}

/**
 * Blocking method, which returns the chunks which changed since the specified version.
 */
//ConfigServerCatalogCacheLoader::getChunksSince中调用
//拉取cfg中config.chunks表对应版本大于本地缓存lastmod的所有增量变化的chunk
CollectionAndChangedChunks getChangedChunks(OperationContext* opCtx,
                                            const NamespaceString& nss,
                                            ChunkVersion sinceVersion) {
    const auto catalogClient = Grid::get(opCtx)->catalogClient();

    // Decide whether to do a full or partial load based on the state of the collection
    const auto coll = uassertStatusOK(catalogClient->getCollection(opCtx, nss.ns())).value;
    uassert(ErrorCodes::NamespaceNotFound,
            str::stream() << "Collection " << nss.ns() << " is dropped.",
            !coll.getDropped());

    // If the collection's epoch has changed, do a full refresh
    //如果表的epoll发生表和，说明 collection 被 drop 或者 collection的shardKey发生refined等
    const ChunkVersion startingCollectionVersion = (sinceVersion.epoch() == coll.getEpoch())
        ? sinceVersion
        : ChunkVersion(0, 0, coll.getEpoch());

    // Diff tracker should *always* find at least one chunk if collection exists
    ////只拉取版本号超过本地缓存lastmod版本的所有chunk信息
    //CollectionAndChangedChunks getChangedChunks调用，只拉取增量chunk,也就是变化了的chunk信息
    const auto diffQuery = createConfigDiffQuery(nss, startingCollectionVersion);

    // Query the chunks which have changed
    std::vector<ChunkType> changedChunks;
    repl::OpTime opTime;
    uassertStatusOK(
        Grid::get(opCtx)->catalogClient()->getChunks(opCtx,
                                                     diffQuery.query,
                                                     diffQuery.sort,
                                                     boost::none,
                                                     &changedChunks,
                                                     &opTime,
                                                     repl::ReadConcernLevel::kMajorityReadConcern));

    uassert(ErrorCodes::ConflictingOperationInProgress,
            "No chunks were found for the collection",
            !changedChunks.empty());
	//
    return CollectionAndChangedChunks(coll.getUUID(),
                                      coll.getEpoch(),
                                      coll.getKeyPattern().toBSON(),
                                      coll.getDefaultCollation(),
                                      coll.getUnique(),
                                      std::move(changedChunks));
}

}  // namespace

ConfigServerCatalogCacheLoader::ConfigServerCatalogCacheLoader()
    : _threadPool(makeDefaultThreadPoolOptions()) {
    _threadPool.startup();
}

ConfigServerCatalogCacheLoader::~ConfigServerCatalogCacheLoader() {
    _threadPool.shutdown();
    _threadPool.join();
}

void ConfigServerCatalogCacheLoader::initializeReplicaSetRole(bool isPrimary) {
    MONGO_UNREACHABLE;
}

void ConfigServerCatalogCacheLoader::onStepDown() {
    MONGO_UNREACHABLE;
}

void ConfigServerCatalogCacheLoader::onStepUp() {
    MONGO_UNREACHABLE;
}

void ConfigServerCatalogCacheLoader::notifyOfCollectionVersionUpdate(const NamespaceString& nss) {
    MONGO_UNREACHABLE;
}

void ConfigServerCatalogCacheLoader::waitForCollectionFlush(OperationContext* opCtx,
                                                            const NamespaceString& nss) {
    MONGO_UNREACHABLE;
}

//CatalogCache::_scheduleCollectionRefresh中调用，利用线程池异步处理function
std::shared_ptr<Notification<void>> 
	ConfigServerCatalogCacheLoader::getChunksSince(
    const NamespaceString& nss,
    ChunkVersion version,
    stdx::function<void(OperationContext*, StatusWith<CollectionAndChangedChunks>)> callbackFn) {

    auto notify = std::make_shared<Notification<void>>();

    uassertStatusOK(_threadPool.schedule([ this, nss, version, notify, callbackFn ]() noexcept {
        auto opCtx = Client::getCurrent()->makeOperationContext();

        auto swCollAndChunks = [&]() -> StatusWith<CollectionAndChangedChunks> {
            try {
                return getChangedChunks(opCtx.get(), nss, version);
            } catch (const DBException& ex) {
                return ex.toStatus();
            }
        }();

        callbackFn(opCtx.get(), std::move(swCollAndChunks));
        notify->set();
    }));

    return notify;
}

}  // namespace mongo
