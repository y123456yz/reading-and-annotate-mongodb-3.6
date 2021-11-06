/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/generic_cursor_gen.h --output build/opt/mongo/db/generic_cursor_gen.cpp src/mongo/db/generic_cursor.idl
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
#include "mongo/crypto/sha256_block.h"
#include "mongo/db/logical_session_id_gen.h"
#include "mongo/db/namespace_string.h"
#include "mongo/idl/idl_parser.h"
#include "mongo/util/net/op_msg.h"
#include "mongo/util/uuid.h"

namespace mongo {

/**
 * A struct representing a cursor in either mongod or mongos
 */
class GenericCursor {
public:
    static constexpr auto kIdFieldName = "id"_sd;
    static constexpr auto kLsidFieldName = "lsid"_sd;
    static constexpr auto kNsFieldName = "ns"_sd;

    GenericCursor();

    static GenericCursor parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    std::int64_t getId() const { return _id; }
    void setId(std::int64_t value) & { _id = std::move(value); _hasId = true; }

    const mongo::NamespaceString& getNs() const { return _ns; }
    void setNs(mongo::NamespaceString value) & { _ns = std::move(value); _hasNs = true; }

    const boost::optional<LogicalSessionId>& getLsid() const& { return _lsid; }
    void getLsid() && = delete;
    void setLsid(boost::optional<LogicalSessionId> value) & { _lsid = std::move(value);  }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    std::int64_t _id;
    mongo::NamespaceString _ns;
    boost::optional<LogicalSessionId> _lsid;
    bool _hasId : 1;
    bool _hasNs : 1;
};

}  // namespace mongo
