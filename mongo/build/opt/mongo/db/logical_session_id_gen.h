/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/logical_session_id_gen.h --output build/opt/mongo/db/logical_session_id_gen.cpp src/mongo/db/logical_session_id.idl
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
#include "mongo/db/namespace_string.h"
#include "mongo/idl/idl_parser.h"
#include "mongo/util/net/op_msg.h"
#include "mongo/util/uuid.h"

namespace mongo {

/**
 * A struct representing a LogicalSessionId
 */
class LogicalSessionId {
public:
    static constexpr auto kIdFieldName = "id"_sd;
    static constexpr auto kUidFieldName = "uid"_sd;

    LogicalSessionId();

    static LogicalSessionId parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const mongo::UUID& getId() const { return _id; }
    void setId(mongo::UUID value) & { _id = std::move(value); _hasId = true; }

    const mongo::SHA256Block& getUid() const { return _uid; }
    void setUid(mongo::SHA256Block value) & { _uid = std::move(value); _hasUid = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    mongo::UUID _id;
    mongo::SHA256Block _uid;
    bool _hasId : 1;
    bool _hasUid : 1;
};

/**
 * A struct representing a LogicalSessionId to external clients
 */
class LogicalSessionIdToClient {
public:
    static constexpr auto kIdFieldName = "id"_sd;

    LogicalSessionIdToClient();

    static LogicalSessionIdToClient parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const mongo::UUID& getId() const { return _id; }
    void setId(mongo::UUID value) & { _id = std::move(value); _hasId = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    mongo::UUID _id;
    bool _hasId : 1;
};

/**
 * A struct representing a LogicalSession reply to external clients
 */
class LogicalSessionToClient {
public:
    static constexpr auto kIdFieldName = "id"_sd;
    static constexpr auto kTimeoutMinutesFieldName = "timeoutMinutes"_sd;

    LogicalSessionToClient();

    static LogicalSessionToClient parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const LogicalSessionIdToClient& getId() const { return _id; }
    LogicalSessionIdToClient& getId() { return _id; }
    void setId(LogicalSessionIdToClient value) & { _id = std::move(value); _hasId = true; }

    std::int32_t getTimeoutMinutes() const { return _timeoutMinutes; }
    void setTimeoutMinutes(std::int32_t value) & { _timeoutMinutes = std::move(value); _hasTimeoutMinutes = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    LogicalSessionIdToClient _id;
    std::int32_t _timeoutMinutes;
    bool _hasId : 1;
    bool _hasTimeoutMinutes : 1;
};

/**
 * A struct representing a LogicalSessionRecord
 */
class LogicalSessionRecord {
public:
    static constexpr auto kIdFieldName = "_id"_sd;
    static constexpr auto kLastUseFieldName = "lastUse"_sd;
    static constexpr auto kUserFieldName = "user"_sd;

    LogicalSessionRecord();

    static LogicalSessionRecord parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const LogicalSessionId& getId() const { return _id; }
    LogicalSessionId& getId() { return _id; }
    void setId(LogicalSessionId value) & { _id = std::move(value); _hasId = true; }

    const mongo::Date_t& getLastUse() const { return _lastUse; }
    void setLastUse(mongo::Date_t value) & { _lastUse = std::move(value); _hasLastUse = true; }

    const boost::optional<StringData> getUser() const& { return boost::optional<StringData>{_user}; }
    void getUser() && = delete;
    void setUser(boost::optional<StringData> value) & { if (value.is_initialized()) {
        _user = value.get().toString();
    } else {
        _user = boost::none;
    }
      }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    LogicalSessionId _id;
    mongo::Date_t _lastUse;
    boost::optional<std::string> _user;
    bool _hasId : 1;
    bool _hasLastUse : 1;
};

/**
 * A struct representing a LogicalSessionId from external clients
 */
class LogicalSessionFromClient {
public:
    static constexpr auto kIdFieldName = "id"_sd;
    static constexpr auto kUidFieldName = "uid"_sd;

    LogicalSessionFromClient();

    static LogicalSessionFromClient parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const mongo::UUID& getId() const { return _id; }
    void setId(mongo::UUID value) & { _id = std::move(value); _hasId = true; }

    const boost::optional<mongo::SHA256Block>& getUid() const& { return _uid; }
    void getUid() && = delete;
    void setUid(boost::optional<mongo::SHA256Block> value) & { _uid = std::move(value);  }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    mongo::UUID _id;
    boost::optional<mongo::SHA256Block> _uid;
    bool _hasId : 1;
};

/**
 * Parser for serializing sessionId/txnNumber combination
 */
class OperationSessionInfo {
public:
    static constexpr auto kSessionIdFieldName = "lsid"_sd;
    static constexpr auto kTxnNumberFieldName = "txnNumber"_sd;

    OperationSessionInfo();

    static OperationSessionInfo parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const boost::optional<LogicalSessionId>& getSessionId() const& { return _sessionId; }
    void getSessionId() && = delete;
    void setSessionId(boost::optional<LogicalSessionId> value) & { _sessionId = std::move(value);  }

    /**
     * The transaction number relative to the session in which a particular write operation executes.
     */
    const boost::optional<std::int64_t> getTxnNumber() const& { return _txnNumber; }
    void getTxnNumber() && = delete;
    void setTxnNumber(boost::optional<std::int64_t> value) & { _txnNumber = std::move(value);  }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    boost::optional<LogicalSessionId> _sessionId;
    boost::optional<std::int64_t> _txnNumber;
};

/**
 * Parser for pulling out the sessionId/txnNumber combination from commands
 */
class OperationSessionInfoFromClient {
public:
    static constexpr auto kSessionIdFieldName = "lsid"_sd;
    static constexpr auto kTxnNumberFieldName = "txnNumber"_sd;

    OperationSessionInfoFromClient();

    static OperationSessionInfoFromClient parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const boost::optional<LogicalSessionFromClient>& getSessionId() const& { return _sessionId; }
    void getSessionId() && = delete;
    void setSessionId(boost::optional<LogicalSessionFromClient> value) & { _sessionId = std::move(value);  }

    /**
     * The transaction number relative to the session in which a particular write operation executes.
     */
    const boost::optional<std::int64_t> getTxnNumber() const& { return _txnNumber; }
    void getTxnNumber() && = delete;
    void setTxnNumber(boost::optional<std::int64_t> value) & { _txnNumber = std::move(value);  }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    boost::optional<LogicalSessionFromClient> _sessionId;
    boost::optional<std::int64_t> _txnNumber;
};

/**
 * Individual result
 */
class SessionsCollectionFetchResultIndividualResult {
public:
    static constexpr auto k_idFieldName = "_id"_sd;

    SessionsCollectionFetchResultIndividualResult();

    static SessionsCollectionFetchResultIndividualResult parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const LogicalSessionId& get_id() const { return __id; }
    LogicalSessionId& get_id() { return __id; }
    void set_id(LogicalSessionId value) & { __id = std::move(value); _has_id = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    LogicalSessionId __id;
    bool _has_id : 1;
};

/**
 * Cursor object
 */
class SessionsCollectionFetchResultCursor {
public:
    static constexpr auto kFirstBatchFieldName = "firstBatch"_sd;

    SessionsCollectionFetchResultCursor();

    static SessionsCollectionFetchResultCursor parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const std::vector<SessionsCollectionFetchResultIndividualResult>& getFirstBatch() const& { return _firstBatch; }
    void getFirstBatch() && = delete;
    void setFirstBatch(std::vector<SessionsCollectionFetchResultIndividualResult> value) & { _firstBatch = std::move(value); _hasFirstBatch = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    std::vector<SessionsCollectionFetchResultIndividualResult> _firstBatch;
    bool _hasFirstBatch : 1;
};

/**
 * Parser for pulling out the fetch results from SessionsCollection::fetch
 */
class SessionsCollectionFetchResult {
public:
    static constexpr auto kCursorFieldName = "cursor"_sd;

    SessionsCollectionFetchResult();

    static SessionsCollectionFetchResult parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const SessionsCollectionFetchResultCursor& getCursor() const { return _cursor; }
    SessionsCollectionFetchResultCursor& getCursor() { return _cursor; }
    void setCursor(SessionsCollectionFetchResultCursor value) & { _cursor = std::move(value); _hasCursor = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    SessionsCollectionFetchResultCursor _cursor;
    bool _hasCursor : 1;
};

/**
 * Id
 */
class SessionsCollectionFetchRequestFilterId {
public:
    static constexpr auto kInFieldName = "$in"_sd;

    SessionsCollectionFetchRequestFilterId();

    static SessionsCollectionFetchRequestFilterId parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const std::vector<LogicalSessionId>& getIn() const& { return _in; }
    void getIn() && = delete;
    void setIn(std::vector<LogicalSessionId> value) & { _in = std::move(value); _hasIn = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    std::vector<LogicalSessionId> _in;
    bool _hasIn : 1;
};

/**
 * filter
 */
class SessionsCollectionFetchRequestFilter {
public:
    static constexpr auto k_idFieldName = "_id"_sd;

    SessionsCollectionFetchRequestFilter();

    static SessionsCollectionFetchRequestFilter parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const SessionsCollectionFetchRequestFilterId& get_id() const { return __id; }
    SessionsCollectionFetchRequestFilterId& get_id() { return __id; }
    void set_id(SessionsCollectionFetchRequestFilterId value) & { __id = std::move(value); _has_id = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    SessionsCollectionFetchRequestFilterId __id;
    bool _has_id : 1;
};

/**
 * projection
 */
class SessionsCollectionFetchRequestProjection {
public:
    static constexpr auto k_idFieldName = "_id"_sd;

    SessionsCollectionFetchRequestProjection();

    static SessionsCollectionFetchRequestProjection parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    std::int32_t get_id() const { return __id; }
    void set_id(std::int32_t value) & { __id = std::move(value); _has_id = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    std::int32_t __id;
    bool _has_id : 1;
};

/**
 * Parser for forming the fetch request for SessionsCollection::fetch
 */
class SessionsCollectionFetchRequest {
public:
    static constexpr auto kAllowPartialResultsFieldName = "allowPartialResults"_sd;
    static constexpr auto kBatchSizeFieldName = "batchSize"_sd;
    static constexpr auto kFilterFieldName = "filter"_sd;
    static constexpr auto kFindFieldName = "find"_sd;
    static constexpr auto kLimitFieldName = "limit"_sd;
    static constexpr auto kProjectionFieldName = "projection"_sd;
    static constexpr auto kSingleBatchFieldName = "singleBatch"_sd;

    SessionsCollectionFetchRequest();

    static SessionsCollectionFetchRequest parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const StringData getFind() const& { return _find; }
    void getFind() && = delete;
    void setFind(StringData value) & { _find = value.toString(); _hasFind = true; }

    const SessionsCollectionFetchRequestFilter& getFilter() const { return _filter; }
    SessionsCollectionFetchRequestFilter& getFilter() { return _filter; }
    void setFilter(SessionsCollectionFetchRequestFilter value) & { _filter = std::move(value); _hasFilter = true; }

    const SessionsCollectionFetchRequestProjection& getProjection() const { return _projection; }
    SessionsCollectionFetchRequestProjection& getProjection() { return _projection; }
    void setProjection(SessionsCollectionFetchRequestProjection value) & { _projection = std::move(value); _hasProjection = true; }

    std::int32_t getBatchSize() const { return _batchSize; }
    void setBatchSize(std::int32_t value) & { _batchSize = std::move(value); _hasBatchSize = true; }

    bool getSingleBatch() const { return _singleBatch; }
    void setSingleBatch(bool value) & { _singleBatch = std::move(value); _hasSingleBatch = true; }

    bool getAllowPartialResults() const { return _allowPartialResults; }
    void setAllowPartialResults(bool value) & { _allowPartialResults = std::move(value); _hasAllowPartialResults = true; }

    std::int32_t getLimit() const { return _limit; }
    void setLimit(std::int32_t value) & { _limit = std::move(value); _hasLimit = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    std::string _find;
    SessionsCollectionFetchRequestFilter _filter;
    SessionsCollectionFetchRequestProjection _projection;
    std::int32_t _batchSize;
    bool _singleBatch;
    bool _allowPartialResults;
    std::int32_t _limit;
    bool _hasFind : 1;
    bool _hasFilter : 1;
    bool _hasProjection : 1;
    bool _hasBatchSize : 1;
    bool _hasSingleBatch : 1;
    bool _hasAllowPartialResults : 1;
    bool _hasLimit : 1;
};

}  // namespace mongo
