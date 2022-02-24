/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/refresh_sessions_gen.h --output build/opt/mongo/db/refresh_sessions_gen.cpp src/mongo/db/refresh_sessions.idl
 */

#include "mongo/db/refresh_sessions_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

constexpr StringData RefreshSessionsCmdFromClient::kRefreshSessionsFieldName;


RefreshSessionsCmdFromClient::RefreshSessionsCmdFromClient() : _hasRefreshSessions(false) {
    // Used for initialization only
}

RefreshSessionsCmdFromClient RefreshSessionsCmdFromClient::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    RefreshSessionsCmdFromClient object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void RefreshSessionsCmdFromClient::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::set<StringData> usedFields;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        auto push_result = usedFields.insert(fieldName);
        if (MONGO_unlikely(push_result.second == false)) {
            ctxt.throwDuplicateField(fieldName);
        }

        if (fieldName == kRefreshSessionsFieldName) {
            _hasRefreshSessions = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kRefreshSessionsFieldName, &ctxt);
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
                        IDLParserErrorContext tempContext(kRefreshSessionsFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(LogicalSessionFromClient::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _refreshSessions = std::move(values);
        }
    }


    if (MONGO_unlikely(usedFields.find(kRefreshSessionsFieldName) == usedFields.end())) {
        ctxt.throwMissingField(kRefreshSessionsFieldName);
    }

}


void RefreshSessionsCmdFromClient::serialize(BSONObjBuilder* builder) const {
    invariant(_hasRefreshSessions);

    {
        BSONArrayBuilder arrayBuilder(builder->subarrayStart(kRefreshSessionsFieldName));
        for (const auto& item : _refreshSessions) {
            BSONObjBuilder subObjBuilder(arrayBuilder.subobjStart());
            item.serialize(&subObjBuilder);
        }
    }

}


BSONObj RefreshSessionsCmdFromClient::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData RefreshSessionsCmdFromClusterMember::kRefreshSessionsInternalFieldName;


RefreshSessionsCmdFromClusterMember::RefreshSessionsCmdFromClusterMember() : _hasRefreshSessionsInternal(false) {
    // Used for initialization only
}

RefreshSessionsCmdFromClusterMember RefreshSessionsCmdFromClusterMember::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    RefreshSessionsCmdFromClusterMember object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void RefreshSessionsCmdFromClusterMember::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::set<StringData> usedFields;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        auto push_result = usedFields.insert(fieldName);
        if (MONGO_unlikely(push_result.second == false)) {
            ctxt.throwDuplicateField(fieldName);
        }

        if (fieldName == kRefreshSessionsInternalFieldName) {
            _hasRefreshSessionsInternal = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kRefreshSessionsInternalFieldName, &ctxt);
            std::vector<LogicalSessionRecord> values;

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
                        IDLParserErrorContext tempContext(kRefreshSessionsInternalFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(LogicalSessionRecord::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _refreshSessionsInternal = std::move(values);
        }
    }


    if (MONGO_unlikely(usedFields.find(kRefreshSessionsInternalFieldName) == usedFields.end())) {
        ctxt.throwMissingField(kRefreshSessionsInternalFieldName);
    }

}


void RefreshSessionsCmdFromClusterMember::serialize(BSONObjBuilder* builder) const {
    invariant(_hasRefreshSessionsInternal);

    {
        BSONArrayBuilder arrayBuilder(builder->subarrayStart(kRefreshSessionsInternalFieldName));
        for (const auto& item : _refreshSessionsInternal) {
            BSONObjBuilder subObjBuilder(arrayBuilder.subobjStart());
            item.serialize(&subObjBuilder);
        }
    }

}


BSONObj RefreshSessionsCmdFromClusterMember::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace mongo
