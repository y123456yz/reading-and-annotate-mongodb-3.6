/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/create_indexes_gen.h --output build/opt/mongo/db/create_indexes_gen.cpp src/mongo/db/create_indexes.idl
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
 * A type representing a spec for a new index
 */
class NewIndexSpec {
public:
    static constexpr auto k2dsphereIndexVersionFieldName = "2dsphereIndexVersion"_sd;
    static constexpr auto kBackgroundFieldName = "background"_sd;
    static constexpr auto kBitsFieldName = "bits"_sd;
    static constexpr auto kBucketSizeFieldName = "bucketSize"_sd;
    static constexpr auto kCollationFieldName = "collation"_sd;
    static constexpr auto kDefault_languageFieldName = "default_language"_sd;
    static constexpr auto kExpireAfterSecondsFieldName = "expireAfterSeconds"_sd;
    static constexpr auto kKeyFieldName = "key"_sd;
    static constexpr auto kLanguage_overrideFieldName = "language_override"_sd;
    static constexpr auto kMaxFieldName = "max"_sd;
    static constexpr auto kMinFieldName = "min"_sd;
    static constexpr auto kNameFieldName = "name"_sd;
    static constexpr auto kPartialFilterExpressionFieldName = "partialFilterExpression"_sd;
    static constexpr auto kSparseFieldName = "sparse"_sd;
    static constexpr auto kStorageEngineFieldName = "storageEngine"_sd;
    static constexpr auto kTextIndexVersionFieldName = "textIndexVersion"_sd;
    static constexpr auto kUniqueFieldName = "unique"_sd;
    static constexpr auto kWeightsFieldName = "weights"_sd;

    NewIndexSpec();

    static NewIndexSpec parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const mongo::BSONObj& getKey() const { return _key; }
    void setKey(mongo::BSONObj value) & { _key = std::move(value); _hasKey = true; }

    const StringData getName() const& { return _name; }
    void getName() && = delete;
    void setName(StringData value) & { _name = value.toString(); _hasName = true; }

    const boost::optional<bool> getBackground() const& { return _background; }
    void getBackground() && = delete;
    void setBackground(boost::optional<bool> value) & { _background = std::move(value);  }

    const boost::optional<bool> getUnique() const& { return _unique; }
    void getUnique() && = delete;
    void setUnique(boost::optional<bool> value) & { _unique = std::move(value);  }

    const boost::optional<mongo::BSONObj>& getPartialFilterExpression() const& { return _partialFilterExpression; }
    void getPartialFilterExpression() && = delete;
    void setPartialFilterExpression(boost::optional<mongo::BSONObj> value) & { _partialFilterExpression = std::move(value);  }

    const boost::optional<bool> getSparse() const& { return _sparse; }
    void getSparse() && = delete;
    void setSparse(boost::optional<bool> value) & { _sparse = std::move(value);  }

    const boost::optional<std::int32_t> getExpireAfterSeconds() const& { return _expireAfterSeconds; }
    void getExpireAfterSeconds() && = delete;
    void setExpireAfterSeconds(boost::optional<std::int32_t> value) & { _expireAfterSeconds = std::move(value);  }

    const boost::optional<mongo::BSONObj>& getStorageEngine() const& { return _storageEngine; }
    void getStorageEngine() && = delete;
    void setStorageEngine(boost::optional<mongo::BSONObj> value) & { _storageEngine = std::move(value);  }

    const boost::optional<mongo::BSONObj>& getWeights() const& { return _weights; }
    void getWeights() && = delete;
    void setWeights(boost::optional<mongo::BSONObj> value) & { _weights = std::move(value);  }

    const boost::optional<StringData> getDefault_language() const& { return boost::optional<StringData>{_default_language}; }
    void getDefault_language() && = delete;
    void setDefault_language(boost::optional<StringData> value) & { if (value.is_initialized()) {
        _default_language = value.get().toString();
    } else {
        _default_language = boost::none;
    }
      }

    const boost::optional<StringData> getLanguage_override() const& { return boost::optional<StringData>{_language_override}; }
    void getLanguage_override() && = delete;
    void setLanguage_override(boost::optional<StringData> value) & { if (value.is_initialized()) {
        _language_override = value.get().toString();
    } else {
        _language_override = boost::none;
    }
      }

    const boost::optional<std::int32_t> getTextIndexVersion() const& { return _textIndexVersion; }
    void getTextIndexVersion() && = delete;
    void setTextIndexVersion(boost::optional<std::int32_t> value) & { _textIndexVersion = std::move(value);  }

    const boost::optional<std::int32_t> get2dsphereIndexVersion() const& { return _2dsphereIndexVersion; }
    void get2dsphereIndexVersion() && = delete;
    void set2dsphereIndexVersion(boost::optional<std::int32_t> value) & { _2dsphereIndexVersion = std::move(value);  }

    const boost::optional<std::int32_t> getBits() const& { return _bits; }
    void getBits() && = delete;
    void setBits(boost::optional<std::int32_t> value) & { _bits = std::move(value);  }

    const boost::optional<double> getMin() const& { return _min; }
    void getMin() && = delete;
    void setMin(boost::optional<double> value) & { _min = std::move(value);  }

    const boost::optional<double> getMax() const& { return _max; }
    void getMax() && = delete;
    void setMax(boost::optional<double> value) & { _max = std::move(value);  }

    const boost::optional<double> getBucketSize() const& { return _bucketSize; }
    void getBucketSize() && = delete;
    void setBucketSize(boost::optional<double> value) & { _bucketSize = std::move(value);  }

    const boost::optional<mongo::BSONObj>& getCollation() const& { return _collation; }
    void getCollation() && = delete;
    void setCollation(boost::optional<mongo::BSONObj> value) & { _collation = std::move(value);  }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    mongo::BSONObj _key;
    std::string _name;
    boost::optional<bool> _background;
    boost::optional<bool> _unique;
    boost::optional<mongo::BSONObj> _partialFilterExpression;
    boost::optional<bool> _sparse;
    boost::optional<std::int32_t> _expireAfterSeconds;
    boost::optional<mongo::BSONObj> _storageEngine;
    boost::optional<mongo::BSONObj> _weights;
    boost::optional<std::string> _default_language;
    boost::optional<std::string> _language_override;
    boost::optional<std::int32_t> _textIndexVersion;
    boost::optional<std::int32_t> _2dsphereIndexVersion;
    boost::optional<std::int32_t> _bits;
    boost::optional<double> _min;
    boost::optional<double> _max;
    boost::optional<double> _bucketSize;
    boost::optional<mongo::BSONObj> _collation;
    bool _hasKey : 1;
    bool _hasName : 1;
};

/**
 * A struct representing a createIndexes command
 */
class CreateIndexesCmd {
public:
    static constexpr auto kCreateIndexesFieldName = "createIndexes"_sd;
    static constexpr auto kIndexesFieldName = "indexes"_sd;

    CreateIndexesCmd();

    static CreateIndexesCmd parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const StringData getCreateIndexes() const& { return _createIndexes; }
    void getCreateIndexes() && = delete;
    void setCreateIndexes(StringData value) & { _createIndexes = value.toString(); _hasCreateIndexes = true; }

    const std::vector<NewIndexSpec>& getIndexes() const& { return _indexes; }
    void getIndexes() && = delete;
    void setIndexes(std::vector<NewIndexSpec> value) & { _indexes = std::move(value); _hasIndexes = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    std::string _createIndexes;
    std::vector<NewIndexSpec> _indexes;
    bool _hasCreateIndexes : 1;
    bool _hasIndexes : 1;
};

}  // namespace mongo
