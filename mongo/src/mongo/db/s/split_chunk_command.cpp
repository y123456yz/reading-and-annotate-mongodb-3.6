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

#include <string>
#include <vector>

#include "mongo/bson/util/bson_extract.h"
#include "mongo/db/auth/action_type.h"
#include "mongo/db/auth/authorization_session.h"
#include "mongo/db/auth/privilege.h"
#include "mongo/db/commands.h"
#include "mongo/db/s/operation_sharding_state.h"
#include "mongo/db/s/sharding_state.h"
#include "mongo/db/s/split_chunk.h"
#include "mongo/s/stale_exception.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {

using std::string;
using std::unique_ptr;
using std::vector;

namespace {
/* https://cloud.tencent.com/developer/article/1004435
chunk分裂的执行过程
1) 向对应的mongod 发起splitVector 命令，获得一个chunk的可分裂点
2) mongos 拿到这些分裂点后，向mongod发起splitChunk 命令

splitVector执行过程：

1) 计算出collection的文档的 avgRecSize= coll.size/ coll.count
2) 计算出分裂后的chunk中，每个chunk应该有的count数， split_count = maxChunkSize / (2 * avgRecSize)
3) 线性遍历collection 的shardkey 对应的index的 [chunk_min_index, chunk_max_index] 范围，在遍历过程中利用split_count 分割出若干spli

splitChunk执行过程：

1) 获得待执行collection的分布式锁（向configSvr 的mongod中写入一条记录实现）
2) 刷新（向configSvr读取）本shard的版本号，检查是否和命令发起者携带的版本号一致
3) 向configSvr中写入分裂后的chunk信息，成功后修改本地的chunk信息与shard的版本号
4) 向configSvr中写入变更日志
5) 通知mongos操作完成，mongos修改自身元数据
*/
//可以参考https://mongoing.com/archives/75945  MongoDB 路由表刷新导致响应慢场景解读
//https://mongoing.com/archives/77370  万亿级MongoDB集群的路由优化之路


//An internal administrative command. To split chunks, use the sh.splitFind() and sh.splitAt() functions in the mongo shell.
//splitChunk为mongodb内部命令，不对外

//mongos通过updateChunkWriteStatsAndSplitIfNeeded->splitChunkAtMultiplePoints发送splitChunk命令给shard server
//sheard server收到splitChunk后通过SplitChunkCommand::run处理
class SplitChunkCommand : public ErrmsgCommandDeprecated {
public:
    SplitChunkCommand() : ErrmsgCommandDeprecated("splitChunk") {}

    void help(std::stringstream& help) const override {
        help << "internal command usage only\n"
                "example:\n"
                " { splitChunk:\"db.foo\" , keyPattern: {a:1} , min : {a:100} , max: {a:200} { "
                "splitKeys : [ {a:150} , ... ]}";
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
                ResourcePattern::forClusterResource(), ActionType::internal)) {
            return Status(ErrorCodes::Unauthorized, "Unauthorized");
        }
        return Status::OK();
    }

    std::string parseNs(const std::string& dbname, const BSONObj& cmdObj) const override {
        return parseNsFullyQualified(dbname, cmdObj);
    }

	
    bool errmsgRun(OperationContext* opCtx,
                   const std::string& dbname,
                   const BSONObj& cmdObj,
                   std::string& errmsg,
                   BSONObjBuilder& result) override {
		//检查是否为primary
        uassertStatusOK(ShardingState::get(opCtx)->canAcceptShardedCommands());

        //
        // Check whether parameters passed to splitChunk are sound
        //
        const NamespaceString nss = NamespaceString(parseNs(dbname, cmdObj));
        if (!nss.isValid()) {
            errmsg = str::stream() << "invalid namespace '" << nss.toString()
                                   << "' specified for command";
            return false;
        }

		//检查keyPattern
        BSONObj keyPatternObj;
        {
            BSONElement keyPatternElem;
            auto keyPatternStatus =
                bsonExtractTypedField(cmdObj, "keyPattern", Object, &keyPatternElem);

            if (!keyPatternStatus.isOK()) {
                errmsg = "need to specify the key pattern the collection is sharded over";
                return false;
            }
            keyPatternObj = keyPatternElem.Obj();
        }

		//
        auto chunkRange = uassertStatusOK(ChunkRange::fromBSON(cmdObj));

        string shardName;
        auto parseShardNameStatus = bsonExtractStringField(cmdObj, "from", &shardName);
        if (!parseShardNameStatus.isOK())
            return appendCommandStatus(result, parseShardNameStatus);

		//2021-06-01T19:21:34.771+0800 I SHARDING [conn1327532] received splitChunk request: 
		//{ splitChunk: "xx.xxx", from: "xx-health_xyKKIMeg_shard_1", keyPattern: { ssoid: "hashed" }, 
		//epoch: ObjectId('5f9aa6ec3af7fbacfbc99a27'), shardVersion: [ Timestamp(33477, 360678), 
		//ObjectId('5f9aa6ec3af7fbacfbc99a27') ], min: { ssoid: -5962517471745203263 }, 
		//max: { ssoid: -5962474513134817833 }, splitKeys: [ { ssoid: -5962516934538529707 }, 
		//{ ssoid: -5962510586313188800 } ], lsid: { id: UUID("eac603aa-5928-445a-83f2-2c08ebb74d61"), 
		//uid: BinData(0, 64A61BF5764A1A00129F0CBAC3D8D4C51E4EAA3B877BF0F06A946E40E9EA172E) }, 
		//$clusterTime: { clusterTime: Timestamp(1622546494, 8538), signature: { hash: BinData(0, B54A8016731B06E8CE42080404F1E2F47BE16682), keyId: 6920984255816273035 } }, 
		//$client: { driver: { name: "mongo-java-driver", version: "3.8.2" }, os: { type: "Linux", name: "Linux", 
		//architecture: "amd64", version: "3.10.0-957.27.2.el7.x86_64" }, platform: "Java/heytap/1.8.0_252-b09", 
		//mongos: { host: "10-85-65-0.mongodb-fatpod-sport-health.bjht:20000", client: "10.85.84.90:49982", version: "3.6.10" } }, 
		//$configServerState: { opTime: { ts: Timestamp(1622546494, 4091), t: 5 } }, $db: "admin" }
        log() << "received splitChunk request: " << redact(cmdObj);

		//splitKeys也就是分裂点
        vector<BSONObj> splitKeys;
		//解析出分裂点信息
        {
            BSONElement splitKeysElem;
            auto splitKeysElemStatus =
                bsonExtractTypedField(cmdObj, "splitKeys", mongo::Array, &splitKeysElem);

            if (!splitKeysElemStatus.isOK()) {
                errmsg = "need to provide the split points to chunk over";
                return false;
            }
            BSONObjIterator it(splitKeysElem.Obj());
            while (it.more()) {
                splitKeys.push_back(it.next().Obj().getOwned());
            }
        }

        OID expectedCollectionEpoch;
		//同一个表的epoch不能变，需要检查，真正检查在cfg server中的ShardingCatalogManager::commitChunkSplit
        if (cmdObj.hasField("epoch")) {
            auto epochStatus = bsonExtractOIDField(cmdObj, "epoch", &expectedCollectionEpoch);
            uassert(
                ErrorCodes::InvalidOptions, "unable to parse collection epoch", epochStatus.isOK());
        } else {
            // Backwards compatibility with v3.4 mongos, which will send 'shardVersion' and not
            // 'epoch'.
            const auto& oss = OperationShardingState::get(opCtx);
            uassert(
                ErrorCodes::InvalidOptions, "collection version is missing", oss.hasShardVersion());
            expectedCollectionEpoch = oss.getShardVersion(nss).epoch();
        }

        auto statusWithOptionalChunkRange = splitChunk(
            opCtx, nss, keyPatternObj, chunkRange, splitKeys, shardName, expectedCollectionEpoch);

        // If the split chunk returns something that is not Status::Ok(), then something failed.
        uassertStatusOK(statusWithOptionalChunkRange.getStatus());

        // Otherwise, we want to check whether or not top-chunk optimization should be performed.
        // If yes, then we should have a ChunkRange that was returned. Regardless of whether it
        // should be performed, we will return true.
        if (auto topChunk = statusWithOptionalChunkRange.getValue()) {
            result.append("shouldMigrate",
                          BSON("min" << topChunk->getMin() << "max" << topChunk->getMax()));
        }
        return true;
    }

} cmdSplitChunk;

}  // namespace
}  // namespace mongo
