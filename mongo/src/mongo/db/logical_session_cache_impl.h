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

#pragma once

#include "mongo/db/logical_session_cache.h"
#include "mongo/db/logical_session_id.h"
#include "mongo/db/refresh_sessions_gen.h"
#include "mongo/db/service_liason.h"
#include "mongo/db/sessions_collection.h"
#include "mongo/db/time_proof_service.h"
#include "mongo/db/transaction_reaper.h"
#include "mongo/platform/atomic_word.h"
#include "mongo/stdx/thread.h"
#include "mongo/util/lru_cache.h"

namespace mongo {

class Client;
class OperationContext;
class ServiceContext;

extern int logicalSessionRefreshMinutes;

/**
 * A thread-safe cache structure for logical session records.
 *
 * The cache takes ownership of the passed-in ServiceLiason and
 * SessionsCollection helper types.
 */
 
//db.aggregate( [  { $listLocalSessions: { allUsers: true } } ] )获取缓存中的
//db.system.sessions.find()获取库中的，库中可能是关闭掉的链接，等待localLogicalSessionTimeoutMinutes配置过期


//session和事务的绑定使用例子，可以参考https://mongoing.com/%3Fp%3D6084
//session相关命令查看https://docs.mongodb.com/manual/reference/method/Session/
//也可以参考https://www.percona.com/blog/2021/06/03/mongodb-message-cannot-add-session-into-the-cache-toomanylogicalsessions/
// makeLogicalSessionCacheD(mongod)  makeLogicalSessionCacheS(mongos)中生成使用
class LogicalSessionCacheImpl final : public LogicalSessionCache {
public:
    static constexpr Minutes kLogicalSessionDefaultRefresh = Minutes(5);

    /**
     * An Options type to support the LogicalSessionCacheImpl.
     */
    struct Options {
        Options(){};

        /**
         * A timeout value to use for sessions in the cache, in minutes.
         *
         * By default, this is set to 30 minutes.
         *
         * May be set with --setParameter localLogicalSessionTimeoutMinutes=X.
         */
        /*
        > db.system.sessions.getIndexes() 过期索引
        [
                {
                        "v" : 2,
                        "key" : {
                                "_id" : 1
                        },
                        "name" : "_id_",
                        "ns" : "config.system.sessions"
                },
                {
                        "v" : 2,
                        "key" : {
                                "lastUse" : 1
                        },
                        "name" : "lsidTTLIndex",
                        "ns" : "config.system.sessions",
                        "expireAfterSeconds" : 1800
                }
        ]
        > 
          */
         //可以通过--setParameter localLogicalSessionTimeoutMinutes=X.
         //或者localLogicalSessionTimeoutMinutes命令行调整，默认30分钟
         //真正生效见SessionsCollection::generateCreateIndexesCmd()
        Minutes sessionTimeout = Minutes(localLogicalSessionTimeoutMinutes);

        /**
         * The interval over which the cache will refresh session records.
         *
         * By default, this is set to every 5 minutes. If the caller is
         * setting the sessionTimeout by hand, it is suggested that they
         * consider also setting the refresh interval accordingly.
         *
         * May be set with --setParameter logicalSessionRefreshMinutes=X.
         */
        Minutes refreshInterval = Minutes(logicalSessionRefreshMinutes);
    };

    /**
     * Construct a new session cache.
     */
    explicit LogicalSessionCacheImpl(std::unique_ptr<ServiceLiason> service,
                                     std::shared_ptr<SessionsCollection> collection,
                                     std::shared_ptr<TransactionReaper> transactionReaper,
                                     Options options = Options{});

    LogicalSessionCacheImpl(const LogicalSessionCacheImpl&) = delete;
    LogicalSessionCacheImpl& operator=(const LogicalSessionCacheImpl&) = delete;

    ~LogicalSessionCacheImpl();

    Status promote(LogicalSessionId lsid) override;

    void startSession(OperationContext* opCtx, LogicalSessionRecord record) override;

    Status refreshSessions(OperationContext* opCtx,
                           const RefreshSessionsCmdFromClient& cmd) override;
    Status refreshSessions(OperationContext* opCtx,
                           const RefreshSessionsCmdFromClusterMember& cmd) override;

    void vivify(OperationContext* opCtx, const LogicalSessionId& lsid) override;

    Status refreshNow(Client* client) override;

    Status reapNow(Client* client) override;

    Date_t now() override;

    size_t size() override;

    std::vector<LogicalSessionId> listIds() const override;

    std::vector<LogicalSessionId> listIds(
        const std::vector<SHA256Block>& userDigest) const override;

    boost::optional<LogicalSessionRecord> peekCached(const LogicalSessionId& id) const override;

    void endSessions(const LogicalSessionIdSet& sessions) override;

    LogicalSessionCacheStats getStats() override;

private:
    /**
     * Internal methods to handle scheduling and perform refreshes for active
     * session records contained within the cache.
     */
    void _periodicRefresh(Client* client);
    void _refresh(Client* client);

    void _periodicReap(Client* client);
    Status _reap(Client* client);

    /**
     * Returns true if a record has passed its given expiration.
     */
    bool _isDead(const LogicalSessionRecord& record, Date_t now) const;

    /**
     * Takes the lock and inserts the given record into the cache.
     */
    void _addToCache(LogicalSessionRecord record);

    const Minutes _refreshInterval;
    const Minutes _sessionTimeout;

    // This value is only modified under the lock, and is modified
    // automatically by the background jobs.
    LogicalSessionCacheStats _stats;

    //mongod也就是ServiceLiasonMongod  mongos对应ServiceLiasonMongos  
    std::unique_ptr<ServiceLiason> _service;
    //参考makeSessionsCollection 分片模式mongos和mongod都对应SessionsCollectionSharded
    std::shared_ptr<SessionsCollection> _sessionsColl;

    mutable stdx::mutex _reaperMutex;
    //mongod对应TransactionReaperImpl  mongos对应null
    std::shared_ptr<TransactionReaper> _transactionReaper;

    mutable stdx::mutex _cacheMutex;

    //参考LogicalSessionCacheImpl::_refresh，注意这里交换后，_endingSessions _activeSessions都为空了，
    //也就是者两个变量中只会记录两个刷新周期的相应session信息
    //LogicalSessionCacheImpl::_addToCache中新增session
    //LogicalSessionCacheImpl::_refresh中剔除end session存入system.sessions表中
    LogicalSessionIdMap<LogicalSessionRecord> _activeSessions;
    //LogicalSessionCacheImpl::endSessions中新增end session
    LogicalSessionIdSet _endingSessions;

    Date_t lastRefreshTime;
};

}  // namespace mongo
