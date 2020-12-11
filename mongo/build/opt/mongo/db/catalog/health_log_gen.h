/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/catalog/health_log_gen.h --output build/opt/mongo/db/catalog/health_log_gen.cpp src/mongo/db/catalog/health_log.idl
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
 * The severity of a healthlog entry.
 */
enum class SeverityEnum : std::int32_t {
    Info ,
    Warning ,
    Error ,
};

SeverityEnum Severity_parse(const IDLParserErrorContext& ctxt, StringData value);
StringData Severity_serializer(SeverityEnum value);

/**
 * The scope covered by a healthlog entry.
 */
enum class ScopeEnum : std::int32_t {
    Cluster ,
    Node ,
    Database ,
    Collection ,
    Index ,
    Document ,
};

ScopeEnum Scope_parse(const IDLParserErrorContext& ctxt, StringData value);
StringData Scope_serializer(ScopeEnum value);

/**
 * An entry in system.local.healthlog.
 */
class HealthLogEntry {
public:
    static constexpr auto kDataFieldName = "data"_sd;
    static constexpr auto kMsgFieldName = "msg"_sd;
    static constexpr auto kNamespaceFieldName = "namespace"_sd;
    static constexpr auto kOperationFieldName = "operation"_sd;
    static constexpr auto kScopeFieldName = "scope"_sd;
    static constexpr auto kSeverityFieldName = "severity"_sd;
    static constexpr auto kTimestampFieldName = "timestamp"_sd;

    HealthLogEntry();

    static HealthLogEntry parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const mongo::NamespaceString& getNamespace() const { return _namespace; }
    void setNamespace(mongo::NamespaceString value) & { _namespace = std::move(value); _hasNamespace = true; }

    const mongo::Date_t& getTimestamp() const { return _timestamp; }
    void setTimestamp(mongo::Date_t value) & { _timestamp = std::move(value); _hasTimestamp = true; }

    const SeverityEnum getSeverity() const { return _severity; }
    void setSeverity(SeverityEnum value) & { _severity = std::move(value); _hasSeverity = true; }

    const StringData getMsg() const& { return _msg; }
    void getMsg() && = delete;
    void setMsg(StringData value) & { _msg = value.toString(); _hasMsg = true; }

    const ScopeEnum getScope() const { return _scope; }
    void setScope(ScopeEnum value) & { _scope = std::move(value); _hasScope = true; }

    const StringData getOperation() const& { return _operation; }
    void getOperation() && = delete;
    void setOperation(StringData value) & { _operation = value.toString(); _hasOperation = true; }

    const mongo::BSONObj& getData() const { return _data; }
    void setData(mongo::BSONObj value) & { _data = std::move(value); _hasData = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    mongo::NamespaceString _namespace;
    mongo::Date_t _timestamp;
    SeverityEnum _severity;
    std::string _msg;
    ScopeEnum _scope;
    std::string _operation;
    mongo::BSONObj _data;
    bool _hasNamespace : 1;
    bool _hasTimestamp : 1;
    bool _hasSeverity : 1;
    bool _hasMsg : 1;
    bool _hasScope : 1;
    bool _hasOperation : 1;
    bool _hasData : 1;
};

}  // namespace mongo
