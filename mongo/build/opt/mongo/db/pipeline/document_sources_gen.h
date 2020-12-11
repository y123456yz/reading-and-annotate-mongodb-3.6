/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/pipeline/document_sources_gen.h --output build/opt/mongo/db/pipeline/document_sources_gen.cpp src/mongo/db/pipeline/document_sources.idl
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
#include "mongo/db/pipeline/resume_token.h"
#include "mongo/idl/idl_parser.h"
#include "mongo/util/net/op_msg.h"
#include "mongo/util/uuid.h"

namespace mongo {

/**
 * The IDL type of cluster time
 */
class ResumeTokenClusterTime {
public:
    static constexpr auto kTimestampFieldName = "ts"_sd;

    ResumeTokenClusterTime();

    static ResumeTokenClusterTime parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    /**
     * The timestamp of the logical time
     */
    const mongo::Timestamp& getTimestamp() const { return _timestamp; }
    void setTimestamp(mongo::Timestamp value) & { _timestamp = std::move(value); _hasTimestamp = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    mongo::Timestamp _timestamp;
    bool _hasTimestamp : 1;
};

/**
 * A document used to specify the $changeStream stage of an aggregation pipeline.
 */
class DocumentSourceChangeStreamSpec {
public:
    static constexpr auto kFullDocumentFieldName = "fullDocument"_sd;
    static constexpr auto kResumeAfterFieldName = "resumeAfter"_sd;
    static constexpr auto kResumeAfterClusterTimeFieldName = "$_resumeAfterClusterTime"_sd;

    DocumentSourceChangeStreamSpec();

    static DocumentSourceChangeStreamSpec parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    /**
     * An object representing the point at which we should resume reporting changes from.
     */
    const boost::optional<ResumeToken>& getResumeAfter() const& { return _resumeAfter; }
    void getResumeAfter() && = delete;
    void setResumeAfter(boost::optional<ResumeToken> value) & { _resumeAfter = std::move(value);  }

    /**
     * The cluster time after which we should start reporting changes. Only one of resumeAfter and _resumeAfterClusterTime should be specified.  For internal use only.
     */
    const boost::optional<ResumeTokenClusterTime>& getResumeAfterClusterTime() const& { return _resumeAfterClusterTime; }
    void getResumeAfterClusterTime() && = delete;
    void setResumeAfterClusterTime(boost::optional<ResumeTokenClusterTime> value) & { _resumeAfterClusterTime = std::move(value);  }

    /**
     * A string '"updateLookup"' or '"default"', indicating whether or not we should return a full document or just changes for an update.
     */
    const StringData getFullDocument() const& { return _fullDocument; }
    void getFullDocument() && = delete;
    void setFullDocument(StringData value) & { _fullDocument = value.toString();  }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    boost::optional<ResumeToken> _resumeAfter;
    boost::optional<ResumeTokenClusterTime> _resumeAfterClusterTime;
    std::string _fullDocument{"default"};
};

/**
 * A struct representing a $listSessions/$listLocalSessions User
 */
class ListSessionsUser {
public:
    static constexpr auto kDbFieldName = "db"_sd;
    static constexpr auto kUserFieldName = "user"_sd;

    ListSessionsUser();

    static ListSessionsUser parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    const StringData getUser() const& { return _user; }
    void getUser() && = delete;
    void setUser(StringData value) & { _user = value.toString(); _hasUser = true; }

    const StringData getDb() const& { return _db; }
    void getDb() && = delete;
    void setDb(StringData value) & { _db = value.toString(); _hasDb = true; }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    std::string _user;
    std::string _db;
    bool _hasUser : 1;
    bool _hasDb : 1;
};

/**
 * $listSessions and $listLocalSessions pipeline spec
 */
class ListSessionsSpec {
public:
    static constexpr auto kAllUsersFieldName = "allUsers"_sd;
    static constexpr auto kUsersFieldName = "users"_sd;

    ListSessionsSpec();

    static ListSessionsSpec parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    bool getAllUsers() const { return _allUsers; }
    void setAllUsers(bool value) & { _allUsers = std::move(value);  }

    const boost::optional<std::vector<ListSessionsUser>>& getUsers() const& { return _users; }
    void getUsers() && = delete;
    void setUsers(boost::optional<std::vector<ListSessionsUser>> value) & { _users = std::move(value);  }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    bool _allUsers{false};
    boost::optional<std::vector<ListSessionsUser>> _users;
};

}  // namespace mongo
