/**
 *    Copyright (C) 2012 10gen Inc.
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

#pragma once

#include <boost/optional.hpp>
#include <string>

#include "mongo/db/jsobj.h"
#include "mongo/db/keypattern.h"
#include "mongo/db/namespace_string.h"
#include "mongo/util/uuid.h"

namespace mongo {

class Status;
template <typename T>
class StatusWith;


/**
 * This class represents the layout and contents of documents contained in the config server's
 * config.collections collection. All manipulation of documents coming from that collection
 * should be done with this class.
 *
 * Expected config server config.collections collection format:
 *   {
 *      "_id" : "foo.bar",
 *      "lastmodEpoch" : ObjectId("58b6fd76132358839e409e47"),
 *      "lastmod" : ISODate("1970-02-19T17:02:47.296Z"),
 *      "dropped" : false,
 *      "key" : {
 *          "_id" : 1
 *      },
 *      "defaultCollation" : {
 *          "locale" : "fr_CA"
 *      },
 *      "unique" : false,
 *      "uuid" : UUID,
 *      "noBalance" : false
 *   }
 *
 */  
/*
const std::string CollectionType::ConfigNS = "config.collections";

const BSONField<std::string> CollectionType::fullNs("_id");
const BSONField<OID> CollectionType::epoch("lastmodEpoch");
const BSONField<Date_t> CollectionType::updatedAt("lastmod");
const BSONField<BSONObj> CollectionType::keyPattern("key");
const BSONField<BSONObj> CollectionType::defaultCollation("defaultCollation");
const BSONField<bool> CollectionType::unique("unique");
const BSONField<UUID> CollectionType::uuid("uuid");

mongos> db.collections.find()
{ "_id" : "config.system.sessions", "lastmodEpoch" : ObjectId("5e1c7c1ae7eea8361b9f29ba"), "lastmod" : ISODate("1970-02-19T17:02:47.296Z"), "dropped" : false, "key" : { "_id" : 1 }, "unique" : false, "uuid" : UUID("6d5d29ff-d979-4c5e-ba10-d5560497c964") }
{ "_id" : "push_open.app_device", "lastmodEpoch" : ObjectId("5f2a6342b2eabbc990d95f12"), "lastmod" : ISODate("1970-02-19T17:02:47.296Z"), "dropped" : false, "key" : { "appId" : 1, "deviceId" : 1 }, "unique" : false, "uuid" : UUID("3dd3b8a4-ab97-44dd-b8b5-bd31980638ac") }
{ "_id" : "push_open.device", "lastmodEpoch" : ObjectId("5efe9809b2eabbc990fa0bfb"), "lastmod" : ISODate("1970-02-19T17:02:47.430Z"), "dropped" : false, "key" : { "_id" : "hashed" }, "unique" : false, "uuid" : UUID("059b876c-74d0-4beb-998a-b2356bad3416"), "noBalance" : false }
*/
//CollectionType和DatabaseType，一个对应表，一个对应库
//CatalogCache::_getDatabase中从cfg复制集的config.database和config.collections中获取dbName库及其下面的表信息
class CollectionType {
public:
    // Name of the collections collection in the config server.
    //"config.collections";表明
    static const std::string ConfigNS;

    //"config.collections"表中的_id字段
    static const BSONField<std::string> fullNs;
    //"config.collections"表中的lastmodEpoch字段
    static const BSONField<OID> epoch;
    //"config.collections"表中的lastmod字段
    static const BSONField<Date_t> updatedAt;
    //"config.collections"表中的key字段
    static const BSONField<BSONObj> keyPattern;
    //"config.collections"表中的defaultCollation字段
    static const BSONField<BSONObj> defaultCollation;
    //"config.collections"表中的unique字段
    static const BSONField<bool> unique;
    //"config.collections"表中的uuid字段
    static const BSONField<UUID> uuid;

    /**
     * Constructs a new DatabaseType object from BSON. Also does validation of the contents.
     *
     * Dropped collections accumulate in the collections list, through 3.6, so that
     * mongos <= 3.4.x, when it retrieves the list from the config server, can delete its
     * cache entries for dropped collections.  See SERVER-27475, SERVER-27474
     */
    static StatusWith<CollectionType> fromBSON(const BSONObj& source);

    /**
     * Returns OK if all fields have been set. Otherwise returns NoSuchKey and information
     * about what is the first field which is missing.
     */
    Status validate() const;

    /**
     * Returns the BSON representation of the entry.
     */
    BSONObj toBSON() const;

    /**
     * Returns a std::string representation of the current internal state.
     */
    std::string toString() const;

    const NamespaceString& getNs() const {
        return _fullNs.get();
    }
    void setNs(const NamespaceString& fullNs);

    OID getEpoch() const {
        return _epoch.get();
    }
    void setEpoch(OID epoch);

    Date_t getUpdatedAt() const {
        return _updatedAt.get();
    }
    void setUpdatedAt(Date_t updatedAt);

    bool getDropped() const {
        return _dropped.get_value_or(false);
    }
    void setDropped(bool dropped) {
        _dropped = dropped;
    }

    const KeyPattern& getKeyPattern() const {
        return _keyPattern.get();
    }
    void setKeyPattern(const KeyPattern& keyPattern);

    const BSONObj& getDefaultCollation() const {
        return _defaultCollation;
    }
    void setDefaultCollation(const BSONObj& collation) {
        _defaultCollation = collation.getOwned();
    }

    bool getUnique() const {
        return _unique.get_value_or(false);
    }
    void setUnique(bool unique) {
        _unique = unique;
    }

    boost::optional<UUID> getUUID() const {
        return _uuid;
    }

    void setUUID(UUID uuid) {
        _uuid = uuid;
    }

    bool getAllowBalance() const {
        return _allowBalance.get_value_or(true);
    }

    bool hasSameOptions(CollectionType& other);

private:
    // Required full namespace (with the database prefix).
    //表名
    boost::optional<NamespaceString> _fullNs;

    // Required to disambiguate collection namespace incarnations.
    //版本纪元
    boost::optional<OID> _epoch;

    // Required last updated time.
    //最后一次更新时间
    boost::optional<Date_t> _updatedAt;

    // Optional, whether the collection has been dropped. If missing, implies false.
    //表是否已经删除了
    boost::optional<bool> _dropped;

    // Sharding key. Required, if collection is not dropped.
    //片建
    boost::optional<KeyPattern> _keyPattern;

    // Optional collection default collation. If empty, implies simple collation.
    //排序方式
    BSONObj _defaultCollation;

    // Optional uniqueness of the sharding key. If missing, implies false.
    //
    boost::optional<bool> _unique;

    // Optional in 3.6 binaries, because UUID does not exist in featureCompatibilityVersion=3.4.
    //该表对应唯一UUID
    boost::optional<UUID> _uuid;

    // Optional whether balancing is allowed for this collection. If missing, implies true.
    //该表是否启用了balance 
    boost::optional<bool> _allowBalance;
};

}  // namespace mongo
