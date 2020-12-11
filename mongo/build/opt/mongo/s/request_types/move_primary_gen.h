/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/s/request_types/move_primary_gen.h --output build/opt/mongo/s/request_types/move_primary_gen.cpp src/mongo/s/request_types/move_primary.idl
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
 * The public movePrimary command on mongos
 *///mongos收到movePrimary命令后，发送_configsvrMovePrimary给cfg，cfg收到后处理
////MoveDatabasePrimaryCommand::run中构造使用，cfg收到后在ConfigSvrMovePrimaryCommand::run中处理
//db.adminCommand( { movePrimary : "test", to : "shard00 01" } )
//构造movePrimary命令，早期可能是moveprimary，所以需要做兼容

class MovePrimary {
public:
    static constexpr auto kMovePrimaryFieldName = "movePrimary"_sd;
    static constexpr auto kMoveprimaryFieldName = "moveprimary"_sd;
    static constexpr auto kToFieldName = "to"_sd;

    MovePrimary();

    static MovePrimary parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    /**
     * The namespace of the database whose primary shard is to be reassigned.
     */
    const boost::optional<mongo::NamespaceString>& getMovePrimary() const& { return _movePrimary; }
    void getMovePrimary() && = delete;
    void setMovePrimary(boost::optional<mongo::NamespaceString> value) & { _movePrimary = std::move(value);  }

    /**
     * The deprecated version of this command's name.
     */
    const boost::optional<mongo::NamespaceString>& getMoveprimary() const& { return _moveprimary; }
    void getMoveprimary() && = delete;
    void setMoveprimary(boost::optional<mongo::NamespaceString> value) & { _moveprimary = std::move(value);  }

    /**
     * The shard serving as the destination for un-sharded collections.
     */
    const StringData getTo() const& { return _to; }
    void getTo() && = delete;
    void setTo(StringData value) & { _to = value.toString(); _hasTo = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    //
    boost::optional<mongo::NamespaceString> _movePrimary;
    boost::optional<mongo::NamespaceString> _moveprimary;
    std::string _to;
    bool _hasTo : 1;
};

/**
 * The internal movePrimary command on the config server
 */
//mongos收到movePrimary命令后，发送_configsvrMovePrimary给cfg，cfg收到后处理
////MoveDatabasePrimaryCommand::run中构造使用，cfg收到后在ConfigSvrMovePrimaryCommand::run中处理
class ConfigsvrMovePrimary {
public:
    static constexpr auto k_configsvrMovePrimaryFieldName = "_configsvrMovePrimary"_sd;
    static constexpr auto kToFieldName = "to"_sd;

    ConfigsvrMovePrimary();

    static ConfigsvrMovePrimary parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    /**
     * The namespace of the database whose primary shard is to be reassigned.
     */
    const mongo::NamespaceString& get_configsvrMovePrimary() const { return __configsvrMovePrimary; }
    void set_configsvrMovePrimary(mongo::NamespaceString value) & { __configsvrMovePrimary = std::move(value); _has_configsvrMovePrimary = true; }

    /**
     * The shard serving as the destination for un-sharded collections.
     */
    const StringData getTo() const& { return _to; }
    void getTo() && = delete;
    void setTo(StringData value) & { _to = value.toString(); _hasTo = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    mongo::NamespaceString __configsvrMovePrimary;
    std::string _to;
    bool _has_configsvrMovePrimary : 1;
    bool _hasTo : 1;
};

}  // namespace mongo
