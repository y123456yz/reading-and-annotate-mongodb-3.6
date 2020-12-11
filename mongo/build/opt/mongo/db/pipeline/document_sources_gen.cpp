/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/pipeline/document_sources_gen.h --output build/opt/mongo/db/pipeline/document_sources_gen.cpp src/mongo/db/pipeline/document_sources.idl
 */

#include "mongo/db/pipeline/document_sources_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

constexpr StringData ResumeTokenClusterTime::kTimestampFieldName;


ResumeTokenClusterTime::ResumeTokenClusterTime() : _hasTimestamp(false) {
    // Used for initialization only
}

ResumeTokenClusterTime ResumeTokenClusterTime::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    ResumeTokenClusterTime object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void ResumeTokenClusterTime::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<1> usedFields;
    const size_t kTimestampBit = 0;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kTimestampFieldName) {
            if (MONGO_unlikely(usedFields[kTimestampBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kTimestampBit);

            _hasTimestamp = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, bsonTimestamp))) {
                _timestamp = element.timestamp();
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kTimestampBit]) {
            ctxt.throwMissingField(kTimestampFieldName);
        }
    }

}


void ResumeTokenClusterTime::serialize(BSONObjBuilder* builder) const {
    invariant(_hasTimestamp);

    builder->append(kTimestampFieldName, _timestamp);

}


BSONObj ResumeTokenClusterTime::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData DocumentSourceChangeStreamSpec::kFullDocumentFieldName;
constexpr StringData DocumentSourceChangeStreamSpec::kResumeAfterFieldName;
constexpr StringData DocumentSourceChangeStreamSpec::kResumeAfterClusterTimeFieldName;


DocumentSourceChangeStreamSpec::DocumentSourceChangeStreamSpec()  {
    // Used for initialization only
}

DocumentSourceChangeStreamSpec DocumentSourceChangeStreamSpec::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    DocumentSourceChangeStreamSpec object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void DocumentSourceChangeStreamSpec::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<3> usedFields;
    const size_t kResumeAfterBit = 0;
    const size_t kResumeAfterClusterTimeBit = 1;
    const size_t kFullDocumentBit = 2;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kResumeAfterFieldName) {
            if (MONGO_unlikely(usedFields[kResumeAfterBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kResumeAfterBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                const BSONObj localObject = element.Obj();
                _resumeAfter = ResumeToken::parse(localObject);
            }
        }
        else if (fieldName == kResumeAfterClusterTimeFieldName) {
            if (MONGO_unlikely(usedFields[kResumeAfterClusterTimeBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kResumeAfterClusterTimeBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                IDLParserErrorContext tempContext(kResumeAfterClusterTimeFieldName, &ctxt);
                const auto localObject = element.Obj();
                _resumeAfterClusterTime = ResumeTokenClusterTime::parse(tempContext, localObject);
            }
        }
        else if (fieldName == kFullDocumentFieldName) {
            if (MONGO_unlikely(usedFields[kFullDocumentBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kFullDocumentBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _fullDocument = element.str();
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kFullDocumentBit]) {
            _fullDocument = "default";
        }
    }

}


void DocumentSourceChangeStreamSpec::serialize(BSONObjBuilder* builder) const {
    if (_resumeAfter.is_initialized()) {
        const BSONObj localObject = _resumeAfter.get().toBSON();
        builder->append(kResumeAfterFieldName, localObject);
    }

    if (_resumeAfterClusterTime.is_initialized()) {
        BSONObjBuilder subObjBuilder(builder->subobjStart(kResumeAfterClusterTimeFieldName));
        _resumeAfterClusterTime.get().serialize(&subObjBuilder);
    }

    builder->append(kFullDocumentFieldName, _fullDocument);

}


BSONObj DocumentSourceChangeStreamSpec::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData ListSessionsUser::kDbFieldName;
constexpr StringData ListSessionsUser::kUserFieldName;


ListSessionsUser::ListSessionsUser() : _hasUser(false), _hasDb(false) {
    // Used for initialization only
}

ListSessionsUser ListSessionsUser::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    ListSessionsUser object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void ListSessionsUser::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<2> usedFields;
    const size_t kUserBit = 0;
    const size_t kDbBit = 1;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kUserFieldName) {
            if (MONGO_unlikely(usedFields[kUserBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kUserBit);

            _hasUser = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _user = element.str();
            }
        }
        else if (fieldName == kDbFieldName) {
            if (MONGO_unlikely(usedFields[kDbBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kDbBit);

            _hasDb = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _db = element.str();
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kUserBit]) {
            ctxt.throwMissingField(kUserFieldName);
        }
        if (!usedFields[kDbBit]) {
            ctxt.throwMissingField(kDbFieldName);
        }
    }

}


void ListSessionsUser::serialize(BSONObjBuilder* builder) const {
    invariant(_hasUser && _hasDb);

    builder->append(kUserFieldName, _user);

    builder->append(kDbFieldName, _db);

}


BSONObj ListSessionsUser::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData ListSessionsSpec::kAllUsersFieldName;
constexpr StringData ListSessionsSpec::kUsersFieldName;


ListSessionsSpec::ListSessionsSpec()  {
    // Used for initialization only
}

ListSessionsSpec ListSessionsSpec::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    ListSessionsSpec object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void ListSessionsSpec::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<2> usedFields;
    const size_t kAllUsersBit = 0;
    const size_t kUsersBit = 1;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kAllUsersFieldName) {
            if (MONGO_unlikely(usedFields[kAllUsersBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kAllUsersBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Bool))) {
                _allUsers = element.boolean();
            }
        }
        else if (fieldName == kUsersFieldName) {
            if (MONGO_unlikely(usedFields[kUsersBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kUsersBit);

            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kUsersFieldName, &ctxt);
            std::vector<ListSessionsUser> values;

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
                        IDLParserErrorContext tempContext(kUsersFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(ListSessionsUser::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _users = std::move(values);
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kAllUsersBit]) {
            _allUsers = false;
        }
    }

}


void ListSessionsSpec::serialize(BSONObjBuilder* builder) const {
    builder->append(kAllUsersFieldName, _allUsers);

    if (_users.is_initialized()) {
        BSONArrayBuilder arrayBuilder(builder->subarrayStart(kUsersFieldName));
        for (const auto& item : _users.get()) {
            BSONObjBuilder subObjBuilder(arrayBuilder.subobjStart());
            item.serialize(&subObjBuilder);
        }
    }

}


BSONObj ListSessionsSpec::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace mongo
