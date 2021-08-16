/**
 * Copyright (C) 2017 MongoDB Inc.
 *
 * This program is free software: you can redistribute it and/or  modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, the copyright holders give permission to link the
 * code of portions of this program with the OpenSSL library under certain
 * conditions as described in each individual source file and distribute
 * linked combinations including the program with the OpenSSL library. You
 * must comply with the GNU Affero General Public License in all respects
 * for all of the code used other than as permitted herein. If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so. If you do not
 * wish to do so, delete this exception statement from your version. If you
 * delete this exception statement from all source files in the program,
 * then also delete it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kControl

#include "mongo/platform/basic.h"

#include "mongo/db/logical_session_cache_impl.h"

#include "mongo/db/logical_session_id.h"
#include "mongo/db/logical_session_id_helpers.h"
#include "mongo/db/operation_context.h"
#include "mongo/db/server_options.h"
#include "mongo/db/server_parameters.h"
#include "mongo/db/service_context.h"
#include "mongo/platform/atomic_word.h"
#include "mongo/util/duration.h"
#include "mongo/util/log.h"
#include "mongo/util/periodic_runner.h"
#include "mongo/util/scopeguard.h"

namespace mongo {

MONGO_EXPORT_STARTUP_SERVER_PARAMETER(
    logicalSessionRefreshMinutes,
    int,
    LogicalSessionCacheImpl::kLogicalSessionDefaultRefresh.count());

MONGO_EXPORT_STARTUP_SERVER_PARAMETER(disableLogicalSessionCacheRefresh, bool, false);

//默认5分钟 static constexpr Minutes kLogicalSessionDefaultRefresh = Minutes(5);
//可以通过 --setParameter logicalSessionRefreshMinutes=X设置
constexpr Minutes LogicalSessionCacheImpl::kLogicalSessionDefaultRefresh;

//makeLogicalSessionCacheS  makeLogicalSessionCacheD调用
LogicalSessionCacheImpl::LogicalSessionCacheImpl(
    std::unique_ptr<ServiceLiason> service,
    std::shared_ptr<SessionsCollection> collection,
    std::shared_ptr<TransactionReaper> transactionReaper,
    Options options)
    //默认5分钟 static constexpr Minutes kLogicalSessionDefaultRefresh = Minutes(5);
	//可以通过 --setParameter logicalSessionRefreshMinutes=X设置
    : _refreshInterval(options.refreshInterval),
      //可以通过--setParameter localLogicalSessionTimeoutMinutes=X.
      //或者localLogicalSessionTimeoutMinutes命令行调整，默认30分钟
      //该参数真正生效见SessionsCollection::generateCreateIndexesCmd()
      _sessionTimeout(options.sessionTimeout),
      _service(std::move(service)),
      _sessionsColl(std::move(collection)),
      //mongos对应null, mongod对应TransactionReaperImpl
      _transactionReaper(std::move(transactionReaper)) {
    if (!disableLogicalSessionCacheRefresh) {
		//定期refresh
        _service->scheduleJob(
            {[this](Client* client) { _periodicRefresh(client); }, _refreshInterval});
		//定期reap
        _service->scheduleJob(
            {[this](Client* client) { _periodicReap(client); }, _refreshInterval});
    }
    _stats.setLastSessionsCollectionJobTimestamp(now());
    _stats.setLastTransactionReaperJobTimestamp(now());
}

LogicalSessionCacheImpl::~LogicalSessionCacheImpl() {
    try {
        _service->join();
    } catch (...) {
        // If we failed to join we might still be running a background thread,
        // log but swallow the error since there is no good way to recover.
        severe() << "Failed to join background service thread";
    }
}

//查找_activeSessions中是否有该lsid
Status LogicalSessionCacheImpl::promote(LogicalSessionId lsid) {
    stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
    auto it = _activeSessions.find(lsid);
    if (it == _activeSessions.end()) {
        return {ErrorCodes::NoSuchSession, "no matching session record found in the cache"};
    }

    return Status::OK();
}

//LogicalSessionCacheImpl::vivify调用
//把record添加到_activeSessions map表中
void LogicalSessionCacheImpl::startSession(OperationContext* opCtx, LogicalSessionRecord record) {
    // Add the new record to our local cache. We will insert it into the sessions collection
    // the next time _refresh is called. If there is already a record in the cache for this
    // session, we'll just write over it with our newer, more recent one.
    _addToCache(record);
}

Status LogicalSessionCacheImpl::refreshSessions(OperationContext* opCtx,
                                                const RefreshSessionsCmdFromClient& cmd) {
    // Update the timestamps of all these records in our cache.
    auto sessions = makeLogicalSessionIds(cmd.getRefreshSessions(), opCtx);
    for (const auto& lsid : sessions) {
		//查找_activeSessions中是否有该lsid，如果没用则insert
        if (!promote(lsid).isOK()) {
            // This is a new record, insert it.
            _addToCache(makeLogicalSessionRecord(opCtx, lsid, now()));
        }
    }

    return Status::OK();
}

//RefreshSessionsCommand::run   RefreshSessionsCommandInternal::run 
Status LogicalSessionCacheImpl::refreshSessions(OperationContext* opCtx,
                                                const RefreshSessionsCmdFromClusterMember& cmd) {
    // Update the timestamps of all these records in our cache.
    auto records = cmd.getRefreshSessionsInternal();
    for (const auto& record : records) {
        if (!promote(record.getId()).isOK()) {
            // This is a new record, insert it.
            _addToCache(record);
        }
    }

    return Status::OK();
}

//execCommandDatabase->initializeOperationSessionInfo调用
void LogicalSessionCacheImpl::vivify(OperationContext* opCtx, const LogicalSessionId& lsid) {
    if (!promote(lsid).isOK()) {
		//没找到，则把该lsid添加到
        startSession(opCtx, makeLogicalSessionRecord(opCtx, lsid, now()));
    }
}

//RefreshLogicalSessionCacheNowCommand::run(测试命令)
Status LogicalSessionCacheImpl::refreshNow(Client* client) {
    try {
        _refresh(client);
    } catch (...) {
        return exceptionToStatus();
    }
    return Status::OK();
}

Status LogicalSessionCacheImpl::reapNow(Client* client) {
    return _reap(client);
}

Date_t LogicalSessionCacheImpl::now() {
    return _service->now();
}

size_t LogicalSessionCacheImpl::size() {
    stdx::lock_guard<stdx::mutex> lock(_cacheMutex);
    return _activeSessions.size();
}

//LogicalSessionCacheImpl::LogicalSessionCacheImpl中启动一个线程专门做定期refresh
void LogicalSessionCacheImpl::_periodicRefresh(Client* client) {
    try {
        _refresh(client);
    } catch (...) {
        log() << "Failed to refresh session cache: " << exceptionToStatus();
    }
}

//LogicalSessionCacheImpl::LogicalSessionCacheImpl中启动一个线程专门做定期reap
void LogicalSessionCacheImpl::_periodicReap(Client* client) {
    auto res = _reap(client);
    if (!res.isOK()) {
        log() << "Failed to reap transaction table: " << res;
    }

    return;
}

//LogicalSessionCacheImpl::reapNow调用
Status LogicalSessionCacheImpl::_reap(Client* client) {
	//mongos为null直接返回，mongod对应TransactionReaperImpl，继续
    if (!_transactionReaper) {
        return Status::OK();
    }

    // Take the lock to update some stats.
    {
        stdx::lock_guard<stdx::mutex> lk(_cacheMutex);

        // Clear the last set of stats for our new run.
        _stats.setLastTransactionReaperJobDurationMillis(0);
        _stats.setLastTransactionReaperJobEntriesCleanedUp(0);

        // Start the new run.
        _stats.setLastTransactionReaperJobTimestamp(now());
        _stats.setTransactionReaperJobCount(_stats.getTransactionReaperJobCount() + 1);
    }

    int numReaped = 0;

    try {
        boost::optional<ServiceContext::UniqueOperationContext> uniqueCtx;
        auto* const opCtx = [&client, &uniqueCtx] {
            if (client->getOperationContext()) {
                return client->getOperationContext();
            }

            uniqueCtx.emplace(client->makeOperationContext());
            return uniqueCtx->get();
        }();

        stdx::lock_guard<stdx::mutex> lk(_reaperMutex);
		//TransactionReaperImpl::reap
        numReaped = _transactionReaper->reap(opCtx);
    } catch (...) {
        {
            stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
            auto millis = now() - _stats.getLastTransactionReaperJobTimestamp();
            _stats.setLastTransactionReaperJobDurationMillis(millis.count());
        }

        return exceptionToStatus();
    }

    {
        stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
        auto millis = now() - _stats.getLastTransactionReaperJobTimestamp();
        _stats.setLastTransactionReaperJobDurationMillis(millis.count());
        _stats.setLastTransactionReaperJobEntriesCleanedUp(numReaped);
    }

    return Status::OK();
}

//LogicalSessionCacheImpl::refreshNow(测试命令)  
//LogicalSessionCacheImpl::_periodicRefresh 调用

//mongos> db.system.sessions.find();
//{ "_id" : { "id" : UUID("14c31e1f-c245-46ea-a229-7c31a4b042db"), "uid" : BinData(0,"47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU=") }, "lastUse" : ISODate("2021-05-13T19:17:23.232Z") }

//向system.sessions发送update，同时upsert:true，没有则添加。也就是更新session内容
void LogicalSessionCacheImpl::_refresh(Client* client) {
    // Do not run this job if we are not in FCV 3.6
    if (serverGlobalParams.featureCompatibility.getVersion() !=
        ServerGlobalParams::FeatureCompatibility::Version::kFullyUpgradedTo36) {
        LOG(1) << "Skipping session refresh job while feature compatibility version is not 3.6";
        return;
    }

    // Stats for serverStatus:
    //本轮统计先初始化为0
    {
        stdx::lock_guard<stdx::mutex> lk(_cacheMutex);

        // Clear the refresh-related stats with the beginning of our run.
        _stats.setLastSessionsCollectionJobDurationMillis(0);
        _stats.setLastSessionsCollectionJobEntriesRefreshed(0);
        _stats.setLastSessionsCollectionJobEntriesEnded(0);
        _stats.setLastSessionsCollectionJobCursorsClosed(0);

        // Start the new run.
        _stats.setLastSessionsCollectionJobTimestamp(now());
        _stats.setSessionsCollectionJobCount(_stats.getSessionsCollectionJobCount() + 1);
    }

    // This will finish timing _refresh for our stats no matter when we return.
    const auto timeRefreshJob = MakeGuard([this] {
        stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
		//上一次做统计的时间
        auto millis = now() - _stats.getLastSessionsCollectionJobTimestamp();
		//也就是统计间隔
        _stats.setLastSessionsCollectionJobDurationMillis(millis.count());
    });

    // get or make an opCtx
    boost::optional<ServiceContext::UniqueOperationContext> uniqueCtx;
    auto* const opCtx = [&client, &uniqueCtx] {
        if (client->getOperationContext()) {
            return client->getOperationContext();
        }

        uniqueCtx.emplace(client->makeOperationContext());
        return uniqueCtx->get();
    }();

	
	//1. mongod对应makeSessionsCollection中构造使用，
	// 和SessionsCollectionSharded	SessionsCollectionConfigServer SessionsCollectionRS SessionsCollectionStandalone同级
	// mongos对应SessionsCollectionSharded，见makeLogicalSessionCacheS
	//2. mongos对应SessionsCollectionSharded::setupSessionsCollection  mongod对应SessionsCollectionRS::setupSessionsCollection

	////system.sessions创建索引及启用分片等
	auto res = _sessionsColl->setupSessionsCollection(opCtx);
    if (!res.isOK()) {
        log() << "Sessions collection is not set up; "
              << "waiting until next sessions refresh interval: " << res.reason();
        return;
    }

    LogicalSessionIdSet staleSessions;
    LogicalSessionIdSet explicitlyEndingSessions;
    LogicalSessionIdMap<LogicalSessionRecord> activeSessions;

    // backSwapper creates a guard that in the case of a exception
    // replaces the ending or active sessions that swapped out of of LogicalSessionCache,
    // and merges in any records that had been added since we swapped them
    // out.
    auto backSwapper = [this](auto& member, auto& temp) {
        return MakeGuard([this, &member, &temp] {
            stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
            using std::swap;
            swap(member, temp);
            for (const auto& it : temp) {
                member.emplace(it);
            }
        });
    };

    {
        using std::swap;
        stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
		//注意这里交换后，_endingSessions _activeSessions都为空了，也就是者两个变量中
		//只会记录两个刷新周期的相应session信息
        swap(explicitlyEndingSessions, _endingSessions);
        swap(activeSessions, _activeSessions);
    }
    auto activeSessionsBackSwapper = backSwapper(_activeSessions, activeSessions);
    auto explicitlyEndingBackSwaper = backSwapper(_endingSessions, explicitlyEndingSessions);

    // remove all explicitlyEndingSessions from activeSessions
    //先从_activeSessions中移除_endingSessions
    for (const auto& lsid : explicitlyEndingSessions) {
        activeSessions.erase(lsid);
    }

    // refresh all recently active sessions as well as for sessions attached to running ops

    LogicalSessionRecordSet activeSessionRecords{};

	//mongod对应ServiceLiasonMongod::getActiveOpSessions()
	//mongos对应ServiceLiasonMongos::getActiveOpSessions()
    auto runningOpSessions = _service->getActiveOpSessions();

    for (const auto& it : runningOpSessions) {
        // if a running op is the cause of an upsert, we won't have a user name for the record
        if (explicitlyEndingSessions.count(it) > 0) {
            continue;
        }
        activeSessionRecords.insert(makeLogicalSessionRecord(it, now()));
    }
    for (const auto& it : activeSessions) {
        activeSessionRecords.insert(it.second);
    }

    // Refresh the active sessions in the sessions collection.
    //1. mongod对应makeSessionsCollection中构造使用，
	// 和SessionsCollectionSharded	SessionsCollectionConfigServer SessionsCollectionRS SessionsCollectionStandalone同级
	// mongos对应SessionsCollectionSharded，见makeLogicalSessionCacheS
	//2. mongos对应SessionsCollectionSharded::refreshSessions  mongod对应SessionsCollectionRS::refreshSessions

	
	//mongos> db.system.sessions.find();
	//{ "_id" : { "id" : UUID("14c31e1f-c245-46ea-a229-7c31a4b042db"), "uid" : BinData(0,"47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU=") }, "lastUse" : ISODate("2021-05-13T19:17:23.232Z") }
	//向config server中的system.sessions发送update，同时upsert:true，没有则添加。也就是更新session内容
	uassertStatusOK(_sessionsColl->refreshSessions(opCtx, activeSessionRecords));
    activeSessionsBackSwapper.Dismiss();
    {
        stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
		//The number of sessions that were refreshed during the last refresh.
		//也就是两次刷新期间内的新session
        _stats.setLastSessionsCollectionJobEntriesRefreshed(activeSessionRecords.size());
    }

    // Remove the ending sessions from the sessions collection.
    //SessionsCollectionSharded::removeRecords
    //删除system.session表中的指定shession
    uassertStatusOK(_sessionsColl->removeRecords(opCtx, explicitlyEndingSessions));
    explicitlyEndingBackSwaper.Dismiss();
    {
        stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
		//也就是两次刷新期间内的新session
        _stats.setLastSessionsCollectionJobEntriesEnded(explicitlyEndingSessions.size());
    }


    // Find which running, but not recently active sessions, are expired, and add them
    // to the list of sessions to kill cursors for

    KillAllSessionsByPatternSet patterns;

    auto openCursorSessions = _service->getOpenCursorSessions();

    // think about pruning ending and active out of openCursorSessions
    //SessionsCollectionSharded::findRemovedSessions
    //先查找，然后删除
    auto statusAndRemovedSessions = _sessionsColl->findRemovedSessions(opCtx, openCursorSessions);

    if (statusAndRemovedSessions.isOK()) {
        auto removedSessions = statusAndRemovedSessions.getValue();
        for (const auto& lsid : removedSessions) {
            patterns.emplace(makeKillAllSessionsByPattern(opCtx, lsid));
        }
    }

    // Add all of the explicitly ended sessions to the list of sessions to kill cursors for.
    //将所有显式结束的会话添加到要杀死其游标的会话列表中。
    //也就是讲客户端end session的session全部添加到patterns中，后面进行回收处理
    for (const auto& lsid : explicitlyEndingSessions) {
        patterns.emplace(makeKillAllSessionsByPattern(opCtx, lsid));
    }

	//cursor游标回收处理
    SessionKiller::Matcher matcher(std::move(patterns));
    auto killRes = _service->killCursorsWithMatchingSessions(opCtx, std::move(matcher));
    {
        stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
		//cursor游标回收处理
        _stats.setLastSessionsCollectionJobCursorsClosed(killRes.second);
    }
}

//EndSessionsCommand::run调用  //链接断开会调用该命令执行
//_endingSessions专门记录end session信息
void LogicalSessionCacheImpl::endSessions(const LogicalSessionIdSet& sessions) {
    stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
    _endingSessions.insert(begin(sessions), end(sessions));
}

/*
ocloud_DydTikPP_shard_1:SECONDARY> db.serverStatus().logicalSessionRecordCache
{
        "activeSessionsCount" : 383,
        "sessionsCollectionJobCount" : 30,
        "lastSessionsCollectionJobDurationMillis" : 31,
        "lastSessionsCollectionJobTimestamp" : ISODate("2021-06-18T10:54:19.065Z"),
        "lastSessionsCollectionJobEntriesRefreshed" : 285,
        "lastSessionsCollectionJobEntriesEnded" : 0,
        "lastSessionsCollectionJobCursorsClosed" : 0,
        "transactionReaperJobCount" : 30,
        "lastTransactionReaperJobDurationMillis" : 4,
        "lastTransactionReaperJobTimestamp" : ISODate("2021-06-18T10:54:19.065Z"),
        "lastTransactionReaperJobEntriesCleanedUp" : 0,
        "sessionCatalogSize" : 0
}
ocloud_DydTikPP_shard_1:SECONDARY> 

*/
//db.serverStatus().logicalSessionRecordCache命令
//LogicalSessionSSS::generateSection中调用
LogicalSessionCacheStats LogicalSessionCacheImpl::getStats() {
    stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
    _stats.setActiveSessionsCount(_activeSessions.size());
    return _stats;
}

//LogicalSessionCacheImpl::startSession  LogicalSessionCacheImpl::refreshSessions
void LogicalSessionCacheImpl::_addToCache(LogicalSessionRecord record) {
    stdx::lock_guard<stdx::mutex> lk(_cacheMutex);

	/*
	if (_activeSessions.size() >= static_cast<size_t>(maxSessions)) {
        return {ErrorCodes::TooManyLogicalSessions, "cannot add session into the cache"};
    }*/
    _activeSessions.insert(std::make_pair(record.getId(), record));
}

//获取所有的LogicalSessionId
//DocumentSourceListLocalSessions::DocumentSourceListLocalSessions
std::vector<LogicalSessionId> LogicalSessionCacheImpl::listIds() const {
    stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
    std::vector<LogicalSessionId> ret;
    ret.reserve(_activeSessions.size());
    for (const auto& id : _activeSessions) {
        ret.push_back(id.first);
    }
    return ret;
}

//db.aggregate( [  { $listLocalSessions: { allUsers: true } } ] )获取缓存中的
//db.system.sessions.find()获取库中的，库中可能是关闭掉的链接，等待localLogicalSessionTimeoutMinutes配置过期

//找出_activeSessions表中有，但是userDigests数组中没有的LogicalSessionId
//DocumentSourceListLocalSessions::DocumentSourceListLocalSessions
std::vector<LogicalSessionId> LogicalSessionCacheImpl::listIds(
    const std::vector<SHA256Block>& userDigests) const {
    stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
    std::vector<LogicalSessionId> ret;
    for (const auto& it : _activeSessions) {
        if (std::find(userDigests.cbegin(), userDigests.cend(), it.first.getUid()) !=
            userDigests.cend()) {
            ret.push_back(it.first);
        }
    }
    return ret;
}

//DocumentSourceListLocalSessions::getNext()
//查找
boost::optional<LogicalSessionRecord> LogicalSessionCacheImpl::peekCached(
    const LogicalSessionId& id) const {
    stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
    const auto it = _activeSessions.find(id);
    if (it == _activeSessions.end()) {
        return boost::none;
    }
    return it->second;
}
}  // namespace mongo
