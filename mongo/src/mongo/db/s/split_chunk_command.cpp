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

        auto chunkRange = uassertStatusOK(ChunkRange::fromBSON(cmdObj));

        string shardName;
        auto parseShardNameStatus = bsonExtractStringField(cmdObj, "from", &shardName);
        if (!parseShardNameStatus.isOK())
            return appendCommandStatus(result, parseShardNameStatus);

        log() << "received splitChunk request: " << redact(cmdObj);

        vector<BSONObj> splitKeys;
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
