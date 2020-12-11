/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/ops/single_write_result_gen.h --output build/opt/mongo/db/ops/single_write_result_gen.cpp src/mongo/db/ops/single_write_result.idl
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
 * A document used for storing session transaction states.
 */
class SingleWriteResult {
public:
    static constexpr auto kNFieldName = "n"_sd;
    static constexpr auto kNModifiedFieldName = "nModified"_sd;
    static constexpr auto kUpsertedIdFieldName = "upsertedId"_sd;

    SingleWriteResult();

    static SingleWriteResult parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    /**
     * For insert: number of documents inserted. For update: number of documents that matched the query predicate. For delete: number of documents deleted.
     */
    std::int64_t getN() const { return _n; }
    void setN(std::int64_t value) & { _n = std::move(value); _hasN = true; }

    /**
     * The number of documents modified by an update operation.
     */
    std::int64_t getNModified() const { return _nModified; }
    void setNModified(std::int64_t value) & { _nModified = std::move(value); _hasNModified = true; }

    /**
     * The _id value of the object that was upserted.
     */
    const mongo::BSONObj& getUpsertedId() const { return _upsertedId; }
    void setUpsertedId(mongo::BSONObj value) & { _upsertedId = std::move(value); _hasUpsertedId = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    std::int64_t _n;
    std::int64_t _nModified;
    mongo::BSONObj _upsertedId;
    bool _hasN : 1;
    bool _hasNModified : 1;
    bool _hasUpsertedId : 1;
};

}  // namespace mongo
