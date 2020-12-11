/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/ops/single_write_result_gen.h --output build/opt/mongo/db/ops/single_write_result_gen.cpp src/mongo/db/ops/single_write_result.idl
 */

#include "mongo/db/ops/single_write_result_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

constexpr StringData SingleWriteResult::kNFieldName;
constexpr StringData SingleWriteResult::kNModifiedFieldName;
constexpr StringData SingleWriteResult::kUpsertedIdFieldName;


SingleWriteResult::SingleWriteResult() : _n(-1), _nModified(-1), _hasN(false), _hasNModified(false), _hasUpsertedId(false) {
    // Used for initialization only
}

SingleWriteResult SingleWriteResult::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    SingleWriteResult object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void SingleWriteResult::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<3> usedFields;
    const size_t kNBit = 0;
    const size_t kNModifiedBit = 1;
    const size_t kUpsertedIdBit = 2;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kNFieldName) {
            if (MONGO_unlikely(usedFields[kNBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kNBit);

            _hasN = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberLong))) {
                _n = element._numberLong();
            }
        }
        else if (fieldName == kNModifiedFieldName) {
            if (MONGO_unlikely(usedFields[kNModifiedBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kNModifiedBit);

            _hasNModified = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberLong))) {
                _nModified = element._numberLong();
            }
        }
        else if (fieldName == kUpsertedIdFieldName) {
            if (MONGO_unlikely(usedFields[kUpsertedIdBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kUpsertedIdBit);

            _hasUpsertedId = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                _upsertedId = element.Obj();
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kNBit]) {
            ctxt.throwMissingField(kNFieldName);
        }
        if (!usedFields[kNModifiedBit]) {
            ctxt.throwMissingField(kNModifiedFieldName);
        }
        if (!usedFields[kUpsertedIdBit]) {
            ctxt.throwMissingField(kUpsertedIdFieldName);
        }
    }

}


void SingleWriteResult::serialize(BSONObjBuilder* builder) const {
    invariant(_hasN && _hasNModified && _hasUpsertedId);

    builder->append(kNFieldName, _n);

    builder->append(kNModifiedFieldName, _nModified);

    builder->append(kUpsertedIdFieldName, _upsertedId);

}


BSONObj SingleWriteResult::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace mongo
