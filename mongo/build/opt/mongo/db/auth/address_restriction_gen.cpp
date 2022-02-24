/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/auth/address_restriction_gen.h --output build/opt/mongo/db/auth/address_restriction_gen.cpp src/mongo/db/auth/address_restriction.idl
 */

#include "mongo/db/auth/address_restriction_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

constexpr StringData Address_restriction::kClientSourceFieldName;
constexpr StringData Address_restriction::kServerAddressFieldName;


Address_restriction::Address_restriction()  {
    // Used for initialization only
}

Address_restriction Address_restriction::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    Address_restriction object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void Address_restriction::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::bitset<2> usedFields;
    const size_t kClientSourceBit = 0;
    const size_t kServerAddressBit = 1;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();


        if (fieldName == kClientSourceFieldName) {
            if (MONGO_unlikely(usedFields[kClientSourceBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kClientSourceBit);

            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kClientSourceFieldName, &ctxt);
            std::vector<std::string> values;

            const BSONObj arrayObject = element.Obj();
            for (const auto& arrayElement : arrayObject) {
                const auto arrayFieldName = arrayElement.fieldNameStringData();
                std::uint32_t fieldNumber;

                Status status = parseNumberFromString(arrayFieldName, &fieldNumber);
                if (status.isOK()) {
                    if (fieldNumber != expectedFieldNumber) {
                        arrayCtxt.throwBadArrayFieldNumberSequence(fieldNumber, expectedFieldNumber);
                    }

                    if (arrayCtxt.checkAndAssertType(arrayElement, String)) {
                        values.emplace_back(arrayElement.str());
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _clientSource = std::move(values);
        }
        else if (fieldName == kServerAddressFieldName) {
            if (MONGO_unlikely(usedFields[kServerAddressBit])) {
                ctxt.throwDuplicateField(element);
            }

            usedFields.set(kServerAddressBit);

            std::uint32_t expectedFieldNumber{0};
            const IDLParserErrorContext arrayCtxt(kServerAddressFieldName, &ctxt);
            std::vector<std::string> values;

            const BSONObj arrayObject = element.Obj();
            for (const auto& arrayElement : arrayObject) {
                const auto arrayFieldName = arrayElement.fieldNameStringData();
                std::uint32_t fieldNumber;

                Status status = parseNumberFromString(arrayFieldName, &fieldNumber);
                if (status.isOK()) {
                    if (fieldNumber != expectedFieldNumber) {
                        arrayCtxt.throwBadArrayFieldNumberSequence(fieldNumber, expectedFieldNumber);
                    }

                    if (arrayCtxt.checkAndAssertType(arrayElement, String)) {
                        values.emplace_back(arrayElement.str());
                    }
                }
                else {
                    arrayCtxt.throwBadArrayFieldNumberValue(arrayFieldName);
                }
                ++expectedFieldNumber;
            }
            _serverAddress = std::move(values);
        }
        else {
            ctxt.throwUnknownField(fieldName);
        }
    }


    if (MONGO_unlikely(!usedFields.all())) {
    }

}


void Address_restriction::serialize(BSONObjBuilder* builder) const {
    if (_clientSource.is_initialized()) {
        builder->append(kClientSourceFieldName, _clientSource.get());
    }

    if (_serverAddress.is_initialized()) {
        builder->append(kServerAddressFieldName, _serverAddress.get());
    }

}


BSONObj Address_restriction::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace mongo
