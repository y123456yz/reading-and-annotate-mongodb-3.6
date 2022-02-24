/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/generic_cursor_gen.h --output build/opt/mongo/db/generic_cursor_gen.cpp src/mongo/db/generic_cursor.idl
 */

#include "mongo/db/generic_cursor_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

constexpr StringData GenericCursor::kIdFieldName;
constexpr StringData GenericCursor::kLsidFieldName;
constexpr StringData GenericCursor::kNsFieldName;


GenericCursor::GenericCursor() : _id(-1), _hasId(false), _hasNs(false) {
    // Used for initialization only
}

GenericCursor GenericCursor::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    GenericCursor object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void GenericCursor::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<3> usedFields;
    const size_t kIdBit = 0;
    const size_t kNsBit = 1;
    const size_t kLsidBit = 2;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kIdFieldName) {
            if (MONGO_unlikely(usedFields[kIdBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kIdBit);

            _hasId = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberLong))) {
                _id = element._numberLong();
            }
        }
        else if (fieldName == kNsFieldName) {
            if (MONGO_unlikely(usedFields[kNsBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kNsBit);

            _hasNs = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _ns = NamespaceString(element.valueStringData());
            }
        }
        else if (fieldName == kLsidFieldName) {
            if (MONGO_unlikely(usedFields[kLsidBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kLsidBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                IDLParserErrorContext tempContext(kLsidFieldName, &ctxt);
                const auto localObject = element.Obj();
                _lsid = LogicalSessionId::parse(tempContext, localObject);
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kIdBit]) {
            ctxt.throwMissingField(kIdFieldName);
        }
        if (!usedFields[kNsBit]) {
            ctxt.throwMissingField(kNsFieldName);
        }
    }

}


void GenericCursor::serialize(BSONObjBuilder* builder) const {
    invariant(_hasId && _hasNs);

    builder->append(kIdFieldName, _id);

    {
        builder->append(kNsFieldName, _ns.toString());
    }

    if (_lsid.is_initialized()) {
        BSONObjBuilder subObjBuilder(builder->subobjStart(kLsidFieldName));
        _lsid.get().serialize(&subObjBuilder);
    }

}


BSONObj GenericCursor::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace mongo
