/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/create_indexes_gen.h --output build/opt/mongo/db/create_indexes_gen.cpp src/mongo/db/create_indexes.idl
 */

#include "mongo/db/create_indexes_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

constexpr StringData NewIndexSpec::k2dsphereIndexVersionFieldName;
constexpr StringData NewIndexSpec::kBackgroundFieldName;
constexpr StringData NewIndexSpec::kBitsFieldName;
constexpr StringData NewIndexSpec::kBucketSizeFieldName;
constexpr StringData NewIndexSpec::kCollationFieldName;
constexpr StringData NewIndexSpec::kDefault_languageFieldName;
constexpr StringData NewIndexSpec::kExpireAfterSecondsFieldName;
constexpr StringData NewIndexSpec::kKeyFieldName;
constexpr StringData NewIndexSpec::kLanguage_overrideFieldName;
constexpr StringData NewIndexSpec::kMaxFieldName;
constexpr StringData NewIndexSpec::kMinFieldName;
constexpr StringData NewIndexSpec::kNameFieldName;
constexpr StringData NewIndexSpec::kPartialFilterExpressionFieldName;
constexpr StringData NewIndexSpec::kSparseFieldName;
constexpr StringData NewIndexSpec::kStorageEngineFieldName;
constexpr StringData NewIndexSpec::kTextIndexVersionFieldName;
constexpr StringData NewIndexSpec::kUniqueFieldName;
constexpr StringData NewIndexSpec::kWeightsFieldName;


NewIndexSpec::NewIndexSpec() : _hasKey(false), _hasName(false) {
    // Used for initialization only
}

NewIndexSpec NewIndexSpec::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    NewIndexSpec object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void NewIndexSpec::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<18> usedFields;
    const size_t kKeyBit = 0;
    const size_t kNameBit = 1;
    const size_t kBackgroundBit = 2;
    const size_t kUniqueBit = 3;
    const size_t kPartialFilterExpressionBit = 4;
    const size_t kSparseBit = 5;
    const size_t kExpireAfterSecondsBit = 6;
    const size_t kStorageEngineBit = 7;
    const size_t kWeightsBit = 8;
    const size_t kDefault_languageBit = 9;
    const size_t kLanguage_overrideBit = 10;
    const size_t kTextIndexVersionBit = 11;
    const size_t k2dsphereIndexVersionBit = 12;
    const size_t kBitsBit = 13;
    const size_t kMinBit = 14;
    const size_t kMaxBit = 15;
    const size_t kBucketSizeBit = 16;
    const size_t kCollationBit = 17;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kKeyFieldName) {
            if (MONGO_unlikely(usedFields[kKeyBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kKeyBit);

            _hasKey = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                _key = element.Obj();
            }
        }
        else if (fieldName == kNameFieldName) {
            if (MONGO_unlikely(usedFields[kNameBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kNameBit);

            _hasName = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _name = element.str();
            }
        }
        else if (fieldName == kBackgroundFieldName) {
            if (MONGO_unlikely(usedFields[kBackgroundBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kBackgroundBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Bool))) {
                _background = element.boolean();
            }
        }
        else if (fieldName == kUniqueFieldName) {
            if (MONGO_unlikely(usedFields[kUniqueBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kUniqueBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Bool))) {
                _unique = element.boolean();
            }
        }
        else if (fieldName == kPartialFilterExpressionFieldName) {
            if (MONGO_unlikely(usedFields[kPartialFilterExpressionBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kPartialFilterExpressionBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                _partialFilterExpression = element.Obj();
            }
        }
        else if (fieldName == kSparseFieldName) {
            if (MONGO_unlikely(usedFields[kSparseBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kSparseBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Bool))) {
                _sparse = element.boolean();
            }
        }
        else if (fieldName == kExpireAfterSecondsFieldName) {
            if (MONGO_unlikely(usedFields[kExpireAfterSecondsBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kExpireAfterSecondsBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _expireAfterSeconds = element._numberInt();
            }
        }
        else if (fieldName == kStorageEngineFieldName) {
            if (MONGO_unlikely(usedFields[kStorageEngineBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kStorageEngineBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                _storageEngine = element.Obj();
            }
        }
        else if (fieldName == kWeightsFieldName) {
            if (MONGO_unlikely(usedFields[kWeightsBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kWeightsBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                _weights = element.Obj();
            }
        }
        else if (fieldName == kDefault_languageFieldName) {
            if (MONGO_unlikely(usedFields[kDefault_languageBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kDefault_languageBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _default_language = element.str();
            }
        }
        else if (fieldName == kLanguage_overrideFieldName) {
            if (MONGO_unlikely(usedFields[kLanguage_overrideBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kLanguage_overrideBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _language_override = element.str();
            }
        }
        else if (fieldName == kTextIndexVersionFieldName) {
            if (MONGO_unlikely(usedFields[kTextIndexVersionBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kTextIndexVersionBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _textIndexVersion = element._numberInt();
            }
        }
        else if (fieldName == k2dsphereIndexVersionFieldName) {
            if (MONGO_unlikely(usedFields[k2dsphereIndexVersionBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(k2dsphereIndexVersionBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _2dsphereIndexVersion = element._numberInt();
            }
        }
        else if (fieldName == kBitsFieldName) {
            if (MONGO_unlikely(usedFields[kBitsBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kBitsBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _bits = element._numberInt();
            }
        }
        else if (fieldName == kMinFieldName) {
            if (MONGO_unlikely(usedFields[kMinBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kMinBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberDouble))) {
                _min = element._numberDouble();
            }
        }
        else if (fieldName == kMaxFieldName) {
            if (MONGO_unlikely(usedFields[kMaxBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kMaxBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberDouble))) {
                _max = element._numberDouble();
            }
        }
        else if (fieldName == kBucketSizeFieldName) {
            if (MONGO_unlikely(usedFields[kBucketSizeBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kBucketSizeBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberDouble))) {
                _bucketSize = element._numberDouble();
            }
        }
        else if (fieldName == kCollationFieldName) {
            if (MONGO_unlikely(usedFields[kCollationBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kCollationBit);

            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                _collation = element.Obj();
            }
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
        if (!usedFields[kKeyBit]) {
            ctxt.throwMissingField(kKeyFieldName);
        }
        if (!usedFields[kNameBit]) {
            ctxt.throwMissingField(kNameFieldName);
        }
    }

}


void NewIndexSpec::serialize(BSONObjBuilder* builder) const {
    invariant(_hasKey && _hasName);

    builder->append(kKeyFieldName, _key);

    builder->append(kNameFieldName, _name);

    if (_background.is_initialized()) {
        builder->append(kBackgroundFieldName, _background.get());
    }

    if (_unique.is_initialized()) {
        builder->append(kUniqueFieldName, _unique.get());
    }

    if (_partialFilterExpression.is_initialized()) {
        builder->append(kPartialFilterExpressionFieldName, _partialFilterExpression.get());
    }

    if (_sparse.is_initialized()) {
        builder->append(kSparseFieldName, _sparse.get());
    }

    if (_expireAfterSeconds.is_initialized()) {
        builder->append(kExpireAfterSecondsFieldName, _expireAfterSeconds.get());
    }

    if (_storageEngine.is_initialized()) {
        builder->append(kStorageEngineFieldName, _storageEngine.get());
    }

    if (_weights.is_initialized()) {
        builder->append(kWeightsFieldName, _weights.get());
    }

    if (_default_language.is_initialized()) {
        builder->append(kDefault_languageFieldName, _default_language.get());
    }

    if (_language_override.is_initialized()) {
        builder->append(kLanguage_overrideFieldName, _language_override.get());
    }

    if (_textIndexVersion.is_initialized()) {
        builder->append(kTextIndexVersionFieldName, _textIndexVersion.get());
    }

    if (_2dsphereIndexVersion.is_initialized()) {
        builder->append(k2dsphereIndexVersionFieldName, _2dsphereIndexVersion.get());
    }

    if (_bits.is_initialized()) {
        builder->append(kBitsFieldName, _bits.get());
    }

    if (_min.is_initialized()) {
        builder->append(kMinFieldName, _min.get());
    }

    if (_max.is_initialized()) {
        builder->append(kMaxFieldName, _max.get());
    }

    if (_bucketSize.is_initialized()) {
        builder->append(kBucketSizeFieldName, _bucketSize.get());
    }

    if (_collation.is_initialized()) {
        builder->append(kCollationFieldName, _collation.get());
    }

}


BSONObj NewIndexSpec::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

constexpr StringData CreateIndexesCmd::kCreateIndexesFieldName;
constexpr StringData CreateIndexesCmd::kIndexesFieldName;


CreateIndexesCmd::CreateIndexesCmd() : _hasCreateIndexes(false), _hasIndexes(false) {
    // Used for initialization only
}

CreateIndexesCmd CreateIndexesCmd::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    CreateIndexesCmd object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void CreateIndexesCmd::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::set<StringData> usedFields;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        auto push_result = usedFields.insert(fieldName);
        if (MONGO_unlikely(push_result.second == false)) {
            ctxt.throwDuplicateField(fieldName);
        }

        if (fieldName == kCreateIndexesFieldName) {
            _hasCreateIndexes = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _createIndexes = element.str();
            }
        }
        else if (fieldName == kIndexesFieldName) {
            _hasIndexes = true;
            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kIndexesFieldName, &ctxt);
            std::vector<NewIndexSpec> values;

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
                        IDLParserErrorContext tempContext(kIndexesFieldName, &ctxt);
                        const auto localObject = arrayElement.Obj();
                        values.emplace_back(NewIndexSpec::parse(tempContext, localObject));
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _indexes = std::move(values);
        }
    }


    if (MONGO_unlikely(usedFields.find(kCreateIndexesFieldName) == usedFields.end())) {
        ctxt.throwMissingField(kCreateIndexesFieldName);
    }
    if (MONGO_unlikely(usedFields.find(kIndexesFieldName) == usedFields.end())) {
        ctxt.throwMissingField(kIndexesFieldName);
    }

}


void CreateIndexesCmd::serialize(BSONObjBuilder* builder) const {
    invariant(_hasCreateIndexes && _hasIndexes);

    builder->append(kCreateIndexesFieldName, _createIndexes);

    {
        BSONArrayBuilder arrayBuilder(builder->subarrayStart(kIndexesFieldName));
        for (const auto& item : _indexes) {
            BSONObjBuilder subObjBuilder(arrayBuilder.subobjStart());
            item.serialize(&subObjBuilder);
        }
    }

}


BSONObj CreateIndexesCmd::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace mongo
