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

#include "mongo/db/s/shard_metadata_util.h"

#include "mongo/client/dbclientinterface.h"
#include "mongo/db/dbdirectclient.h"
#include "mongo/db/ops/write_ops.h"
#include "mongo/db/write_concern_options.h"
#include "mongo/rpc/get_status_from_command_result.h"
#include "mongo/rpc/unique_message.h"
#include "mongo/s/catalog/type_chunk.h"
#include "mongo/s/catalog/type_shard_collection.h"
#include "mongo/s/chunk_version.h"
#include "mongo/s/write_ops/batched_command_response.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/log.h"

namespace mongo {
namespace shardmetadatautil {

namespace {

const WriteConcernOptions kLocalWriteConcern(1,
                                             WriteConcernOptions::SyncMode::UNSET,
                                             Milliseconds(0));

/**
 * Processes a command result for errors, including write concern errors.
 */
Status getStatusFromWriteCommandResponse(const BSONObj& commandResult) {
    BatchedCommandResponse batchResponse;
    std::string errmsg;
    if (!batchResponse.parseBSON(commandResult, &errmsg)) {
        return Status(ErrorCodes::FailedToParse,
                      str::stream() << "Failed to parse write response: " << errmsg);
    }

    return batchResponse.toStatus();
}

}  // namespace

//类似db.cache.chunks.db.coll.find({"lastmod" :{$gte:Timestamp(xx, xx)}}).sort({"lastmod" : 1})
QueryAndSort createShardChunkDiffQuery(const ChunkVersion& collectionVersion) {
    return {BSON(ChunkType::lastmod() << BSON("$gte" << Timestamp(collectionVersion.toLong()))),
            BSON(ChunkType::lastmod() << 1)};
}

bool RefreshState::operator==(const RefreshState& other) const {
    return (other.epoch == epoch) && (other.refreshing == refreshing) &&
        (other.lastRefreshedCollectionVersion == lastRefreshedCollectionVersion);
}

std::string RefreshState::toString() const {
    return str::stream() << "epoch: " << epoch
                         << ", refreshing: " << (refreshing ? "true" : "false")
                         << ", lastRefreshedCollectionVersion: "
                         << lastRefreshedCollectionVersion.toString();
}

//persistCollectionAndChangedChunks
// Mark the chunk metadata as refreshing, so that secondaries are aware of refresh.

//设置cache.collections表中的对应表的refreshing字段为true，标记当前真再刷新chunk路由信息
Status setPersistedRefreshFlags(OperationContext* opCtx, const NamespaceString& nss) {
    // Set 'refreshing' to true.
    BSONObj update = BSON(ShardCollectionType::refreshing() << true);
    return updateShardCollectionsEntry(
        opCtx, BSON(ShardCollectionType::ns() << nss.ns()), update, BSONObj(), false /*upsert*/);
}

//设置cache.collections表中的对应表的refreshing字段为false，标记当前刷新chunk路由信息结束
Status unsetPersistedRefreshFlags(OperationContext* opCtx,
                                  const NamespaceString& nss,
                                  const ChunkVersion& refreshedVersion) {
    // Set 'refreshing' to false and update the last refreshed collection version.
    BSONObjBuilder updateBuilder;
    updateBuilder.append(ShardCollectionType::refreshing(), false);
	//记录更新的chunks中最大chunk的版本信息
    updateBuilder.appendTimestamp(ShardCollectionType::lastRefreshedCollectionVersion(),
                                  refreshedVersion.toLong());

    return updateShardCollectionsEntry(opCtx,
                                       BSON(ShardCollectionType::ns() << nss.ns()),
                                       updateBuilder.obj(),
                                       BSONObj(),
                                       false /*upsert*/);
}

StatusWith<RefreshState> getPersistedRefreshFlags(OperationContext* opCtx,
                                                  const NamespaceString& nss) {
    auto statusWithCollectionEntry = readShardCollectionsEntry(opCtx, nss);
    if (!statusWithCollectionEntry.isOK()) {
        return statusWithCollectionEntry.getStatus();
    }
    ShardCollectionType entry = statusWithCollectionEntry.getValue();

    // Ensure the results have not been incorrectly set somehow.
    if (entry.hasRefreshing()) {
        // If 'refreshing' is present and false, a refresh must have occurred (otherwise the field
        // would never have been added to the document) and there should always be a refresh
        // version.
        invariant(entry.getRefreshing() ? true : entry.hasLastRefreshedCollectionVersion());
    } else {
        // If 'refreshing' is not present, no refresh version should exist.
        invariant(!entry.hasLastRefreshedCollectionVersion());
    }

    return RefreshState{entry.getEpoch(),
                        // If the refreshing field has not yet been added, this means that the first
                        // refresh has started, but no chunks have ever yet been applied, around
                        // which these flags are set. So default to refreshing true because the
                        // chunk metadata is being updated and is not yet ready to be read.
                        entry.hasRefreshing() ? entry.getRefreshing() : true,
                        entry.hasLastRefreshedCollectionVersion()
                            ? entry.getLastRefreshedCollectionVersion()
                            : ChunkVersion(0, 0, entry.getEpoch())};
}

//getPersistedMaxVersion  
//查找config.cache.collections表中的nss表内容，启用了分片功能的表这里面都会有记录
StatusWith<ShardCollectionType> readShardCollectionsEntry(OperationContext* opCtx,
                                                          const NamespaceString& nss) {

    Query fullQuery(BSON(ShardCollectionType::ns() << nss.ns()));

    try {
        DBDirectClient client(opCtx);
        std::unique_ptr<DBClientCursor> cursor =
			//config.cache.collections表
            client.query(ShardCollectionType::ConfigNS.c_str(), fullQuery, 1);
        if (!cursor) {
            return Status(ErrorCodes::OperationFailed,
                          str::stream() << "Failed to establish a cursor for reading "
                                        << ShardCollectionType::ConfigNS
                                        << " from local storage");
        }

        if (!cursor->more()) {
            // The collection has been dropped.
            return Status(ErrorCodes::NamespaceNotFound,
                          str::stream() << "collection " << nss.ns() << " not found");
        }

        BSONObj document = cursor->nextSafe();
        auto statusWithCollectionEntry = ShardCollectionType::fromBSON(document);
        if (!statusWithCollectionEntry.isOK()) {
            return statusWithCollectionEntry.getStatus();
        }

        return statusWithCollectionEntry.getValue();
    } catch (const DBException& ex) {
        return {ex.toStatus().code(),
                str::stream() << "Failed to read the '" << nss.ns()
                              << "' entry locally from config.collections"
                              << causedBy(ex.toStatus())};
    }
}

//更新"config.cache.collections"表中的内容，更新分片表信息
Status updateShardCollectionsEntry(OperationContext* opCtx,
                                   const BSONObj& query,
                                   const BSONObj& update,
                                   const BSONObj& inc,
                                   const bool upsert) {
    invariant(query.hasField("_id"));
    if (upsert) {
        // If upserting, this should be an update from the config server that does not have shard
        // refresh / migration inc signal information.
        invariant(!update.hasField(ShardCollectionType::refreshing()));
        invariant(!update.hasField(ShardCollectionType::lastRefreshedCollectionVersion()));
        invariant(inc.isEmpty());
    }

    try {
        DBDirectClient client(opCtx);

        BSONObjBuilder builder;
        if (!update.isEmpty()) {
            // Want to modify the document if it already exists, not replace it.
            builder.append("$set", update);
        }
        if (!inc.isEmpty()) {
            builder.append("$inc", inc);
        }

        auto commandResponse = client.runCommand([&] {
            write_ops::Update updateOp(NamespaceString{ShardCollectionType::ConfigNS});
            updateOp.setUpdates({[&] {
                write_ops::UpdateOpEntry entry;
                entry.setQ(query);
                entry.setU(builder.obj());
                entry.setUpsert(upsert);
                return entry;
            }()});
            return updateOp.serialize({});
        }());
        uassertStatusOK(getStatusFromWriteCommandResponse(commandResponse->getCommandReply()));

        return Status::OK();
    } catch (const DBException& ex) {
        return ex.toStatus();
    }
}

//getPersistedMaxVersion
//按照参数中指定条件读取"config.cache.chunks."中内容
StatusWith<std::vector<ChunkType>> readShardChunks(OperationContext* opCtx,
                                                   const NamespaceString& nss,
                                                   const BSONObj& query,
                                                   const BSONObj& sort,
                                                   boost::optional<long long> limit,
                                                   const OID& epoch) {
    try {
        Query fullQuery(query);
        fullQuery.sort(sort);

        DBDirectClient client(opCtx);

        const std::string chunkMetadataNs = ChunkType::ShardNSPrefix + nss.ns();

        std::unique_ptr<DBClientCursor> cursor =
            client.query(chunkMetadataNs, fullQuery, limit.get_value_or(0));
        uassert(ErrorCodes::OperationFailed,
                str::stream() << "Failed to establish a cursor for reading " << chunkMetadataNs
                              << " from local storage",
                cursor);

        std::vector<ChunkType> chunks;
        while (cursor->more()) {
            BSONObj document = cursor->nextSafe().getOwned();
            auto statusWithChunk = ChunkType::fromShardBSON(document, epoch);
            if (!statusWithChunk.isOK()) {
                return {statusWithChunk.getStatus().code(),
                        str::stream() << "Failed to parse chunk '" << document.toString()
                                      << "' due to "
                                      << statusWithChunk.getStatus().reason()};
            }

            chunks.push_back(std::move(statusWithChunk.getValue()));
        }

        return chunks;
    } catch (const DBException& ex) {
        return ex.toStatus();
    }
}

//persistCollectionAndChangedChunks
//更新config.cache.chunks.表 中的chunks信息到最新的chunks，先找出有表中和chunks有交集的chunk，然后删除插入新的chunks
Status updateShardChunks(OperationContext* opCtx,
                         const NamespaceString& nss,
                         const std::vector<ChunkType>& chunks,
                         const OID& currEpoch) {
    invariant(!chunks.empty());

    NamespaceString chunkMetadataNss(ChunkType::ShardNSPrefix + nss.ns());

    try {
        DBDirectClient client(opCtx);

        // This may be the first update, so the first opportunity to create an index.
        // If the index already exists, this is a no-op.
        client.createIndex(chunkMetadataNss.ns(), BSON(ChunkType::lastmod() << 1));

        /**
         * Here are examples of the operations that can happen on the config server to update
         * the config.chunks collection. 'chunks' only includes the chunks that result from the
         * operations, which can be read from the config server, not any that were removed, so
         * we must delete any chunks that overlap with the new 'chunks'.
         *
         * CollectionVersion = 10.3
         *
         * moveChunk
         * {_id: 3, max: 5, version: 10.1} --> {_id: 3, max: 5, version: 11.0}
         *
         * splitChunk
         * {_id: 3, max: 9, version 10.3} --> {_id: 3, max: 5, version 10.4}
         *                                    {_id: 5, max: 8, version 10.5}
         *                                    {_id: 8, max: 9, version 10.6}
         *
         * mergeChunk
         * {_id: 10, max: 14, version 4.3} --> {_id: 10, max: 22, version 10.4}
         * {_id: 14, max: 19, version 7.1}
         * {_id: 19, max: 22, version 2.0}
         *

icmgo-test36_0:SECONDARY> db.cache.chunks.testdb2.testcol.find()
{ "_id" : { "id" : 1 }, "max" : { "id" : 490013220 }, "shard" : "icmgo-test36_0", "lastmod" : Timestamp(3, 0) }
{ "_id" : { "id" : 490013220 }, "max" : { "id" : 990022103 }, "shard" : "icmgo-test36_0", "lastmod" : Timestamp(4, 0) }
{ "_id" : { "id" : NumberLong("4150084103") }, "max" : { "id" : NumberLong("4810100663") }, "shard" : "icmgo-test36_1", "lastmod" : Timestamp(4, 3) }
         */
        for (auto& chunk : chunks) {
			//epoch必须一致
            invariant(chunk.getVersion().hasEqualEpoch(currEpoch));

            // Delete any overlapping chunk ranges. Overlapping chunks will have a min value
            // ("_id") between (chunk.min, chunk.max].
            // 把有交集的chunk先删除，然后插入新的
            // query: { "_id" : {"$gte": chunk.min, "$lt": chunk.max}}
            auto deleteCommandResponse = client.runCommand([&] {
                write_ops::Delete deleteOp(chunkMetadataNss);
                deleteOp.setDeletes({[&] {
                    write_ops::DeleteOpEntry entry;
                    entry.setQ(BSON(ChunkType::minShardID
                                    << BSON("$gte" << chunk.getMin() << "$lt" << chunk.getMax())));
                    entry.setMulti(true);
                    return entry;
                }()});
                return deleteOp.serialize({});
            }());
            uassertStatusOK(
                getStatusFromWriteCommandResponse(deleteCommandResponse->getCommandReply()));

            // Now the document can be expected to cleanly insert without overlap
            auto insertCommandResponse = client.runCommand([&] {
                write_ops::Insert insertOp(chunkMetadataNss);
                insertOp.setDocuments({chunk.toShardBSON()});
                return insertOp.serialize({});
            }());
            uassertStatusOK(
                getStatusFromWriteCommandResponse(insertCommandResponse->getCommandReply()));
        }

        return Status::OK();
    } catch (const DBException& ex) {
        return ex.toStatus();
    }
}

Status dropChunksAndDeleteCollectionsEntry(OperationContext* opCtx, const NamespaceString& nss) {
    try {
        DBDirectClient client(opCtx);

        auto deleteCommandResponse = client.runCommand([&] {
            write_ops::Delete deleteOp(
                NamespaceString{NamespaceString::kShardConfigCollectionsCollectionName});
            deleteOp.setDeletes({[&] {
                write_ops::DeleteOpEntry entry;
                entry.setQ(BSON(ShardCollectionType::ns << nss.ns()));
                entry.setMulti(true);
                return entry;
            }()});
            return deleteOp.serialize({});
        }());
        uassertStatusOK(
            getStatusFromWriteCommandResponse(deleteCommandResponse->getCommandReply()));

        // Drop the corresponding config.chunks.ns collection
        BSONObj result;
        if (!client.dropCollection(
                ChunkType::ShardNSPrefix + nss.ns(), kLocalWriteConcern, &result)) {
            Status status = getStatusFromCommandResult(result);
            if (status != ErrorCodes::NamespaceNotFound) {
                uassertStatusOK(status);
            }
        }

        LOG(1) << "Successfully cleared persisted chunk metadata for collection '" << nss << "'.";
        return Status::OK();
    } catch (const DBException& ex) {
        return ex.toStatus();
    }
}

}  // namespace shardmetadatautil
}  // namespace mongo
