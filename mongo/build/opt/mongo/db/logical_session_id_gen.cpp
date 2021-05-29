/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/logical_session_id_gen.h --output build/opt/mongo/db/logical_session_id_gen.cpp src/mongo/db/logical_session_id.idl
 */

#include "mongo/db/logical_session_id_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

constexpr StringData LogicalSessionId::kIdFieldName;
constexpr StringData LogicalSessionId::kUidFieldName;


LogicalSessionId::LogicalSessionId() : _hasId(false), _hasUid(false) {
    // Used for initialization only
}

LogicalSessionId LogicalSessionId::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    LogicalSessionId object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void LogicalSessionId::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<2> usedFields;
    const size_t kIdBit = 0;
    const size_t kUidBit = 1;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kIdFieldName) {
            if (MONGO_unlikely(usedFields[kIdBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kIdBit);

            _hasId = true;
            if (MONGO_likely(ctxt.checkAndAssertBinDataType(element, newUUID))) {
                _id = UUID(element.uuid());
            }
        }
        else if (fieldName == kUidFieldName) {
            if (MONGO_unlikely(usedFields[kUidBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kUidBit);

            _hasUid = true;
            if (MONGO_likely(ctxt.checkAndAssertBinDataType(element, BinDataGeneral))) {
                _uid = SHA256Block::fromBinData(element._binDataVector());
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
        if (!usedFields[kUidBit]) {
            ctxt.throwMissingField(kUidFieldName);
        }
    }

}


void LogicalSessionId::serialize(BSONObjBuilder* builder) const {
    invariant(_hasId && _hasUid);

    {
        ConstDataRange tempCDR = _id.toCDR();
        builder->append(kIdFieldName, BSONBinData(tempCDR.data(), tempCDR.length(), newUUID));
    }

    {
        ConstDataRange tempCDR = _uid.toCDR();
        builder->append(kUidFieldName, BSONBinData(tempCDR.data(), tempCDR.length(), BinDataGeneral));
    }

}


BSONObj LogicalSessionId::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData LogicalSessionIdToClient::kIdFieldName;


LogicalSessionIdToClient::LogicalSessionIdToClient() : _hasId(false) {
    // Used for initialization only
}

LogicalSessionIdToClient LogicalSessionIdToClient::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    LogicalSessionIdToClient object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void LogicalSessionIdToClient::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<1> usedFields;
    const size_t kIdBit = 0;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kIdFieldName) {
            if (MONGO_unlikely(usedFields[kIdBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kIdBit);

            _hasId = true;
            if (MONGO_likely(ctxt.checkAndAssertBinDataType(element, newUUID))) {
                _id = UUID(element.uuid());
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
    }

}


void LogicalSessionIdToClient::serialize(BSONObjBuilder* builder) const {
    invariant(_hasId);

    {
        ConstDataRange tempCDR = _id.toCDR();
        builder->append(kIdFieldName, BSONBinData(tempCDR.data(), tempCDR.length(), newUUID));
    }

}


BSONObj LogicalSessionIdToClient::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData LogicalSessionToClient::kIdFieldName;
constexpr StringData LogicalSessionToClient::kTimeoutMinutesFieldName;


LogicalSessionToClient::LogicalSessionToClient() : _timeoutMinutes(-1), _hasId(false), _hasTimeoutMinutes(false) {
    // Used for initialization only
}

LogicalSessionToClient LogicalSessionToClient::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    LogicalSessionToClient object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void LogicalSessionToClient::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<2> usedFields;
    const size_t kIdBit = 0;
    const size_t kTimeoutMinutesBit = 1;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kIdFieldName) {
            if (MONGO_unlikely(usedFields[kIdBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kIdBit);

            _hasId = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                IDLParserErrorContext tempContext(kIdFieldName, &ctxt);
                const auto localObject = element.Obj();
                _id = LogicalSessionIdToClient::parse(tempContext, localObject);
            }
        }
        else if (fieldName == kTimeoutMinutesFieldName) {
            if (MONGO_unlikely(usedFields[kTimeoutMinutesBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kTimeoutMinutesBit);

            _hasTimeoutMinutes = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _timeoutMinutes = element._numberInt();
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
        if (!usedFields[kTimeoutMinutesBit]) {
            ctxt.throwMissingField(kTimeoutMinutesFieldName);
        }
    }

}


void LogicalSessionToClient::serialize(BSONObjBuilder* builder) const {
    invariant(_hasId && _hasTimeoutMinutes);

    {
        BSONObjBuilder subObjBuilder(builder->subobjStart(kIdFieldName));
        _id.serialize(&subObjBuilder);
    }

    builder->append(kTimeoutMinutesFieldName, _timeoutMinutes);

}


BSONObj LogicalSessionToClient::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData LogicalSessionRecord::kIdFieldName;
constexpr StringData LogicalSessionRecord::kLastUseFieldName;
constexpr StringData LogicalSessionRecord::kUserFieldName;


LogicalSessionRecord::LogicalSessionRecord() : _hasId(false), _hasLastUse(false) {
    // Used for initialization only
}

LogicalSessionRecord LogicalSessionRecord::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    LogicalSessionRecord object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void LogicalSessionRecord::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<3> usedFields;
    const size_t kIdBit = 0;
    const size_t kLastUseBit = 1;
    const size_t kUserBit = 2;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kIdFieldName) {
            if (MONGO_unlikely(usedFields[kIdBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kIdBit);

            _hasId = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                IDLParserErrorContext tempContext(kIdFieldName, &ctxt);
                const auto localObject = element.Obj();
                _id = LogicalSessionId::parse(tempContext, localObject);
            }
        }
        else if (fieldName == kLastUseFieldName) {
            if (MONGO_unlikely(usedFields[kLastUseBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kLastUseBit);

            _hasLastUse = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Date))) {
                _lastUse = element.date();
            }
        }
        else if (fieldName == kUserFieldName) {
            if (MONGO_unlikely(usedFields[kUserBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kUserBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _user = element.str();
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
        if (!usedFields[kLastUseBit]) {
            ctxt.throwMissingField(kLastUseFieldName);
        }
    }

}


void LogicalSessionRecord::serialize(BSONObjBuilder* builder) const {
    invariant(_hasId && _hasLastUse);

    {
        BSONObjBuilder subObjBuilder(builder->subobjStart(kIdFieldName));
        _id.serialize(&subObjBuilder);
    }

    builder->append(kLastUseFieldName, _lastUse);

    if (_user.is_initialized()) {
        builder->append(kUserFieldName, _user.get());
    }

}


BSONObj LogicalSessionRecord::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData LogicalSessionFromClient::kIdFieldName;
constexpr StringData LogicalSessionFromClient::kUidFieldName;


LogicalSessionFromClient::LogicalSessionFromClient() : _hasId(false) {
    // Used for initialization only
}

LogicalSessionFromClient LogicalSessionFromClient::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    LogicalSessionFromClient object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void LogicalSessionFromClient::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<2> usedFields;
    const size_t kIdBit = 0;
    const size_t kUidBit = 1;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kIdFieldName) {
            if (MONGO_unlikely(usedFields[kIdBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kIdBit);

            _hasId = true;
            if (MONGO_likely(ctxt.checkAndAssertBinDataType(element, newUUID))) {
                _id = UUID(element.uuid());
            }
        }
        else if (fieldName == kUidFieldName) {
            if (MONGO_unlikely(usedFields[kUidBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kUidBit);

            if (MONGO_likely(ctxt.checkAndAssertBinDataType(element, BinDataGeneral))) {
                _uid = SHA256Block::fromBinData(element._binDataVector());
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
    }

}


void LogicalSessionFromClient::serialize(BSONObjBuilder* builder) const {
    invariant(_hasId);

    {
        ConstDataRange tempCDR = _id.toCDR();
        builder->append(kIdFieldName, BSONBinData(tempCDR.data(), tempCDR.length(), newUUID));
    }

    if (_uid.is_initialized()) {
        ConstDataRange tempCDR = _uid.get().toCDR();
        builder->append(kUidFieldName, BSONBinData(tempCDR.data(), tempCDR.length(), BinDataGeneral));
    }

}


BSONObj LogicalSessionFromClient::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData OperationSessionInfo::kSessionIdFieldName;
constexpr StringData OperationSessionInfo::kTxnNumberFieldName;


OperationSessionInfo::OperationSessionInfo()  {
    // Used for initialization only
}

OperationSessionInfo OperationSessionInfo::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    OperationSessionInfo object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void OperationSessionInfo::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::set<StringData> usedFields;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        auto push_result = usedFields.insert(fieldName);
        if (MONGO_unlikely(push_result.second == false)) {
            ctxt.throwDuplicateField(fieldName);
        }

        if (fieldName == kSessionIdFieldName) {
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                IDLParserErrorContext tempContext(kSessionIdFieldName, &ctxt);
                const auto localObject = element.Obj();
                _sessionId = LogicalSessionId::parse(tempContext, localObject);
            }
        }
        else if (fieldName == kTxnNumberFieldName) {
            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberLong))) {
                _txnNumber = element._numberLong();
            }
        }
    }



}


void OperationSessionInfo::serialize(BSONObjBuilder* builder) const {
    if (_sessionId.is_initialized()) {
        BSONObjBuilder subObjBuilder(builder->subobjStart(kSessionIdFieldName));
        _sessionId.get().serialize(&subObjBuilder);
    }

    if (_txnNumber.is_initialized()) {
        builder->append(kTxnNumberFieldName, _txnNumber.get());
    }

}


BSONObj OperationSessionInfo::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData OperationSessionInfoFromClient::kSessionIdFieldName;
constexpr StringData OperationSessionInfoFromClient::kTxnNumberFieldName;


OperationSessionInfoFromClient::OperationSessionInfoFromClient()  {
    // Used for initialization only
}

//initializeOperationSessionInfoµ÷ÓÃ
OperationSessionInfoFromClient OperationSessionInfoFromClient::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    OperationSessionInfoFromClient object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void OperationSessionInfoFromClient::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::set<StringData> usedFields;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        auto push_result = usedFields.insert(fieldName);
        if (MONGO_unlikely(push_result.second == false)) {
            ctxt.throwDuplicateField(fieldName);
        }

        if (fieldName == kSessionIdFieldName) {
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                IDLParserErrorContext tempContext(kSessionIdFieldName, &ctxt);
                const auto localObject = element.Obj();
                _sessionId = LogicalSessionFromClient::parse(tempContext, localObject);
            }
        }
        else if (fieldName == kTxnNumberFieldName) {
            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberLong))) {
                _txnNumber = element._numberLong();
            }
        }
    }



}


void OperationSessionInfoFromClient::serialize(BSONObjBuilder* builder) const {
    if (_sessionId.is_initialized()) {
        BSONObjBuilder subObjBuilder(builder->subobjStart(kSessionIdFieldName));
        _sessionId.get().serialize(&subObjBuilder);
    }

    if (_txnNumber.is_initialized()) {
        builder->append(kTxnNumberFieldName, _txnNumber.get());
    }

}


BSONObj OperationSessionInfoFromClient::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData SessionsCollectionFetchResultIndividualResult::k_idFieldName;


SessionsCollectionFetchResultIndividualResult::SessionsCollectionFetchResultIndividualResult() : _has_id(false) {
    // Used for initialization only
}

SessionsCollectionFetchResultIndividualResult SessionsCollectionFetchResultIndividualResult::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    SessionsCollectionFetchResultIndividualResult object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void SessionsCollectionFetchResultIndividualResult::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<1> usedFields;
    const size_t k_idBit = 0;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == k_idFieldName) {
            if (MONGO_unlikely(usedFields[k_idBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(k_idBit);

            _has_id = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                IDLParserErrorContext tempContext(k_idFieldName, &ctxt);
                const auto localObject = element.Obj();
                __id = LogicalSessionId::parse(tempContext, localObject);
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[k_idBit]) {
            ctxt.throwMissingField(k_idFieldName);
        }
    }

}


void SessionsCollectionFetchResultIndividualResult::serialize(BSONObjBuilder* builder) const {
    invariant(_has_id);

    {
        BSONObjBuilder subObjBuilder(builder->subobjStart(k_idFieldName));
        __id.serialize(&subObjBuilder);
    }

}


BSONObj SessionsCollectionFetchResultIndividualResult::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData SessionsCollectionFetchResultCursor::kFirstBatchFieldName;


SessionsCollectionFetchResultCursor::SessionsCollectionFetchResultCursor() : _hasFirstBatch(false) {
    // Used for initialization only
}

SessionsCollectionFetchResultCursor SessionsCollectionFetchResultCursor::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    SessionsCollectionFetchResultCursor object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void SessionsCollectionFetchResultCursor::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::set<StringData> usedFields;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        auto push_result = usedFields.insert(fieldName);
        if (MONGO_unlikely(push_result.second == false)) {
            ctxt.throwDuplicateField(fieldName);
        }

        if (fieldName == kFirstBatchFieldName) {
            _hasFirstBatch = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kFirstBatchFieldName, &ctxt);
            std::vector<SessionsCollectionFetchResultIndividualResult> values;

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
                        IDLParserErrorContext tempContext(kFirstBatchFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(SessionsCollectionFetchResultIndividualResult::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _firstBatch = std::move(values);
        }
    }


    if (MONGO_unlikely(usedFields.find(kFirstBatchFieldName) == usedFields.end())) {
        ctxt.throwMissingField(kFirstBatchFieldName);
    }

}


void SessionsCollectionFetchResultCursor::serialize(BSONObjBuilder* builder) const {
    invariant(_hasFirstBatch);

    {
        BSONArrayBuilder arrayBuilder(builder->subarrayStart(kFirstBatchFieldName));
        for (const auto& item : _firstBatch) {
            BSONObjBuilder subObjBuilder(arrayBuilder.subobjStart());
            item.serialize(&subObjBuilder);
        }
    }

}


BSONObj SessionsCollectionFetchResultCursor::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData SessionsCollectionFetchResult::kCursorFieldName;


SessionsCollectionFetchResult::SessionsCollectionFetchResult() : _hasCursor(false) {
    // Used for initialization only
}

SessionsCollectionFetchResult SessionsCollectionFetchResult::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    SessionsCollectionFetchResult object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void SessionsCollectionFetchResult::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::set<StringData> usedFields;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        auto push_result = usedFields.insert(fieldName);
        if (MONGO_unlikely(push_result.second == false)) {
            ctxt.throwDuplicateField(fieldName);
        }

        if (fieldName == kCursorFieldName) {
            _hasCursor = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                IDLParserErrorContext tempContext(kCursorFieldName, &ctxt);
                const auto localObject = element.Obj();
                _cursor = SessionsCollectionFetchResultCursor::parse(tempContext, localObject);
            }
        }
    }


    if (MONGO_unlikely(usedFields.find(kCursorFieldName) == usedFields.end())) {
        ctxt.throwMissingField(kCursorFieldName);
    }

}


void SessionsCollectionFetchResult::serialize(BSONObjBuilder* builder) const {
    invariant(_hasCursor);

    {
        BSONObjBuilder subObjBuilder(builder->subobjStart(kCursorFieldName));
        _cursor.serialize(&subObjBuilder);
    }

}


BSONObj SessionsCollectionFetchResult::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData SessionsCollectionFetchRequestFilterId::kInFieldName;


SessionsCollectionFetchRequestFilterId::SessionsCollectionFetchRequestFilterId() : _hasIn(false) {
    // Used for initialization only
}

SessionsCollectionFetchRequestFilterId SessionsCollectionFetchRequestFilterId::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    SessionsCollectionFetchRequestFilterId object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void SessionsCollectionFetchRequestFilterId::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<1> usedFields;
    const size_t kInBit = 0;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kInFieldName) {
            if (MONGO_unlikely(usedFields[kInBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kInBit);

            _hasIn = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kInFieldName, &ctxt);
            std::vector<LogicalSessionId> values;

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
                        IDLParserErrorContext tempContext(kInFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(LogicalSessionId::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _in = std::move(values);
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kInBit]) {
            ctxt.throwMissingField(kInFieldName);
        }
    }

}


void SessionsCollectionFetchRequestFilterId::serialize(BSONObjBuilder* builder) const {
    invariant(_hasIn);

    {
        BSONArrayBuilder arrayBuilder(builder->subarrayStart(kInFieldName));
        for (const auto& item : _in) {
            BSONObjBuilder subObjBuilder(arrayBuilder.subobjStart());
            item.serialize(&subObjBuilder);
        }
    }

}


BSONObj SessionsCollectionFetchRequestFilterId::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData SessionsCollectionFetchRequestFilter::k_idFieldName;


SessionsCollectionFetchRequestFilter::SessionsCollectionFetchRequestFilter() : _has_id(false) {
    // Used for initialization only
}

SessionsCollectionFetchRequestFilter SessionsCollectionFetchRequestFilter::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    SessionsCollectionFetchRequestFilter object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void SessionsCollectionFetchRequestFilter::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<1> usedFields;
    const size_t k_idBit = 0;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == k_idFieldName) {
            if (MONGO_unlikely(usedFields[k_idBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(k_idBit);

            _has_id = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                IDLParserErrorContext tempContext(k_idFieldName, &ctxt);
                const auto localObject = element.Obj();
                __id = SessionsCollectionFetchRequestFilterId::parse(tempContext, localObject);
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[k_idBit]) {
            ctxt.throwMissingField(k_idFieldName);
        }
    }

}


void SessionsCollectionFetchRequestFilter::serialize(BSONObjBuilder* builder) const {
    invariant(_has_id);

    {
        BSONObjBuilder subObjBuilder(builder->subobjStart(k_idFieldName));
        __id.serialize(&subObjBuilder);
    }

}


BSONObj SessionsCollectionFetchRequestFilter::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData SessionsCollectionFetchRequestProjection::k_idFieldName;


SessionsCollectionFetchRequestProjection::SessionsCollectionFetchRequestProjection() : __id(-1), _has_id(false) {
    // Used for initialization only
}

SessionsCollectionFetchRequestProjection SessionsCollectionFetchRequestProjection::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    SessionsCollectionFetchRequestProjection object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void SessionsCollectionFetchRequestProjection::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<1> usedFields;
    const size_t k_idBit = 0;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == k_idFieldName) {
            if (MONGO_unlikely(usedFields[k_idBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(k_idBit);

            _has_id = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                __id = element._numberInt();
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[k_idBit]) {
            ctxt.throwMissingField(k_idFieldName);
        }
    }

}


void SessionsCollectionFetchRequestProjection::serialize(BSONObjBuilder* builder) const {
    invariant(_has_id);

    builder->append(k_idFieldName, __id);

}


BSONObj SessionsCollectionFetchRequestProjection::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData SessionsCollectionFetchRequest::kAllowPartialResultsFieldName;
constexpr StringData SessionsCollectionFetchRequest::kBatchSizeFieldName;
constexpr StringData SessionsCollectionFetchRequest::kFilterFieldName;
constexpr StringData SessionsCollectionFetchRequest::kFindFieldName;
constexpr StringData SessionsCollectionFetchRequest::kLimitFieldName;
constexpr StringData SessionsCollectionFetchRequest::kProjectionFieldName;
constexpr StringData SessionsCollectionFetchRequest::kSingleBatchFieldName;


SessionsCollectionFetchRequest::SessionsCollectionFetchRequest() : _batchSize(-1), _singleBatch(false), _allowPartialResults(false), _limit(-1), _hasFind(false), _hasFilter(false), _hasProjection(false), _hasBatchSize(false), _hasSingleBatch(false), _hasAllowPartialResults(false), _hasLimit(false) {
    // Used for initialization only
}

SessionsCollectionFetchRequest SessionsCollectionFetchRequest::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    SessionsCollectionFetchRequest object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void SessionsCollectionFetchRequest::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<7> usedFields;
    const size_t kFindBit = 0;
    const size_t kFilterBit = 1;
    const size_t kProjectionBit = 2;
    const size_t kBatchSizeBit = 3;
    const size_t kSingleBatchBit = 4;
    const size_t kAllowPartialResultsBit = 5;
    const size_t kLimitBit = 6;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kFindFieldName) {
            if (MONGO_unlikely(usedFields[kFindBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kFindBit);

            _hasFind = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _find = element.str();
            }
        }
        else if (fieldName == kFilterFieldName) {
            if (MONGO_unlikely(usedFields[kFilterBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kFilterBit);

            _hasFilter = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                IDLParserErrorContext tempContext(kFilterFieldName, &ctxt);
                const auto localObject = element.Obj();
                _filter = SessionsCollectionFetchRequestFilter::parse(tempContext, localObject);
            }
        }
        else if (fieldName == kProjectionFieldName) {
            if (MONGO_unlikely(usedFields[kProjectionBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kProjectionBit);

            _hasProjection = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                IDLParserErrorContext tempContext(kProjectionFieldName, &ctxt);
                const auto localObject = element.Obj();
                _projection = SessionsCollectionFetchRequestProjection::parse(tempContext, localObject);
            }
        }
        else if (fieldName == kBatchSizeFieldName) {
            if (MONGO_unlikely(usedFields[kBatchSizeBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kBatchSizeBit);

            _hasBatchSize = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _batchSize = element._numberInt();
            }
        }
        else if (fieldName == kSingleBatchFieldName) {
            if (MONGO_unlikely(usedFields[kSingleBatchBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kSingleBatchBit);

            _hasSingleBatch = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Bool))) {
                _singleBatch = element.boolean();
            }
        }
        else if (fieldName == kAllowPartialResultsFieldName) {
            if (MONGO_unlikely(usedFields[kAllowPartialResultsBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kAllowPartialResultsBit);

            _hasAllowPartialResults = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Bool))) {
                _allowPartialResults = element.boolean();
            }
        }
        else if (fieldName == kLimitFieldName) {
            if (MONGO_unlikely(usedFields[kLimitBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kLimitBit);

            _hasLimit = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _limit = element._numberInt();
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kFindBit]) {
            ctxt.throwMissingField(kFindFieldName);
        }
        if (!usedFields[kFilterBit]) {
            ctxt.throwMissingField(kFilterFieldName);
        }
        if (!usedFields[kProjectionBit]) {
            ctxt.throwMissingField(kProjectionFieldName);
        }
        if (!usedFields[kBatchSizeBit]) {
            ctxt.throwMissingField(kBatchSizeFieldName);
        }
        if (!usedFields[kSingleBatchBit]) {
            ctxt.throwMissingField(kSingleBatchFieldName);
        }
        if (!usedFields[kAllowPartialResultsBit]) {
            ctxt.throwMissingField(kAllowPartialResultsFieldName);
        }
        if (!usedFields[kLimitBit]) {
            ctxt.throwMissingField(kLimitFieldName);
        }
    }

}


void SessionsCollectionFetchRequest::serialize(BSONObjBuilder* builder) const {
    invariant(_hasFind && _hasFilter && _hasProjection && _hasBatchSize && _hasSingleBatch && _hasAllowPartialResults && _hasLimit);

    builder->append(kFindFieldName, _find);

    {
        BSONObjBuilder subObjBuilder(builder->subobjStart(kFilterFieldName));
        _filter.serialize(&subObjBuilder);
    }

    {
        BSONObjBuilder subObjBuilder(builder->subobjStart(kProjectionFieldName));
        _projection.serialize(&subObjBuilder);
    }

    builder->append(kBatchSizeFieldName, _batchSize);

    builder->append(kSingleBatchFieldName, _singleBatch);

    builder->append(kAllowPartialResultsFieldName, _allowPartialResults);

    builder->append(kLimitFieldName, _limit);

}


BSONObj SessionsCollectionFetchRequest::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace mongo
