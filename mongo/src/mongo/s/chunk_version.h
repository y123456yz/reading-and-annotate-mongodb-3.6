/**
 *    Copyright (C) 2012-2015 MongoDB Inc.
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

#pragma once

#include "mongo/db/jsobj.h"

namespace mongo {

class BSONObj;
template <typename T>
class StatusWith;

/**
 * ChunkVersions consist of a major/minor version scoped to a version epoch
 *
 * Version configurations (format: major version, epoch):
 *
 * 1. (0, 0) - collection is dropped.
 * 2. (0, n), n > 0 - applicable only to shardVersion; shard has no chunk.
 * 3. (n, 0), n > 0 - invalid configuration.
 * 4. (n, m), n > 0, m > 0 - normal sharded collection version.
 *
 * TODO: This is a "manual type" but, even so, still needs to comform to what's
 * expected from types.
 //ShardingState::_refreshMetadata中返回获取
 */
//可以参考https://mongoing.com/archives/75945  MongoDB 路由表刷新导致响应慢场景解读
//https://mongoing.com/archives/77370  万亿级MongoDB集群的路由优化之路

 
//chunk 版本信息 https://developer.aliyun.com/article/778536?spm=a2c6h.17698244.wenzhang.9.7b934d126DdIOU
//chunk版本信息，也就是ShardVersion 可以参考https://developer.aliyun.com/article/58689
//MigrateInfo.version  BatchedCommandRequest._shardVersion成员为该类型        getCollVersion和getShardVersion都是该类型，参考MigrationSourceManager::MigrationSourceManager
struct ChunkVersion {
public:
    /**
     * The name for the shard version information field, which shard-aware commands should include
     * if they want to convey shard version.
     */
    static const char kShardVersionField[];

    ChunkVersion() : _combined(0), _epoch(OID()) {}

    //一个ChunkVersion主要由这三个成员组成
    ChunkVersion(int major, int minor, const OID& epoch)
        : _combined(static_cast<uint64_t>(minor) | (static_cast<uint64_t>(major) << 32)),
          _epoch(epoch) {}

    /**
     * Interprets the specified BSON content as the format for commands, which is in the form:
     *  { ..., shardVersion: [ <combined major/minor>, <OID epoch> ], ... }
     */
    static StatusWith<ChunkVersion> parseFromBSONForCommands(const BSONObj& obj);

    /**
     * Parses the BSON formatted by ChunkVersion::appendWithFieldForCommands.
     *
     * Interprets the specified BSON content as the format for commands, which is in the form:
     *  { ..., <field>: [ <combined major/minor>, <OID epoch> ], ... }.
     */
    static StatusWith<ChunkVersion> parseFromBSONWithFieldForCommands(const BSONObj& obj,
                                                                      StringData field);

    /**
     * Note: if possible, use ChunkVersion::parseFromBSONForCommands or
     * ChunkVersion::parseFromBSONWithFieldForCommands instead. Phasing out this function.
     *
     * Interprets the specified BSON content as the format for the setShardVersion command, which
     * is in the form:
     *  { ..., version: [ <combined major/minor> ], versionEpoch: [ <OID epoch> ], ... }
     */
    static StatusWith<ChunkVersion> parseFromBSONForSetShardVersion(const BSONObj& obj);

    /**
     * Note: if possible, use ChunkVersion::parseFromBSONForCommands or
     * ChunkVersion::parseFromBSONWithFieldForCommands instead. Phasing out this function.
     *
     * Interprets the specified BSON content as the format for chunk persistence, which is in the
     * form:
     *  { ..., lastmod: [ <combined major/minor> ], lastmodEpoch: [ <OID epoch> ], ... }
     */
    static StatusWith<ChunkVersion> parseFromBSONForChunk(const BSONObj& obj);

    /**
     * Interprets the lastmod (combined major/minor) from a BSONObj without an epoch
     *  { ..., <field>: [ <combined major/minor> ], ... }
     * and then sets the returned ChunkVersion's epoch field to 'epoch'.
     */
    static StatusWith<ChunkVersion> parseFromBSONWithFieldAndSetEpoch(const BSONObj& obj,
                                                                      StringData field,
                                                                      const OID& epoch);

    /**
     * Indicates a dropped collection. All components are zeroes (OID is zero time, zero
     * machineId/inc).
     */
    static ChunkVersion DROPPED() {
        return ChunkVersion(0, 0, OID());
    }

    /**
     * Indicates that the collection is not sharded. Same as DROPPED.
     */
    //没有使能分片功能
    static ChunkVersion UNSHARDED() {
        return ChunkVersion(0, 0, OID());
    }

    static ChunkVersion IGNORED() {
        ChunkVersion version = ChunkVersion();
        version._epoch.init(Date_t(), true);  // ignored OID is zero time, max machineId/inc
        return version;
    }

    static ChunkVersion fromDeprecatedLong(unsigned long long num, const OID& epoch) {
        ChunkVersion version(0, 0, epoch);
        version._combined = num;
        return version;
    }

    static bool isIgnoredVersion(const ChunkVersion& version) {
        return version.majorVersion() == 0 && version.minorVersion() == 0 &&
            version.epoch() == IGNORED().epoch();
    }

    //ChunkManagerTargeter::noteStaleResponse调用
    void incMajor() {
        _combined = static_cast<uint64_t>(majorVersion() + 1) << 32;
    }

    //ShardingCatalogManager::commitChunkSplit(splitchunk的时候)
    //ShardingCatalogManager::commitChunkMerge
    //createFirstChunks
    void incMinor() {
        _combined++;
    }

    // Note: this shouldn't be used as a substitute for version except in specific cases -
    // epochs make versions more complex
    unsigned long long toLong() const {
        return _combined;
    }

    bool isSet() const {
        return _combined > 0;
    }

    /*
    实际上在3.2的实现版本是一个(majorVersion, minorVersion)的二元组)， 当
    chunk 发生split 之后， split 之后的所有 chunk minor version增加， 当 chunk 在 shard 之间发生迁移时， 迁移的 chunk
    在标上增加其major version， 并且在迁移源上选择一个chunk增加其major version， 这样确保不论是访问到源还是目
    标， mongos 看到的版本都增加了

    shard version 和 collection version 形式均为 「25550573|83||5f59e113f7f9b49e704c227f」，这即是一个 chunk version，通过 "|" 和 "||" 将版本信息分为三段：
第一段为 major version : 整数，用于辨识路由指向是否发生变化，以便各节点及时更新路由。比如在发生chunk 在 shard 之间迁移时会增加
第二段为 minor version : 整数，主要用于记录不影响路由指向的一些变化。比如chunk 发生 split 时增加
第三段为 epoch : objectID，标识集合的唯一实例，用于辨识集合是否发生了变化。只有当 collection 被 drop 或者 collection的shardKey发生refined时 会重新生成
    https://developer.aliyun.com/article/778536?spm=a2c6h.17698244.wenzhang.9.7b934d126DdIOU
    */
    int majorVersion() const {
        return _combined >> 32;
    }
    int minorVersion() const {
        return _combined & 0xFFFF;
    }

    

    OID epoch() const {
        return _epoch;
    }

    //
    // Explicit comparison operators - versions with epochs have non-trivial comparisons.
    // > < operators do not check epoch cases.  Generally if using == we need to handle
    // more complex cases.
    //

    bool operator==(const ChunkVersion& otherVersion) const {
        return equals(otherVersion);
    }

    bool operator>(const ChunkVersion& otherVersion) const {
        return this->_combined > otherVersion._combined;
    }

    bool operator>=(const ChunkVersion& otherVersion) const {
        return this->_combined >= otherVersion._combined;
    }

    bool operator<(const ChunkVersion& otherVersion) const {
        return this->_combined < otherVersion._combined;
    }

    bool operator<=(const ChunkVersion& otherVersion) const {
        return this->_combined <= otherVersion._combined;
    }

    //
    // Equivalence comparison types.
    //

    // Can we write to this data and not have a problem?
    bool isWriteCompatibleWith(const ChunkVersion& otherVersion) const {
        if (!hasEqualEpoch(otherVersion))
            return false;
        return otherVersion.majorVersion() == majorVersion();
    }

    // Is this the same version?
    bool equals(const ChunkVersion& otherVersion) const {
        if (!hasEqualEpoch(otherVersion))
            return false;
        return otherVersion._combined == _combined;
    }

    /**
     * Returns true if the otherVersion is the same as this version and enforces strict epoch
     * checking (empty epochs are not wildcards).
     */
    bool isStrictlyEqualTo(const ChunkVersion& otherVersion) const {
        if (otherVersion._epoch != _epoch)
            return false;
        return otherVersion._combined == _combined;
    }

    /**
     * Returns true if this version is (strictly) in the same epoch as the other version and
     * this version is older.  Returns false if we're not sure because the epochs are different
     * or if this version is newer.
     */
    bool isOlderThan(const ChunkVersion& otherVersion) const {
        if (otherVersion._epoch != _epoch)
            return false;

        if (majorVersion() != otherVersion.majorVersion())
            return majorVersion() < otherVersion.majorVersion();

        return minorVersion() < otherVersion.minorVersion();
    }

    // Is this in the same epoch?
    bool hasEqualEpoch(const ChunkVersion& otherVersion) const {
        return hasEqualEpoch(otherVersion._epoch);
    }

    bool hasEqualEpoch(const OID& otherEpoch) const {
        return _epoch == otherEpoch;
    }

    //
    // BSON input/output
    //
    // The idea here is to make the BSON input style very flexible right now, so we
    // can then tighten it up in the next version.  We can accept either a BSONObject field
    // with version and epoch, or version and epoch in different fields (either is optional).
    // In this case, epoch always is stored in a field name of the version field name + "Epoch"
    //

    //
    // { version : <TS> } and { version : [<TS>,<OID>] } format
    //
    
    static ChunkVersion fromBSON(const BSONElement& el, const std::string& prefix, bool* canParse) {
        *canParse = true;

        int type = el.type();

        if (type == Array) {
            return fromBSON(BSONArray(el.Obj()), canParse);
        }

        if (type == jstOID) {
            return ChunkVersion(0, 0, el.OID());
        }

        if (type == bsonTimestamp || type == Date) {
            return fromDeprecatedLong(el._numberLong(), OID());
        }

        *canParse = false;

        return ChunkVersion(0, 0, OID());
    }

    //
    // { version : <TS>, versionEpoch : <OID> } object format
    //

    static ChunkVersion fromBSON(const BSONObj& obj, const std::string& prefix = "") {
        bool canParse;
        return fromBSON(obj, prefix, &canParse);
    }

    //OperationShardingState::initializeShardVersion调用
    static ChunkVersion fromBSON(const BSONObj& obj, const std::string& prefixIn, bool* canParse) {
        *canParse = true;

        std::string prefix = prefixIn;
        // "version" doesn't have a "cluster constanst" because that field is never
        // written to the config.
        if (prefixIn == "" && !obj["version"].eoo()) {
            prefix = (std::string) "version";
        }
        // TODO: use ChunkType::lastmod()
        // NOTE: type_chunk.h includes this file
        else if (prefixIn == "" && !obj["lastmod"].eoo()) {
            prefix = (std::string) "lastmod";
        }

        ChunkVersion version = fromBSON(obj[prefix], prefixIn, canParse);

        if (obj[prefix + "Epoch"].type() == jstOID) {
            version._epoch = obj[prefix + "Epoch"].OID();
            *canParse = true;
        }

        return version;
    }

    //
    // { version : [<TS>, <OID>] } format
    //

    static ChunkVersion fromBSON(const BSONArray& arr, bool* canParse) {
        *canParse = false;

        ChunkVersion version;

        BSONObjIterator it(arr);
        if (!it.more())
            return version;

        version = fromBSON(it.next(), "", canParse);
        if (!(*canParse))
            return version;

        *canParse = true;

        if (!it.more())
            return version;
        BSONElement next = it.next();
        if (next.type() != jstOID)
            return version;

        version._epoch = next.OID();

        return version;
    }

    //
    // Currently our BSON output is to two different fields, to cleanly work with older
    // versions that know nothing about epochs.
    //

    BSONObj toBSONWithPrefix(const std::string& prefix) const {
        invariant(!prefix.empty());

        BSONObjBuilder b;
        b.appendTimestamp(prefix, _combined);
        b.append(prefix + "Epoch", _epoch);
        return b.obj();
    }

    /**
     * Note: if possible, use ChunkVersion::appendForCommands or
     * ChunkVersion::appendWithFieldForCommands instead. Phasing out this function.
     */
    void addToBSON(BSONObjBuilder& b, const std::string& prefix) const {
        b.appendElements(toBSONWithPrefix(prefix));
    }

    /**
     * Appends the contents to the specified builder in the format expected by the setShardVersion
     * command.
     */
    void appendForSetShardVersion(BSONObjBuilder* builder) const;

    /**
     * Appends the contents to the specified builder in the format expected by the sharded commands.
     */
    void appendForCommands(BSONObjBuilder* builder) const;

    /**
     * Appends the contents as an array to "builder" with the field name "field" in the format
     * expected by the sharded commands.
     *
     * { ..., <field>: [ <combined major/minor>, <OID epoch> ], ... }
     *
     * Use ChunkVersion::parseFromBSONWithFieldForCommands to retrieve the ChunkVersion from the
     * BSON created by this function.
     */
    void appendWithFieldForCommands(BSONObjBuilder* builder, StringData field) const;

    /**
     * Appends the contents to the specified builder in the format expected by the chunk
     * serialization/deserialization code.
     */
    void appendForChunk(BSONObjBuilder* builder) const;

    std::string toString() const {
        StringBuilder sb;
        sb << majorVersion() << "|" << minorVersion() << "||" << _epoch;
        return sb.str();
    }

    BSONObj toBSON() const;

private:
    //版本 majority | minorVersion
    uint64_t _combined;

    OID _epoch;
};

inline std::ostream& operator<<(std::ostream& s, const ChunkVersion& v) {
    s << v.toString();
    return s;
}

}  // namespace mongo
