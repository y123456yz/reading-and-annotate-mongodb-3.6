/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/s/request_types/create_database_gen.h --output build/opt/mongo/s/request_types/create_database_gen.cpp src/mongo/s/request_types/create_database.idl
 */

#include "mongo/s/request_types/create_database_gen.h"

#include <bitset>
#include <set>

#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/commands.h"

namespace mongo {

constexpr StringData ConfigsvrCreateDatabase::k_configsvrCreateDatabaseFieldName;


ConfigsvrCreateDatabase::ConfigsvrCreateDatabase() : _has_configsvrCreateDatabase(false) {
    // Used for initialization only
}

ConfigsvrCreateDatabase ConfigsvrCreateDatabase::parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    ConfigsvrCreateDatabase object;
    object.parseProtected(ctxt, bsonObject);
    return object;
}
void ConfigsvrCreateDatabase::parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject) {
    std::set<StringData> usedFields;

    for (const auto& element :bsonObject) {
        const auto fieldName = element.fieldNameStringData();

        auto push_result = usedFields.insert(fieldName);
        if (MONGO_unlikely(push_result.second == false)) {
            ctxt.throwDuplicateField(fieldName);
        }

        if (fieldName == k_configsvrCreateDatabaseFieldName) {
            _has_configsvrCreateDatabase = true;
            if (MONGO_likely(ctxt.checkAndAssertType(element, String))) {
                __configsvrCreateDatabase = element.str();
            }
        }
    }


    if (MONGO_unlikely(usedFields.find(k_configsvrCreateDatabaseFieldName) == usedFields.end())) {
        ctxt.throwMissingField(k_configsvrCreateDatabaseFieldName);
    }

}


void ConfigsvrCreateDatabase::serialize(BSONObjBuilder* builder) const {
    invariant(_has_configsvrCreateDatabase);

    builder->append(k_configsvrCreateDatabaseFieldName, __configsvrCreateDatabase);

}


BSONObj ConfigsvrCreateDatabase::toBSON() const {
    BSONObjBuilder builder;
    serialize(&builder);
    return builder.obj();
}

}  // namespace mongo
