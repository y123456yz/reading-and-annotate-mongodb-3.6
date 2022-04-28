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

#include "mongo/s/chunk_version.h"

#include "mongo/base/status_with.h"
#include "mongo/bson/bsonobj.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/bson/util/bson_extract.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {
namespace {
/*
3.2版本里，config server 从以前的多个镜像节点换成了复制集，换成复制集后，由于复制自身的特性，
Sharded Cluster 在实现上也面临一些挑战。

挑战1：复制集原Primary 上的数据可能会发生回滚，对 mongos 而言，就是『读到的路由表后来又被回滚了』。
挑战2：复制集备节点的数据比主节点落后，如果仅从主节点上读，读能力不能扩展，如果从备节点上读，可能读
到的数据不是最新的，对 mongos 的影响是『可能读到过期的路由表，在上述例子中，mongos 发现自己的路由表
版本低了，于是去 config server 拉取最新的路由表，而如果这时请求到未更新的备节点上，可能并不能成功的
更新路由表』。


应对第一个问题，MongoDB 在3.2版本里增加了 ReadConcern 特性的支持，ReadConcern支持『local』和『majority』2个级别。
local 即普通的 read，majority 级别保证应用读到的数据已经成功写入到了复制集的大多数成员。
而一旦数据成功写入到大多数成员，这样的数据就肯定不会发生 rollback，mongos 在从 config server 读取数据时
，会指定 readConcern 为 majority 级别，确保读取到的路由信息肯定不会被回滚。

Mongos 带着路由表版本信息请求 某个 shard，shard发现自己的版本比 mongos 新（发生过 chunk 迁移），
此时shard 除了告诉 mongos 自己应该去更新路由表，还会把自己迁移 chunk 后更新 config server 时的 
optime告诉mongos，mongos 请求 config server 时，指定 readConcern 级别为 majority，并指定 
afterOpTime 参数，以确保不会从备节点读到过期的路由表。



应对第二个问题，MongoDB 在majority 级别的基础上，增加了 afterOpTime 的参数，这个参数目前只在 Sharded Cluster 内部使用。这个参数的意思是『被请求节点的最新oplog时间戳必须大于 afterOpTime 指定的时间戳』。

*/


const char kVersion[] = "version";
/*
mongos> db.chunks.find({ns:"xx.xx"}).limit(1).pretty()
{
        "_id" : "sporthealth.stepsDetail-ssoid_811088201705515807",
        //Lastmod:第一部分是major version，一次movechunk命令操作（Chunk从一个Shard迁移到另一个Shard）会加1；
        //第二个数字是Minor Version，一次split命令操作会加1。
        "lastmod" : Timestamp(143, 61),
        //lastmodEpoch: epoch : objectID，标识集合的唯一实例，用于辨识集合是否发生了变化。只有当 collection 被 drop 或者 collection的shardKey发生refined时 会重新生成
        "lastmodEpoch" : ObjectId("5f9aa6ec3af7fbacfbc99a27"),
        "ns" : "sporthealth.stepsDetail",
        "min" : {
                "ssoid" : NumberLong("811088201705515807")
        },
        "max" : {
                "ssoid" : NumberLong("811127732696226936")
        },
        "shard" : "sport-health_xyKKIMeg_shard_1"
}
mongos> 
*/
//一个(majorVersion, minorVersion)的二元组)
const char kLastmod[] = "lastmod";

}  // namespace

//2021-05-31T19:29:33.775+0800 I COMMAND  [conn3479863] command sporthealth.stepsDetail command: insert { insert: "stepsDetail", bypassDocumentValidation: false, ordered: true, documents: [ { _id: ObjectId('60b4c89dbba026408eaa55ec'), id: 559306090572558338, clientDataId: "0f015c36e44b4559a2a0e5baa4998d58", ssoid: "212848537", deviceUniqueId: "xxx", deviceType: "Phone", startTimestamp: xxx, endTimestamp: 1622460360000, sportMode: 6, steps: 62, distance: 41, calories: 1499, altitudeOffset: 0, display: 1, syncStatus: 0, workout: 0, modifiedTime: 1622460573621, createTime: new Date(1622460573625), updateTime: new Date(1622460573625), _class: "com.oppo.sporthealthdataprocess.po.StepsDetail" } ], shardVersion: [ Timestamp(33477, 353588), ObjectId('5f9aa6ec3af7fbacfbc99a27') ], lsid: { id: UUID("e8a1985f-c3ff-4b2e-8d6c-5136818c3ba7"), uid: BinData(0, 64A61BF5764A1A00129F0CBAC3D8D4C51E4EAA3B877BF0F06A946E40E9EA172E) }, $clusterTime: { clusterTime: Timestamp(1622460573, 5482), signature: { hash: BinData(0, 4CF2584792D377D057BADF6FE58DE436A017BD13), keyId: 6920984255816273035 } }, $client: { driver: { name: "mongo-java-driver", version: "3.8.2" }, os: { type: "Linux", name: "Linux", architecture: "amd64", version: "3.10.0-957.27.2.el7.x86_64" }, platform: "Java/heytap/1.8.0_252-b09", mongos: { host: "xx.xx:xx", client: "10.xxx.231:xx", version: "3.6.10" } }, $configServerState: { opTime: { ts: Timestamp(1622460570, 3548), t: 5 } }, $db: "sporthealth" } ninserted:1 keysInserted:6 numYields:0 reslen:355 locks:{ Global: { acquireCount: { r: 2, w: 2 } }, Database: { acquireCount: { w: 2 } }, Collection: { acquireCount: { w: 1 } }, oplog: { acquireCount: { w: 1 } } } protocol:op_msg 108ms
//mongos发送到mongod的请求中会携带shardVersion: shardVersion: [ Timestamp(33477, 353588), ObjectId('5f9aa6ec3af7fbacfbc99a27') ]
const char ChunkVersion::kShardVersionField[] = "shardVersion";

//constructBatchedCommandRequest中调用，解析shardVersion内容
StatusWith<ChunkVersion> ChunkVersion::parseFromBSONForCommands(const BSONObj& obj) {
    return parseFromBSONWithFieldForCommands(obj, kShardVersionField);
}

StatusWith<ChunkVersion> ChunkVersion::parseFromBSONWithFieldForCommands(const BSONObj& obj,
                                                                         StringData field) {
    BSONElement versionElem;
    Status status = bsonExtractField(obj, field, &versionElem);
    if (!status.isOK())
        return status;

    if (versionElem.type() != Array) {
        return {ErrorCodes::TypeMismatch,
                str::stream() << "Invalid type " << versionElem.type()
                              << " for shardVersion element. Expected an array"};
    }

    BSONObjIterator it(versionElem.Obj());
    if (!it.more())
        return {ErrorCodes::BadValue, "Unexpected empty version"};

    ChunkVersion version;

    // Expect the timestamp
    {
        BSONElement tsPart = it.next();
        if (tsPart.type() != bsonTimestamp)
            return {ErrorCodes::TypeMismatch,
                    str::stream() << "Invalid type " << tsPart.type()
                                  << " for version timestamp part."};

        version._combined = tsPart.timestamp().asULL();
    }

    // Expect the epoch OID
    {
        BSONElement epochPart = it.next();
        if (epochPart.type() != jstOID)
            return {ErrorCodes::TypeMismatch,
                    str::stream() << "Invalid type " << epochPart.type()
                                  << " for version epoch part."};

        version._epoch = epochPart.OID();
    }

    return version;
}

StatusWith<ChunkVersion> ChunkVersion::parseFromBSONForSetShardVersion(const BSONObj& obj) {
    bool canParse;
    const ChunkVersion chunkVersion = ChunkVersion::fromBSON(obj, kVersion, &canParse);
    if (!canParse)
        return {ErrorCodes::BadValue, "Unable to parse shard version"};

    return chunkVersion;
}

StatusWith<ChunkVersion> ChunkVersion::parseFromBSONForChunk(const BSONObj& obj) {
    bool canParse;
    const ChunkVersion chunkVersion = ChunkVersion::fromBSON(obj, kLastmod, &canParse);
    if (!canParse)
        return {ErrorCodes::BadValue, "Unable to parse shard version"};

    return chunkVersion;
}

//从obj中解析出lastmod字段，也就是chunkversion
StatusWith<ChunkVersion> ChunkVersion::parseFromBSONWithFieldAndSetEpoch(const BSONObj& obj,
                                                                         StringData field,
                                                                         const OID& epoch) {
    bool canParse;
    ChunkVersion chunkVersion = ChunkVersion::fromBSON(obj, field.toString(), &canParse);
    if (!canParse)
        return {ErrorCodes::BadValue, "Unable to parse shard version"};
    chunkVersion._epoch = epoch;
    return chunkVersion;
}

void ChunkVersion::appendForSetShardVersion(BSONObjBuilder* builder) const {
    addToBSON(*builder, kVersion);
}

void ChunkVersion::appendForCommands(BSONObjBuilder* builder) const {
    appendWithFieldForCommands(builder, kShardVersionField);
}

void ChunkVersion::appendWithFieldForCommands(BSONObjBuilder* builder, StringData field) const {
    builder->appendArray(field, toBSON());
}

void ChunkVersion::appendForChunk(BSONObjBuilder* builder) const {
    addToBSON(*builder, kLastmod);
}

BSONObj ChunkVersion::toBSON() const {
    BSONArrayBuilder b;
    b.appendTimestamp(_combined);
    b.append(_epoch);
    return b.arr();
}

}  // namespace mongo
