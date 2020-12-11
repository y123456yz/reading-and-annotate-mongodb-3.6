/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/s/request_types/shard_collection_gen.h --output build/opt/mongo/s/request_types/shard_collection_gen.cpp src/mongo/s/request_types/shard_collection.idl
 */

#pragma once

#include <algorithm>
#include <boost/optional.hpp>
#include <cstdint>
#include <string>
#include <vector>

#include "mongo/base/data_range.h"
#include "mongo/base/string_data.h"
#include "mongo/bson/bsonobj.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/namespace_string.h"
#include "mongo/idl/idl_parser.h"
#include "mongo/util/net/op_msg.h"
#include "mongo/util/uuid.h"

namespace mongo {

/**
 * The public shardCollection command on mongos
 */
class ShardCollection {
public:
    static constexpr auto kCollationFieldName = "collation"_sd;
    static constexpr auto kKeyFieldName = "key"_sd;
    static constexpr auto kNumInitialChunksFieldName = "numInitialChunks"_sd;
    static constexpr auto kShardCollectionFieldName = "shardCollection"_sd;
    static constexpr auto kShardcollectionFieldName = "shardcollection"_sd;
    static constexpr auto kUniqueFieldName = "unique"_sd;

    ShardCollection();

    static ShardCollection parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    /**
     * The namespace of the collection to shard in the form <database>.<collection>.
     */
    const boost::optional<mongo::NamespaceString>& getShardCollection() const& { return _shardCollection; }
    void getShardCollection() && = delete;
    void setShardCollection(boost::optional<mongo::NamespaceString> value) & { _shardCollection = std::move(value);  }

    /**
     * The deprecated version of this command's name
     */
    const boost::optional<mongo::NamespaceString>& getShardcollection() const& { return _shardcollection; }
    void getShardcollection() && = delete;
    void setShardcollection(boost::optional<mongo::NamespaceString> value) & { _shardcollection = std::move(value);  }

    /**
     * The index specification document to use as the shard key.
     */
    const mongo::BSONObj& getKey() const { return _key; }
    void setKey(mongo::BSONObj value) & { _key = std::move(value); _hasKey = true; }

    /**
     * Whether the shard key index should enforce a unique constraint
     */
    bool getUnique() const { return _unique; }
    void setUnique(bool value) & { _unique = std::move(value);  }

    /**
     * The number of chunks to create initially when sharding an empty collection with a hashed shard key.
     */
    std::int64_t getNumInitialChunks() const { return _numInitialChunks; }
    void setNumInitialChunks(std::int64_t value) & { _numInitialChunks = std::move(value);  }

    /**
     * The collation to use for the shard key index.
     */
    const boost::optional<mongo::BSONObj>& getCollation() const& { return _collation; }
    void getCollation() && = delete;
    void setCollation(boost::optional<mongo::BSONObj> value) & { _collation = std::move(value);  }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    boost::optional<mongo::NamespaceString> _shardCollection;
    boost::optional<mongo::NamespaceString> _shardcollection;
    mongo::BSONObj _key;
    bool _unique{false};
    std::int64_t _numInitialChunks{0};
    boost::optional<mongo::BSONObj> _collation;
    bool _hasKey : 1;
};

/**
 * The request format of the internal shardCollection command on the config server
 */
class ConfigsvrShardCollectionRequest {
public:
    static constexpr auto k_configsvrShardCollectionFieldName = "_configsvrShardCollection"_sd;
    static constexpr auto kCollationFieldName = "collation"_sd;
    static constexpr auto kInitialSplitPointsFieldName = "initialSplitPoints"_sd;
    static constexpr auto kKeyFieldName = "key"_sd;
    static constexpr auto kNumInitialChunksFieldName = "numInitialChunks"_sd;
    static constexpr auto kUniqueFieldName = "unique"_sd;

    ConfigsvrShardCollectionRequest();

    static ConfigsvrShardCollectionRequest parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    /**
     * The namespace of the collection to shard in the form <database>.<collection>.
     */
    const boost::optional<mongo::NamespaceString>& get_configsvrShardCollection() const& { return __configsvrShardCollection; }
    void get_configsvrShardCollection() && = delete;
    void set_configsvrShardCollection(boost::optional<mongo::NamespaceString> value) & { __configsvrShardCollection = std::move(value);  }

    /**
     * The index specification document to use as the shard key.
     */
    const mongo::BSONObj& getKey() const { return _key; }
    void setKey(mongo::BSONObj value) & { _key = std::move(value); _hasKey = true; }

    /**
     * Whether the shard key index should enforce a unique constraint
     */
    bool getUnique() const { return _unique; }
    void setUnique(bool value) & { _unique = std::move(value);  }

    /**
     * The number of chunks to create initially when sharding an empty collection with a hashed shard key.
     */
    std::int64_t getNumInitialChunks() const { return _numInitialChunks; }
    void setNumInitialChunks(std::int64_t value) & { _numInitialChunks = std::move(value);  }

    /**
     * A specific set of points to create initial splits at, currently used only by mapReduce
     */
    const boost::optional<std::vector<mongo::BSONObj>>& getInitialSplitPoints() const& { return _initialSplitPoints; }
    void getInitialSplitPoints() && = delete;
    void setInitialSplitPoints(boost::optional<std::vector<mongo::BSONObj>> value) & { _initialSplitPoints = std::move(value);  }

    /**
     * The collation to use for the shard key index.
     */
    const boost::optional<mongo::BSONObj>& getCollation() const& { return _collation; }
    void getCollation() && = delete;
    void setCollation(boost::optional<mongo::BSONObj> value) & { _collation = std::move(value);  }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    boost::optional<mongo::NamespaceString> __configsvrShardCollection;
    mongo::BSONObj _key;
    bool _unique{false};
    std::int64_t _numInitialChunks{0};
    boost::optional<std::vector<mongo::BSONObj>> _initialSplitPoints;
    boost::optional<mongo::BSONObj> _collation;
    bool _hasKey : 1;
};

/**
 * The response format of the internal shardCollection command on the config server
 */
class ConfigsvrShardCollectionResponse {
public:
    static constexpr auto kCollectionUUIDFieldName = "collectionUUID"_sd;
    static constexpr auto kCollectionshardedFieldName = "collectionsharded"_sd;

    ConfigsvrShardCollectionResponse();

    static ConfigsvrShardCollectionResponse parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    /**
     * The name of the collection that just got sharded.
     */
    const StringData getCollectionsharded() const& { return _collectionsharded; }
    void getCollectionsharded() && = delete;
    void setCollectionsharded(StringData value) & { _collectionsharded = value.toString(); _hasCollectionsharded = true; }

    /**
     * The UUID of the collection that just got sharded.
     */
    const boost::optional<mongo::UUID>& getCollectionUUID() const& { return _collectionUUID; }
    void getCollectionUUID() && = delete;
    void setCollectionUUID(boost::optional<mongo::UUID> value) & { _collectionUUID = std::move(value);  }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    std::string _collectionsharded;
    boost::optional<mongo::UUID> _collectionUUID;
    bool _hasCollectionsharded : 1;
};

}  // namespace mongo
