/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/repl/oplog_entry_gen.h --output build/opt/mongo/db/repl/oplog_entry_gen.cpp src/mongo/db/repl/oplog_entry.idl
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
#include "mongo/db/repl/optime.h"
#include "mongo/idl/idl_parser.h"
#include "mongo/util/net/op_msg.h"
#include "mongo/util/uuid.h"

namespace mongo {
namespace repl {

/**
 * The type of an operation in the oplog
 */
enum class OpTypeEnum : std::int32_t {
    kCommand ,
    kInsert ,
    kUpdate ,
    kDelete ,
    kNoop ,
};

OpTypeEnum OpType_parse(const IDLParserErrorContext& ctxt, StringData value);
StringData OpType_serializer(OpTypeEnum value);

/**
 * A document in which the server stores an oplog entry.
 */
class OplogEntryBase {
public:
    static constexpr auto kOperationSessionInfoFieldName = "OperationSessionInfo"_sd;
    static constexpr auto k_idFieldName = "_id"_sd;
    static constexpr auto kFromMigrateFieldName = "fromMigrate"_sd;
    static constexpr auto kHashFieldName = "h"_sd;
    static constexpr auto kNamespaceFieldName = "ns"_sd;
    static constexpr auto kObjectFieldName = "o"_sd;
    static constexpr auto kObject2FieldName = "o2"_sd;
    static constexpr auto kOpTypeFieldName = "op"_sd;
    static constexpr auto kPostImageOpTimeFieldName = "postImageOpTime"_sd;
    static constexpr auto kPreImageOpTimeFieldName = "preImageOpTime"_sd;
    static constexpr auto kPrevWriteOpTimeInTransactionFieldName = "prevOpTime"_sd;
    static constexpr auto kSessionIdFieldName = "lsid"_sd;
    static constexpr auto kStatementIdFieldName = "stmtId"_sd;
    static constexpr auto kTermFieldName = "t"_sd;
    static constexpr auto kTimestampFieldName = "ts"_sd;
    static constexpr auto kTxnNumberFieldName = "txnNumber"_sd;
    static constexpr auto kUuidFieldName = "ui"_sd;
    static constexpr auto kVersionFieldName = "v"_sd;
    static constexpr auto kWallClockTimeFieldName = "wall"_sd;

    OplogEntryBase();

    static OplogEntryBase parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    /**
     * Parser for serializing sessionId/txnNumber combination
     */
    const OperationSessionInfo& getOperationSessionInfo() const { return _operationSessionInfo; }
    OperationSessionInfo& getOperationSessionInfo() { return _operationSessionInfo; }
    /**
     * The time when the oplog entry was created
     */
    const mongo::Timestamp& getTimestamp() const { return _timestamp; }
    /**
     * The term of the primary that created the oplog entry
     */
    const boost::optional<std::int64_t> getTerm() const& { return _term; }
    void getTerm() && = delete;
    /**
     * The hash of the oplog entry
     */
    std::int64_t getHash() const { return _hash; }
    /**
     * The version of the oplog
     */
    std::int64_t getVersion() const { return _version; }
    /**
     * The operation type
     */
    const OpTypeEnum getOpType() const { return _opType; }
    /**
     * The namespace on which to apply the operation
     */
    const mongo::NamespaceString& getNamespace() const { return _namespace; }
    /**
     * The UUID of the collection
     */
    const boost::optional<mongo::UUID>& getUuid() const& { return _uuid; }
    void getUuid() && = delete;
    /**
     * An operation caused by a chunk migration
     */
    const boost::optional<bool> getFromMigrate() const& { return _fromMigrate; }
    void getFromMigrate() && = delete;
    /**
     * The operation applied
     */
    const mongo::BSONObj& getObject() const { return _object; }
    /**
     * Additional information about the operation applied
     */
    const boost::optional<mongo::BSONObj>& getObject2() const& { return _object2; }
    void getObject2() && = delete;
    /**
     * An optional _id field for tests that manually insert oplog entries
     */
    const boost::optional<mongo::OID>& get_id() const& { return __id; }
    void get_id() && = delete;
    /**
     * A wallclock time with MS resolution
     */
    const boost::optional<mongo::Date_t>& getWallClockTime() const& { return _wallClockTime; }
    void getWallClockTime() && = delete;
    /**
     * Identifier of the transaction statement which generated this oplog entry
     */
    const boost::optional<std::int32_t> getStatementId() const& { return _statementId; }
    void getStatementId() && = delete;
    /**
     * The opTime of the previous write with the same transaction.
     */
    const boost::optional<OpTime>& getPrevWriteOpTimeInTransaction() const& { return _prevWriteOpTimeInTransaction; }
    void getPrevWriteOpTimeInTransaction() && = delete;
    /**
     * The optime of another oplog entry that contains the document before an update/remove was applied.
     */
    const boost::optional<OpTime>& getPreImageOpTime() const& { return _preImageOpTime; }
    void getPreImageOpTime() && = delete;
    /**
     * The optime of another oplog entry that contains the document after an update was applied.
     */
    const boost::optional<OpTime>& getPostImageOpTime() const& { return _postImageOpTime; }
    void getPostImageOpTime() && = delete;
protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    OperationSessionInfo _operationSessionInfo;
    mongo::Timestamp _timestamp;
    boost::optional<std::int64_t> _term;
    std::int64_t _hash;
    std::int64_t _version{1};
    OpTypeEnum _opType;
    mongo::NamespaceString _namespace;
    boost::optional<mongo::UUID> _uuid;
    boost::optional<bool> _fromMigrate;
    mongo::BSONObj _object;
    boost::optional<mongo::BSONObj> _object2;
    boost::optional<mongo::OID> __id;
    boost::optional<mongo::Date_t> _wallClockTime;
    boost::optional<std::int32_t> _statementId;
    boost::optional<OpTime> _prevWriteOpTimeInTransaction;
    boost::optional<OpTime> _preImageOpTime;
    boost::optional<OpTime> _postImageOpTime;
    bool _hasTimestamp : 1;
    bool _hasHash : 1;
    bool _hasOpType : 1;
    bool _hasNamespace : 1;
    bool _hasObject : 1;
};

}  // namespace repl
}  // namespace mongo
