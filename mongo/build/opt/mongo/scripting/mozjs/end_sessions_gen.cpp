/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/scripting/mozjs/end_sessions_gen.h --output build/opt/mongo/scripting/mozjs/end_sessions_gen.cpp src/mongo/scripting/mozjs/end_sessions.idl
 */

#include "mongo/scripting/mozjs/end_sessions_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

constexpr StringData EndSessions::kEndSessionsFieldName;


EndSessions::EndSessions() : _hasEndSessions(false) {
    // Used for initialization only
}

EndSessions EndSessions::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    EndSessions object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void EndSessions::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<1> usedFields;
    const size_t kEndSessionsBit = 0;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kEndSessionsFieldName) {
            if (MONGO_unlikely(usedFields[kEndSessionsBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kEndSessionsBit);

            _hasEndSessions = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kEndSessionsFieldName, &ctxt);
            std::vector<mongo::BSONObj> values;

            const BSONObj arrayObject = element.Obj();
            for (const auto& arrayElement : arrayObject) {
                const auto arrayFieldName = arrayElement.fieldNameStringData();
                std::uint32_t fieldNumber;

                Status status = parseNumberFromString(arrayFieldName, &fieldNumber);
                if (status.isOK()) {
                    if (fieldNumber != expectedFieldNumber) {
                        arrayCtxt.throwBadArrayFieldNumberSequence(fieldNumber, expectedFieldNumber);
                    }

                    if (arrayCtxt.checkAndAssertType(arrayElement, Object)) {
                        values.emplace_back(arrayElement.Obj());
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _endSessions = std::move(values);
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kEndSessionsBit]) {
            ctxt.throwMissingField(kEndSessionsFieldName);
        }
    }

}


void EndSessions::serialize(BSONObjBuilder* builder) const {
    invariant(_hasEndSessions);

    {
        builder->append(kEndSessionsFieldName, _endSessions);
    }

}


BSONObj EndSessions::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace mongo
