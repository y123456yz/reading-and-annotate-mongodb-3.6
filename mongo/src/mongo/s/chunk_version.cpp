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

//一个(majorVersion, minorVersion)的二元组)
const char kVersion[] = "version";
const char kLastmod[] = "lastmod";

}  // namespace

const char ChunkVersion::kShardVersionField[] = "shardVersion";

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
