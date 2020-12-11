/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/repl/replication_consistency_markers_gen.h --output build/opt/mongo/db/repl/replication_consistency_markers_gen.cpp src/mongo/db/repl/replication_consistency_markers.idl
 */

#include "mongo/db/repl/replication_consistency_markers_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {
namespace repl {

constexpr StringData MinValidDocument::k_idFieldName;
constexpr StringData MinValidDocument::kAppliedThroughFieldName;
constexpr StringData MinValidDocument::kInitialSyncFlagFieldName;
constexpr StringData MinValidDocument::kMinValidTermFieldName;
constexpr StringData MinValidDocument::kMinValidTimestampFieldName;
constexpr StringData MinValidDocument::kOldOplogDeleteFromPointFieldName;


MinValidDocument::MinValidDocument() : _minValidTerm(-1), _hasMinValidTimestamp(false), _hasMinValidTerm(false) {
    // Used for initialization only
}

MinValidDocument MinValidDocument::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    MinValidDocument object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void MinValidDocument::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<6> usedFields;
    const size_t kMinValidTimestampBit = 0;
    const size_t kMinValidTermBit = 1;
    const size_t kAppliedThroughBit = 2;
    const size_t kOldOplogDeleteFromPointBit = 3;
    const size_t kInitialSyncFlagBit = 4;
    const size_t k_idBit = 5;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kMinValidTimestampFieldName) {
            if (MONGO_unlikely(usedFields[kMinValidTimestampBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kMinValidTimestampBit);

            _hasMinValidTimestamp = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, bsonTimestamp))) {
                _minValidTimestamp = element.timestamp();
            }
        }
        else if (fieldName == kMinValidTermFieldName) {
            if (MONGO_unlikely(usedFields[kMinValidTermBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kMinValidTermBit);

            _hasMinValidTerm = true;
            if (MONGO_likely(ctxt.checkAndAssertTypes(element, {NumberLong, NumberInt, NumberDecimal, NumberDouble}))) {
                _minValidTerm = element.numberInt();
            }
        }
        else if (fieldName == kAppliedThroughFieldName) {
            if (MONGO_unlikely(usedFields[kAppliedThroughBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kAppliedThroughBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                const BSONObj localObject = element.Obj();
                _appliedThrough = OpTime::parse(localObject);
            }
        }
        else if (fieldName == kOldOplogDeleteFromPointFieldName) {
            if (MONGO_unlikely(usedFields[kOldOplogDeleteFromPointBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kOldOplogDeleteFromPointBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, bsonTimestamp))) {
                _oldOplogDeleteFromPoint = element.timestamp();
            }
        }
        else if (fieldName == kInitialSyncFlagFieldName) {
            if (MONGO_unlikely(usedFields[kInitialSyncFlagBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kInitialSyncFlagBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Bool))) {
                _initialSyncFlag = element.boolean();
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
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kMinValidTimestampBit]) {
            ctxt.throwMissingField(kMinValidTimestampFieldName);
        }
        if (!usedFields[kMinValidTermBit]) {
            ctxt.throwMissingField(kMinValidTermFieldName);
        }
    }

}


void MinValidDocument::serialize(BSONObjBuilder* builder) const {
    invariant(_hasMinValidTimestamp && _hasMinValidTerm);

    builder->append(kMinValidTimestampFieldName, _minValidTimestamp);

    builder->append(kMinValidTermFieldName, _minValidTerm);

    if (_appliedThrough.is_initialized()) {
        const BSONObj localObject = _appliedThrough.get().toBSON();
        builder->append(kAppliedThroughFieldName, localObject);
    }

    if (_oldOplogDeleteFromPoint.is_initialized()) {
        builder->append(kOldOplogDeleteFromPointFieldName, _oldOplogDeleteFromPoint.get());
    }

    if (_initialSyncFlag.is_initialized()) {
        builder->append(kInitialSyncFlagFieldName, _initialSyncFlag.get());
    }

    if (__id.is_initialized()) {
        builder->append(k_idFieldName, __id.get());
    }

}


BSONObj MinValidDocument::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData OplogTruncateAfterPointDocument::k_idFieldName;
constexpr StringData OplogTruncateAfterPointDocument::kOplogTruncateAfterPointFieldName;


OplogTruncateAfterPointDocument::OplogTruncateAfterPointDocument() : _hasOplogTruncateAfterPoint(false), _has_id(false) {
    // Used for initialization only
}

OplogTruncateAfterPointDocument OplogTruncateAfterPointDocument::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    OplogTruncateAfterPointDocument object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void OplogTruncateAfterPointDocument::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<2> usedFields;
    const size_t kOplogTruncateAfterPointBit = 0;
    const size_t k_idBit = 1;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kOplogTruncateAfterPointFieldName) {
            if (MONGO_unlikely(usedFields[kOplogTruncateAfterPointBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kOplogTruncateAfterPointBit);

            _hasOplogTruncateAfterPoint = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, bsonTimestamp))) {
                _oplogTruncateAfterPoint = element.timestamp();
            }
        }
        else if (fieldName == k_idFieldName) {
            if (MONGO_unlikely(usedFields[k_idBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(k_idBit);

            _has_id = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                __id = element.str();
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kOplogTruncateAfterPointBit]) {
            ctxt.throwMissingField(kOplogTruncateAfterPointFieldName);
        }
        if (!usedFields[k_idBit]) {
            ctxt.throwMissingField(k_idFieldName);
        }
    }

}


void OplogTruncateAfterPointDocument::serialize(BSONObjBuilder* builder) const {
    invariant(_hasOplogTruncateAfterPoint && _has_id);

    builder->append(kOplogTruncateAfterPointFieldName, _oplogTruncateAfterPoint);

    builder->append(k_idFieldName, __id);

}


BSONObj OplogTruncateAfterPointDocument::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData CheckpointTimestampDocument::k_idFieldName;
constexpr StringData CheckpointTimestampDocument::kCheckpointTimestampFieldName;


CheckpointTimestampDocument::CheckpointTimestampDocument() : _hasCheckpointTimestamp(false), _has_id(false) {
    // Used for initialization only
}

CheckpointTimestampDocument CheckpointTimestampDocument::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    CheckpointTimestampDocument object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void CheckpointTimestampDocument::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<2> usedFields;
    const size_t kCheckpointTimestampBit = 0;
    const size_t k_idBit = 1;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kCheckpointTimestampFieldName) {
            if (MONGO_unlikely(usedFields[kCheckpointTimestampBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kCheckpointTimestampBit);

            _hasCheckpointTimestamp = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, bsonTimestamp))) {
                _checkpointTimestamp = element.timestamp();
            }
        }
        else if (fieldName == k_idFieldName) {
            if (MONGO_unlikely(usedFields[k_idBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(k_idBit);

            _has_id = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                __id = element.str();
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kCheckpointTimestampBit]) {
            ctxt.throwMissingField(kCheckpointTimestampFieldName);
        }
        if (!usedFields[k_idBit]) {
            ctxt.throwMissingField(k_idFieldName);
        }
    }

}


void CheckpointTimestampDocument::serialize(BSONObjBuilder* builder) const {
    invariant(_hasCheckpointTimestamp && _has_id);

    builder->append(kCheckpointTimestampFieldName, _checkpointTimestamp);

    builder->append(k_idFieldName, __id);

}


BSONObj CheckpointTimestampDocument::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace repl
}  // namespace mongo
