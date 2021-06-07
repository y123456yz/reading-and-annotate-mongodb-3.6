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

#include "mongo/db/auth/action_set.h"
#include "mongo/db/auth/action_type.h"
#include "mongo/db/auth/authorization_manager.h"
#include "mongo/db/auth/authorization_session.h"
#include "mongo/db/commands.h"
#include "mongo/s/catalog_cache.h"
#include "mongo/s/commands/cluster_commands_helpers.h"
#include "mongo/s/grid.h"
#include "mongo/util/log.h"

namespace mongo {
namespace {
//GetShardVersion::run
//mongos¶ÔÓ¦getshardversion´òÓ¡:
/*
mongos> db.runCommand({getShardVersion :"xx.xx"})
{
        "version" : Timestamp(8, 1),
        "versionEpoch" : ObjectId("60a54411465698e8e2cfc526"),
        "ok" : 1,
        "operationTime" : Timestamp(1623041936, 11641),
        "$clusterTime" : {
                "clusterTime" : Timestamp(1623041936, 11641),
                "signature" : {
                        "hash" : BinData(0,"/L1J9J+1uV1ltc0SYjJD0TFC9ik="),
                        "keyId" : NumberLong("6933088392779399171")
                }
        }
}
mongos> 
*/
class GetShardVersion : public BasicCommand {
public:
    GetShardVersion() : BasicCommand("getShardVersion", "getshardversion") {}

    bool slaveOk() const override {
        return true;
    }

    bool adminOnly() const override {
        return true;
    }

    bool supportsWriteConcern(const BSONObj& cmd) const override {
        return false;
    }

    void help(std::stringstream& help) const override {
        help << " example: { getShardVersion : 'alleyinsider.foo'  } ";
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

        auto routingInfo = getShardedCollection(opCtx, nss);
        const auto cm = routingInfo.cm();

        for (const auto& chunk : cm->chunks()) {
//2021-06-07T13:12:23.221+0800 I SHARDING [conn2978379] shard: ocloud_oFEAkecX_shard_ImGeDQnU, lastmod: 2297|0||6096c391465698e8e2f5cb65, [{ userId: MinKey }, { userId: -9221177729864509767 })
//2021-06-07T13:12:23.221+0800 I SHARDING [conn2978379] shard: ocloud_oFEAkecX_shard_ImGeDQnU, lastmod: 2298|0||6096c391465698e8e2f5cb65, [{ userId: -9221177729864509767 }, { userId: -9218993915059435151 })
//2021-06-07T13:12:23.221+0800 I SHARDING [conn2978379] shard: ocloud_oFEAkecX_shard_2, lastmod: 2299|0||6096c391465698e8e2f5cb65, [{ userId: -9218993915059435151 }, { userId: -9217500997357642837 })
//2021-06-07T13:12:23.221+0800 I SHARDING [conn2978379] shard: ocloud_oFEAkecX_shard_ImGeDQnU, lastmod: 2312|0||6096c391465698e8e2f5cb65, [{ userId: -9217500997357642837 }, { userId: -9215415726037377072 })
//2021-06-07T13:12:23.221+0800 I SHARDING [conn2978379] shard: ocloud_oFEAkecX_shard_ImGeDQnU, lastmod: 2302|0||6096c391465698e8e2f5cb65, [{ userId: -9215415726037377072 }, { userId: -9213289395663766669 })
//2021-06-07T13:12:23.221+0800 I SHARDING [conn2978379] shard: ocloud_oFEAkecX_shard_2, lastmod: 2291|0||6096c391465698e8e2f5cb65, [{ userId: -9213289395663766669 }, { userId: -9211743986421468420 })
            log() << redact(chunk->toString());
        }

        cm->getVersion().addToBSON(result, "version");

        return true;
    }

} getShardVersionCmd;

}  // namespace
}  // namespace mongo
