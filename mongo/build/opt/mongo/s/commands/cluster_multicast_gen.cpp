/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/s/commands/cluster_multicast_gen.h --output build/opt/mongo/s/commands/cluster_multicast_gen.cpp src/mongo/s/commands/cluster_multicast.idl
 */

#include "mongo/s/commands/cluster_multicast_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

constexpr StringData ClusterMulticastArgs::kConcurrencyFieldName;
constexpr StringData ClusterMulticastArgs::kDbFieldName;
constexpr StringData ClusterMulticastArgs::kMulticastFieldName;
constexpr StringData ClusterMulticastArgs::kTimeoutFieldName;


ClusterMulticastArgs::ClusterMulticastArgs() : _hasMulticast(false), _hasDb(false) {
    // Used for initialization only
}

ClusterMulticastArgs ClusterMulticastArgs::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    ClusterMulticastArgs object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void ClusterMulticastArgs::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::set<StringData> usedFields;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        auto push_result = usedFields.insert(fieldName);
        if (MONGO_unlikely(push_result.second == false)) {
            ctxt.throwDuplicateField(fieldName);
        }

        if (fieldName == kMulticastFieldName) {
            _hasMulticast = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, Object))) {
                _multicast = element.Obj();
            }
        }
        else if (fieldName == kDbFieldName) {
            _hasDb = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                _db = element.str();
            }
        }
        else if (fieldName == kConcurrencyFieldName) {
            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _concurrency = element._numberInt();
            }
        }
        else if (fieldName == kTimeoutFieldName) {
            if (MONGO_likely(ctxt.checkAndAssertType(element, NumberInt))) {
                _timeout = element._numberInt();
            }
        }
    }


    if (MONGO_unlikely(usedFields.find(kMulticastFieldName) == usedFields.end())) {
        ctxt.throwMissingField(kMulticastFieldName);
    }
    if (MONGO_unlikely(usedFields.find(kDbFieldName) == usedFields.end())) {
        ctxt.throwMissingField(kDbFieldName);
    }

}


void ClusterMulticastArgs::serialize(BSONObjBuilder* builder) const {
    invariant(_hasMulticast && _hasDb);

    builder->append(kMulticastFieldName, _multicast);

    builder->append(kDbFieldName, _db);

    if (_concurrency.is_initialized()) {
        builder->append(kConcurrencyFieldName, _concurrency.get());
    }

    if (_timeout.is_initialized()) {
        builder->append(kTimeoutFieldName, _timeout.get());
    }

}


BSONObj ClusterMulticastArgs::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace mongo
