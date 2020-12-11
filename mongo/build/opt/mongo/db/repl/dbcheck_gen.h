/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/repl/dbcheck_gen.h --output build/opt/mongo/db/repl/dbcheck_gen.cpp src/mongo/db/repl/dbcheck.idl
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
#include "mongo/db/repl/dbcheck_idl.h"
#include "mongo/idl/idl_parser.h"
#include "mongo/util/net/op_msg.h"
#include "mongo/util/uuid.h"

namespace mongo {

/**
 * The type of dbCheck oplog entry.
 */
enum class OplogEntriesEnum : std::int32_t {
    Batch ,
    Collection ,
};

OplogEntriesEnum OplogEntries_parse(const IDLParserErrorContext& ctxt, StringData value);
StringData OplogEntries_serializer(OplogEntriesEnum value);

/**
 * Command object for dbCheck invocation
 */
class DbCheckSingleInvocation {
public:
    static constexpr auto kCollFieldName = "dbCheck"_sd;
    static constexpr auto kMaxCountFieldName = "maxCount"_sd;
    static constexpr auto kMaxCountPerSecondFieldName = "maxCountPerSecond"_sd;
    static constexpr auto kMaxKeyFieldName = "maxKey"_sd;
    static constexpr auto kMaxSizeFieldName = "maxSize"_sd;
    static constexpr auto kMinKeyFieldName = "minKey"_sd;

    DbCheckSingleInvocation();

    static DbCheckSingleInvocation parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const StringData getColl() const& { return _coll; }
    void getColl() && = delete;
    void setColl(StringData value) & { _coll = value.toString(); _hasColl = true; }

    const mongo::BSONKey& getMinKey() const { return _minKey; }
    void setMinKey(mongo::BSONKey value) & { _minKey = std::move(value);  }

    const mongo::BSONKey& getMaxKey() const { return _maxKey; }
    void setMaxKey(mongo::BSONKey value) & { _maxKey = std::move(value);  }

    std::int64_t getMaxCount() const { return _maxCount; }
    void setMaxCount(std::int64_t value) & { _maxCount = std::move(value);  }

    std::int64_t getMaxSize() const { return _maxSize; }
    void setMaxSize(std::int64_t value) & { _maxSize = std::move(value);  }

    std::int64_t getMaxCountPerSecond() const { return _maxCountPerSecond; }
    void setMaxCountPerSecond(std::int64_t value) & { _maxCountPerSecond = std::move(value);  }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    std::string _coll;
    mongo::BSONKey _minKey{BSONKey::min()};
    mongo::BSONKey _maxKey{BSONKey::max()};
    std::int64_t _maxCount{std::numeric_limits<int64_t>::max()};
    std::int64_t _maxSize{std::numeric_limits<int64_t>::max()};
    std::int64_t _maxCountPerSecond{std::numeric_limits<int64_t>::max()};
    bool _hasColl : 1;
};

/**
 * Command object for database-wide form of dbCheck invocation
 */
class DbCheckAllInvocation {
public:
    static constexpr auto kMaxCountPerSecondFieldName = "maxCountPerSecond"_sd;
    static constexpr auto kTagFieldName = "dbCheck"_sd;

    DbCheckAllInvocation();

    static DbCheckAllInvocation parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    std::int64_t getTag() const { return _tag; }
    void setTag(std::int64_t value) & { _tag = std::move(value); _hasTag = true; }

    std::int64_t getMaxCountPerSecond() const { return _maxCountPerSecond; }
    void setMaxCountPerSecond(std::int64_t value) & { _maxCountPerSecond = std::move(value);  }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    std::int64_t _tag;
    std::int64_t _maxCountPerSecond{std::numeric_limits<int64_t>::max()};
    bool _hasTag : 1;
};

/**
 * Oplog entry for a dbCheck batch
 */
class DbCheckOplogBatch {
public:
    static constexpr auto kMaxKeyFieldName = "maxKey"_sd;
    static constexpr auto kMaxRateFieldName = "maxRate"_sd;
    static constexpr auto kMd5FieldName = "md5"_sd;
    static constexpr auto kMinKeyFieldName = "minKey"_sd;
    static constexpr auto kNssFieldName = "dbCheck"_sd;
    static constexpr auto kTypeFieldName = "type"_sd;

    DbCheckOplogBatch();

    static DbCheckOplogBatch parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const mongo::NamespaceString& getNss() const { return _nss; }
    void setNss(mongo::NamespaceString value) & { _nss = std::move(value); _hasNss = true; }

    const OplogEntriesEnum getType() const { return _type; }
    void setType(OplogEntriesEnum value) & { _type = std::move(value); _hasType = true; }

    const StringData getMd5() const& { return _md5; }
    void getMd5() && = delete;
    void setMd5(StringData value) & { _md5 = value.toString(); _hasMd5 = true; }

    const mongo::BSONKey& getMinKey() const { return _minKey; }
    void setMinKey(mongo::BSONKey value) & { _minKey = std::move(value); _hasMinKey = true; }

    const mongo::BSONKey& getMaxKey() const { return _maxKey; }
    void setMaxKey(mongo::BSONKey value) & { _maxKey = std::move(value); _hasMaxKey = true; }

    const boost::optional<std::int64_t> getMaxRate() const& { return _maxRate; }
    void getMaxRate() && = delete;
    void setMaxRate(boost::optional<std::int64_t> value) & { _maxRate = std::move(value);  }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    mongo::NamespaceString _nss;
    OplogEntriesEnum _type;
    std::string _md5;
    mongo::BSONKey _minKey;
    mongo::BSONKey _maxKey;
    boost::optional<std::int64_t> _maxRate;
    bool _hasNss : 1;
    bool _hasType : 1;
    bool _hasMd5 : 1;
    bool _hasMinKey : 1;
    bool _hasMaxKey : 1;
};

/**
 * Oplog entry for dbCheck collection metadata
 */
class DbCheckOplogCollection {
public:
    static constexpr auto kIndexesFieldName = "indexes"_sd;
    static constexpr auto kNextFieldName = "next"_sd;
    static constexpr auto kNssFieldName = "dbCheck"_sd;
    static constexpr auto kOptionsFieldName = "options"_sd;
    static constexpr auto kPrevFieldName = "prev"_sd;
    static constexpr auto kTypeFieldName = "type"_sd;
    static constexpr auto kUuidFieldName = "uuid"_sd;

    DbCheckOplogCollection();

    static DbCheckOplogCollection parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const mongo::NamespaceString& getNss() const { return _nss; }
    void setNss(mongo::NamespaceString value) & { _nss = std::move(value); _hasNss = true; }

    const OplogEntriesEnum getType() const { return _type; }
    void setType(OplogEntriesEnum value) & { _type = std::move(value); _hasType = true; }

    const mongo::UUID& getUuid() const { return _uuid; }
    void setUuid(mongo::UUID value) & { _uuid = std::move(value); _hasUuid = true; }

    const boost::optional<mongo::UUID>& getPrev() const& { return _prev; }
    void getPrev() && = delete;
    void setPrev(boost::optional<mongo::UUID> value) & { _prev = std::move(value);  }

    const boost::optional<mongo::UUID>& getNext() const& { return _next; }
    void getNext() && = delete;
    void setNext(boost::optional<mongo::UUID> value) & { _next = std::move(value);  }

    const std::vector<mongo::BSONObj>& getIndexes() const& { return _indexes; }
    void getIndexes() && = delete;
    void setIndexes(std::vector<mongo::BSONObj> value) & { _indexes = std::move(value); _hasIndexes = true; }

    const mongo::BSONObj& getOptions() const { return _options; }
    void setOptions(mongo::BSONObj value) & { _options = std::move(value); _hasOptions = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    mongo::NamespaceString _nss;
    OplogEntriesEnum _type;
    mongo::UUID _uuid;
    boost::optional<mongo::UUID> _prev;
    boost::optional<mongo::UUID> _next;
    std::vector<mongo::BSONObj> _indexes;
    mongo::BSONObj _options;
    bool _hasNss : 1;
    bool _hasType : 1;
    bool _hasUuid : 1;
    bool _hasIndexes : 1;
    bool _hasOptions : 1;
};

}  // namespace mongo
