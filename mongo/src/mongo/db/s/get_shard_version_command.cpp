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
 *    must comply with the GNU Affero General Public License in all respects
 *    for all of the code used other than as permitted herein. If you modify
 *    file(s) with this exception, you may extend this exception to your
 *    version of the file(s), but you are not obligated to do so. If you do not
 *    wish to do so, delete this exception statement from your version. If you
 *    delete this exception statement from all source files in the program,
 *    then also delete it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kSharding

#include "mongo/platform/basic.h"

#include "mongo/db/auth/action_set.h"
#include "mongo/db/auth/action_type.h"
#include "mongo/db/auth/authorization_manager.h"
#include "mongo/db/auth/authorization_session.h"
#include "mongo/db/auth/privilege.h"
#include "mongo/db/commands.h"
#include "mongo/db/db_raii.h"
#include "mongo/db/s/collection_metadata.h"
#include "mongo/db/s/collection_sharding_state.h"
#include "mongo/db/s/sharded_connection_info.h"
#include "mongo/db/s/sharding_state.h"
#include "mongo/util/log.h"
#include "mongo/util/stringutils.h"

namespace mongo {
namespace {

/*
分片1信息如下:
mongodb_4.0_shard1:PRIMARY> db.runCommand({getShardVersion :"test.test"})
{
        "configServer" : "mongodb_4.0_config/192.168.10.1:10001,192.168.10.1:10002,192.168.10.1:10003",
        "inShardedMode" : false,
        "mine" : Timestamp(0, 0),
        "global" : Timestamp(13, 4),
        "ok" : 1,
        "operationTime" : Timestamp(1650100503, 1),
        "$gleStats" : {
                "lastOpTime" : Timestamp(0, 0),
                "electionId" : ObjectId("7fffffff0000000000000017")
        },
        "lastCommittedOpTime" : Timestamp(1650100503, 1),
        "$configServerState" : {
                "opTime" : {
                        "ts" : Timestamp(1650100505, 3),
                        "t" : NumberLong(13)
                }
        },
        "$clusterTime" : {
                "clusterTime" : Timestamp(1650100505, 3),
                "signature" : {
                        "hash" : BinData(0,"AAAAAAAAAAAAAAAAAAAAAAAAAAA="),
                        "keyId" : NumberLong(0)
                }
        }
}
mongodb_4.0_shard1:PRIMARY> 
switched to db config
mongodb_4.0_shard1:PRIMARY> db.cache.chunks.test.test.find({"shard" : "mongodb_4.0_shard1"}).sort({lastmod:-1}).limit(1)
{ "_id" : { "id" : 8154998 }, "max" : { "id" : 8168354 }, "shard" : "mongodb_4.0_shard1", "lastmod" : Timestamp(13, 4), "history" : [ { "validAfter" : Timestamp(1642842039, 9549), "shard" : "mongodb_4.0_shard1" } ] }
mongodb_4.0_shard1:PRIMARY> 
mongodb_4.0_shard1:PRIMARY> db.cache.chunks.test.test.find({"shard" : "mongodb_4.0_shard2"}).sort({lastmod:-1}).limit(1)
{ "_id" : { "id" : 24872948 }, "max" : { "id" : 32452630 }, "shard" : "mongodb_4.0_shard2", "lastmod" : Timestamp(13, 1), "history" : [ { "validAfter" : Timestamp(1642840304, 1), "shard" : "mongodb_4.0_shard2" } ] }
mongodb_4.0_shard1:PRIMARY> 



分片2信息如下:

mongodb_4.0_shard2:PRIMARY> use admin
switched to db admin
mongodb_4.0_shard2:PRIMARY> db.runCommand({getShardVersion :"test.test"})
{
        "configServer" : "mongodb_4.0_config/192.168.10.1:10001,192.168.10.1:10002,192.168.10.1:10003",
        "inShardedMode" : false,
        "mine" : Timestamp(0, 0),
        "global" : Timestamp(13, 1),
        "ok" : 1,
        "operationTime" : Timestamp(1650100053, 1),
        "$gleStats" : {
                "lastOpTime" : Timestamp(0, 0),
                "electionId" : ObjectId("7fffffff0000000000000018")
        },
        "lastCommittedOpTime" : Timestamp(1650100053, 1),
        "$configServerState" : {
                "opTime" : {
                        "ts" : Timestamp(1650100055, 3),
                        "t" : NumberLong(13)
                }
        },
        "$clusterTime" : {
                "clusterTime" : Timestamp(1650100055, 3),
                "signature" : {
                        "hash" : BinData(0,"GTITjn7FTQgOb6lVJh6Sz6D4TBE="),
                        "keyId" : NumberLong("7051130195905871878")
                }
        }
}
mongodb_4.0_shard2:PRIMARY> use config
switched to db config
mongodb_4.0_shard2:PRIMARY> 
mongodb_4.0_shard2:PRIMARY> 
mongodb_4.0_shard2:PRIMARY> 
mongodb_4.0_shard2:PRIMARY> db.cache.chunks.test.test.find({"shard" : "mongodb_4.0_shard2"}).sort({lastmod:-1}).limit(1)
{ "_id" : { "id" : 24872948 }, "max" : { "id" : 32452630 }, "shard" : "mongodb_4.0_shard2", "lastmod" : Timestamp(13, 1), "history" : [ { "validAfter" : Timestamp(1642840304, 1), "shard" : "mongodb_4.0_shard2" } ] }
mongodb_4.0_shard2:PRIMARY> 
mongodb_4.0_shard2:PRIMARY> 
mongodb_4.0_shard2:PRIMARY> db.cache.chunks.test.test.find({"shard" : "mongodb_4.0_shard1"}).sort({lastmod:-1}).limit(1)
{ "_id" : { "id" : 8154998 }, "max" : { "id" : 8168354 }, "shard" : "mongodb_4.0_shard1", "lastmod" : Timestamp(13, 4), "history" : [ { "validAfter" : Timestamp(1642842039, 9549), "shard" : "mongodb_4.0_shard1" } ] }
mongodb_4.0_shard2:PRIMARY> 
mongodb_4.0_shard2:PRIMARY> 

从上面可以看出cache.chunks.db.collection中记录的是该表所有的chunk,不仅仅包括本分片，还包括其他分片的，shard version为本分片最大的chunk版本

//collection version 为 sharded collection 在所有shard上最高的 chunk version
//shard version为本分片该collection最大的版本信息，也就是本分片config.cache.chunks.db.collection中最大的版本号；
//collection version为该表在所有分片的版本信息的最大值，也就是config server中该表版本信息的最大值
*/ 
//db.runCommand({getShardVersion :"test.test"})
//db.runCommand({getShardVersion: "test.test", fullMetadata: true});
class GetShardVersion : public BasicCommand {
public:
    GetShardVersion() : BasicCommand("getShardVersion") {}

    void help(std::stringstream& help) const override {
        help << " example: { getShardVersion : 'alleyinsider.foo'  } ";
    }

    virtual bool supportsWriteConcern(const BSONObj& cmd) const override {
        return false;
    }

    bool slaveOk() const override {
        return false;
    }

    bool adminOnly() const override {
        return true;
    }

    Status checkAuthForCommand(Client* client,
                               const std::string& dbname,
                               const BSONObj& cmdObj) override {
        if (!AuthorizationSession::get(client)->isAuthorizedForActionsOnResource(
                ResourcePattern::forExactNamespace(NamespaceString(parseNs(dbname, cmdObj))),
                ActionType::getShardVersion)) {
            return Status(ErrorCodes::Unauthorized, "Unauthorized");
        }
        return Status::OK();
    }

    std::string parseNs(const std::string& dbname, const BSONObj& cmdObj) const override {
        return parseNsFullyQualified(dbname, cmdObj);
    }

    bool run(OperationContext* opCtx,
             const std::string& dbname,
             const BSONObj& cmdObj,
             BSONObjBuilder& result) override {
        const NamespaceString nss(parseNs(dbname, cmdObj));

        ShardingState* const shardingState = ShardingState::get(opCtx);
        if (shardingState->enabled()) {//启用了分片功能
            result.append("configServer", shardingState->getConfigServer(opCtx).toString());
        } else {
            result.append("configServer", "");
        }

		//记录是不是从mongos发过来的
        ShardedConnectionInfo* const sci = ShardedConnectionInfo::get(opCtx->getClient(), false);
        result.appendBool("inShardedMode", sci != nullptr); 
        if (sci) { //client是mongos过来的连接
            result.appendTimestamp("mine", sci->getVersion(nss.ns()).toLong());
        } else {
            result.appendTimestamp("mine", 0);
        }

        AutoGetCollection autoColl(opCtx, nss, MODE_IS);
		
        CollectionShardingState* const css = CollectionShardingState::get(opCtx, nss);


		//CollectionShardingState::getMetadata          mongos通过getShardedCollection获取chunk及版本信息
        const auto metadata = css->getMetadata();
        if (metadata) {
			//ScopedCollectionMetadata中的->()，最终走到CollectionMetadata::getShardVersion
            result.appendTimestamp("global", metadata->getShardVersion().toLong());
        } else {
            result.appendTimestamp("global", ChunkVersion(0, 0, OID()).toLong());
        }

        if (cmdObj["fullMetadata"].trueValue()) {
            BSONObjBuilder metadataBuilder(result.subobjStart("metadata"));
            if (metadata) {
                metadata->toBSONBasic(metadataBuilder);

                BSONArrayBuilder chunksArr(metadataBuilder.subarrayStart("chunks"));
                metadata->toBSONChunks(chunksArr);
                chunksArr.doneFast();

                BSONArrayBuilder pendingArr(metadataBuilder.subarrayStart("pending"));
                css->toBSONPending(pendingArr);
                pendingArr.doneFast();
            }
            metadataBuilder.doneFast();
        }

        return true;
    }

} getShardVersionCmd;

}  // namespace
}  // namespace mongo
