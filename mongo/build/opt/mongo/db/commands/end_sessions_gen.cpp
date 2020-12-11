/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/commands/end_sessions_gen.h --output build/opt/mongo/db/commands/end_sessions_gen.cpp src/mongo/db/commands/end_sessions.idl
 */

#include "mongo/db/commands/end_sessions_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

constexpr StringData EndSessionsCmdFromClient::kEndSessionsFieldName;


EndSessionsCmdFromClient::EndSessionsCmdFromClient() : _hasEndSessions(false) {
    // Used for initialization only
}

EndSessionsCmdFromClient EndSessionsCmdFromClient::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    EndSessionsCmdFromClient object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void EndSessionsCmdFromClient::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::set<StringData> usedFields;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        auto push_result = usedFields.insert(fieldName);
        if (MONGO_unlikely(push_result.second == false)) {
            ctxt.throwDuplicateField(fieldName);
        }

        if (fieldName == kEndSessionsFieldName) {
            _hasEndSessions = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kEndSessionsFieldName, &ctxt);
            std::vector<LogicalSessionFromClient> values;

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
                        IDLParserErrorContext tempContext(kEndSessionsFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(LogicalSessionFromClient::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _endSessions = std::move(values);
        }
    }


    if (MONGO_unlikely(usedFields.find(kEndSessionsFieldName) == usedFields.end())) {
        ctxt.throwMissingField(kEndSessionsFieldName);
    }

}


void EndSessionsCmdFromClient::serialize(BSONObjBuilder* builder) const {
    invariant(_hasEndSessions);

    {
        BSONArrayBuilder arrayBuilder(builder->subarrayStart(kEndSessionsFieldName));
        for (const auto& item : _endSessions) {
            BSONObjBuilder subObjBuilder(arrayBuilder.subobjStart());
            item.serialize(&subObjBuilder);
        }
    }

}


BSONObj EndSessionsCmdFromClient::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace mongo
