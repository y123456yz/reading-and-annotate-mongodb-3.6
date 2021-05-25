/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/s/request_types/create_database_gen.h --output build/opt/mongo/s/request_types/create_database_gen.cpp src/mongo/s/request_types/create_database.idl
 */

#pragma once

#include <algorithm>
#include <boost/optional.hpp>
#include <cstdint>
#include <string>
#include <vector>

#include "mongo/base/data_range.h"
#include "mongo/base/string_data.h"
#include "mongo/bson/bsonobj.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/namespace_string.h"
#include "mongo/idl/idl_parser.h"
#include "mongo/util/net/op_msg.h"
#include "mongo/util/uuid.h"

namespace mongo {

/**
 * The internal createDatabase command on the config server
 */
class ConfigsvrCreateDatabase {
public:
    static constexpr auto k_configsvrCreateDatabaseFieldName = "_configsvrCreateDatabase"_sd;

    ConfigsvrCreateDatabase();

    static ConfigsvrCreateDatabase parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    /**
     * The namespace of the database to be created.
     */
    const StringData get_configsvrCreateDatabase() const& { return __configsvrCreateDatabase; }
    void get_configsvrCreateDatabase() && = delete;
    void set_configsvrCreateDatabase(StringData value) & { __configsvrCreateDatabase = value.toString(); _has_configsvrCreateDatabase = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    std::string __configsvrCreateDatabase;
    bool _has_configsvrCreateDatabase : 1;
};

}  // namespace mongo
