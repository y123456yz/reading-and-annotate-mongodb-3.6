/**
 *    Copyright (C) 2017 MongoDB Inc.
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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kExecutor;

#include "mongo/platform/basic.h"

#include "mongo/transport/service_executor_synchronous.h"

#include "mongo/db/server_parameters.h"
#include "mongo/stdx/thread.h"
#include "mongo/transport/service_entry_point_utils.h"
#include "mongo/util/log.h"
#include "mongo/util/net/thread_idle_callback.h"
#include "mongo/util/processinfo.h"

namespace mongo {
namespace transport {
namespace {

// Tasks scheduled with MayRecurse may be called recursively if the recursion depth is below this
// value.
MONGO_EXPORT_SERVER_PARAMETER(synchronousServiceExecutorRecursionLimit, int, 8);

//当前线程数，也就是当前conn线程数量
constexpr auto kThreadsRunning = "threadsRunning"_sd;
constexpr auto kExecutorLabel = "executor"_sd;
constexpr auto kExecutorName = "passthrough"_sd;
}  // namespace

//线程级别的变量，只针对本链接session对应的线程
thread_local std::deque<ServiceExecutor::Task> ServiceExecutorSynchronous::_localWorkQueue = {}; //链接入队
thread_local int ServiceExecutorSynchronous::_localRecursionDepth = 0;
thread_local int64_t ServiceExecutorSynchronous::_localThreadIdleCounter = 0;

ServiceExecutorSynchronous::ServiceExecutorSynchronous(ServiceContext* ctx) {}

//获取CPU个数
Status ServiceExecutorSynchronous::start() {
    _numHardwareCores = [] {
        ProcessInfo p;
        if (auto availCores = p.getNumAvailableCores()) {
            return static_cast<size_t>(*availCores);
        }
        return static_cast<size_t>(p.getNumCores());
    }();

    _stillRunning.store(true);

    return Status::OK();
}

//实际上测试发现db.shutdown的时候并没有进入该函数
Status ServiceExecutorSynchronous::shutdown(Milliseconds timeout) {
    LOG(3) << "Shutting down passthrough executor";
	log() << "Shutting down passthrough executor";

    _stillRunning.store(false);

    stdx::unique_lock<stdx::mutex> lock(_shutdownMutex);
    bool result = _shutdownCondition.wait_for(lock, timeout.toSystemDuration(), [this]() {
        return _numRunningWorkerThreads.load() == 0;
    });

    return result
        ? Status::OK()
        : Status(ErrorCodes::Error::ExceededTimeLimit,
                 "passthrough executor couldn't shutdown all worker threads within time limit.");
}

//ServiceStateMachine::_scheduleNextWithGuard 启动新的conn线程
Status ServiceExecutorSynchronous::schedule(Task task, ScheduleFlags flags) {
    if (!_stillRunning.load()) {
        return Status{ErrorCodes::ShutdownInProgress, "Executor is not running"};
    }

	//除了第一次进入该函数会走后面的创建线程流程，后续的任务进来都是进入该if循环，因为状态机中始终会有任务运行
    if (!_localWorkQueue.empty()) {
        /*
         * In perf testing we found that yielding after running a each request produced
         * at 5% performance boost in microbenchmarks if the number of worker threads
         * was greater than the number of available cores.
         */
         //在perf测试中，我们发现，如果工作线程的数量大于可用内核的数量，那么在微基准测试中，运行每个请
         //求后产生的性能提升为5%。
        if (flags & ScheduleFlags::kMayYieldBeforeSchedule) {
            if ((_localThreadIdleCounter++ & 0xf) == 0) {
				//短暂休息会儿后再处理该链接的下一个用户请求
				//实际上是调用TCMalloc MarkThreadTemporarilyIdle实现
                markThreadIdle();
            }
            if (_numRunningWorkerThreads.loadRelaxed() > _numHardwareCores) {
                stdx::this_thread::yield();//线程本次不参与CPU调度，也就是放慢脚步
            }
        }
		//log() << "yang test Starting ServiceExecutorSynchronous::schedule 11";

        // Execute task directly (recurse) if allowed by the caller as it produced better
        // performance in testing. Try to limit the amount of recursion so we don't blow up the
        // stack, even though this shouldn't happen with this executor that uses blocking network
        // I/O.
        /*
		如果调用者允许，直接执行任务(递归)，因为它在测试中产生了更好的性能。尽量限制递归的数量，这样我们
		就不会破坏堆栈，即使对于使用阻塞网络I/O的执行器来说，这是不应该发生的。
		*/
		//本线程优先处理对应链接的
        if ((flags & ScheduleFlags::kMayRecurse) &&  //带kMayRecurse标识，则直接递归执行
            (_localRecursionDepth < synchronousServiceExecutorRecursionLimit.loadRelaxed())) {
            ++_localRecursionDepth;
			if (_localRecursionDepth > 2)
				log() << "yang test Starting digui ##  1111111111111 ServiceExecutorSynchronous::schedule, depth:" << _localRecursionDepth;
            task();
        } else {
        	if (_localRecursionDepth > 2)
        		log() << "yang test Starting no digui ## 222222222222 ServiceExecutorSynchronous::schedule, depth:" << _localRecursionDepth;
            _localWorkQueue.emplace_back(std::move(task)); //入队
        }
        return Status::OK();
    }

    // First call to schedule() for this connection, spawn a worker thread that will push jobs
    // into the thread local job queue.
    log() << "Starting new executor thread in passthrough mode";

	//创建conn线程，执行对应的task
    Status status = launchServiceWorkerThread([ this, task = std::move(task) ] {
		//这个func是线程回调函数
	
        int ret = _numRunningWorkerThreads.addAndFetch(1);

		//task对应 ServiceStateMachine::_runNextInGuard
        _localWorkQueue.emplace_back(std::move(task));
		//每个新链接都会在该while中循环进行网络IO处理和DB storage处理
        while (!_localWorkQueue.empty() && _stillRunning.loadRelaxed()) {
			if (_localRecursionDepth > 2)
				log() << "yang test Starting while deal ## 333333333333 ServiceExecutorSynchronous::schedule, depth:" << _localRecursionDepth;
            _localRecursionDepth = 1;
			//log() << "Starting new executor thread in passthrough mode yang tesst 11 size:" << _localWorkQueue.size() << "  _numRunningWorkerThreads:" << ret;
			//队列中获取一个task，并执行, task执行过程中会走入SSM状态机，会一直循环，除非该线程对应的客户端关闭链接才会走到下面的_localWorkQueue.pop_front();
			//对应:ServiceStateMachine::_runNextInGuard  该线程负责接收新链接的所有数据包解析处理
            _localWorkQueue.front()(); 
			
            _localWorkQueue.pop_front();  //去除该task删除
            
        }
		ret = _numRunningWorkerThreads.subtractAndFetch(1);
		//log() << "Starting new executor thread in passthrough mode yang tesst 22 size:" << _localWorkQueue.size() << "	_numRunningWorkerThreads:" << ret;
		if (ret == 0) { //当最后一个链接断开的时候会走到该if
        //if (_numRunningWorkerThreads.subtractAndFetch(1) == 0) { //
        	//说明已经没有可用链接了，shutdown可以真正退出了
        	//mongo shell敲shutdown的时候，只有当没有任何链接存在的时候才能真正的退出
            _shutdownCondition.notify_all();
			//log() << "Starting new executor thread in passthrough mode yang tesst 44";
        }

		//客户端对应链接断开的时候走到这里
		LOG(3) << "Starting new executor thread in passthrough mode yang tesst end ";
		//log() << "Starting new executor thread in passthrough mode yang tesst end ";
    });

    return status;
}

/*
mongos> db.serverStatus().network
{
        "bytesIn" : NumberLong("32650556117"),
        "bytesOut" : NumberLong("596811224034"),
        "physicalBytesIn" : NumberLong("32650556117"),
        "physicalBytesOut" : NumberLong("596811224034"),
        "numRequests" : NumberLong(238541401),
        "compression" : {
                "snappy" : {
                        "compressor" : {
                                "bytesIn" : NumberLong("11389624237"),
                                "bytesOut" : NumberLong("10122531881")
                        },
                        "decompressor" : {
                                "bytesIn" : NumberLong("54878702006"),
                                "bytesOut" : NumberLong("341091660385")
                        }
                }
        },
        "serviceExecutorTaskStats" : {
                "executor" : "passthrough",
                "threadsRunning" : 102
        }
}
*/
void ServiceExecutorSynchronous::appendStats(BSONObjBuilder* bob) const {
    BSONObjBuilder section(bob->subobjStart("serviceExecutorTaskStats"));
    section << kExecutorLabel << kExecutorName << kThreadsRunning
            << static_cast<int>(_numRunningWorkerThreads.loadRelaxed());
}

}  // namespace transport
}  // namespace mongo
