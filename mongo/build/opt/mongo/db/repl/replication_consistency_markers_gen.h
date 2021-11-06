/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/repl/replication_consistency_markers_gen.h --output build/opt/mongo/db/repl/replication_consistency_markers_gen.cpp src/mongo/db/repl/replication_consistency_markers.idl
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
#include "mongo/db/repl/optime.h"
#include "mongo/idl/idl_parser.h"
#include "mongo/util/net/op_msg.h"
#include "mongo/util/uuid.h"

namespace mongo {
namespace repl {

/**
 * A document in which the server stores its minValid and appliedThrough information.
 */
class MinValidDocument {
public:
    static constexpr auto k_idFieldName = "_id"_sd;
    static constexpr auto kAppliedThroughFieldName = "begin"_sd;
    static constexpr auto kInitialSyncFlagFieldName = "doingInitialSync"_sd;
    static constexpr auto kMinValidTermFieldName = "t"_sd;
    static constexpr auto kMinValidTimestampFieldName = "ts"_sd;
    static constexpr auto kOldOplogDeleteFromPointFieldName = "oplogDeleteFromPoint"_sd;

    MinValidDocument();

    static MinValidDocument parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    /**
     * The timestamp for the OpTime at which the data on disk will be consistent
     */
    const mongo::Timestamp& getMinValidTimestamp() const { return _minValidTimestamp; }
    void setMinValidTimestamp(mongo::Timestamp value) & { _minValidTimestamp = std::move(value); _hasMinValidTimestamp = true; }

    /**
     * The term for the OpTime at which the data on disk will be consistent; -1 for PV0.
     */
    std::int64_t getMinValidTerm() const { return _minValidTerm; }
    void setMinValidTerm(std::int64_t value) & { _minValidTerm = std::move(value); _hasMinValidTerm = true; }

    /**
     * The OpTime of the last oplog entry we applied
     */
    const boost::optional<OpTime>& getAppliedThrough() const& { return _appliedThrough; }
    void getAppliedThrough() && = delete;
    void setAppliedThrough(boost::optional<OpTime> value) & { _appliedThrough = std::move(value);  }

    /**
     * The timestamp of the first oplog entry in a batch when we are writing oplog entries to the oplog after which the oplog may be inconsistent. This field only exists on 3.4 upgrade.
     */
    const boost::optional<mongo::Timestamp>& getOldOplogDeleteFromPoint() const& { return _oldOplogDeleteFromPoint; }
    void getOldOplogDeleteFromPoint() && = delete;
    void setOldOplogDeleteFromPoint(boost::optional<mongo::Timestamp> value) & { _oldOplogDeleteFromPoint = std::move(value);  }

    /**
     * A flag for if we are in the middle of initial sync
     */
    const boost::optional<bool> getInitialSyncFlag() const& { return _initialSyncFlag; }
    void getInitialSyncFlag() && = delete;
    void setInitialSyncFlag(boost::optional<bool> value) & { _initialSyncFlag = std::move(value);  }

    /**
     * An objectid that is not used but is automatically generated
     */
    const boost::optional<mongo::OID>& get_id() const& { return __id; }
    void get_id() && = delete;
    void set_id(boost::optional<mongo::OID> value) & { __id = std::move(value);  }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    mongo::Timestamp _minValidTimestamp;
    std::int64_t _minValidTerm;
    boost::optional<OpTime> _appliedThrough;
    boost::optional<mongo::Timestamp> _oldOplogDeleteFromPoint;
    boost::optional<bool> _initialSyncFlag;
    boost::optional<mongo::OID> __id;
    bool _hasMinValidTimestamp : 1;
    bool _hasMinValidTerm : 1;
};

/**
 * A document in which the server stores information on where to truncate the oplog on unclean shutdown.
 */
class OplogTruncateAfterPointDocument {
public:
    static constexpr auto k_idFieldName = "_id"_sd;
    static constexpr auto kOplogTruncateAfterPointFieldName = "oplogTruncateAfterPoint"_sd;

    OplogTruncateAfterPointDocument();

    static OplogTruncateAfterPointDocument parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    /**
     * The timestamp of the first oplog entry in a batch when we are writing oplog entries to the oplog after which the oplog may be inconsistent.
     */
    const mongo::Timestamp& getOplogTruncateAfterPoint() const { return _oplogTruncateAfterPoint; }
    void setOplogTruncateAfterPoint(mongo::Timestamp value) & { _oplogTruncateAfterPoint = std::move(value); _hasOplogTruncateAfterPoint = true; }

    /**
     * Always set to 'oplogTruncateAfterPoint' to easily retrieve it.
     */
    const StringData get_id() const& { return __id; }
    void get_id() && = delete;
    void set_id(StringData value) & { __id = value.toString(); _has_id = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    mongo::Timestamp _oplogTruncateAfterPoint;
    std::string __id;
    bool _hasOplogTruncateAfterPoint : 1;
    bool _has_id : 1;
};

/**
 * A document that stores the latest timestamp the database can recover to.
 */
class CheckpointTimestampDocument {
public:
    static constexpr auto k_idFieldName = "_id"_sd;
    static constexpr auto kCheckpointTimestampFieldName = "checkpointTimestamp"_sd;

    CheckpointTimestampDocument();

    static CheckpointTimestampDocument parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    /**
     * The checkpoint timestamp. Should be set by a storage engine before a checkpoint is taken.
     */
    const mongo::Timestamp& getCheckpointTimestamp() const { return _checkpointTimestamp; }
    void setCheckpointTimestamp(mongo::Timestamp value) & { _checkpointTimestamp = std::move(value); _hasCheckpointTimestamp = true; }

    /**
     * Always set to 'checkpointTimestamp' to easily retrieve it.
     */
    const StringData get_id() const& { return __id; }
    void get_id() && = delete;
    void set_id(StringData value) & { __id = value.toString(); _has_id = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    mongo::Timestamp _checkpointTimestamp;
    std::string __id;
    bool _hasCheckpointTimestamp : 1;
    bool _has_id : 1;
};

}  // namespace repl
}  // namespace mongo
