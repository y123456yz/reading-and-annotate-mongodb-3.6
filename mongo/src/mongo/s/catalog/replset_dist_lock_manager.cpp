/**
 *    Copyright (C) 2015 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kSharding

#include "mongo/platform/basic.h"

#include "mongo/s/catalog/replset_dist_lock_manager.h"

#include "mongo/base/status.h"
#include "mongo/base/status_with.h"
#include "mongo/db/operation_context_noop.h"
#include "mongo/db/service_context.h"
#include "mongo/s/catalog/dist_lock_catalog.h"
#include "mongo/s/catalog/type_lockpings.h"
#include "mongo/s/catalog/type_locks.h"
#include "mongo/s/client/shard_registry.h"
#include "mongo/s/grid.h"
#include "mongo/stdx/chrono.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/concurrency/idle_thread_block.h"
#include "mongo/util/concurrency/thread_name.h"
#include "mongo/util/fail_point_service.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"
#include "mongo/util/time_support.h"
#include "mongo/util/timer.h"

namespace mongo {

MONGO_FP_DECLARE(setDistLockTimeout);

using std::string;
using std::unique_ptr;

namespace {

// How many times to retry acquiring the lock after the first attempt fails
//获取lock重试次数
const int kMaxNumLockAcquireRetries = 2;

// How frequently to poll the distributed lock when it is found to be locked
//重试获取lock的频率
const Milliseconds kLockRetryInterval(500);

}  // namespace

const Seconds ReplSetDistLockManager::kDistLockPingInterval{30};
const Minutes ReplSetDistLockManager::kDistLockExpirationTime{15};

//makeCatalogClient中构造使用
ReplSetDistLockManager::ReplSetDistLockManager(ServiceContext* globalContext,
                                               StringData processID,
                                               unique_ptr<DistLockCatalog> catalog,
                                               Milliseconds pingInterval,
                                               Milliseconds lockExpiration)
    : _serviceContext(globalContext),
      _processID(processID.toString()),
      //对应DistLockCatalogImpl
      _catalog(std::move(catalog)),
      //ReplSetDistLockManager::kDistLockPingInterval   ping周期
      _pingInterval(pingInterval),
      // ReplSetDistLockManager::kDistLockExpirationTime  锁占用最长时间
      _lockExpiration(lockExpiration) {}

ReplSetDistLockManager::~ReplSetDistLockManager() = default;

void ReplSetDistLockManager::startUp() {
    if (!_execThread) {
        _execThread = stdx::make_unique<stdx::thread>(&ReplSetDistLockManager::doTask, this);
    }
}

void ReplSetDistLockManager::shutDown(OperationContext* opCtx) {
    {
        stdx::lock_guard<stdx::mutex> lk(_mutex);
        _isShutDown = true;
        _shutDownCV.notify_all();
    }

    // Don't grab _mutex, otherwise will deadlock trying to join. Safe to read
    // _execThread since it is modified only at statrUp().
    if (_execThread && _execThread->joinable()) {
        _execThread->join();
        _execThread.reset();
    }

    auto status = _catalog->stopPing(opCtx, _processID);
    if (!status.isOK()) {
        warning() << "error encountered while cleaning up distributed ping entry for " << _processID
                  << causedBy(redact(status));
    }
}

std::string ReplSetDistLockManager::getProcessID() {
    return _processID;
}

bool ReplSetDistLockManager::isShutDown() {
    stdx::lock_guard<stdx::mutex> lk(_mutex);
    return _isShutDown;
}

void ReplSetDistLockManager::doTask() {
    LOG(0) << "creating distributed lock ping thread for process " << _processID
           << " (sleeping for " << _pingInterval << ")";

    Timer elapsedSincelastPing(_serviceContext->getTickSource());
    Client::initThread("replSetDistLockPinger");

    while (!isShutDown()) {
        {
            auto opCtx = cc().makeOperationContext();
			//DistLockCatalogImpl::ping
            auto pingStatus = _catalog->ping(opCtx.get(), _processID, Date_t::now());

            if (!pingStatus.isOK() && pingStatus != ErrorCodes::NotMaster) {
                warning() << "pinging failed for distributed lock pinger" << causedBy(pingStatus);
            }

			//也就是循环执行一次的时间
            const Milliseconds elapsed(elapsedSincelastPing.millis());
            if (elapsed > 10 * _pingInterval) {
                warning() << "Lock pinger for proc: " << _processID << " was inactive for "
                          << elapsed << " ms";
            }
            elapsedSincelastPing.reset();

            std::deque<std::pair<DistLockHandle, boost::optional<std::string>>> toUnlockBatch;
            {
                stdx::unique_lock<stdx::mutex> lk(_mutex);
                toUnlockBatch.swap(_unlockList);
            }

			////ReplSetDistLockManager::lockWithSessionID获取锁的时候出现的极端异常情况需要这里统一处理
            for (const auto& toUnlock : toUnlockBatch) {
                std::string nameMessage = "";
                Status unlockStatus(ErrorCodes::NotYetInitialized,
                                    "status unlock not initialized!");
                if (toUnlock.second) {
                    // A non-empty _id (name) field was provided, unlock by ts (sessionId) and _id.
                    unlockStatus = _catalog->unlock(opCtx.get(), toUnlock.first, *toUnlock.second);
                    nameMessage = " and " + LocksType::name() + ": " + *toUnlock.second;
                } else {
                    unlockStatus = _catalog->unlock(opCtx.get(), toUnlock.first);
                }

                if (!unlockStatus.isOK()) {
                    warning() << "Failed to unlock lock with " << LocksType::lockID() << ": "
                              << toUnlock.first << nameMessage << causedBy(unlockStatus);
                    queueUnlock(toUnlock.first, toUnlock.second);
                } else {
                    LOG(0) << "distributed lock with " << LocksType::lockID() << ": "
                           << toUnlock.first << nameMessage << " unlocked.";
                }

                if (isShutDown()) {
                    return;
                }
            }
        }

        stdx::unique_lock<stdx::mutex> lk(_mutex);
        MONGO_IDLE_THREAD_BLOCK;
        _shutDownCV.wait_for(lk, _pingInterval.toSystemDuration(), [this] { return _isShutDown; });
    }
}

//ReplSetDistLockManager::lockWithSessionID调用
//检查lockDoc对应的锁是否过期
StatusWith<bool> ReplSetDistLockManager::isLockExpired(OperationContext* opCtx,
													   //config.locks中的lockDoc文档
                                                       LocksType lockDoc,
                                                       const Milliseconds& lockExpiration) {
	//获取locks表中的LocksType._process信息
	const auto& processID = lockDoc.getProcess(); 
	//获取lockping表中通过processID查找对应lockDoc数据 
    auto pingStatus = _catalog->getPing(opCtx, processID);

    Date_t pingValue;
    if (pingStatus.isOK()) {
        const auto& pingDoc = pingStatus.getValue();
		//返回数据的有效性检查
        Status pingDocValidationStatus = pingDoc.validate();
        if (!pingDocValidationStatus.isOK()) {
            return {ErrorCodes::UnsupportedFormat,
                    str::stream() << "invalid ping document for " << processID << ": "
                                  << pingDocValidationStatus.toString()};
        }

		//获取对应数据的ping信息，类似这里面的ping值{ "_id" : "ConfigServer", "ping" : ISODate("2020-08-09T10:29:20.291Z") }
        pingValue = pingDoc.getPing();
    } else if (pingStatus.getStatus() != ErrorCodes::NoMatchingDocument) {
        return pingStatus.getStatus();
    }  // else use default pingValue if ping document does not exist.

	//初始化一个计时器
    Timer timer(_serviceContext->getTickSource());
	//从serverstatus中解析出localTime和repl.electionId字段，填充到DistLockCatalog::ServerInfo结构
	//DistLockCatalogImpl::getServerInfo
	auto serverInfoStatus = _catalog->getServerInfo(opCtx);
    if (!serverInfoStatus.isOK()) { //异常
		////没有主节点
        if (serverInfoStatus.getStatus() == ErrorCodes::NotMaster) {
            return false; //没有主节点
        }

		//直接返回DistLockCatalog::ServerInfo信息
        return serverInfoStatus.getStatus();
    }

    // Be conservative when determining that lock expiration has elapsed by
    // taking into account the roundtrip delay of trying to get the local
    // time from the config server.
    Milliseconds delay(timer.millis() / 2);  // Assuming symmetrical delay.

	//获取DistLockCatalog::ServerInfo信息
    const auto& serverInfo = serverInfoStatus.getValue();

    stdx::lock_guard<stdx::mutex> lk(_mutex);
	//stdx::unordered_map<std::string, DistLockPingInfo> _pingHistory; 
	//根据config.locks表中的_id字段在_pingHistory map表中查找
    auto pingIter = _pingHistory.find(lockDoc.getName());//LocksType::getName

	//从_pingHistory表中查找没找到, 说明不应该过期，直接返回false
    if (pingIter == _pingHistory.end()) {
        // We haven't seen this lock before so we don't have any point of reference
        // to compare and determine the elapsed time. Save the current ping info
        // for this lock.
        //
        _pingHistory.emplace(std::piecewise_construct,
                             std::forward_as_tuple(lockDoc.getName()),
                             		//也就是config.locks中的process字段，即config.lockpings中的_id字段
                             std::forward_as_tuple(processID, 
								   //config.lockpings中的ping字段内容
								   pingValue,  //缓存到lastPing，下面用到if (pingInfo->lastPing != pingValue 
	                               //也就是db.serverStatus().localTime
	                               serverInfo.serverTime, 
	                               //config.locks表中的ts字段
	                               lockDoc.getLockID(),
	                               //db.serverStatus().repl.electionId获取的值
	                               serverInfo.electionId));
        return false;
    }

	////db.serverStatus().localTime - delay;
    auto configServerLocalTime = serverInfo.serverTime - delay;

	//获取对应的DistLockPingInfo
    auto* pingInfo = &pingIter->second;

    LOG(1) << "checking last ping for lock '" << lockDoc.getName() << "' against last seen process "
           << pingInfo->processId << " and ping " << pingInfo->lastPing;

    if (pingInfo->lastPing != pingValue ||  // ping is active

        // Owner of this lock is now different from last time so we can't
        // use the ping data.
        pingInfo->lockSessionId != lockDoc.getLockID() ||

        // Primary changed, we can't trust that clocks are synchronized so
        // treat as if this is a new entry.
        pingInfo->electionId != serverInfo.electionId) {
        pingInfo->lastPing = pingValue;
        pingInfo->electionId = serverInfo.electionId;
        pingInfo->configLocalTime = configServerLocalTime;
        pingInfo->lockSessionId = lockDoc.getLockID();
        return false;
    }

	//过期时间点比历史记录表中的configLocalTime还小，直接返回false
    if (configServerLocalTime < pingInfo->configLocalTime) {
        warning() << "config server local time went backwards, from last seen: "
                  << pingInfo->configLocalTime << " to " << configServerLocalTime;
        return false;
    }

	//也就是lockDoc对应的锁持有时间超过了lockExpiration，说明持有锁时间超时了
    Milliseconds elapsedSinceLastPing(configServerLocalTime - pingInfo->configLocalTime);
    if (elapsedSinceLastPing >= lockExpiration) {
        LOG(0) << "forcing lock '" << lockDoc.getName() << "' because elapsed time "
               << elapsedSinceLastPing << " >= takeover time " << lockExpiration;
        return true;
    }

	//说明锁还没有超时，没有过期
    LOG(1) << "could not force lock '" << lockDoc.getName() << "' because elapsed time "
           << durationCount<Milliseconds>(elapsedSinceLastPing) << " < takeover time "
           << durationCount<Milliseconds>(lockExpiration) << " ms";
    return false;
}

//获取分布式锁 
//DistLockManager::lock调用
StatusWith<DistLockHandle> ReplSetDistLockManager::lockWithSessionID(OperationContext* opCtx,
                                                                     StringData name,
                                                                     StringData whyMessage,
                                                                     const OID& lockSessionID,
    	                                                                Milliseconds waitFor) {
	//计时器  
	Timer timer(_serviceContext->getTickSource());
    Timer msgTimer(_serviceContext->getTickSource());

    // Counts how many attempts have been made to grab the lock, which have failed with network
    // error. This value is reset for each lock acquisition attempt because these are
    // independent write operations.
    //网络异常重试次数
    int networkErrorRetries = 0;

	//获取configShard信息
    auto configShard = Grid::get(opCtx)->shardRegistry()->getConfigShard();

    // Distributed lock acquisition works by tring to update the state of the lock to 'taken'. If
    // the lock is currently taken, we will back off and try the acquisition again, repeating this
    // until the lockTryInterval has been reached. If a network error occurs at each lock
    // acquisition attempt, the lock acquisition will be retried immediately.
    //获取分布式锁，获取失败则重试
    while (waitFor <= Milliseconds::zero() || Milliseconds(timer.millis()) < waitFor) {
        const string who = str::stream() << _processID << ":" << getThreadName();

		//lock过期时间
        auto lockExpiration = _lockExpiration;
        MONGO_FAIL_POINT_BLOCK(setDistLockTimeout, customTimeout) {
            const BSONObj& data = customTimeout.getData();
            lockExpiration = Milliseconds(data["timeoutMs"].numberInt());
        }
		/*  sh.enableSharding("test") 对应打印如下:
		 D SHARDING [conn13] trying to acquire new distributed lock for test-movePrimary ( lock timeout : 900000 ms, ping interval : 30000 ms, 
		     process : ConfigServer ) with lockSessionID: 5f29454be701d489a5999c54, why: enableSharding
		 D SHARDING [conn13] trying to acquire new distributed lock for test ( lock timeout : 900000 ms, ping interval : 30000 ms, 
		     process : ConfigServer ) with lockSessionID: 5f29454be701d489a5999c58, why: enableShardin
		*/

        LOG(1) << "trying to acquire new distributed lock for " << name
               << " ( lock timeout : " << durationCount<Milliseconds>(lockExpiration)
               << " ms, ping interval : " << durationCount<Milliseconds>(_pingInterval)
               << " ms, process : " << _processID << " )"
               << " with lockSessionID: " << lockSessionID << ", why: " << whyMessage.toString();

		//DistLockCatalogImpl::grabLock
        auto lockResult = _catalog->grabLock(
            opCtx, name, lockSessionID, who, _processID, Date_t::now(), whyMessage.toString());

        auto status = lockResult.getStatus();

		//获取锁成功，返回新的lockSessionID
        if (status.isOK()) {
            // Lock is acquired since findAndModify was able to successfully modify
            // the lock document.

			/*
		     例如enableShard (test)打印信息如下:
		     distributed lock 'test-movePrimary' acquired for 'enableSharding', ts : 5f29344b032e473f1999e552
		     distributed lock 'test' acquired for 'enableSharding', ts : 5f29344b032e473f1999e556
			*/
            log() << "distributed lock '" << name << "' acquired for '" << whyMessage.toString()
                  << "', ts : " << lockSessionID;
            return lockSessionID;
        }

		//获取分布式锁没有成功，则继续下面的处理
		

        // If a network error occurred, unlock the lock synchronously and try again
		//ShardRemote::isRetriableError 通过RemoteCommandRetryScheduler::kAllRetriableErrors获取错误码
		//说明网络错误或者没有主节点，如果本函数在cfg执行说明没有主节点，cfg不存在远程调用
		if (configShard->isRetriableError(status.code(), Shard::RetryPolicy::kIdempotent) &&
            networkErrorRetries < kMaxNumLockAcquireRetries) {
            LOG(1) << "Failed to acquire distributed lock because of retriable error. Retrying "
                      "acquisition by first unlocking the stale entry, which possibly exists now"
                   << causedBy(redact(status));

			//异常次数
            networkErrorRetries++;

			//DistLockCatalogImpl::unlock
			//config.locks表中的{ts:lockSessionID, _id:name}这条数据对应的stat字段设置为0，也就是解锁
            status = _catalog->unlock(opCtx, lockSessionID, name);
            if (status.isOK()) {
                // We certainly do not own the lock, so we can retry
                //继续重试获取lock, 可能是因为cfg没有主节点 
                continue;
            }

            // Fall-through to the error checking logic below
            invariant(status != ErrorCodes::LockStateChangeFailed);

			//多次重试都没有成功获取到锁，异常打印
            LOG(1)
                << "Failed to retry acquisition of distributed lock. No more attempts will be made"
                << causedBy(redact(status));
        }

        if (status != ErrorCodes::LockStateChangeFailed) {
			//该错误码见extractFindAndModifyNewObj，说明没有在config.locks表中找到name对应的文档
            // An error occurred but the write might have actually been applied on the
            // other side. Schedule an unlock to clean it up just in case.

			//把本次{ts:lockSessionID, _id:name}记录到队列，在ReplSetDistLockManager::doTask()中集中处理集中处理
            queueUnlock(lockSessionID, name.toString());
            return status;
        }

        // Get info from current lock and check if we can overtake it.
        //从config.locks中查找id:name的数据，返回对应的一条LocksType
        auto getLockStatusResult = _catalog->getLockByName(opCtx, name);
        const auto& getLockStatus = getLockStatusResult.getStatus();

		//没找到或者异常
        if (!getLockStatusResult.isOK() && getLockStatus != ErrorCodes::LockNotFound) {
            return getLockStatus;
        }

        // Note: Only attempt to overtake locks that actually exists. If lock was not
        // found, use the normal grab lock path to acquire it.
        if (getLockStatusResult.isOK()) {
			//获取查找到的数据，也就是locks中的一条LocksType
            auto currentLock = getLockStatusResult.getValue();
			//本次获取锁失败，说明锁当前正被其他操作占用，我们可以检查下这个锁是否过期了
            auto isLockExpiredResult = isLockExpired(opCtx, currentLock, lockExpiration);

			//说明该锁还没有过期，或者有异常(如cfg没有master等)
            if (!isLockExpiredResult.isOK()) {
                return isLockExpiredResult.getStatus();
            }

			//
            if (isLockExpiredResult.getValue() || (lockSessionID == currentLock.getLockID())) {
				//DistLockCatalogImpl::overtakeLock
				//把{id:lockID,state:0} or {id:lockID,ts:currentHolderTS}这条数据更新为新的{ts:lockSessionID, state:2,who:who,...}
				auto overtakeResult = _catalog->overtakeLock(opCtx,
                                                             name,
                                                             lockSessionID,
                                                             currentLock.getLockID(),
                                                             who,
                                                             _processID,
                                                             Date_t::now(),
                                                             whyMessage);

                const auto& overtakeStatus = overtakeResult.getStatus();

                if (overtakeResult.isOK()) { //获取锁成功，也就是overtakeLock中update成功
                    // Lock is acquired since findAndModify was able to successfully modify
                    // the lock document.

                    LOG(0) << "lock '" << name << "' successfully forced";
                    LOG(0) << "distributed lock '" << name << "' acquired, ts : " << lockSessionID;
                    return lockSessionID;
                }

                if (overtakeStatus != ErrorCodes::LockStateChangeFailed) {
                    // An error occurred but the write might have actually been applied on the
                    // other side. Schedule an unlock to clean it up just in case.
                    queueUnlock(lockSessionID, boost::none);
                    return overtakeStatus;
                }
            }
        }

        LOG(1) << "distributed lock '" << name << "' was not acquired.";

        if (waitFor == Milliseconds::zero()) {
            break;
        }

        // Periodically message for debugging reasons
        if (msgTimer.seconds() > 10) {
            LOG(0) << "waited " << timer.seconds() << "s for distributed lock " << name << " for "
                   << whyMessage.toString();

            msgTimer.reset();
        }

        // A new lock acquisition attempt will begin now (because the previous found the lock to be
        // busy, so reset the retries counter)
        networkErrorRetries = 0;

        const Milliseconds timeRemaining =
            std::max(Milliseconds::zero(), waitFor - Milliseconds(timer.millis()));
        sleepFor(std::min(kLockRetryInterval, timeRemaining));
    }

    return {ErrorCodes::LockBusy, str::stream() << "timed out waiting for " << name};
}

StatusWith<DistLockHandle> ReplSetDistLockManager::tryLockWithLocalWriteConcern(
    OperationContext* opCtx, StringData name, StringData whyMessage, const OID& lockSessionID) {
    const string who = str::stream() << _processID << ":" << getThreadName();

    LOG(1) << "trying to acquire new distributed lock for " << name
           << " ( lock timeout : " << durationCount<Milliseconds>(_lockExpiration)
           << " ms, ping interval : " << durationCount<Milliseconds>(_pingInterval)
           << " ms, process : " << _processID << " )"
           << " with lockSessionID: " << lockSessionID << ", why: " << whyMessage.toString();

    auto lockStatus = _catalog->grabLock(opCtx,
                                         name,
                                         lockSessionID,
                                         who,
                                         _processID,
                                         Date_t::now(),
                                         whyMessage.toString(),
                                         DistLockCatalog::kLocalWriteConcern);

    if (lockStatus.isOK()) {
        log() << "distributed lock '" << name << "' acquired for '" << whyMessage.toString()
              << "', ts : " << lockSessionID;
        return lockSessionID;
    }

    LOG(1) << "distributed lock '" << name << "' was not acquired.";

    if (lockStatus == ErrorCodes::LockStateChangeFailed) {
        return {ErrorCodes::LockBusy, str::stream() << "Unable to acquire " << name};
    }

    return lockStatus.getStatus();
}

void ReplSetDistLockManager::unlock(OperationContext* opCtx, const DistLockHandle& lockSessionID) {
    auto unlockStatus = _catalog->unlock(opCtx, lockSessionID);

    if (!unlockStatus.isOK()) {
        queueUnlock(lockSessionID, boost::none);
    } else {
        LOG(0) << "distributed lock with " << LocksType::lockID() << ": " << lockSessionID
               << "' unlocked.";
    }
}

void ReplSetDistLockManager::unlock(OperationContext* opCtx,
                                    const DistLockHandle& lockSessionID,
                                    StringData name) {
    auto unlockStatus = _catalog->unlock(opCtx, lockSessionID, name);

    if (!unlockStatus.isOK()) {
        queueUnlock(lockSessionID, name.toString());
    } else {
        LOG(0) << "distributed lock with " << LocksType::lockID() << ": '" << lockSessionID
               << "' and " << LocksType::name() << ": '" << name.toString() << "' unlocked.";
    }
}

void ReplSetDistLockManager::unlockAll(OperationContext* opCtx, const std::string& processID) {
    Status status = _catalog->unlockAll(opCtx, processID);
    if (!status.isOK()) {
        warning() << "Error while trying to unlock existing distributed locks"
                  << causedBy(redact(status));
    }
}

Status ReplSetDistLockManager::checkStatus(OperationContext* opCtx,
                                           const DistLockHandle& lockHandle) {
    return _catalog->getLockByTS(opCtx, lockHandle).getStatus();
}

//ReplSetDistLockManager::lockWithSessionID调用，把异常{ts:lockSessionID, _id:name}记录到_unlockList
//在ReplSetDistLockManager::doTask()中集中处理
void ReplSetDistLockManager::queueUnlock(const DistLockHandle& lockSessionID,
                                         const boost::optional<std::string>& name) {
    stdx::unique_lock<stdx::mutex> lk(_mutex);
    _unlockList.push_back(std::make_pair(lockSessionID, name));
}

}  // namespace mongo
