/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/s/commands/cluster_multicast_gen.h --output build/opt/mongo/s/commands/cluster_multicast_gen.cpp src/mongo/s/commands/cluster_multicast.idl
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
 * A struct representing cluster multicast args
 */
class ClusterMulticastArgs {
public:
    static constexpr auto kConcurrencyFieldName = "concurrency"_sd;
    static constexpr auto kDbFieldName = "$db"_sd;
    static constexpr auto kMulticastFieldName = "multicast"_sd;
    static constexpr auto kTimeoutFieldName = "timeout"_sd;

    ClusterMulticastArgs();

    static ClusterMulticastArgs parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const mongo::BSONObj& getMulticast() const { return _multicast; }
    void setMulticast(mongo::BSONObj value) & { _multicast = std::move(value); _hasMulticast = true; }

    const StringData getDb() const& { return _db; }
    void getDb() && = delete;
    void setDb(StringData value) & { _db = value.toString(); _hasDb = true; }

    const boost::optional<std::int32_t> getConcurrency() const& { return _concurrency; }
    void getConcurrency() && = delete;
    void setConcurrency(boost::optional<std::int32_t> value) & { _concurrency = std::move(value);  }

    const boost::optional<std::int32_t> getTimeout() const& { return _timeout; }
    void getTimeout() && = delete;
    void setTimeout(boost::optional<std::int32_t> value) & { _timeout = std::move(value);  }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    mongo::BSONObj _multicast;
    std::string _db;
    boost::optional<std::int32_t> _concurrency;
    boost::optional<std::int32_t> _timeout;
    bool _hasMulticast : 1;
    bool _hasDb : 1;
};

}  // namespace mongo
