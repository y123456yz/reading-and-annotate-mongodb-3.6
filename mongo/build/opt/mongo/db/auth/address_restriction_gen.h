/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/auth/address_restriction_gen.h --output build/opt/mongo/db/auth/address_restriction_gen.cpp src/mongo/db/auth/address_restriction.idl
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
#include "mongo/db/auth/address_restriction.h"
#include "mongo/db/namespace_string.h"
#include "mongo/idl/idl_parser.h"
#include "mongo/util/net/op_msg.h"
#include "mongo/util/uuid.h"

namespace mongo {

/**
 * clientSource/serverAddress restriction pair
 */
class Address_restriction {
public:
    static constexpr auto kClientSourceFieldName = "clientSource"_sd;
    static constexpr auto kServerAddressFieldName = "serverAddress"_sd;

    Address_restriction();

    static Address_restriction parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const boost::optional<std::vector<StringData>> getClientSource() const& { if (_clientSource.is_initialized()) {
        return transformVector(_clientSource.get());
    } else {
        return boost::none;
    }
     }
    void getClientSource() && = delete;
    void setClientSource(boost::optional<std::vector<StringData>> value) & { if (value.is_initialized()) {
        _clientSource = transformVector(value.get());
    } else {
        _clientSource = boost::none;
    }
      }

    const boost::optional<std::vector<StringData>> getServerAddress() const& { if (_serverAddress.is_initialized()) {
        return transformVector(_serverAddress.get());
    } else {
        return boost::none;
    }
     }
    void getServerAddress() && = delete;
    void setServerAddress(boost::optional<std::vector<StringData>> value) & { if (value.is_initialized()) {
        _serverAddress = transformVector(value.get());
    } else {
        _serverAddress = boost::none;
    }
      }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    boost::optional<std::vector<std::string>> _clientSource;
    boost::optional<std::vector<std::string>> _serverAddress;
};

}  // namespace mongo
