/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/scripting/mozjs/end_sessions_gen.h --output build/opt/mongo/scripting/mozjs/end_sessions_gen.cpp src/mongo/scripting/mozjs/end_sessions.idl
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
 * A struct representing an endSessions command
 */
class EndSessions {
public:
    static constexpr auto kEndSessionsFieldName = "endSessions"_sd;

    EndSessions();

    static EndSessions parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const std::vector<mongo::BSONObj>& getEndSessions() const& { return _endSessions; }
    void getEndSessions() && = delete;
    void setEndSessions(std::vector<mongo::BSONObj> value) & { _endSessions = std::move(value); _hasEndSessions = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    std::vector<mongo::BSONObj> _endSessions;
    bool _hasEndSessions : 1;
};

}  // namespace mongo
