/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/s/request_types/move_primary_gen.h --output build/opt/mongo/s/request_types/move_primary_gen.cpp src/mongo/s/request_types/move_primary.idl
 */

#include "mongo/s/request_types/move_primary_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

constexpr StringData MovePrimary::kMovePrimaryFieldName;
constexpr StringData MovePrimary::kMoveprimaryFieldName;
constexpr StringData MovePrimary::kToFieldName;


MovePrimary::MovePrimary() : _hasTo(false) {
    // Used for initialization only
}

MovePrimary MovePrimary::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    MovePrimary object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void MovePrimary::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::set<StringData> usedFields;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        auto push_result = usedFields.insert(fieldName);
        if (MONGO_unlikely(push_result.second == false)) {
            ctxt.throwDuplicateField(fieldName);
        }

        if (fieldName == kMovePrimaryFieldName) {
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _movePrimary = NamespaceString(element.valueStringData());
            }
        }
        else if (fieldName == kMoveprimaryFieldName) {
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _moveprimary = NamespaceString(element.valueStringData());
            }
        }
        else if (fieldName == kToFieldName) {
            _hasTo = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _to = element.str();
            }
        }
    }


    if (MONGO_unlikely(usedFields.find(kToFieldName) == usedFields.end())) {
        ctxt.throwMissingField(kToFieldName);
    }

}


void MovePrimary::serialize(BSONObjBuilder* builder) const {
    invariant(_hasTo);

    if (_movePrimary.is_initialized()) {
        builder->append(kMovePrimaryFieldName, _movePrimary.get().toString());
    }

    if (_moveprimary.is_initialized()) {
        builder->append(kMoveprimaryFieldName, _moveprimary.get().toString());
    }

    builder->append(kToFieldName, _to);

}


BSONObj MovePrimary::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData ConfigsvrMovePrimary::k_configsvrMovePrimaryFieldName;
constexpr StringData ConfigsvrMovePrimary::kToFieldName;


ConfigsvrMovePrimary::ConfigsvrMovePrimary() : _has_configsvrMovePrimary(false), _hasTo(false) {
    // Used for initialization only
}

ConfigsvrMovePrimary ConfigsvrMovePrimary::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    ConfigsvrMovePrimary object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void ConfigsvrMovePrimary::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::set<StringData> usedFields;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        auto push_result = usedFields.insert(fieldName);
        if (MONGO_unlikely(push_result.second == false)) {
            ctxt.throwDuplicateField(fieldName);
        }

        if (fieldName == k_configsvrMovePrimaryFieldName) {
            _has_configsvrMovePrimary = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                __configsvrMovePrimary = NamespaceString(element.valueStringData());
            }
        }
        else if (fieldName == kToFieldName) {
            _hasTo = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _to = element.str();
            }
        }
    }


    if (MONGO_unlikely(usedFields.find(k_configsvrMovePrimaryFieldName) == usedFields.end())) {
        ctxt.throwMissingField(k_configsvrMovePrimaryFieldName);
    }
    if (MONGO_unlikely(usedFields.find(kToFieldName) == usedFields.end())) {
        ctxt.throwMissingField(kToFieldName);
    }

}


void ConfigsvrMovePrimary::serialize(BSONObjBuilder* builder) const {
    invariant(_has_configsvrMovePrimary && _hasTo);

    {
        builder->append(k_configsvrMovePrimaryFieldName, __configsvrMovePrimary.toString());
    }

    builder->append(kToFieldName, _to);

}


BSONObj ConfigsvrMovePrimary::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace mongo
