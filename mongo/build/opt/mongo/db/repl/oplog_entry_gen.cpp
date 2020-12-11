/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/repl/oplog_entry_gen.h --output build/opt/mongo/db/repl/oplog_entry_gen.cpp src/mongo/db/repl/oplog_entry.idl
 */

#include "mongo/db/repl/oplog_entry_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {
namespace repl {

/**
 * The type of an operation in the oplog
 */
namespace  {
constexpr StringData kOpType_kCommand = "c"_sd;
constexpr StringData kOpType_kInsert = "i"_sd;
constexpr StringData kOpType_kUpdate = "u"_sd;
constexpr StringData kOpType_kDelete = "d"_sd;
constexpr StringData kOpType_kNoop = "n"_sd;
}  // namespace 

OpTypeEnum OpType_parse(const IDLParserErrorContext& ctxt, StringData value) {
    if (value == kOpType_kCommand) {
        return OpTypeEnum::kCommand;
    }
    if (value == kOpType_kInsert) {
        return OpTypeEnum::kInsert;
    }
    if (value == kOpType_kUpdate) {
        return OpTypeEnum::kUpdate;
    }
    if (value == kOpType_kDelete) {
        return OpTypeEnum::kDelete;
    }
    if (value == kOpType_kNoop) {
        return OpTypeEnum::kNoop;
    }
    ctxt.throwBadEnumValue(value);
}

StringData OpType_serializer(OpTypeEnum value) {
    if (value == OpTypeEnum::kCommand) {
        return kOpType_kCommand;
    }
    if (value == OpTypeEnum::kInsert) {
        return kOpType_kInsert;
    }
    if (value == OpTypeEnum::kUpdate) {
        return kOpType_kUpdate;
    }
    if (value == OpTypeEnum::kDelete) {
        return kOpType_kDelete;
    }
    if (value == OpTypeEnum::kNoop) {
        return kOpType_kNoop;
    }
    MONGO_UNREACHABLE;
    return StringData();
}

constexpr StringData OplogEntryBase::kOperationSessionInfoFieldName;
constexpr StringData OplogEntryBase::k_idFieldName;
constexpr StringData OplogEntryBase::kFromMigrateFieldName;
constexpr StringData OplogEntryBase::kHashFieldName;
constexpr StringData OplogEntryBase::kNamespaceFieldName;
constexpr StringData OplogEntryBase::kObjectFieldName;
constexpr StringData OplogEntryBase::kObject2FieldName;
constexpr StringData OplogEntryBase::kOpTypeFieldName;
constexpr StringData OplogEntryBase::kPostImageOpTimeFieldName;
constexpr StringData OplogEntryBase::kPreImageOpTimeFieldName;
constexpr StringData OplogEntryBase::kPrevWriteOpTimeInTransactionFieldName;
constexpr StringData OplogEntryBase::kSessionIdFieldName;
constexpr StringData OplogEntryBase::kStatementIdFieldName;
constexpr StringData OplogEntryBase::kTermFieldName;
constexpr StringData OplogEntryBase::kTimestampFieldName;
constexpr StringData OplogEntryBase::kTxnNumberFieldName;
constexpr StringData OplogEntryBase::kUuidFieldName;
constexpr StringData OplogEntryBase::kVersionFieldName;
constexpr StringData OplogEntryBase::kWallClockTimeFieldName;


OplogEntryBase::OplogEntryBase() : _hash(-1), _hasTimestamp(false), _hasHash(false), _hasOpType(false), _hasNamespace(false), _hasObject(false) {
    // Used for initialization only
}

OplogEntryBase OplogEntryBase::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    OplogEntryBase object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void OplogEntryBase::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<19> usedFields;
    const size_t kSessionIdBit = 0;
    const size_t kTxnNumberBit = 1;
    const size_t kTimestampBit = 2;
    const size_t kTermBit = 3;
    const size_t kHashBit = 4;
    const size_t kVersionBit = 5;
    const size_t kOpTypeBit = 6;
    const size_t kNamespaceBit = 7;
    const size_t kUuidBit = 8;
    const size_t kFromMigrateBit = 9;
    const size_t kObjectBit = 10;
    const size_t kObject2Bit = 11;
    const size_t k_idBit = 12;
    const size_t kWallClockTimeBit = 13;
    const size_t kStatementIdBit = 14;
    const size_t kPrevWriteOpTimeInTransactionBit = 15;
    const size_t kPreImageOpTimeBit = 16;
    const size_t kPostImageOpTimeBit = 17;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kSessionIdFieldName) {
            if (MONGO_unlikely(usedFields[kSessionIdBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kSessionIdBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                IDLParserErrorContext tempContext(kSessionIdFieldName, &ctxt);
                const auto localObject = element.Obj();
                _operationSessionInfo.setSessionId(LogicalSessionId::parse(tempContext, localObject));
            }
        }
        else if (fieldName == kTxnNumberFieldName) {
            if (MONGO_unlikely(usedFields[kTxnNumberBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kTxnNumberBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberLong))) {
                _operationSessionInfo.setTxnNumber(element._numberLong());
            }
        }
        else if (fieldName == kTimestampFieldName) {
            if (MONGO_unlikely(usedFields[kTimestampBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kTimestampBit);

            _hasTimestamp = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, bsonTimestamp))) {
                _timestamp = element.timestamp();
            }
        }
        else if (fieldName == kTermFieldName) {
            if (MONGO_unlikely(usedFields[kTermBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kTermBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberLong))) {
                _term = element._numberLong();
            }
        }
        else if (fieldName == kHashFieldName) {
            if (MONGO_unlikely(usedFields[kHashBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kHashBit);

            _hasHash = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberLong))) {
                _hash = element._numberLong();
            }
        }
        else if (fieldName == kVersionFieldName) {
            if (MONGO_unlikely(usedFields[kVersionBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kVersionBit);

            if (MONGO_likely(ctxt.checkAndAssertTypes(element, {NumberLong, NumberInt, NumberDecimal, NumberDouble}))) {
                _version = element.numberInt();
            }
        }
        else if (fieldName == kOpTypeFieldName) {
            if (MONGO_unlikely(usedFields[kOpTypeBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kOpTypeBit);

            _hasOpType = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                IDLParserErrorContext tempContext(kOpTypeFieldName, &ctxt);
                _opType = OpType_parse(tempContext, element.valueStringData());
            }
        }
        else if (fieldName == kNamespaceFieldName) {
            if (MONGO_unlikely(usedFields[kNamespaceBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kNamespaceBit);

            _hasNamespace = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _namespace = NamespaceString(element.valueStringData());
            }
        }
        else if (fieldName == kUuidFieldName) {
            if (MONGO_unlikely(usedFields[kUuidBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kUuidBit);

            if (MONGO_likely(ctxt.checkAndAssertBinDataType(element, newUUID))) {
                _uuid = UUID(element.uuid());
            }
        }
        else if (fieldName == kFromMigrateFieldName) {
            if (MONGO_unlikely(usedFields[kFromMigrateBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kFromMigrateBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Bool))) {
                _fromMigrate = element.boolean();
            }
        }
        else if (fieldName == kObjectFieldName) {
            if (MONGO_unlikely(usedFields[kObjectBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kObjectBit);

            _hasObject = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                _object = element.Obj();
            }
        }
        else if (fieldName == kObject2FieldName) {
            if (MONGO_unlikely(usedFields[kObject2Bit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kObject2Bit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                _object2 = element.Obj();
            }
        }
        else if (fieldName == k_idFieldName) {
            if (MONGO_unlikely(usedFields[k_idBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(k_idBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, jstOID))) {
                __id = element.OID();
            }
        }
        else if (fieldName == kWallClockTimeFieldName) {
            if (MONGO_unlikely(usedFields[kWallClockTimeBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kWallClockTimeBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Date))) {
                _wallClockTime = element.date();
            }
        }
        else if (fieldName == kStatementIdFieldName) {
            if (MONGO_unlikely(usedFields[kStatementIdBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kStatementIdBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _statementId = element._numberInt();
            }
        }
        else if (fieldName == kPrevWriteOpTimeInTransactionFieldName) {
            if (MONGO_unlikely(usedFields[kPrevWriteOpTimeInTransactionBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kPrevWriteOpTimeInTransactionBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                const BSONObj localObject = element.Obj();
                _prevWriteOpTimeInTransaction = OpTime::parse(localObject);
            }
        }
        else if (fieldName == kPreImageOpTimeFieldName) {
            if (MONGO_unlikely(usedFields[kPreImageOpTimeBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kPreImageOpTimeBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                const BSONObj localObject = element.Obj();
                _preImageOpTime = OpTime::parse(localObject);
            }
        }
        else if (fieldName == kPostImageOpTimeFieldName) {
            if (MONGO_unlikely(usedFields[kPostImageOpTimeBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kPostImageOpTimeBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                const BSONObj localObject = element.Obj();
                _postImageOpTime = OpTime::parse(localObject);
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
        if (!usedFields[kHashBit]) {
            ctxt.throwMissingField(kHashFieldName);
        }
        if (!usedFields[kVersionBit]) {
            _version = 1;
        }
        if (!usedFields[kOpTypeBit]) {
            ctxt.throwMissingField(kOpTypeFieldName);
        }
        if (!usedFields[kNamespaceBit]) {
            ctxt.throwMissingField(kNamespaceFieldName);
        }
        if (!usedFields[kObjectBit]) {
            ctxt.throwMissingField(kObjectFieldName);
        }
    }

}


void OplogEntryBase::serialize(BSONObjBuilder* builder) const {
    invariant(_hasTimestamp && _hasHash && _hasOpType && _hasNamespace && _hasObject);

    {
        _operationSessionInfo.serialize(builder);
    }

    builder->append(kTimestampFieldName, _timestamp);

    if (_term.is_initialized()) {
        builder->append(kTermFieldName, _term.get());
    }

    builder->append(kHashFieldName, _hash);

    builder->append(kVersionFieldName, _version);

    {
        builder->append(kOpTypeFieldName, OpType_serializer(_opType));
    }

    {
        builder->append(kNamespaceFieldName, _namespace.toString());
    }

    if (_uuid.is_initialized()) {
        ConstDataRange tempCDR = _uuid.get().toCDR();
        builder->append(kUuidFieldName, BSONBinData(tempCDR.data(), tempCDR.length(), newUUID));
    }

    if (_fromMigrate.is_initialized()) {
        builder->append(kFromMigrateFieldName, _fromMigrate.get());
    }

    builder->append(kObjectFieldName, _object);

    if (_object2.is_initialized()) {
        builder->append(kObject2FieldName, _object2.get());
    }

    if (__id.is_initialized()) {
        builder->append(k_idFieldName, __id.get());
    }

    if (_wallClockTime.is_initialized()) {
        builder->append(kWallClockTimeFieldName, _wallClockTime.get());
    }

    if (_statementId.is_initialized()) {
        builder->append(kStatementIdFieldName, _statementId.get());
    }

    if (_prevWriteOpTimeInTransaction.is_initialized()) {
        const BSONObj localObject = _prevWriteOpTimeInTransaction.get().toBSON();
        builder->append(kPrevWriteOpTimeInTransactionFieldName, localObject);
    }

    if (_preImageOpTime.is_initialized()) {
        const BSONObj localObject = _preImageOpTime.get().toBSON();
        builder->append(kPreImageOpTimeFieldName, localObject);
    }

    if (_postImageOpTime.is_initialized()) {
        const BSONObj localObject = _postImageOpTime.get().toBSON();
        builder->append(kPostImageOpTimeFieldName, localObject);
    }

}


BSONObj OplogEntryBase::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace repl
}  // namespace mongo
