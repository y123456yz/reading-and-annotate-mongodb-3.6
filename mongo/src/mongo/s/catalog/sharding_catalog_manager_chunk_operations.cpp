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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kSharding

#include "mongo/platform/basic.h"

#include "mongo/s/catalog/sharding_catalog_manager.h"

#include "mongo/base/status_with.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/bson/util/bson_extract.h"
#include "mongo/client/connection_string.h"
#include "mongo/client/read_preference.h"
#include "mongo/db/db_raii.h"
#include "mongo/db/dbdirectclient.h"
#include "mongo/db/namespace_string.h"
#include "mongo/db/operation_context.h"
#include "mongo/rpc/get_status_from_command_result.h"
#include "mongo/s/catalog/sharding_catalog_client.h"
#include "mongo/s/catalog/type_chunk.h"
#include "mongo/s/client/shard.h"
#include "mongo/s/client/shard_registry.h"
#include "mongo/s/grid.h"
#include "mongo/s/shard_key_pattern.h"
#include "mongo/util/fail_point_service.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {
namespace {

MONGO_FP_DECLARE(migrationCommitVersionError);

/**
 * Append min, max and version information from chunk to the buffer for logChange purposes.
 */
void appendShortVersion(BufBuilder* b, const ChunkType& chunk) {
    BSONObjBuilder bb(*b);
    bb.append(ChunkType::min(), chunk.getMin());
    bb.append(ChunkType::max(), chunk.getMax());
    if (chunk.isVersionSet())
        chunk.getVersion().addToBSON(bb, ChunkType::lastmod());
    bb.done();
}

BSONArray buildMergeChunksApplyOpsUpdates(const std::vector<ChunkType>& chunksToMerge,
                                          const ChunkVersion& mergeVersion) {
    BSONArrayBuilder updates;

    // Build an update operation to expand the first chunk into the newly merged chunk
    {
        BSONObjBuilder op;
        op.append("op", "u");
        op.appendBool("b", false);  // no upsert
        op.append("ns", ChunkType::ConfigNS);

        // expand first chunk into newly merged chunk
        ChunkType mergedChunk(chunksToMerge.front());
        mergedChunk.setMax(chunksToMerge.back().getMax());

        // fill in additional details for sending through applyOps
        mergedChunk.setVersion(mergeVersion);

        // add the new chunk information as the update object
        op.append("o", mergedChunk.toConfigBSON());

        // query object
        op.append("o2", BSON(ChunkType::name(mergedChunk.getName())));

        updates.append(op.obj());
    }

    // Build update operations to delete the rest of the chunks to be merged. Remember not
    // to delete the first chunk we're expanding
    for (size_t i = 1; i < chunksToMerge.size(); ++i) {
        BSONObjBuilder op;
        op.append("op", "d");
        op.append("ns", ChunkType::ConfigNS);

        op.append("o", BSON(ChunkType::name(chunksToMerge[i].getName())));

        updates.append(op.obj());
    }

    return updates.arr();
}

BSONArray buildMergeChunksApplyOpsPrecond(const std::vector<ChunkType>& chunksToMerge,
                                          const ChunkVersion& collVersion) {
    BSONArrayBuilder preCond;

    for (auto chunk : chunksToMerge) {
        BSONObjBuilder b;
        b.append("ns", ChunkType::ConfigNS);
        b.append(
            "q",
            BSON("query" << BSON(ChunkType::ns(chunk.getNS()) << ChunkType::min(chunk.getMin())
                                                              << ChunkType::max(chunk.getMax()))
                         << "orderby"
                         << BSON(ChunkType::lastmod() << -1)));
        b.append("res",
                 BSON(ChunkType::epoch(collVersion.epoch())
                      << ChunkType::shard(chunk.getShard().toString())));
        preCond.append(b.obj());
    }
    return preCond.arr();
}

Status checkChunkIsOnShard(OperationContext* opCtx,
                           const NamespaceString& nss,
                           const BSONObj& min,
                           const BSONObj& max,
                           const ShardId& shard) {
    BSONObj chunkQuery =
        BSON(ChunkType::ns() << nss.ns() << ChunkType::min() << min << ChunkType::max() << max
                             << ChunkType::shard()
                             << shard);

    // Must use local read concern because we're going to perform subsequent writes.
    auto findResponseWith =
        Grid::get(opCtx)->shardRegistry()->getConfigShard()->exhaustiveFindOnConfig(
            opCtx,
            ReadPreferenceSetting{ReadPreference::PrimaryOnly},
            repl::ReadConcernLevel::kLocalReadConcern,
            NamespaceString(ChunkType::ConfigNS),
            chunkQuery,
            BSONObj(),
            1);
    if (!findResponseWith.isOK()) {
        return findResponseWith.getStatus();
    }

    if (findResponseWith.getValue().docs.empty()) {
        return {ErrorCodes::Error(40165),
                str::stream()
                    << "Could not find the chunk ("
                    << chunkQuery.toString()
                    << ") on the shard. Cannot execute the migration commit with invalid chunks."};
    }

    return Status::OK();
}

BSONObj makeCommitChunkApplyOpsCommand(const NamespaceString& nss,
                                       const ChunkType& migratedChunk,
                                       const boost::optional<ChunkType>& controlChunk,
                                       StringData fromShard,
                                       StringData toShard) {

    // Update migratedChunk's version and shard.
    BSONArrayBuilder updates;
    {
        BSONObjBuilder op;
        op.append("op", "u");
        op.appendBool("b", false);  // No upserting
        op.append("ns", ChunkType::ConfigNS);

        BSONObjBuilder n(op.subobjStart("o"));
        n.append(ChunkType::name(), ChunkType::genID(nss.ns(), migratedChunk.getMin()));
        migratedChunk.getVersion().addToBSON(n, ChunkType::lastmod());
        n.append(ChunkType::ns(), nss.ns());
        n.append(ChunkType::min(), migratedChunk.getMin());
        n.append(ChunkType::max(), migratedChunk.getMax());
        n.append(ChunkType::shard(), toShard);
        n.done();

        BSONObjBuilder q(op.subobjStart("o2"));
        q.append(ChunkType::name(), ChunkType::genID(nss.ns(), migratedChunk.getMin()));
        q.done();

        updates.append(op.obj());
    }

    // If we have a controlChunk, update its chunk version.
    if (controlChunk) {
        BSONObjBuilder op;
        op.append("op", "u");
        op.appendBool("b", false);
        op.append("ns", ChunkType::ConfigNS);

        BSONObjBuilder n(op.subobjStart("o"));
        n.append(ChunkType::name(), ChunkType::genID(nss.ns(), controlChunk->getMin()));
        controlChunk->getVersion().addToBSON(n, ChunkType::lastmod());
        n.append(ChunkType::ns(), nss.ns());
        n.append(ChunkType::min(), controlChunk->getMin());
        n.append(ChunkType::max(), controlChunk->getMax());
        n.append(ChunkType::shard(), fromShard);
        n.done();

        BSONObjBuilder q(op.subobjStart("o2"));
        q.append(ChunkType::name(), ChunkType::genID(nss.ns(), controlChunk->getMin()));
        q.done();

        updates.append(op.obj());
    }

    // Do not give applyOps a write concern. If applyOps tries to wait for replication, it will fail
    // because of the GlobalWrite lock CommitChunkMigration already holds. Replication will not be
    // able to take the lock it requires.
    return BSON("applyOps" << updates.arr());
}

}  // namespace

//ConfigSvrSplitChunkCommand::run中调用
Status ShardingCatalogManager::commitChunkSplit(OperationContext* opCtx,
                                                const NamespaceString& nss,
                                                const OID& requestEpoch,
                                                const ChunkRange& range,
                                                const std::vector<BSONObj>& splitPoints,
                                                const std::string& shardName) {
    // Take _kChunkOpLock in exclusive mode to prevent concurrent chunk splits, merges, and
    // migrations
    // TODO(SERVER-25359): Replace with a collection-specific lock map to allow splits/merges/
    // move chunks on different collections to proceed in parallel
    Lock::ExclusiveLock lk(opCtx->lockState(), _kChunkOpLock);

    std::string errmsg;

    // Get the max chunk version for this namespace.
    //switched to db config
	//mongos> db.chunks.find({"ns" : "cloud_track.xx"}).sort({"lastmod":-1}).limit(1)
	//{ "_id" : "cloud_track.dailyCloudOperateInfo_01-userId_-8799279245254197987", "lastmod" : Timestamp(2269, 1), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8"), "ns" : "cloud_track.dailyCloudOperateInfo_01", "min" : { "userId" : NumberLong("-8799279245254197987") }, "max" : { "userId" : NumberLong("-8795846687425704091") }, "shard" : "ocloud_oFEAkecX_shard_MrFIjpKf" }
	//mongos> 

	//ShardLocal::_exhaustiveFindOnConfig
    auto findStatus = Grid::get(opCtx)->shardRegistry()->getConfigShard()->exhaustiveFindOnConfig(
        opCtx,
        ReadPreferenceSetting{ReadPreference::PrimaryOnly},
        repl::ReadConcernLevel::kLocalReadConcern,
        NamespaceString(ChunkType::ConfigNS),
        BSON("ns" << nss.ns()),
        BSON(ChunkType::lastmod << -1),
        1); //获取config.chunk中lastmod字段最大的一条数据

    if (!findStatus.isOK()) {
        return findStatus.getStatus();
    }

	//db.chunks.find({"ns" : "cloud_track.xx"}).sort({"lastmod":-1}).limit(1)
	//获取命令返回的数据
    const auto& chunksVector = findStatus.getValue().docs;
    if (chunksVector.empty()) {
        errmsg = str::stream() << "splitChunk cannot split chunk " << range.toString()
                               << ". Collection '" << nss.ns()
                               << "' no longer either exists, is sharded, or has chunks";
        return {ErrorCodes::IllegalOperation, errmsg};
    }

	//按照lastmod，"lastmod" : Timestamp(2269, 1)解析出整调数据存入ChunkVersion相应字段
    ChunkVersion collVersion = ChunkVersion::fromBSON(chunksVector.front(), ChunkType::lastmod());

    // Return an error if collection epoch does not match epoch of request.
    //shard server发送过来的epoll和cfg中记录的不一致，直接报错
    if (collVersion.epoch() != requestEpoch) {
        errmsg = str::stream() << "splitChunk cannot split chunk " << range.toString()
                               << ". Collection '" << nss.ns() << "' was dropped and re-created."
                               << " Current epoch: " << collVersion.epoch()
                               << ", cmd epoch: " << requestEpoch;
        return {ErrorCodes::StaleEpoch, errmsg};
    }

    std::vector<ChunkType> newChunks;

    ChunkVersion currentMaxVersion = collVersion;

    auto startKey = range.getMin();
    auto newChunkBounds(splitPoints);
    newChunkBounds.push_back(range.getMax());

    
	

	//把一个chunk从中间拆分为多个chunk，则需要在更新config.chunks表，一条表为多条，每条的版本minor自增
	//该拆分chunk后的ChunkVersion取该表版本最大的一条，然后minor下版本按照拆分的条数递增
	//注意只是这个指定的拆分chunk的lastmod才会变，其他的chunk的lastmode不会变，如下：
	/*
	mongos> db.chunks.find({"ns" : "cloud_track.dailyCloudOperateInfo_01", "shard" : "ocloud_oFEAkecX_shard_3"}, {lastmod:1, lastmodEpoch:1}).sort({"lastmod":-1}).limit(44)
	{ "_id" : "cloud_track.xx-userId_-9162413882254975667", "lastmod" : Timestamp(2267, 1), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-1471557197776300291", "lastmod" : Timestamp(2259, 25), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-1473163376515744185", "lastmod" : Timestamp(2259, 24), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-1474756339512334207", "lastmod" : Timestamp(2259, 23), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-1476389563485460429", "lastmod" : Timestamp(2259, 22), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-1478013688926177289", "lastmod" : Timestamp(2259, 21), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-1479635589959175514", "lastmod" : Timestamp(2259, 20), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-1481180083839929633", "lastmod" : Timestamp(2259, 19), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-1482768361464343623", "lastmod" : Timestamp(2259, 18), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-1484418967151675848", "lastmod" : Timestamp(2259, 17), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-1486049861433680287", "lastmod" : Timestamp(2259, 16), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-1487635543913662499", "lastmod" : Timestamp(2259, 15), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-8623635608235383019", "lastmod" : Timestamp(2240, 1), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-8486473701912544949", "lastmod" : Timestamp(2232, 6), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-8488087652520488732", "lastmod" : Timestamp(2232, 5), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-8489664254966506173", "lastmod" : Timestamp(2232, 4), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-8491251519298231089", "lastmod" : Timestamp(2232, 3), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-8492910962204222674", "lastmod" : Timestamp(2232, 2), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-7571123874431886469", "lastmod" : Timestamp(2226, 4), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	{ "_id" : "cloud_track.xx-userId_-7572758537799358327", "lastmod" : Timestamp(2226, 3), "lastmodEpoch" : ObjectId("60942092465698e8e2a03ed8") }
	Type "it" for more
	mongos> 
	*/

	//拆分后的chunk信息组装到updates中
	BSONArrayBuilder updates;
    for (const auto& endKey : newChunkBounds) {
        // Verify the split points are all within the chunk
        if (endKey.woCompare(range.getMax()) != 0 && !range.containsKey(endKey)) {
            return {ErrorCodes::InvalidOptions,
                    str::stream() << "Split key " << endKey << " not contained within chunk "
                                  << range.toString()};
        }

        // Verify the split points came in increasing order
        if (endKey.woCompare(startKey) < 0) {
            return {
                ErrorCodes::InvalidOptions,
                str::stream() << "Split keys must be specified in strictly increasing order. Key "
                              << endKey
                              << " was specified after "
                              << startKey
                              << "."};
        }

        // Verify that splitPoints are not repeated
        if (endKey.woCompare(startKey) == 0) {
            return {ErrorCodes::InvalidOptions,
                    str::stream() << "Split on lower bound of chunk "
                                  << ChunkRange(startKey, endKey).toString()
                                  << "is not allowed"};
        }

        // verify that splits don't create too-big shard keys
        Status shardKeyStatus = ShardKeyPattern::checkShardKeySize(endKey);
        if (!shardKeyStatus.isOK()) {
            return shardKeyStatus;
        }

        // splits only update the 'minor' portion of version
        //split chunk增加minor
        currentMaxVersion.incMinor();

        // build an update operation against the chunks collection of the config database
        // with upsert true  upsert true，没用则写入
        BSONObjBuilder op;
        op.append("op", "u");
        op.appendBool("b", true);
        op.append("ns", ChunkType::ConfigNS);

        // add the modified (new) chunk information as the update object
        BSONObjBuilder n(op.subobjStart("o"));
        n.append(ChunkType::name(), ChunkType::genID(nss.ns(), startKey));
        currentMaxVersion.addToBSON(n, ChunkType::lastmod());
        n.append(ChunkType::ns(), nss.ns());
        n.append(ChunkType::min(), startKey);
        n.append(ChunkType::max(), endKey);
        n.append(ChunkType::shard(), shardName);
        n.done();

        // add the chunk's _id as the query part of the update statement
        BSONObjBuilder q(op.subobjStart("o2"));
        q.append(ChunkType::name(), ChunkType::genID(nss.ns(), startKey));
        q.done();

        updates.append(op.obj());

        // remember this chunk info for logging later
        ChunkType chunk;
        chunk.setMin(startKey);
        chunk.setMax(endKey);
        chunk.setVersion(currentMaxVersion);

        newChunks.push_back(std::move(chunk));

        startKey = endKey;
    }

    BSONArrayBuilder preCond;
    {
        BSONObjBuilder b;
        b.append("ns", ChunkType::ConfigNS);
        b.append("q",
                 BSON("query" << BSON(ChunkType::ns(nss.ns()) << ChunkType::min() << range.getMin()
                                                              << ChunkType::max()
                                                              << range.getMax())
                              << "orderby"
                              << BSON(ChunkType::lastmod() << -1)));
        {
            BSONObjBuilder bb(b.subobjStart("res"));
            bb.append(ChunkType::epoch(), requestEpoch);
            bb.append(ChunkType::shard(), shardName);
        }
        preCond.append(b.obj());
    }

    // apply the batch of updates to local metadata.
    //ShardingCatalogClientImpl::applyChunkOpsDeprecated
    //拆分后的chunk update到config.chunk表中
    Status applyOpsStatus = Grid::get(opCtx)->catalogClient()->applyChunkOpsDeprecated(
        opCtx,
        updates.arr(),
        preCond.arr(),
        nss.ns(),
        currentMaxVersion,
        WriteConcernOptions(),
        repl::ReadConcernLevel::kLocalReadConcern);
    if (!applyOpsStatus.isOK()) {
        return applyOpsStatus;
    }

	//splite相关日志记录
    // log changes
    BSONObjBuilder logDetail;
    {
        BSONObjBuilder b(logDetail.subobjStart("before"));
        b.append(ChunkType::min(), range.getMin());
        b.append(ChunkType::max(), range.getMax());
        collVersion.addToBSON(b, ChunkType::lastmod());
    }
	
    if (newChunks.size() == 2) {
	//如果是一个chunk拆分为2个chunk，则记录split log日志，如下：
	//mongos> db.changelog.find({what:"split"}).limit(1)
	//{ "_id" : "bjcp3642-2021-05-09T01:00:02.337+0800-6096c392465698e8e2f5cd0b", "server" : "bjcp3642", "clientAddr" : "10.130.148.84:57482", "time" : ISODate("2021-05-08T17:00:02.337Z"), "what" : "split", "ns" : "cloud_track.dailyCloudOperateInfo_03", "details" : { "before" : { "min" : { "userId" : { "$minKey" : 1 } }, "max" : { "userId" : NumberLong("-6148914691236517200") }, "lastmod" : Timestamp(2, 1), "lastmodEpoch" : ObjectId("6096c391465698e8e2f5cb65") }, "left" : { "min" : { "userId" : { "$minKey" : 1 } }, "max" : { "userId" : NumberLong("-7686143364045646500") }, "lastmod" : Timestamp(2, 2), "lastmodEpoch" : ObjectId("6096c391465698e8e2f5cb65") }, "right" : { "min" : { "userId" : NumberLong("-7686143364045646500") }, "max" : { "userId" : NumberLong("-6148914691236517200") }, "lastmod" : Timestamp(2, 3), "lastmodEpoch" : ObjectId("6096c391465698e8e2f5cb65") } } }

        appendShortVersion(&logDetail.subobjStart("left"), newChunks[0]);
        appendShortVersion(&logDetail.subobjStart("right"), newChunks[1]);

        Grid::get(opCtx)
            ->catalogClient()
            ->logChange(opCtx, "split", nss.ns(), logDetail.obj(), WriteConcernOptions())
            .transitional_ignore();
    } else {
    
	//如果是一个chunk拆分为多个(超过2个)chunk，则记录split log日志，如下：
	//mongos> db.changelog.find({what:"multi-split"}).limit(1)
	//{ "_id" : "bjcp3642-2021-05-08T23:18:47.593+0800-6096abd7465698e8e2f2bc91", "server" : "bjcp3642", "clientAddr" : "10.130.148.84:34880", "time" : ISODate("2021-05-08T15:18:47.593Z"), "what" : "multi-split", "ns" : "cloud_track.dailyCloudOperateInfo_08", "details" : { "before" : { "min" : { "userId" : NumberLong("-6100234755828683737") }, "max" : { "userId" : NumberLong("-6093498987149518800") }, "lastmod" : Timestamp(2237, 1), "lastmodEpoch" : ObjectId("6075ce13465698e8e2bddd8e") }, "number" : 1, "of" : 5, "chunk" : { "min" : { "userId" : NumberLong("-6100234755828683737") }, "max" : { "userId" : NumberLong("-6098626556525707060") }, "lastmod" : Timestamp(2237, 2), "lastmodEpoch" : ObjectId("6075ce13465698e8e2bddd8e") } } }  BSONObj beforeDetailObj = logDetail.obj();
		BSONObj beforeDetailObj = logDetail.obj();
		BSONObj firstDetailObj = beforeDetailObj.getOwned();
        const int newChunksSize = newChunks.size();

        for (int i = 0; i < newChunksSize; i++) {
            BSONObjBuilder chunkDetail;
            chunkDetail.appendElements(beforeDetailObj);
            chunkDetail.append("number", i + 1);
            chunkDetail.append("of", newChunksSize);
            appendShortVersion(&chunkDetail.subobjStart("chunk"), newChunks[i]);

            Grid::get(opCtx)
                ->catalogClient()
                ->logChange(
                    opCtx, "multi-split", nss.ns(), chunkDetail.obj(), WriteConcernOptions())
                .transitional_ignore();
        }
    }

    return applyOpsStatus;
}

Status ShardingCatalogManager::commitChunkMerge(OperationContext* opCtx,
                                                const NamespaceString& ns,
                                                const OID& requestEpoch,
                                                const std::vector<BSONObj>& chunkBoundaries,
                                                const std::string& shardName) {
    // This method must never be called with empty chunks to merge
    invariant(!chunkBoundaries.empty());

    // Take _kChunkOpLock in exclusive mode to prevent concurrent chunk splits, merges, and
    // migrations
    // TODO(SERVER-25359): Replace with a collection-specific lock map to allow splits/merges/
    // move chunks on different collections to proceed in parallel
    Lock::ExclusiveLock lk(opCtx->lockState(), _kChunkOpLock);

    // Get the chunk with the highest version for this namespace
    auto findStatus = Grid::get(opCtx)->shardRegistry()->getConfigShard()->exhaustiveFindOnConfig(
        opCtx,
        ReadPreferenceSetting{ReadPreference::PrimaryOnly},
        repl::ReadConcernLevel::kLocalReadConcern,
        NamespaceString(ChunkType::ConfigNS),
        BSON("ns" << ns.ns()),
        BSON(ChunkType::lastmod << -1),
        1);

    if (!findStatus.isOK()) {
        return findStatus.getStatus();
    }

    const auto& chunksVector = findStatus.getValue().docs;
    if (chunksVector.empty())
        return {ErrorCodes::IllegalOperation,
                "collection does not exist, isn't sharded, or has no chunks"};

    ChunkVersion collVersion = ChunkVersion::fromBSON(chunksVector.front(), ChunkType::lastmod());

    // Return an error if epoch of chunk does not match epoch of request
    if (collVersion.epoch() != requestEpoch) {
        return {ErrorCodes::StaleEpoch,
                "epoch of chunk does not match epoch of request. This most likely means "
                "that the collection was dropped and re-created."};
    }

    // Build chunks to be merged
    std::vector<ChunkType> chunksToMerge;

    ChunkType itChunk;
    itChunk.setMax(chunkBoundaries.front());
    itChunk.setNS(ns.ns());
    itChunk.setShard(shardName);

    // Do not use the first chunk boundary as a max bound while building chunks
    for (size_t i = 1; i < chunkBoundaries.size(); ++i) {
        itChunk.setMin(itChunk.getMax());

        // Ensure the chunk boundaries are strictly increasing
        if (chunkBoundaries[i].woCompare(itChunk.getMin()) <= 0) {
            return {
                ErrorCodes::InvalidOptions,
                str::stream()
                    << "Chunk boundaries must be specified in strictly increasing order. Boundary "
                    << chunkBoundaries[i]
                    << " was specified after "
                    << itChunk.getMin()
                    << "."};
        }

        itChunk.setMax(chunkBoundaries[i]);
        chunksToMerge.push_back(itChunk);
    }

    ChunkVersion mergeVersion = collVersion;
    mergeVersion.incMinor();

    auto updates = buildMergeChunksApplyOpsUpdates(chunksToMerge, mergeVersion);
    auto preCond = buildMergeChunksApplyOpsPrecond(chunksToMerge, collVersion);

    // apply the batch of updates to local metadata
    Status applyOpsStatus = Grid::get(opCtx)->catalogClient()->applyChunkOpsDeprecated(
        opCtx,
        updates,
        preCond,
        ns.ns(),
        mergeVersion,
        WriteConcernOptions(),
        repl::ReadConcernLevel::kLocalReadConcern);
    if (!applyOpsStatus.isOK()) {
        return applyOpsStatus;
    }

    // log changes
    BSONObjBuilder logDetail;
    {
        BSONArrayBuilder b(logDetail.subarrayStart("merged"));
        for (auto chunkToMerge : chunksToMerge) {
            b.append(chunkToMerge.toConfigBSON());
        }
    }
    collVersion.addToBSON(logDetail, "prevShardVersion");
    mergeVersion.addToBSON(logDetail, "mergedVersion");

    Grid::get(opCtx)
        ->catalogClient()
        ->logChange(opCtx, "merge", ns.ns(), logDetail.obj(), WriteConcernOptions())
        .transitional_ignore();

    return applyOpsStatus;
}

StatusWith<BSONObj> ShardingCatalogManager::commitChunkMigration(
    OperationContext* opCtx,
    const NamespaceString& nss,
    const ChunkType& migratedChunk,
    const boost::optional<ChunkType>& controlChunk,
    const OID& collectionEpoch,
    const ShardId& fromShard,
    const ShardId& toShard) {

    auto const configShard = Grid::get(opCtx)->shardRegistry()->getConfigShard();

    // Take _kChunkOpLock in exclusive mode to prevent concurrent chunk splits, merges, and
    // migrations.
    //
    // ConfigSvrCommitChunkMigration commands must be run serially because the new ChunkVersions
    // for migrated chunks are generated within the command and must be committed to the database
    // before another chunk commit generates new ChunkVersions in the same manner.
    //
    // TODO(SERVER-25359): Replace with a collection-specific lock map to allow splits/merges/
    // move chunks on different collections to proceed in parallel.
    // (Note: This is not needed while we have a global lock, taken here only for consistency.)
    Lock::ExclusiveLock lk(opCtx->lockState(), _kChunkOpLock);

    // Must use local read concern because we will perform subsequent writes.
    auto findResponse =
        configShard->exhaustiveFindOnConfig(opCtx,
                                            ReadPreferenceSetting{ReadPreference::PrimaryOnly},
                                            repl::ReadConcernLevel::kLocalReadConcern,
                                            NamespaceString(ChunkType::ConfigNS),
                                            BSON("ns" << nss.ns()),
                                            BSON(ChunkType::lastmod << -1),
                                            1);
    if (!findResponse.isOK()) {
        return findResponse.getStatus();
    }

    if (MONGO_FAIL_POINT(migrationCommitVersionError)) {
        uassert(ErrorCodes::StaleEpoch,
                "failpoint 'migrationCommitVersionError' generated error",
                false);
    }

    const auto chunksVector = std::move(findResponse.getValue().docs);
    if (chunksVector.empty()) {
        return {ErrorCodes::IncompatibleShardingMetadata,
                str::stream() << "Tried to find max chunk version for collection '" << nss.ns()
                              << ", but found no chunks"};
    }

    const auto swChunk = ChunkType::fromConfigBSON(chunksVector.front());
    if (!swChunk.isOK()) {
        return swChunk.getStatus();
    }

    const auto currentCollectionVersion = swChunk.getValue().getVersion();

    // It is possible for a migration to end up running partly without the protection of the
    // distributed lock if the config primary stepped down since the start of the migration and
    // failed to recover the migration. Check that the collection has not been dropped and recreated
    // since the migration began, unbeknown to the shard when the command was sent.
    if (currentCollectionVersion.epoch() != collectionEpoch) {
        return {ErrorCodes::StaleEpoch,
                str::stream() << "The collection '" << nss.ns()
                              << "' has been dropped and recreated since the migration began."
                                 " The config server's collection version epoch is now '"
                              << currentCollectionVersion.epoch().toString()
                              << "', but the shard's is "
                              << collectionEpoch.toString()
                              << "'. Aborting migration commit for chunk ("
                              << migratedChunk.getRange().toString()
                              << ")."};
    }

    // Check that migratedChunk and controlChunk are where they should be, on fromShard.

    auto migratedOnShard =
        checkChunkIsOnShard(opCtx, nss, migratedChunk.getMin(), migratedChunk.getMax(), fromShard);
    if (!migratedOnShard.isOK()) {
        return migratedOnShard;
    }

    if (controlChunk) {
        auto controlOnShard = checkChunkIsOnShard(
            opCtx, nss, controlChunk->getMin(), controlChunk->getMax(), fromShard);
        if (!controlOnShard.isOK()) {
            return controlOnShard;
        }
    }

    // Generate the new versions of migratedChunk and controlChunk. Migrating chunk's minor version
    // will be 0.
    ChunkType newMigratedChunk = migratedChunk;
    newMigratedChunk.setVersion(ChunkVersion(
        currentCollectionVersion.majorVersion() + 1, 0, currentCollectionVersion.epoch()));

    // Control chunk's minor version will be 1 (if control chunk is present).
    boost::optional<ChunkType> newControlChunk = boost::none;
    if (controlChunk) {
        newControlChunk = controlChunk.get();
        newControlChunk->setVersion(ChunkVersion(
            currentCollectionVersion.majorVersion() + 1, 1, currentCollectionVersion.epoch()));
    }

    auto command = makeCommitChunkApplyOpsCommand(
        nss, newMigratedChunk, newControlChunk, fromShard.toString(), toShard.toString());

    StatusWith<Shard::CommandResponse> applyOpsCommandResponse =
        configShard->runCommandWithFixedRetryAttempts(
            opCtx,
            ReadPreferenceSetting{ReadPreference::PrimaryOnly},
            nss.db().toString(),
            command,
            Shard::RetryPolicy::kIdempotent);

    if (!applyOpsCommandResponse.isOK()) {
        return applyOpsCommandResponse.getStatus();
    }

    if (!applyOpsCommandResponse.getValue().commandStatus.isOK()) {
        return applyOpsCommandResponse.getValue().commandStatus;
    }

    BSONObjBuilder result;
    newMigratedChunk.getVersion().appendWithFieldForCommands(&result, "migratedChunkVersion");
    if (controlChunk) {
        newControlChunk->getVersion().appendWithFieldForCommands(&result, "controlChunkVersion");
    }

    return result.obj();
}

}  // namespace mongo
