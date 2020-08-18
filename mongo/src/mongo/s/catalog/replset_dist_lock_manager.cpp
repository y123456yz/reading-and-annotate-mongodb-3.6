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
    //generateDistLockProcessId生成
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

	//DistLockCatalogImpl::stopPing
	//从config.pings中移除id:processId这条记录
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
/* 
线程栈打印如下:
[root@XX ~]# pstack  438787
Thread 1 (process 438787):
#0  0x00007f92b9e2b965 in pthread_cond_wait@@GLIBC_2.3.2 () from /lib64/libpthread.so.0
#1  0x0000555ca0f46bdc in std::condition_variable::wait(std::unique_lock<std::mutex>&) ()
#2  0x0000555ca084936b in mongo::executor::ThreadPoolTaskExecutor::wait(mongo::executor::TaskExecutor::CallbackHandle const&) ()
#3  0x0000555ca057e422 in mongo::ShardRemote::_runCommand(mongo::OperationContext*, mongo::ReadPreferenceSetting const&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, mongo::Duration<std::ratio<1l, 1000l> >, mongo::BSONObj const&) ()
#4  0x0000555ca05bce24 in mongo::Shard::runCommandWithFixedRetryAttempts(mongo::OperationContext*, mongo::ReadPreferenceSetting const&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, mongo::BSONObj const&, mongo::Duration<std::ratio<1l, 1000l> >, mongo::Shard::RetryPolicy) ()
#5  0x0000555ca027b985 in mongo::DistLockCatalogImpl::ping(mongo::OperationContext*, mongo::StringData, mongo::Date_t) ()
#6  0x0000555ca0273aba in mongo::ReplSetDistLockManager::doTask() ()
#7  0x0000555ca0f439f0 in ?? ()
#8  0x00007f92b9e27dd5 in start_thread () from /lib64/libpthread.so.0
#9  0x00007f92b9b50ead in clone () from /lib64/libc.so.6
[root@XX ~]# 
[root@XX ~]# 
[root@XX ~]# 
[root@XX ~]# pstack  438787
Thread 1 (process 438787):
#0  0x00007f92b9e2bd12 in pthread_cond_timedwait@@GLIBC_2.3.2 () from /lib64/libpthread.so.0
#1  0x0000555ca0274184 in mongo::ReplSetDistLockManager::doTask() ()
#2  0x0000555ca0f439f0 in ?? ()
#3  0x00007f92b9e27dd5 in start_thread () from /lib64/libpthread.so.0
#4  0x00007f92b9b50ead in clone () from /lib64/libc.so.6

*/
//mongos mongod cfg都启用了该线程
void ReplSetDistLockManager::doTask() {
	//mongod打印 I SHARDING [thread1] I SHARDING [thread1] creating distributed lock ping thread for process bjcp4287:20001:1581573577:-6950517477465643150 (sleeping for 30000ms)
	//config打印 I SHARDING [thread1] creating distributed lock ping thread for process ConfigServer (sleeping for 30000ms)
	LOG(0) << "creating distributed lock ping thread for process " << _processID
           << " (sleeping for " << _pingInterval << ")";

    Timer elapsedSincelastPing(_serviceContext->getTickSource());
    Client::initThread("replSetDistLockPinger");

    while (!isShutDown()) {
        {
            auto opCtx = cc().makeOperationContext();
/*
2020-07-09T11:21:00.163+0800 I COMMAND  [replSetDistLockPinger] command config.lockpings 
command: findAndModify { findAndModify: "lockpings", query: { _id: "ConfigServer" }, update: 
{ $set: { ping: new Date(1594264859964) } }, upsert: true, writeConcern: { w: "majority", wtimeout: 
15000 }, $db: "config" } planSummary: IDHACK keysExamined:1 docsExamined:1 nMatched:1 nModified:1 
keysInserted:1 keysDeleted:1 numYields:0 reslen:322 locks:{ Global: { acquireCount: { r: 2, w: 2 } }, 
Database: { acquireCount: { w: 2 } }, Collection: { acquireCount: { w: 1 } }, oplog: { acquireCount: 
{ w: 1 } } } protocol:op_msg 199ms
*/
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
			//elapsedSincelastPing时间重置
            elapsedSincelastPing.reset();

            std::deque<std::pair<DistLockHandle, boost::optional<std::string>>> toUnlockBatch;
            {
				//等锁
                stdx::unique_lock<stdx::mutex> lk(_mutex);
                toUnlockBatch.swap(_unlockList);
            }

			//ReplSetDistLockManager::lockWithSessionID获取锁的时候出现的极端异常情况需要这里统一处理
            for (const auto& toUnlock : toUnlockBatch) {
                std::string nameMessage = "";
                Status unlockStatus(ErrorCodes::NotYetInitialized,
                                    "status unlock not initialized!");
                if (toUnlock.second) {
                    // A non-empty _id (name) field was provided, unlock by ts (sessionId) and _id.
                    //DistLockCatalogImpl::unlock
                    unlockStatus = _catalog->unlock(opCtx.get(), toUnlock.first, *toUnlock.second);
                    nameMessage = " and " + LocksType::name() + ": " + *toUnlock.second;
                } else {
                	//DistLockCatalogImpl::unlock
                    unlockStatus = _catalog->unlock(opCtx.get(), toUnlock.first);
                }

                if (!unlockStatus.isOK()) {
                    warning() << "Failed to unlock lock with " << LocksType::lockID() << ": "
                              << toUnlock.first << nameMessage << causedBy(unlockStatus);
					//重新入队，等待下次循环unlock
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

		//等锁
        stdx::unique_lock<stdx::mutex> lk(_mutex);
        MONGO_IDLE_THREAD_BLOCK;
		//等待最多_pingIntervals,默认30s,也就是30秒向cfg进行config.lockpings操作，也就是30s检查一次
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

	//从_pingHistory表中查找没找到, 说明又有一个需要获取该分布式锁的任务，把本次获取锁的任务信息记录到_pingHistory表中
	//如果下次又有新任务获取这个锁，同时发现该锁还是获取失败，则需要检查判断该锁是否过期，如果过期，则在外层需要强制过期了。
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

	// ping is active 
	//集群中的这个节点和cfg通信正常，所以会每隔30s更新一次，就不会相等, 也就是对应节点一直在线，
	//锁需要对应节点自己解锁，更新在ReplSetDistLockManager::doTask
    if (pingInfo->lastPing != pingValue ||  

        // Owner of this lock is now different from last time so we can't
        // use the ping data.
        pingInfo->lockSessionId != lockDoc.getLockID() ||

        // Primary changed, we can't trust that clocks are synchronized so
        // treat as if this is a new entry.
        //说明发生了主从切换
        pingInfo->electionId != serverInfo.electionId) {
        pingInfo->lastPing = pingValue;
        pingInfo->electionId = serverInfo.electionId;
        pingInfo->configLocalTime = configServerLocalTime;
        pingInfo->lockSessionId = lockDoc.getLockID();
        return false;
    }

	//过期时间点比历史记录表中的configLocalTime还小，直接返回false，一般不会出现这种情况
    if (configServerLocalTime < pingInfo->configLocalTime) {
        warning() << "config server local time went backwards, from last seen: "
                  << pingInfo->configLocalTime << " to " << configServerLocalTime;
        return false;
    }

	//也就是lockDoc对应的锁持有时间超过了lockExpiration(一般是持有该锁的实例和cfg失联过久)，说明持有锁时间超时了
	//多次获取锁失败，并且从第一次获取锁失败到本次获取锁还是失败的时间间隔超过lockExpiration时间，则该任务强制获取该锁(外层实现)
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

		//如果获取锁的实例已经很久没有和cfg通过lockpings保活了，这该实例可能失联，所以需要过期检查，避免一直持有锁，lock过期时间
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

		//DistLockCatalogImpl::grabLock 尝试获取锁
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
		//可能一会儿就有主节点了，例如当前正在主从切换过程中，则需要重试
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
			//如果获取该锁的实例已经很久没有和cfg通信了，则可能该锁已经
            auto isLockExpiredResult = isLockExpired(opCtx, currentLock, lockExpiration);

			//说明该锁还没有过期，或者有异常(如cfg没有master等)
            if (!isLockExpiredResult.isOK()) {
                return isLockExpiredResult.getStatus();
            }

			//
            if (isLockExpiredResult.getValue() || (lockSessionID == currentLock.getLockID())) {
				//DistLockCatalogImpl::overtakeLock 强制获取锁
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
