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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kCommand

#include "mongo/platform/basic.h"

#include "mongo/db/audit.h"
#include "mongo/db/auth/action_set.h"
#include "mongo/db/auth/action_type.h"
#include "mongo/db/auth/authorization_manager.h"
#include "mongo/db/auth/authorization_session.h"
#include "mongo/db/client.h"
#include "mongo/db/commands.h"
#include "mongo/db/write_concern_options.h"
#include "mongo/s/balancer_configuration.h"
#include "mongo/s/catalog_cache.h"
#include "mongo/s/client/shard_registry.h"
#include "mongo/s/commands/cluster_commands_helpers.h"
#include "mongo/s/config_server_client.h"
#include "mongo/s/grid.h"
#include "mongo/s/migration_secondary_throttle_options.h"
#include "mongo/util/log.h"
#include "mongo/util/timer.h"

namespace mongo {

using std::shared_ptr;
using std::string;

namespace {
/*
mongos执行:db.runCommand({movePrimary:"test", to:"XX_gQmJGvRW_shard_2"})

{
原分片打印
2020-09-10T20:41:10.672+0800 I COMMAND	[conn378169] dropDatabase test - starting
2020-09-10T20:41:10.672+0800 I COMMAND	[conn378169] dropDatabase test - dropping 2 collections
2020-09-10T20:41:10.672+0800 I COMMAND	[conn378169] dropDatabase test - dropping collection: test.item_commit_info
2020-09-10T20:41:10.672+0800 I STORAGE	[conn378169] dropCollection: test.item_commit_info (cf56fa2d-6d8b-4320-8a4b-0119aa13125a) - renaming to drop-pending collection: test.system.drop.1599741670i2990t13.item_commit_info with drop optime { ts: Timestamp(1599741670, 2990), t: 13 }
2020-09-10T20:41:10.674+0800 I COMMAND	[conn378169] dropDatabase test - dropping collection: test.test1
2020-09-10T20:41:10.674+0800 I STORAGE	[conn378169] dropCollection: test.test1 (44fb57d7-b804-424f-9695-c4d8aac078f9) - renaming to drop-pending collection: test.system.drop.1599741670i3000t13.test1 with drop optime { ts: Timestamp(1599741670, 3000), t: 13 }
2020-09-10T20:41:10.681+0800 I REPL 	[replication-8766] Completing collection drop for test.system.drop.1599741670i2990t13.item_commit_info with drop optime { ts: Timestamp(1599741670, 2990), t: 13 } (notification optime: { ts: Timestamp(1599741670, 2990), t: 13 })
2020-09-10T20:41:10.681+0800 I STORAGE	[replication-8766] Finishing collection drop for test.system.drop.1599741670i2990t13.item_commit_info (cf56fa2d-6d8b-4320-8a4b-0119aa13125a).
2020-09-10T20:41:10.682+0800 I REPL 	[replication-8768] Completing collection drop for test.system.drop.1599741670i2990t13.item_commit_info with drop optime { ts: Timestamp(1599741670, 2990), t: 13 } (notification optime: { ts: Timestamp(1599741670, 3000), t: 13 })
2020-09-10T20:41:10.682+0800 I COMMAND	[conn378169] dropDatabase test - successfully dropped 2 collections (most recent drop optime: { ts: Timestamp(1599741670, 3000), t: 13 }) after 7ms. dropping database
2020-09-10T20:41:10.710+0800 I REPL 	[replication-8768] Completing collection drop for test.system.drop.1599741670i3000t13.test1 with drop optime { ts: Timestamp(1599741670, 3000), t: 13 } (notification optime: { ts: Timestamp(1599741670, 3000), t: 13 })
2020-09-10T20:41:10.728+0800 I REPL 	[replication-8766] Completing collection drop for test.system.drop.1599741670i3000t13.test1 with drop optime { ts: Timestamp(1599741670, 3000), t: 13 } (notification optime: { ts: Timestamp(1599741670, 3011), t: 13 })
2020-09-10T20:41:10.728+0800 I REPL 	[replication-8769] Completing collection drop for test.system.drop.1599741670i3000t13.test1 with drop optime { ts: Timestamp(1599741670, 3000), t: 13 } (notification optime: { ts: Timestamp(1599741670, 3012), t: 13 })
2020-09-10T20:41:10.730+0800 I REPL 	[replication-8770] Completing collection drop for test.system.drop.1599741670i3000t13.test1 with drop optime { ts: Timestamp(1599741670, 3000), t: 13 } (notification optime: { ts: Timestamp(1599741670, 3048), t: 13 })
2020-09-10T20:41:10.731+0800 I COMMAND	[conn378169] dropDatabase test - finished

目的分片主节点打印：
2020-09-10T20:41:10.559+0800 I STORAGE	[conn2055546] createCollection: test.item_commit_info with generated UUID: 5d22309b-ef17-4d01-adf2-b9f494ee143a
2020-09-10T20:41:10.579+0800 I STORAGE	[conn2055546] createCollection: test.test1 with generated UUID: 649b14a7-7a75-4ef1-a528-91f89adc046e
2020-09-10T20:41:10.636+0800 I STORAGE	[conn2055546] copying indexes for: { name: "item_commit_info", type: "collection", options: {}, info: { readOnly: false, uuid: UUID("cf56fa2d-6d8b-4320-8a4b-0119aa13125a") }, idIndex: { v: 2, key: { _id: 1 }, name: "_id_", ns: "test.item_commit_info" } }
2020-09-10T20:41:10.645+0800 I INDEX	[conn2055546] build index on: test.item_commit_info properties: { v: 2, unique: true, key: { tag: 1.0 }, name: "tag_1", ns: "test.item_commit_info" }
2020-09-10T20:41:10.645+0800 I INDEX	[conn2055546]	 building index using bulk method; build may temporarily use up to 250 megabytes of RAM
2020-09-10T20:41:10.652+0800 I INDEX	[conn2055546] build index on: test.item_commit_info properties: { v: 2, unique: true, key: { tag2: 1.0 }, name: "testindex", ns: "test.item_commit_info" }
2020-09-10T20:41:10.652+0800 I INDEX	[conn2055546]	 building index using bulk method; build may temporarily use up to 250 megabytes of RAM
2020-09-10T20:41:10.655+0800 I INDEX	[conn2055546] build index done.  scanned 1 total records. 0 secs
2020-09-10T20:41:10.655+0800 I STORAGE	[conn2055546] copying indexes for: { name: "test1", type: "collection", options: {}, info: { readOnly: false, uuid: UUID("44fb57d7-b804-424f-9695-c4d8aac078f9") }, idIndex: { v: 2, key: { _id: 1 }, name: "_id_", ns: "test.test1" } }
2020-09-10T20:41:10.668+0800 I COMMAND	[conn2055546] command test.$cmd appName: "MongoDB Shell" command: clone { clone: "opush_gQmJGvRW_shard_1/10.36.116.42:20001,10.37.72.102:20001,10.37.76.22:20001", collsToIgnore: [], bypassDocumentValidation: true, writeConcern: { w: "majority", wtimeout: 60000 }, $db: "test", $clusterTime: { clusterTime: Timestamp(1599741670, 2347), signature: { hash: BinData(0, C29F3B6CBB7BB2A931E07D1CFA6E71953A464885), keyId: 6829778851464216577 } }, $client: { application: { name: "MongoDB Shell" }, driver: { name: "MongoDB Internal Client", version: "3.6.14" }, os: { type: "Linux", name: "CentOS release 6.8 (Final)", architecture: "x86_64", version: "Kernel 2.6.32-642.el6.x86_64" }, mongos: { host: "bjht7266:20003", client: "10.35.150.17:44094", version: "3.6.10" } }, $configServerState: { opTime: { ts: Timestamp(1599741670, 2347), t: 7 } } } numYields:0 reslen:353 locks:{ Global: { acquireCount: { r: 17, w: 15, W: 2 }, acquireWaitCount: { W: 2 }, timeAcquiringMicros: { W: 6420 } }, Database: { acquireCount: { w: 10, W: 5 } }, oplog: { acquireCount: { w: 10 } } } protocol:op_msg 123ms

*/

class MoveChunkCmd : public ErrmsgCommandDeprecated {
public:
    MoveChunkCmd() : ErrmsgCommandDeprecated("moveChunk", "movechunk") {}

    bool slaveOk() const override {
        return true;
    }

    bool adminOnly() const override {
        return true;
    }

    bool supportsWriteConcern(const BSONObj& cmd) const override {
        return true;
    }

    void help(std::stringstream& help) const override {
        help << "Example: move chunk that contains the doc {num : 7} to shard001\n"
             << "  { movechunk : 'test.foo' , find : { num : 7 } , to : 'shard0001' }\n"
             << "Example: move chunk with lower bound 0 and upper bound 10 to shard001\n"
             << "  { movechunk : 'test.foo' , bounds : [ { num : 0 } , { num : 10 } ] "
             << " , to : 'shard001' }\n";
    }

    Status checkAuthForCommand(Client* client,
                               const std::string& dbname,
                               const BSONObj& cmdObj) override {
        if (!AuthorizationSession::get(client)->isAuthorizedForActionsOnResource(
                ResourcePattern::forExactNamespace(NamespaceString(parseNs(dbname, cmdObj))),
                ActionType::moveChunk)) {
            return Status(ErrorCodes::Unauthorized, "Unauthorized");
        }

        return Status::OK();
    }

    std::string parseNs(const std::string& dbname, const BSONObj& cmdObj) const override {
        return parseNsFullyQualified(dbname, cmdObj);
    }

	//MoveChunkCmd::errmsgRun
    bool errmsgRun(OperationContext* opCtx,
                   const std::string& dbname,
                   const BSONObj& cmdObj,
                   std::string& errmsg,
                   BSONObjBuilder& result) override {
        Timer t;

        const NamespaceString nss(parseNs(dbname, cmdObj));

        auto routingInfo = uassertStatusOK(
            Grid::get(opCtx)->catalogCache()->getShardedCollectionRoutingInfoWithRefresh(opCtx,
                                                                                         nss));
        const auto cm = routingInfo.cm();

        const auto toElt = cmdObj["to"];
        uassert(ErrorCodes::TypeMismatch,
                "'to' must be of type String",
                toElt.type() == BSONType::String);
        const std::string toString = toElt.str();
        if (!toString.size()) {
            errmsg = "you have to specify where you want to move the chunk";
            return false;
        }

        const auto toStatus = Grid::get(opCtx)->shardRegistry()->getShard(opCtx, toString);
        if (!toStatus.isOK()) {
            string msg(str::stream() << "Could not move chunk in '" << nss.ns() << "' to shard '"
                                     << toString
                                     << "' because that shard does not exist");
            log() << msg;
            return appendCommandStatus(result, Status(ErrorCodes::ShardNotFound, msg));
        }

        const auto to = toStatus.getValue();

        // so far, chunk size serves test purposes; it may or may not become a supported parameter
        long long maxChunkSizeBytes = cmdObj["maxChunkSizeBytes"].numberLong();
        if (maxChunkSizeBytes == 0) {
            maxChunkSizeBytes =
                Grid::get(opCtx)->getBalancerConfiguration()->getMaxChunkSizeBytes();
        }

        BSONObj find = cmdObj.getObjectField("find");
        BSONObj bounds = cmdObj.getObjectField("bounds");

        // check that only one of the two chunk specification methods is used
        if (find.isEmpty() == bounds.isEmpty()) {
            errmsg = "need to specify either a find query, or both lower and upper bounds.";
            return false;
        }

        shared_ptr<Chunk> chunk;

        if (!find.isEmpty()) {
            // find
            BSONObj shardKey =
                uassertStatusOK(cm->getShardKeyPattern().extractShardKeyFromQuery(opCtx, find));
            if (shardKey.isEmpty()) {
                errmsg = str::stream() << "no shard key found in chunk query " << find;
                return false;
            }

            chunk = cm->findIntersectingChunkWithSimpleCollation(shardKey);
        } else {
            // bounds
            if (!cm->getShardKeyPattern().isShardKey(bounds[0].Obj()) ||
                !cm->getShardKeyPattern().isShardKey(bounds[1].Obj())) {
                errmsg = str::stream() << "shard key bounds "
                                       << "[" << bounds[0].Obj() << "," << bounds[1].Obj() << ")"
                                       << " are not valid for shard key pattern "
                                       << cm->getShardKeyPattern().toBSON();
                return false;
            }

            BSONObj minKey = cm->getShardKeyPattern().normalizeShardKey(bounds[0].Obj());
            BSONObj maxKey = cm->getShardKeyPattern().normalizeShardKey(bounds[1].Obj());

            chunk = cm->findIntersectingChunkWithSimpleCollation(minKey);

            if (chunk->getMin().woCompare(minKey) != 0 || chunk->getMax().woCompare(maxKey) != 0) {
                errmsg = str::stream() << "no chunk found with the shard key bounds "
                                       << ChunkRange(minKey, maxKey).toString();
                return false;
            }
        }

        const auto secondaryThrottle =
            uassertStatusOK(MigrationSecondaryThrottleOptions::createFromCommand(cmdObj));

        ChunkType chunkType;
        chunkType.setNS(nss.ns());
        chunkType.setMin(chunk->getMin());
        chunkType.setMax(chunk->getMax());
        chunkType.setShard(chunk->getShardId());
        chunkType.setVersion(cm->getVersion());

        uassertStatusOK(configsvr_client::moveChunk(opCtx,
                                                    chunkType,
                                                    to->getId(),
                                                    maxChunkSizeBytes,
                                                    secondaryThrottle,
                                                    cmdObj["_waitForDelete"].trueValue() ||
                                                        cmdObj["waitForDelete"].trueValue()));

        Grid::get(opCtx)->catalogCache()->onStaleConfigError(std::move(routingInfo));

        result.append("millis", t.millis());
        return true;
    }

} moveChunk;

}  // namespace
}  // namespace mongo
