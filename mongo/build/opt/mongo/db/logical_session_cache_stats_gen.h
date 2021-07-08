/**
 * WARNING: This is a generated file. Do not modify.
 *
 * Source: buildscripts/idl/idlc.py --include src --base_dir build/opt --header build/opt/mongo/db/logical_session_cache_stats_gen.h --output build/opt/mongo/db/logical_session_cache_stats_gen.cpp src/mongo/db/logical_session_cache_stats.idl
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
 * A struct representing the section of the server status command with information about the logical session cache
 */
class LogicalSessionCacheStats {
public:
    static constexpr auto kActiveSessionsCountFieldName = "activeSessionsCount"_sd;
    static constexpr auto kLastSessionsCollectionJobCursorsClosedFieldName = "lastSessionsCollectionJobCursorsClosed"_sd;
    static constexpr auto kLastSessionsCollectionJobDurationMillisFieldName = "lastSessionsCollectionJobDurationMillis"_sd;
    static constexpr auto kLastSessionsCollectionJobEntriesEndedFieldName = "lastSessionsCollectionJobEntriesEnded"_sd;
    static constexpr auto kLastSessionsCollectionJobEntriesRefreshedFieldName = "lastSessionsCollectionJobEntriesRefreshed"_sd;
    static constexpr auto kLastSessionsCollectionJobTimestampFieldName = "lastSessionsCollectionJobTimestamp"_sd;
    static constexpr auto kLastTransactionReaperJobDurationMillisFieldName = "lastTransactionReaperJobDurationMillis"_sd;
    static constexpr auto kLastTransactionReaperJobEntriesCleanedUpFieldName = "lastTransactionReaperJobEntriesCleanedUp"_sd;
    static constexpr auto kLastTransactionReaperJobTimestampFieldName = "lastTransactionReaperJobTimestamp"_sd;
    static constexpr auto kSessionsCollectionJobCountFieldName = "sessionsCollectionJobCount"_sd;
    static constexpr auto kTransactionReaperJobCountFieldName = "transactionReaperJobCount"_sd;

    LogicalSessionCacheStats();

    static LogicalSessionCacheStats parse(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);
    void serialize(BSONObjBuilder* builder) const;
    BSONObj toBSON() const;

    std::int32_t getActiveSessionsCount() const { return _activeSessionsCount; }
    void setActiveSessionsCount(std::int32_t value) & { _activeSessionsCount = std::move(value);  }

    std::int32_t getSessionsCollectionJobCount() const { return _sessionsCollectionJobCount; }
    void setSessionsCollectionJobCount(std::int32_t value) & { _sessionsCollectionJobCount = std::move(value);  }

    std::int32_t getLastSessionsCollectionJobDurationMillis() const { return _lastSessionsCollectionJobDurationMillis; }
    void setLastSessionsCollectionJobDurationMillis(std::int32_t value) & { _lastSessionsCollectionJobDurationMillis = std::move(value);  }

    const mongo::Date_t& getLastSessionsCollectionJobTimestamp() const { return _lastSessionsCollectionJobTimestamp; }
    void setLastSessionsCollectionJobTimestamp(mongo::Date_t value) & { _lastSessionsCollectionJobTimestamp = std::move(value); _hasLastSessionsCollectionJobTimestamp = true; }

    std::int32_t getLastSessionsCollectionJobEntriesRefreshed() const { return _lastSessionsCollectionJobEntriesRefreshed; }
    void setLastSessionsCollectionJobEntriesRefreshed(std::int32_t value) & { _lastSessionsCollectionJobEntriesRefreshed = std::move(value);  }

    std::int32_t getLastSessionsCollectionJobEntriesEnded() const { return _lastSessionsCollectionJobEntriesEnded; }
    void setLastSessionsCollectionJobEntriesEnded(std::int32_t value) & { _lastSessionsCollectionJobEntriesEnded = std::move(value);  }

    std::int32_t getLastSessionsCollectionJobCursorsClosed() const { return _lastSessionsCollectionJobCursorsClosed; }
    void setLastSessionsCollectionJobCursorsClosed(std::int32_t value) & { _lastSessionsCollectionJobCursorsClosed = std::move(value);  }

    std::int32_t getTransactionReaperJobCount() const { return _transactionReaperJobCount; }
    void setTransactionReaperJobCount(std::int32_t value) & { _transactionReaperJobCount = std::move(value);  }

    std::int32_t getLastTransactionReaperJobDurationMillis() const { return _lastTransactionReaperJobDurationMillis; }
    void setLastTransactionReaperJobDurationMillis(std::int32_t value) & { _lastTransactionReaperJobDurationMillis = std::move(value);  }

    const mongo::Date_t& getLastTransactionReaperJobTimestamp() const { return _lastTransactionReaperJobTimestamp; }
    void setLastTransactionReaperJobTimestamp(mongo::Date_t value) & { _lastTransactionReaperJobTimestamp = std::move(value); _hasLastTransactionReaperJobTimestamp = true; }

    std::int32_t getLastTransactionReaperJobEntriesCleanedUp() const { return _lastTransactionReaperJobEntriesCleanedUp; }
    void setLastTransactionReaperJobEntriesCleanedUp(std::int32_t value) & { _lastTransactionReaperJobEntriesCleanedUp = std::move(value);  }

protected:
    void parseProtected(const IDLParserErrorContext& ctxt, const BSONObj& bsonObject);

private:
    std::int32_t _activeSessionsCount{0};
    std::int32_t _sessionsCollectionJobCount{0};
    std::int32_t _lastSessionsCollectionJobDurationMillis{0};
    mongo::Date_t _lastSessionsCollectionJobTimestamp;
    std::int32_t _lastSessionsCollectionJobEntriesRefreshed{0};
    std::int32_t _lastSessionsCollectionJobEntriesEnded{0};
    std::int32_t _lastSessionsCollectionJobCursorsClosed{0};
    std::int32_t _transactionReaperJobCount{0};
    std::int32_t _lastTransactionReaperJobDurationMillis{0};
    mongo::Date_t _lastTransactionReaperJobTimestamp;
    std::int32_t _lastTransactionReaperJobEntriesCleanedUp{0};
    bool _hasLastSessionsCollectionJobTimestamp : 1;
    bool _hasLastTransactionReaperJobTimestamp : 1;
};

}  // namespace mongo
