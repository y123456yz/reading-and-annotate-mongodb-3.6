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
//LogicalSessionCacheImpl._stats为该类型
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
/*
logicalSessionRecordCache?
Provides metrics around the caching of server sessions.

logicalSessionRecordCache.activeSessionsCount
The number of all active local sessions cached in memory by the mongod or mongos instance since the last refresh period.

TIP
See also:
$listLocalSessions
logicalSessionRefreshMillis
logicalSessionRecordCache.sessionsCollectionJobCount
The number that tracks the number of times the refresh process has run on the config.system.sessions collection.

TIP
See also:
logicalSessionRefreshMillis

logicalSessionRecordCache.lastSessionsCollectionJobDurationMillis
The length in milliseconds of the last refresh.

logicalSessionRecordCache.lastSessionsCollectionJobTimestamp
The time at which the last refresh occurred.

logicalSessionRecordCache.lastSessionsCollectionJobEntriesRefreshed
The number of sessions that were refreshed during the last refresh.

logicalSessionRecordCache.lastSessionsCollectionJobEntriesEnded
The number of sessions that ended during the last refresh.

logicalSessionRecordCache.lastSessionsCollectionJobCursorsClosed
The number of cursors that were closed during the last config.system.sessions collection refresh.

logicalSessionRecordCache.transactionReaperJobCount
The number that tracks the number of times the transaction record cleanup process has run on the config.transactions collection.

logicalSessionRecordCache.lastTransactionReaperJobDurationMillis
The length (in milliseconds) of the last transaction record cleanup.

logicalSessionRecordCache.lastTransactionReaperJobTimestamp
The time of the last transaction record cleanup.

logicalSessionRecordCache.lastTransactionReaperJobEntriesCleanedUp
The number of entries in the config.transactions collection that were deleted during the last transaction record cleanup.

logicalSessionRecordCache.sessionCatalogSize
For a mongod instance,
The size of its in-memory cache of the config.transactions entries. This corresponds to retryable writes or transactions whose sessions have not expired within the localLogicalSessionTimeoutMinutes.
For a mongos instance,
The number of the in-memory cache of its sessions that have had transactions within the most recent localLogicalSessionTimeoutMinutes interval.
New in version 4.2.

*/
    //当前session数
    std::int32_t _activeSessionsCount{0};
    
    //以下统计详见：LogicalSessionCacheImpl::_refresh
    //刷新次数统计
    std::int32_t _sessionsCollectionJobCount{0};
    //统计间隔
    std::int32_t _lastSessionsCollectionJobDurationMillis{0};
    //上一次做统计的时间
    mongo::Date_t _lastSessionsCollectionJobTimestamp;
    //两次refres刷新周期新增的session会话统计
    std::int32_t _lastSessionsCollectionJobEntriesRefreshed{0};
    //两次refres刷新周期新增的 end session统计
    std::int32_t _lastSessionsCollectionJobEntriesEnded{0};
    //两次刷新周期内cursor游标回收处理消耗的时间
    std::int32_t _lastSessionsCollectionJobCursorsClosed{0};


    //以下统计详见：LogicalSessionCacheImpl::_reap
    std::int32_t _transactionReaperJobCount{0};
    std::int32_t _lastTransactionReaperJobDurationMillis{0};
    mongo::Date_t _lastTransactionReaperJobTimestamp;
    std::int32_t _lastTransactionReaperJobEntriesCleanedUp{0};
    bool _hasLastSessionsCollectionJobTimestamp : 1;
    bool _hasLastTransactionReaperJobTimestamp : 1;
};

}  // namespace mongo
