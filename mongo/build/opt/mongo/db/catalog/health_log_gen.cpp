/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/catalog/health_log_gen.h --output build/opt/mongo/db/catalog/health_log_gen.cpp src/mongo/db/catalog/health_log.idl
 */

#include "mongo/db/catalog/health_log_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

/**
 * The severity of a healthlog entry.
 */
namespace  {
constexpr StringData kSeverity_Info = "info"_sd;
constexpr StringData kSeverity_Warning = "warning"_sd;
constexpr StringData kSeverity_Error = "error"_sd;
}  // namespace 

SeverityEnum Severity_parse(const IDLParserErrorContext& ctxt, StringData value) {
    if (value == kSeverity_Info) {
        return SeverityEnum::Info;
    }
    if (value == kSeverity_Warning) {
        return SeverityEnum::Warning;
    }
    if (value == kSeverity_Error) {
        return SeverityEnum::Error;
    }
    ctxt.throwBadEnumValue(value);
}

StringData Severity_serializer(SeverityEnum value) {
    if (value == SeverityEnum::Info) {
        return kSeverity_Info;
    }
    if (value == SeverityEnum::Warning) {
        return kSeverity_Warning;
    }
    if (value == SeverityEnum::Error) {
        return kSeverity_Error;
    }
    MONGO_UNREACHABLE;
    return StringData();
}

/**
 * The scope covered by a healthlog entry.
 */
namespace  {
constexpr StringData kScope_Cluster = "cluster"_sd;
constexpr StringData kScope_Node = "node"_sd;
constexpr StringData kScope_Database = "database"_sd;
constexpr StringData kScope_Collection = "collection"_sd;
constexpr StringData kScope_Index = "index"_sd;
constexpr StringData kScope_Document = "document"_sd;
}  // namespace 

ScopeEnum Scope_parse(const IDLParserErrorContext& ctxt, StringData value) {
    if (value == kScope_Cluster) {
        return ScopeEnum::Cluster;
    }
    if (value == kScope_Node) {
        return ScopeEnum::Node;
    }
    if (value == kScope_Database) {
        return ScopeEnum::Database;
    }
    if (value == kScope_Collection) {
        return ScopeEnum::Collection;
    }
    if (value == kScope_Index) {
        return ScopeEnum::Index;
    }
    if (value == kScope_Document) {
        return ScopeEnum::Document;
    }
    ctxt.throwBadEnumValue(value);
}

StringData Scope_serializer(ScopeEnum value) {
    if (value == ScopeEnum::Cluster) {
        return kScope_Cluster;
    }
    if (value == ScopeEnum::Node) {
        return kScope_Node;
    }
    if (value == ScopeEnum::Database) {
        return kScope_Database;
    }
    if (value == ScopeEnum::Collection) {
        return kScope_Collection;
    }
    if (value == ScopeEnum::Index) {
        return kScope_Index;
    }
    if (value == ScopeEnum::Document) {
        return kScope_Document;
    }
    MONGO_UNREACHABLE;
    return StringData();
}

constexpr StringData HealthLogEntry::kDataFieldName;
constexpr StringData HealthLogEntry::kMsgFieldName;
constexpr StringData HealthLogEntry::kNamespaceFieldName;
constexpr StringData HealthLogEntry::kOperationFieldName;
constexpr StringData HealthLogEntry::kScopeFieldName;
constexpr StringData HealthLogEntry::kSeverityFieldName;
constexpr StringData HealthLogEntry::kTimestampFieldName;


HealthLogEntry::HealthLogEntry() : _hasNamespace(false), _hasTimestamp(false), _hasSeverity(false), _hasMsg(false), _hasScope(false), _hasOperation(false), _hasData(false) {
    // Used for initialization only
}

HealthLogEntry HealthLogEntry::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    HealthLogEntry object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void HealthLogEntry::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<7> usedFields;
    const size_t kNamespaceBit = 0;
    const size_t kTimestampBit = 1;
    const size_t kSeverityBit = 2;
    const size_t kMsgBit = 3;
    const size_t kScopeBit = 4;
    const size_t kOperationBit = 5;
    const size_t kDataBit = 6;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kNamespaceFieldName) {
            if (MONGO_unlikely(usedFields[kNamespaceBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kNamespaceBit);

            _hasNamespace = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _namespace = NamespaceString(element.valueStringData());
            }
        }
        else if (fieldName == kTimestampFieldName) {
            if (MONGO_unlikely(usedFields[kTimestampBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kTimestampBit);

            _hasTimestamp = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Date))) {
                _timestamp = element.date();
            }
        }
        else if (fieldName == kSeverityFieldName) {
            if (MONGO_unlikely(usedFields[kSeverityBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kSeverityBit);

            _hasSeverity = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                IDLParserErrorContext tempContext(kSeverityFieldName, &ctxt);
                _severity = Severity_parse(tempContext, element.valueStringData());
            }
        }
        else if (fieldName == kMsgFieldName) {
            if (MONGO_unlikely(usedFields[kMsgBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kMsgBit);

            _hasMsg = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _msg = element.str();
            }
        }
        else if (fieldName == kScopeFieldName) {
            if (MONGO_unlikely(usedFields[kScopeBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kScopeBit);

            _hasScope = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                IDLParserErrorContext tempContext(kScopeFieldName, &ctxt);
                _scope = Scope_parse(tempContext, element.valueStringData());
            }
        }
        else if (fieldName == kOperationFieldName) {
            if (MONGO_unlikely(usedFields[kOperationBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kOperationBit);

            _hasOperation = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _operation = element.str();
            }
        }
        else if (fieldName == kDataFieldName) {
            if (MONGO_unlikely(usedFields[kDataBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kDataBit);

            _hasData = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                _data = element.Obj();
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kNamespaceBit]) {
            ctxt.throwMissingField(kNamespaceFieldName);
        }
        if (!usedFields[kTimestampBit]) {
            ctxt.throwMissingField(kTimestampFieldName);
        }
        if (!usedFields[kSeverityBit]) {
            ctxt.throwMissingField(kSeverityFieldName);
        }
        if (!usedFields[kMsgBit]) {
            ctxt.throwMissingField(kMsgFieldName);
        }
        if (!usedFields[kScopeBit]) {
            ctxt.throwMissingField(kScopeFieldName);
        }
        if (!usedFields[kOperationBit]) {
            ctxt.throwMissingField(kOperationFieldName);
        }
        if (!usedFields[kDataBit]) {
            ctxt.throwMissingField(kDataFieldName);
        }
    }

}


void HealthLogEntry::serialize(BSONObjBuilder* builder) const {
    invariant(_hasNamespace && _hasTimestamp && _hasSeverity && _hasMsg && _hasScope && _hasOperation && _hasData);

    {
        builder->append(kNamespaceFieldName, _namespace.toString());
    }

    builder->append(kTimestampFieldName, _timestamp);

    {
        builder->append(kSeverityFieldName, Severity_serializer(_severity));
    }

    builder->append(kMsgFieldName, _msg);

    {
        builder->append(kScopeFieldName, Scope_serializer(_scope));
    }

    builder->append(kOperationFieldName, _operation);

    builder->append(kDataFieldName, _data);

}


BSONObj HealthLogEntry::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace mongo
