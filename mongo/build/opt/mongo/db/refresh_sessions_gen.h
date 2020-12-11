/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/refresh_sessions_gen.h --output build/opt/mongo/db/refresh_sessions_gen.cpp src/mongo/db/refresh_sessions.idl
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
#include "mongo/crypto/sha256_block.h"
#include "mongo/db/logical_session_id_gen.h"
#include "mongo/db/namespace_string.h"
#include "mongo/idl/idl_parser.h"
#include "mongo/util/net/op_msg.h"
#include "mongo/util/uuid.h"

namespace mongo {

/**
 * A struct representing a refreshSessions command from a client
 */
class RefreshSessionsCmdFromClient {
public:
    static constexpr auto kRefreshSessionsFieldName = "refreshSessions"_sd;

    RefreshSessionsCmdFromClient();

    static RefreshSessionsCmdFromClient parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const std::vector<LogicalSessionFromClient>& getRefreshSessions() const& { return _refreshSessions; }
    void getRefreshSessions() && = delete;
    void setRefreshSessions(std::vector<LogicalSessionFromClient> value) & { _refreshSessions = std::move(value); _hasRefreshSessions = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    std::vector<LogicalSessionFromClient> _refreshSessions;
    bool _hasRefreshSessions : 1;
};

/**
 * A struct representing a refreshSessions command from a cluster member
 */
class RefreshSessionsCmdFromClusterMember {
public:
    static constexpr auto kRefreshSessionsInternalFieldName = "refreshSessionsInternal"_sd;

    RefreshSessionsCmdFromClusterMember();

    static RefreshSessionsCmdFromClusterMember parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const std::vector<LogicalSessionRecord>& getRefreshSessionsInternal() const& { return _refreshSessionsInternal; }
    void getRefreshSessionsInternal() && = delete;
    void setRefreshSessionsInternal(std::vector<LogicalSessionRecord> value) & { _refreshSessionsInternal = std::move(value); _hasRefreshSessionsInternal = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    std::vector<LogicalSessionRecord> _refreshSessionsInternal;
    bool _hasRefreshSessionsInternal : 1;
};

}  // namespace mongo
