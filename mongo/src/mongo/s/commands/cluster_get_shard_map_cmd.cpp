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

#include "mongo/platform/basic.h"

#include "mongo/db/auth/action_set.h"
#include "mongo/db/auth/action_type.h"
#include "mongo/db/auth/privilege.h"
#include "mongo/db/commands.h"
#include "mongo/s/client/shard_registry.h"
#include "mongo/s/grid.h"

namespace mongo {
namespace {

/*
xxxxxx_oFEAkecX_xxx_1:PRIMARY> db.runCommand({"getShardMap":1})
{
        "map" : {
                "xx.128.22.211xx.0" : "xxxxxx_oFEAkecX_xxx_ImGeDQnU/xx.128.22.211xx.0,xx.xxxxxx.209.54xx.0,xx.13xxxxxx.159.194xx.0",
                "xx.xxxxxx.155.189xx.0" : "xxxxxx_oFEAkecX_xxx_UeRnftkj/xx.xxxxxx.155.189xx.0,xx.13xxxxxx.159.247xx.0,xx.13xxxxxx.159.248xx.0",
                "xx.xxxxxx.209.54xx.0" : "xxxxxx_oFEAkecX_xxx_ImGeDQnU/xx.128.22.211xx.0,xx.xxxxxx.209.54xx.0,xx.13xxxxxx.159.194xx.0",
                "xx.13xxxxxx.148.84xx.0" : "xxxxxx_oFEAkecX_xxx_1/xx.13xxxxxx.148.84xx.0,xx.xx.46.2:20015,xx.xx.xx.xx1:20015",
                "xx.13xxxxxx.148.85xx.0" : "xxxxxx_oFEAkecX_xxx_3/xx.13xxxxxx.148.85xx.0,xx.131.18.40xx.0,xx.xx.xxxxxxxx.84:20010",
                "xx.13xxxxxx.148.86xx.0" : "xxxxxx_oFEAkecX_xxx_2/xx.13xxxxxx.148.86xx.0,xx.xx.56.238:20013,xx.xx.56.44xx.7",
                "xx.13xxxxxx.159.188xx.0" : "xxxxxx_oFEAkecX_xxx_MrFIjpKf/xx.13xxxxxx.159.188xx.0,xx.13xxxxxx.159.189xx.0,xx.13xxxxxx.159.190xx.0",
                "xx.13xxxxxx.159.189xx.0" : "xxxxxx_oFEAkecX_xxx_MrFIjpKf/xx.13xxxxxx.159.188xx.0,xx.13xxxxxx.159.189xx.0,xx.13xxxxxx.159.190xx.0",
                "xx.13xxxxxx.159.190xx.0" : "xxxxxx_oFEAkecX_xxx_MrFIjpKf/xx.13xxxxxx.159.188xx.0,xx.13xxxxxx.159.189xx.0,xx.13xxxxxx.159.190xx.0",
                "xx.13xxxxxx.159.194xx.0" : "xxxxxx_oFEAkecX_xxx_ImGeDQnU/xx.128.22.211xx.0,xx.xxxxxx.209.54xx.0,xx.13xxxxxx.159.194xx.0",
                "xx.13xxxxxx.159.247xx.0" : "xxxxxx_oFEAkecX_xxx_UeRnftkj/xx.xxxxxx.155.189xx.0,xx.13xxxxxx.159.247xx.0,xx.13xxxxxx.159.248xx.0",
                "xx.13xxxxxx.159.248xx.0" : "xxxxxx_oFEAkecX_xxx_UeRnftkj/xx.xxxxxx.155.189xx.0,xx.13xxxxxx.159.247xx.0,xx.13xxxxxx.159.248xx.0",
                "xx.131.18.40xx.0" : "xxxxxx_oFEAkecX_xxx_3/xx.13xxxxxx.148.85xx.0,xx.131.18.40xx.0,xx.xx.xxxxxxxx.84:20010",
                "xx.xx.46.2:20015" : "xxxxxx_oFEAkecX_xxx_1/xx.13xxxxxx.148.84xx.0,xx.xx.46.2:20015,xx.xx.xx.xx1:20015",
                "xx.xx.56.238:20013" : "xxxxxx_oFEAkecX_xxx_2/xx.13xxxxxx.148.86xx.0,xx.xx.56.238:20013,xx.xx.56.44xx.7",
                "xx.xx.56.238:20014" : "xxxxxx_oFEAkecX_configdb/xx.xx.56.238:20014,xx.xx.78.234xx.xx,xx.xx.xx.xx1:20016",
                "xx.xx.56.44xx.7" : "xxxxxx_oFEAkecX_xxx_2/xx.13xxxxxx.148.86xx.0,xx.xx.56.238:20013,xx.xx.56.44xx.7",
                "xx.xx.78.234xx.xx" : "xxxxxx_oFEAkecX_configdb/xx.xx.56.238:20014,xx.xx.78.234xx.xx,xx.xx.xx.xx1:20016",
                "xx.xx.xx.xx1:20015" : "xxxxxx_oFEAkecX_xxx_1/xx.13xxxxxx.148.84xx.0,xx.xx.46.2:20015,xx.xx.xx.xx1:20015",
                "xx.xx.xx.xx1:20016" : "xxxxxx_oFEAkecX_configdb/xx.xx.56.238:20014,xx.xx.78.234xx.xx,xx.xx.xx.xx1:20016",
                "xx.xx.xxxxxxxx.84:20010" : "xxxxxx_oFEAkecX_xxx_3/xx.13xxxxxx.148.85xx.0,xx.131.18.40xx.0,xx.xx.xxxxxxxx.84:20010",
                "config" : "xxxxxx_oFEAkecX_configdb/xx.xx.56.238:20014,xx.xx.78.234xx.xx,xx.xx.xx.xx1:20016",
                "xxxxxx_oFEAkecX_configdb/xx.xx.56.238:20014,xx.xx.78.234xx.xx,xx.xx.xx.xx1:20016" : "xxxxxx_oFEAkecX_configdb/xx.xx.56.238:20014,xx.xx.78.234xx.xx,xx.xx.xx.xx1:20016",
                "xxxxxx_oFEAkecX_xxx_1" : "xxxxxx_oFEAkecX_xxx_1/xx.13xxxxxx.148.84xx.0,xx.xx.46.2:20015,xx.xx.xx.xx1:20015",
                "xxxxxx_oFEAkecX_xxx_1/xx.13xxxxxx.148.84xx.0,xx.xx.46.2:20015,xx.xx.xx.xx1:20015" : "xxxxxx_oFEAkecX_xxx_1/xx.13xxxxxx.148.84xx.0,xx.xx.46.2:20015,xx.xx.xx.xx1:20015",
                "xxxxxx_oFEAkecX_xxx_2" : "xxxxxx_oFEAkecX_xxx_2/xx.13xxxxxx.148.86xx.0,xx.xx.56.238:20013,xx.xx.56.44xx.7",
                "xxxxxx_oFEAkecX_xxx_2/xx.13xxxxxx.148.86xx.0,xx.xx.56.238:20013,xx.xx.56.44xx.7" : "xxxxxx_oFEAkecX_xxx_2/xx.13xxxxxx.148.86xx.0,xx.xx.56.238:20013,xx.xx.56.44xx.7",
                "xxxxxx_oFEAkecX_xxx_3" : "xxxxxx_oFEAkecX_xxx_3/xx.13xxxxxx.148.85xx.0,xx.131.18.40xx.0,xx.xx.xxxxxxxx.84:20010",
                "xxxxxx_oFEAkecX_xxx_3/xx.13xxxxxx.148.85xx.0,xx.131.18.40xx.0,xx.xx.xxxxxxxx.84:20010" : "xxxxxx_oFEAkecX_xxx_3/xx.13xxxxxx.148.85xx.0,xx.131.18.40xx.0,xx.xx.xxxxxxxx.84:20010",
                "xxxxxx_oFEAkecX_xxx_ImGeDQnU" : "xxxxxx_oFEAkecX_xxx_ImGeDQnU/xx.128.22.211xx.0,xx.xxxxxx.209.54xx.0,xx.13xxxxxx.159.194xx.0",
                "xxxxxx_oFEAkecX_xxx_ImGeDQnU/xx.128.22.211xx.0,xx.xxxxxx.209.54xx.0,xx.13xxxxxx.159.194xx.0" : "xxxxxx_oFEAkecX_xxx_ImGeDQnU/xx.128.22.211xx.0,xx.xxxxxx.209.54xx.0,xx.13xxxxxx.159.194xx.0",
                "xxxxxx_oFEAkecX_xxx_MrFIjpKf" : "xxxxxx_oFEAkecX_xxx_MrFIjpKf/xx.13xxxxxx.159.188xx.0,xx.13xxxxxx.159.189xx.0,xx.13xxxxxx.159.190xx.0",
                "xxxxxx_oFEAkecX_xxx_MrFIjpKf/xx.13xxxxxx.159.188xx.0,xx.13xxxxxx.159.189xx.0,xx.13xxxxxx.159.190xx.0" : "xxxxxx_oFEAkecX_xxx_MrFIjpKf/xx.13xxxxxx.159.188xx.0,xx.13xxxxxx.159.189xx.0,xx.13xxxxxx.159.190xx.0",
                "xxxxxx_oFEAkecX_xxx_UeRnftkj" : "xxxxxx_oFEAkecX_xxx_UeRnftkj/xx.xxxxxx.155.189xx.0,xx.13xxxxxx.159.247xx.0,xx.13xxxxxx.159.248xx.0",
                "xxxxxx_oFEAkecX_xxx_UeRnftkj/xx.xxxxxx.155.189xx.0,xx.13xxxxxx.159.247xx.0,xx.13xxxxxx.159.248xx.0" : "xxxxxx_oFEAkecX_xxx_UeRnftkj/xx.xxxxxx.155.189xx.0,xx.13xxxxxx.159.247xx.0,xx.13xxxxxx.159.248xx.0"
        },
        "ok" : 1,
        "operationTime" : Timestamp(1623043296, 10134),
        "$gleStats" : {
                "lastOpTime" : Timestamp(0, 0),
                "electionId" : ObjectId("7fffffff0000000000000012")
        },
        "$configServerState" : {
                "opTime" : {
                        "ts" : Timestamp(1623043294, 5344),
                        "t" : NumberLong(12)
                }
        },
        "$clusterTime" : {
                "clusterTime" : Timestamp(1623043296, 10134),
                "signature" : {
                        "hash" : BinData(0,"YTFMjq4M2d9I2wkcjIDcX1wCzds="),
                        "keyId" : NumberLong("6933088392779399171")
                }
        }
}
xxxxxx_oFEAkecX_xxx_1:PRIMARY> 
//mongod和mongos获取集群所有得节点信息，包括其他分片得，以及config得，不包括mongos的
//注意这里mongod和mongos用的同一个代码，比较特殊
*/

class CmdGetShardMap : public BasicCommand {
public:
    CmdGetShardMap() : BasicCommand("getShardMap") {}


    virtual bool supportsWriteConcern(const BSONObj& cmd) const override {
        return false;
    }

    virtual bool slaveOk() const {
        return true;
    }

    virtual void help(std::stringstream& help) const {
        help << "lists the set of shards known to this instance";
    }

    virtual bool adminOnly() const {
        return true;
    }

    virtual void addRequiredPrivileges(const std::string& dbname,
                                       const BSONObj& cmdObj,
                                       std::vector<Privilege>* out) {
        ActionSet actions;
        actions.addAction(ActionType::getShardMap);
        out->push_back(Privilege(ResourcePattern::forClusterResource(), actions));
    }

    virtual bool run(OperationContext* opCtx,
                     const std::string& dbname,
                     const BSONObj& cmdObj,
                     BSONObjBuilder& result) {
        // MongoD instances do not know that they are part of a sharded cluster until they
        // receive a setShardVersion command and that's when the catalog manager and the shard
        // registry get initialized.
        if (grid.shardRegistry()) {
            grid.shardRegistry()->toBSON(&result);
        }

        return true;
    }

} getShardMapCmd;

}  // namespace
}  // namespace mongo
