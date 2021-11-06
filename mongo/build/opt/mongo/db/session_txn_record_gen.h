/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/session_txn_record_gen.h --output build/opt/mongo/db/session_txn_record_gen.cpp src/mongo/db/session_txn_record.idl
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
#include "mongo/db/logical_session_id.h"
#include "mongo/db/logical_session_id_gen.h"
#include "mongo/db/namespace_string.h"
#include "mongo/db/repl/optime.h"
#include "mongo/idl/idl_parser.h"
#include "mongo/util/net/op_msg.h"
#include "mongo/util/uuid.h"

namespace mongo {

/**
 * A document used for storing session transaction states.
 */
class SessionTxnRecord {
public:
    static constexpr auto kLastWriteDateFieldName = "lastWriteDate"_sd;
    static constexpr auto kLastWriteOpTimeFieldName = "lastWriteOpTime"_sd;
    static constexpr auto kSessionIdFieldName = "_id"_sd;
    static constexpr auto kTxnNumFieldName = "txnNum"_sd;

    SessionTxnRecord();

    static SessionTxnRecord parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    /**
     * The id of the session this transaction belongs to.
     */
    const LogicalSessionId& getSessionId() const { return _sessionId; }
    LogicalSessionId& getSessionId() { return _sessionId; }
    void setSessionId(LogicalSessionId value) & { _sessionId = std::move(value); _hasSessionId = true; }

    /**
     * The id representing this transaction.
     */
    std::int64_t getTxnNum() const { return _txnNum; }
    void setTxnNum(std::int64_t value) & { _txnNum = std::move(value); _hasTxnNum = true; }

    /**
     * The optime timestamp component of the last write on this transaction.
     */
    const repl::OpTime& getLastWriteOpTime() const { return _lastWriteOpTime; }
    void setLastWriteOpTime(repl::OpTime value) & { _lastWriteOpTime = std::move(value); _hasLastWriteOpTime = true; }

    /**
     * Wall clock time of the last write which happened on on this transaction.
     */
    const mongo::Date_t& getLastWriteDate() const { return _lastWriteDate; }
    void setLastWriteDate(mongo::Date_t value) & { _lastWriteDate = std::move(value); _hasLastWriteDate = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    LogicalSessionId _sessionId;
    std::int64_t _txnNum;
    repl::OpTime _lastWriteOpTime;
    mongo::Date_t _lastWriteDate;
    bool _hasSessionId : 1;
    bool _hasTxnNum : 1;
    bool _hasLastWriteOpTime : 1;
    bool _hasLastWriteDate : 1;
};

}  // namespace mongo
