/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/session_txn_record_gen.h --output build/opt/mongo/db/session_txn_record_gen.cpp src/mongo/db/session_txn_record.idl
 */

#include "mongo/db/session_txn_record_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

constexpr StringData SessionTxnRecord::kLastWriteDateFieldName;
constexpr StringData SessionTxnRecord::kLastWriteOpTimeFieldName;
constexpr StringData SessionTxnRecord::kSessionIdFieldName;
constexpr StringData SessionTxnRecord::kTxnNumFieldName;


SessionTxnRecord::SessionTxnRecord() : _txnNum(-1), _hasSessionId(false), _hasTxnNum(false), _hasLastWriteOpTime(false), _hasLastWriteDate(false) {
    // Used for initialization only
}

SessionTxnRecord SessionTxnRecord::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    SessionTxnRecord object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void SessionTxnRecord::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<4> usedFields;
    const size_t kSessionIdBit = 0;
    const size_t kTxnNumBit = 1;
    const size_t kLastWriteOpTimeBit = 2;
    const size_t kLastWriteDateBit = 3;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kSessionIdFieldName) {
            if (MONGO_unlikely(usedFields[kSessionIdBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kSessionIdBit);

            _hasSessionId = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                IDLParserErrorContext tempContext(kSessionIdFieldName, &ctxt);
                const auto localObject = element.Obj();
                _sessionId = LogicalSessionId::parse(tempContext, localObject);
            }
        }
        else if (fieldName == kTxnNumFieldName) {
            if (MONGO_unlikely(usedFields[kTxnNumBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kTxnNumBit);

            _hasTxnNum = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberLong))) {
                _txnNum = element._numberLong();
            }
        }
        else if (fieldName == kLastWriteOpTimeFieldName) {
            if (MONGO_unlikely(usedFields[kLastWriteOpTimeBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kLastWriteOpTimeBit);

            _hasLastWriteOpTime = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                const BSONObj localObject = element.Obj();
                _lastWriteOpTime = repl::OpTime::parse(localObject);
            }
        }
        else if (fieldName == kLastWriteDateFieldName) {
            if (MONGO_unlikely(usedFields[kLastWriteDateBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kLastWriteDateBit);

            _hasLastWriteDate = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Date))) {
                _lastWriteDate = element.date();
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kSessionIdBit]) {
            ctxt.throwMissingField(kSessionIdFieldName);
        }
        if (!usedFields[kTxnNumBit]) {
            ctxt.throwMissingField(kTxnNumFieldName);
        }
        if (!usedFields[kLastWriteOpTimeBit]) {
            ctxt.throwMissingField(kLastWriteOpTimeFieldName);
        }
        if (!usedFields[kLastWriteDateBit]) {
            ctxt.throwMissingField(kLastWriteDateFieldName);
        }
    }

}


void SessionTxnRecord::serialize(BSONObjBuilder* builder) const {
    invariant(_hasSessionId && _hasTxnNum && _hasLastWriteOpTime && _hasLastWriteDate);

    {
        BSONObjBuilder subObjBuilder(builder->subobjStart(kSessionIdFieldName));
        _sessionId.serialize(&subObjBuilder);
    }

    builder->append(kTxnNumFieldName, _txnNum);

    {
        const BSONObj localObject = _lastWriteOpTime.toBSON();
        builder->append(kLastWriteOpTimeFieldName, localObject);
    }

    builder->append(kLastWriteDateFieldName, _lastWriteDate);

}


BSONObj SessionTxnRecord::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace mongo
