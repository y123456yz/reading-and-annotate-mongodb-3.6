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

#include "mongo/transport/service_executor_adaptive.h"

#include <random>

#include "mongo/db/server_parameters.h"
#include "mongo/transport/service_entry_point_utils.h"
#include "mongo/util/concurrency/thread_name.h"
#include "mongo/util/log.h"
#include "mongo/util/processinfo.h"
#include "mongo/util/scopeguard.h"
#include "mongo/util/stringutils.h"

#include <asio.hpp>

namespace mongo {
namespace transport {
namespace {
// The executor will always keep this many number of threads around. If the value is -1,
// (the default) then it will be set to number of cores / 2.
//实时在线调整db.adminCommand( { setParameter: 1, adaptiveServiceExecutorReservedThreads: 10} ) 

//最少默认都要这么多个线程运行
MONGO_EXPORT_SERVER_PARAMETER(adaptiveServiceExecutorReservedThreads, int, 1); //yang change debug

// Each worker thread will allow ASIO to run for this many milliseconds before checking
// whether it should exit
//在判断该线程是否应该销毁之前，线程可以运行网络异步ASIO相关的功能这么多时间
MONGO_EXPORT_SERVER_PARAMETER(adaptiveServiceExecutorRunTimeMillis, int, 5000);

// The above parameter will be offset of some random value between -runTimeJitters/
// +runTimeJitters so that not all threads are starting/stopping execution at the same time
//随机数，保证所有线程不会同时创建和销毁
MONGO_EXPORT_SERVER_PARAMETER(adaptiveServiceExecutorRunTimeJitterMillis, int, 500);

// This is the maximum amount of time the controller thread will sleep before doing any
// stuck detection
//controller控制线程最大睡眠时间
MONGO_EXPORT_SERVER_PARAMETER(adaptiveServiceExecutorStuckThreadTimeoutMillis, int, 250);

// The maximum allowed latency between when a task is scheduled and a thread is started to
// service it.
//任务被调度等待执行的最长时间
MONGO_EXPORT_SERVER_PARAMETER(adaptiveServiceExecutorMaxQueueLatencyMicros, int, 500);

// Threads will exit themselves if they spent less than this percentage of the time they ran
// doing actual work.
//如果一个线程处理请求的时间/总时间(处理请求时间)+空闲时间  也就是如果线程比较空闲
//执行 IO 操作的时间小于 adaptiveServiceExecutorIdlePctThreshold 比例时，则会自动销毁线程
//线程空闲百分百
//db.adminCommand( { setParameter: 1, adaptiveServiceExecutorIdlePctThreshold: 30} ) 

MONGO_EXPORT_SERVER_PARAMETER(adaptiveServiceExecutorIdlePctThreshold, int, 60);

// Tasks scheduled with MayRecurse may be called recursively if the recursion depth is below this
// value.
//任务递归调用的深度最大值
MONGO_EXPORT_SERVER_PARAMETER(adaptiveServiceExecutorRecursionLimit, int, 8);

//db.serverStatus().network.serviceExecutorTaskStats获取
constexpr auto kTotalQueued = "totalQueued"_sd;
constexpr auto kTotalExecuted = "totalExecuted"_sd;
constexpr auto kTasksQueued = "tasksQueued"_sd;
constexpr auto kDeferredTasksQueued = "deferredTasksQueued"_sd;
//在线程池内执行的时间，单位微秒。注意这个时间是所有worker的线程的汇总信息，包含历史worker的统计时间。
constexpr auto kTotalTimeExecutingUs = "totalTimeExecutingMicros"_sd;
constexpr auto kTotalTimeRunningUs = "totalTimeRunningMicros"_sd;
constexpr auto kTotalTimeQueuedUs = "totalTimeQueuedMicros"_sd;
// 正在使用(有IO操作)的worker线程数
constexpr auto kThreadsInUse = "threadsInUse"_sd;
constexpr auto kThreadsRunning = "threadsRunning"_sd;
constexpr auto kThreadsPending = "threadsPending"_sd;
constexpr auto kExecutorLabel = "executor"_sd;
constexpr auto kExecutorName = "adaptive"_sd;

//ticks转换为以us为单位的值
int64_t ticksToMicros(TickSource::Tick ticks, TickSource* tickSource) {
    invariant(tickSource->getTicksPerSecond() >= 1000000);
	//一个us包含多少个ticks,默认1
    static const auto ticksPerMicro = tickSource->getTicksPerSecond() / 1000000;
    return ticks / ticksPerMicro;
}

struct ServerParameterOptions : public ServiceExecutorAdaptive::Options {
    int reservedThreads() const final {
        int value = adaptiveServiceExecutorReservedThreads.load();
        if (value == -1) { //默认线程数等于CPU核心数/2
            ProcessInfo pi;
            value = pi.getNumAvailableCores().value_or(pi.getNumCores()) / 2;
            value = std::max(value, 2);
            adaptiveServiceExecutorReservedThreads.store(value);
            log() << "No thread count configured for executor. Using number of cores / 2: "
                  << value;
        }
        return value;
    }

	//在判断该线程是否应该销毁之前，线程可以运行网络异步ASIO相关的功能这么多时间
    Milliseconds workerThreadRunTime() const final {
        return Milliseconds{adaptiveServiceExecutorRunTimeMillis.load()};
    }

	////随机数，保证所有线程不会同时创建和销毁
    int runTimeJitter() const final {
        return adaptiveServiceExecutorRunTimeJitterMillis.load();
    }
	
	//controller控制线程最大睡眠时间
    Milliseconds stuckThreadTimeout() const final {
        return Milliseconds{adaptiveServiceExecutorStuckThreadTimeoutMillis.load()};
    }
	
	//worker-control线程等待work pending线程的时间  参考ServiceExecutorAdaptive::_controllerThreadRoutine
    Microseconds maxQueueLatency() const final {
        return Microseconds{adaptiveServiceExecutorMaxQueueLatencyMicros.load()};
    }

	//线程空闲百分百，决定worker线程是否退出及其controller线程启动新的worker线程
	//参考ServiceExecutorAdaptive::_controllerThreadRoutine
    int idlePctThreshold() const final {
        return adaptiveServiceExecutorIdlePctThreshold.load();
    }

	//任务递归调用的深度最大值
    int recursionLimit() const final {
        return adaptiveServiceExecutorRecursionLimit.load();
    }
};

}  // namespace

thread_local ServiceExecutorAdaptive::ThreadState* ServiceExecutorAdaptive::_localThreadState =
    nullptr;

//ServiceExecutorAdaptive类构造函数 
//TransportLayerManager::createWithConfig赋值调用
ServiceExecutorAdaptive::ServiceExecutorAdaptive(ServiceContext* ctx,
                                                 std::shared_ptr<asio::io_context> ioCtx)
    : ServiceExecutorAdaptive(ctx, std::move(ioCtx), stdx::make_unique<ServerParameterOptions>()) {}

//上面的TransportLayerManager::createWithConfig调用
ServiceExecutorAdaptive::ServiceExecutorAdaptive(ServiceContext* ctx,
                                                 std::shared_ptr<asio::io_context> ioCtx,
                                                 std::unique_ptr<Options> config)
    : _ioContext(std::move(ioCtx)),
      _config(std::move(config)),
      _tickSource(ctx->getTickSource()),
      _lastScheduleTimer(_tickSource) {}

ServiceExecutorAdaptive::~ServiceExecutorAdaptive() {
    invariant(!_isRunning.load());
}

//runMongosServer中调用
Status ServiceExecutorAdaptive::start() {
    invariant(!_isRunning.load());
    _isRunning.store(true);
	//线程回调ServiceExecutorAdaptive::_controllerThreadRoutine
    _controllerThread = stdx::thread(&ServiceExecutorAdaptive::_controllerThreadRoutine, this);
    for (auto i = 0; i < _config->reservedThreads(); i++) {
        _startWorkerThread(); //启动时候默认启用CPU核心数/2个worker线程
    }

    return Status::OK();
}

Status ServiceExecutorAdaptive::shutdown(Milliseconds timeout) {
    if (!_isRunning.load())
        return Status::OK();

    _isRunning.store(false);

    _scheduleCondition.notify_one();
    _controllerThread.join();

    stdx::unique_lock<stdx::mutex> lk(_threadsMutex);
    _ioContext->stop();
    bool result =
        _deathCondition.wait_for(lk, timeout.toSystemDuration(), [&] { return _threads.empty(); });

    return result
        ? Status::OK()
        : Status(ErrorCodes::Error::ExceededTimeLimit,
                 "adaptive executor couldn't shutdown all worker threads within time limit.");
}

//ServiceStateMachine::_scheduleNextWithGuard
//ServiceStateMachine::_scheduleNextWithGuard  
//adaptive模式，分发链接的线程给_ioContext

//ServiceExecutorAdaptive::schedule中任务入队，ServiceExecutorAdaptive::_workerThreadRoutine出对执行
Status ServiceExecutorAdaptive::schedule(ServiceExecutorAdaptive::Task task, ScheduleFlags flags) {
	//获取当前时间
	auto scheduleTime = _tickSource->getTicks();

	//kTasksQueued: 当前入队还没执行的task数
    //_deferredTasksQueued: 当前入队还没执行的deferredTask数
	//defered task和普通task分开记录   _totalQueued=_deferredTasksQueued+_tasksQueued
    auto pendingCounterPtr = (flags & kDeferredTask) ? &_deferredTasksQueued : &_tasksQueued;
    pendingCounterPtr->addAndFetch(1); 

    if (!_isRunning.load()) {
        return {ErrorCodes::ShutdownInProgress, "Executor is not running"};
    }

    auto wrappedTask = [ this, task = std::move(task), scheduleTime, pendingCounterPtr ] {
		//worker线程回调会执行该wrappedTask，
        pendingCounterPtr->subtractAndFetch(1);
        auto start = _tickSource->getTicks();
		//从任务被调度入队，到真正被执行这段过程的时间，也就是等待被调度的时间
        _totalSpentQueued.addAndFetch(start - scheduleTime); //从任务被调度入队，到真正被执行这段过程的时间

        if (_localThreadState->recursionDepth++ == 0) {
			//记录wrappedTask被worker线程调度执行的起始时间
            _localThreadState->executing.markRunning();
			//当前正在执行wrappedTask的线程加1
            _threadsInUse.addAndFetch(1);
        }

		//guard实际上是在 _ioContext->run_for(runTime.toSystemDuration());中调用的
		//ServiceExecutorAdaptive::_workerThreadRoutine执行wrappedTask后会调用guard这里的func 
        const auto guard = MakeGuard([this, start] {
            if (--_localThreadState->recursionDepth == 0) {
				//wrappedTask任务被执行消耗的总时间   _localThreadState->executing.markStopped()代表任务该task执行的时间
                _localThreadState->executingCurRun += _localThreadState->executing.markStopped();
				//下面的task()执行完后，正在执行task的线程-1
				_threadsInUse.subtractAndFetch(1);
            }
			//总执行的任务数，task没执行一次增加一次
            _totalExecuted.addAndFetch(1);
        });

        task();
    };

    // Dispatching a task on the io_context will run the task immediately, and may run it
    // on the current thread (if the current thread is running the io_context right now).
    //
    // Posting a task on the io_context will run the task without recursion.
    //
    // If the task is allowed to recurse and we are not over the depth limit, dispatch it so it
    // can be called immediately and recursively.
    /*
	//accept流程
	//TransportLayerASIO::_acceptConnection->basic_socket_acceptor::async_accept
	//->start_accept_op->epoll_reactor::post_immediate_completion
	
	//普通read write op操作入队流程
	//mongodb的ServiceExecutorAdaptive::schedule调用->io_context::post(ASIO_MOVE_ARG(CompletionHandler) handler)
	//->scheduler::post_immediate_completion
	//mongodb的ServiceExecutorAdaptive::schedule调用->io_context::dispatch(ASIO_MOVE_ARG(CompletionHandler) handler)
	//->scheduler::do_dispatch
	
	//普通读写read write 从队列获取op执行流程
	//ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_for->scheduler::wait_one->scheduler::do_wait_one调用
	//mongodb中ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_one_for->io_context::run_one_until->schedule::wait_one

    
	post 优先将任务排进处理队列，然后返回，任务会在某个时机被完成。
	dispatch的任务会立刻执行
	早期版本Io_servie，现在版本Io_context
	*/ //队列中的wrappedTask任务在ServiceExecutorAdaptive::_workerThreadRoutine中运行
    if ((flags & kMayRecurse) &&
        (_localThreadState->recursionDepth + 1 < _config->recursionLimit())) {
        //io_context::dispatch   io_context::dispatch 
        _ioContext->dispatch(std::move(wrappedTask));  
    } else { //入队   io_context::post
        _ioContext->post(std::move(wrappedTask));
    }

    _lastScheduleTimer.reset();
	//总的入队任务数,也就是调用ServiceStateMachine::_scheduleNextWithGuard->ServiceExecutorAdaptive::schedule的次数
    _totalQueued.addAndFetch(1); 

    // Deferred tasks never count against the thread starvation avoidance. For other tasks, we
    // notify the controller thread that a task has been scheduled and we should monitor thread
    // starvation.
    if (_isStarved() && !(flags & kDeferredTask)) {
        _scheduleCondition.notify_one();
    }

    return Status::OK();
}

//判断队列中的任务数和可用线程数大小，避免任务task饥饿
bool ServiceExecutorAdaptive::_isStarved() const {
    // If threads are still starting, then assume we won't be starved pretty soon, return false
    //如果有正在启动的线程，则直接返回
    if (_threadsPending.load() > 0)
        return false;

    auto tasksQueued = _tasksQueued.load();
    // If there are no pending tasks, then we definitely aren't starved
    if (tasksQueued == 0)
        return false;

    // The available threads is the number that are running - the number that are currently
    // executing
    auto available = _threadsRunning.load() - _threadsInUse.load();

	//队列中的任务数大于可用线程数，说明worker压力过大，需要创建新的worker线程
    return (tasksQueued > available);
}

//worker-x线程默认是CPU/2个，但是在controller线程会根据负载在_controllerThreadRoutine中动态调整worker线程数
//如果controller线程发现负载高，那么worker线程数也就是增加，如果负载下去了，worker线程根据自身情况来觉得是否退出消耗自身线程

//worker-controller thread   ServiceExecutorAdaptive::start中创建  woker控制线程
void ServiceExecutorAdaptive::_controllerThreadRoutine() {
    setThreadName("worker-controller"_sd);  
    // The scheduleCondition needs a lock to wait on.
    stdx::mutex fakeMutex;
    stdx::unique_lock<stdx::mutex> fakeLk(fakeMutex);

    TickTimer sinceLastControlRound(_tickSource);
    TickSource::Tick lastSpentExecuting = _getThreadTimerTotal(ThreadTimer::Executing);
    TickSource::Tick lastSpentRunning = _getThreadTimerTotal(ThreadTimer::Running);

    while (_isRunning.load()) {
        // Make sure that the timer gets reset whenever this loop completes
        //一次while结束的时候执行对应func ,也就是结束的时候计算为起始时间
        const auto timerResetGuard =
            MakeGuard([&sinceLastControlRound] { sinceLastControlRound.reset(); });

        _scheduleCondition.wait_for(fakeLk, _config->stuckThreadTimeout().toSystemDuration());

        // If the executor has stopped, then stop the controller altogether
        if (!_isRunning.load())
            break;

        double utilizationPct;
        {
			//获取所有线程执行任务的总时间
            auto spentExecuting = _getThreadTimerTotal(ThreadTimer::Executing);
			//获取所有线程整个生命周期时间(包括空闲时间+执行任务时间+创建线程的时间)
            auto spentRunning = _getThreadTimerTotal(ThreadTimer::Running);

			//也就是while中执行一次这个过程中spentExecuting差值，
			//也就是spentExecuting代表while一次循环的Executing time开始值, lastSpentExecuting代表一次循环对应的结束time值
            auto diffExecuting = spentExecuting - lastSpentExecuting;
			////也就是spentRunning代表while一次循环的running time开始值, lastSpentRunning代表一次循环对应的结束time值
            auto diffRunning = spentRunning - lastSpentRunning;

            // If no threads have run yet, then don't update anything
            if (spentRunning == 0 || diffRunning == 0)
                utilizationPct = 0.0;
            else {
                lastSpentExecuting = spentExecuting;
                lastSpentRunning = spentRunning;

				//一次while循环过程中所有线程执行任务的时间和线程运行总时间的比值
                utilizationPct = diffExecuting / static_cast<double>(diffRunning);
                utilizationPct *= 100;
            }
        }

        // If the wait timed out then either the executor is idle or stuck
        //也就是本while()执行一次的时间差值
        if (sinceLastControlRound.sinceStart() >= _config->stuckThreadTimeout()) {
            // Each call to schedule updates the last schedule ticks so we know the last time a
            // task was scheduled
            Milliseconds sinceLastSchedule = _lastScheduleTimer.sinceStart();

            // If the number of tasks executing is the number of threads running (that is all
            // threads are currently busy), and the last time a task was able to be scheduled was
            // longer than our wait timeout, then we can assume all threads are stuck.
            //
            // In that case we should start the reserve number of threads so fully unblock the
            // thread pool.
            //
            if ((_threadsInUse.load() == _threadsRunning.load()) &&
                (sinceLastSchedule >= _config->stuckThreadTimeout())) {
                log() << "Detected blocked worker threads, "
                      << "starting new reserve threads to unblock service executor";
                for (int i = 0; i < _config->reservedThreads(); i++) {
                    _startWorkerThread();
                }
            }
            continue;
        }

		//当前的worker线程数
        auto threadsRunning = _threadsRunning.load();
		//保证线程池中worker线程数最少都要reservedThreads个
        if (threadsRunning < _config->reservedThreads()) {
            log() << "Starting " << _config->reservedThreads() - threadsRunning
                  << " to replenish reserved worker threads";
            while (_threadsRunning.load() < _config->reservedThreads()) {
                _startWorkerThread();
            }
        }

        // If the utilization percentage is lower than our idle threshold, then the threads we
        // already have aren't saturated and we shouldn't consider adding new threads at this
        // time.

		//所有worker线程非空闲占比小于该阀值，说明压力不大，不需要增加worker线程数
        if (utilizationPct < _config->idlePctThreshold()) {
            continue;
        }
		//走到这里，说明整体worker工作压力还是很大的

        // While there are threads pending sleep for 50 microseconds (this is our thread latency
        // perf budget).
        //
        // If waiting for pending threads takes longer than the stuckThreadTimeout, then the
        // pending threads may be stuck and we should loop back around.
        //我们在这里循环stuckThreadTimeout毫秒，直到我们等待worker线程创建起来并正常运行task
        //因为如果有正在创建的worker线程，我们等待一小会，最多等待stuckThreadTimeout ms
        do {
            stdx::this_thread::sleep_for(_config->maxQueueLatency().toSystemDuration());
        } while ((_threadsPending.load() > 0) &&
                 (sinceLastControlRound.sinceStart() < _config->stuckThreadTimeout()));


        // If the number of pending tasks is greater than the number of running threads minus the
        // number of tasks executing (the number of free threads), then start a new worker to
        // avoid starvation.
        //队列中任务数多余可用空闲线程数，说明压力有点大，给线程池增加一个新的线程
        if (_isStarved()) {
            log() << "Starting worker thread to avoid starvation.";
            _startWorkerThread();
        }
    }
}

//创建新的worker-n线程ServiceExecutorAdaptive::_startWorkerThread->_workerThreadRoutine   conn线程创建见ServiceStateMachine::create 
//ServiceExecutorAdaptive::start  _controllerThreadRoutine  中调用
void ServiceExecutorAdaptive::_startWorkerThread() {
    stdx::unique_lock<stdx::mutex> lk(_threadsMutex);
	//warning() << "yang test   _startWorkerThread:  num1:" << _threads.size();
	//该stdx::list list中追加一个ThreadState
    auto num = _threads.size();
    auto it = _threads.emplace(_threads.begin(), _tickSource); 
	warning() << "yang test   _startWorkerThread: 22222 num2:" << _threads.size();

	//_threadsPending代表当前刚创建或者正在创建的线程总数，也就是创建起来还没有执行task的线程数
    _threadsPending.addAndFetch(1);
	//_threadsRunning代表已经执行过task的线程总数，也就是这些线程不是刚刚创建起来的
    _threadsRunning.addAndFetch(1);

    lk.unlock();

    const auto launchResult = //线程回调函数为_workerThreadRoutine
        launchServiceWorkerThread([this, num, it] { _workerThreadRoutine(num, it); });

    if (!launchResult.isOK()) {
        warning() << "Failed to launch new worker thread: " << launchResult;
        lk.lock();
        _threadsPending.subtractAndFetch(1);
        _threadsRunning.subtractAndFetch(1);
        _threads.erase(it);
    }
}

//产生一个随机数
Milliseconds ServiceExecutorAdaptive::_getThreadJitter() const {
    static stdx::mutex jitterMutex;
    static std::default_random_engine randomEngine = [] {
        std::random_device seed;
        return std::default_random_engine(seed());
    }();

    auto jitterParam = _config->runTimeJitter();
    if (jitterParam == 0)
        return Milliseconds{0};

    std::uniform_int_distribution<> jitterDist(-jitterParam, jitterParam);

    stdx::lock_guard<stdx::mutex> lk(jitterMutex);
    auto jitter = jitterDist(randomEngine);
    if (jitter > _config->workerThreadRunTime().count())
        jitter = 0;

    return Milliseconds{jitter};
}

//在线程池内执行的时间。注意这个时间是所有worker的线程的汇总信息，包含历史worker的统计时间。
TickSource::Tick ServiceExecutorAdaptive::_getThreadTimerTotal(ThreadTimer which) const {
    TickSource::Tick accumulator;
    switch (which) { //获取生命周期已经结束的线程执行任务的总时间
        case ThreadTimer::Running:
            accumulator = _pastThreadsSpentRunning.load();
            break;
        case ThreadTimer::Executing: //获取生命周期已经结束的线程整个生命周期时间(包括空闲时间+执行任务时间)
            accumulator = _pastThreadsSpentExecuting.load();
            break;
    }

    stdx::lock_guard<stdx::mutex> lk(_threadsMutex);
    for (auto& thread : _threads) { 
        switch (which) {
            case ThreadTimer::Running://获取当前线程池中所有工作线程执行任务时间
                accumulator += thread.running.totalTime();
                break;
            case ThreadTimer::Executing: //获取当前线程池中所有工作线程整个生命周期时间(包括空闲时间+执行任务时间)
                accumulator += thread.executing.totalTime();
                break;
        }
    }

    return accumulator;
}

//ServiceExecutorAdaptive::_startWorkerThread
//worker-x线程默认是CPU/2个，但是在controller线程会根据负载在_controllerThreadRoutine中动态调整worker线程数
//如果controller线程发现负载高，那么worker线程数也就是增加，如果负载下去了，worker线程根据自身情况来觉得是否退出消耗自身线程

//ServiceExecutorAdaptive::schedule中任务入队，ServiceExecutorAdaptive::_workerThreadRoutine出对执行
void ServiceExecutorAdaptive::_workerThreadRoutine(
    int threadId, ServiceExecutorAdaptive::ThreadList::iterator state) {

    _localThreadState = &(*state);
    {
        std::string threadName = str::stream() << "worker-" << threadId;
        setThreadName(threadName);
    }

    log() << "Started new database worker thread " << threadId;

    // Whether a thread is "pending" reflects whether its had a chance to do any useful work.
    // When a thread is pending, it will only try to run one task through ASIO, and report back
    // as soon as possible so that the thread controller knows not to keep starting threads while
    // the threads it's already created are finishing starting up.
    //该线程第一次执行while中的任务的时候为ture，后面都是false
    bool stillPending = true; //表示该线程是刚创建起来的，还没有执行任何一个task

	//MakeGuard构造的是一个零时变量，则函数退出后会执行func，也就是本线程自动退出销毁的时候执行
    const auto guard = MakeGuard([this, &stillPending, state] {
    	//()中的func在消耗的时候会执行，也就是该函数退出的时候需要消耗guard，在析构函数中调用该func
        if (stillPending)
            _threadsPending.subtractAndFetch(1);
		//线程退出后，当前正在运行的线程数减1
        _threadsRunning.subtractAndFetch(1);
		//记录这个退出的线程生命期内执行任务的总时间
        _pastThreadsSpentRunning.addAndFetch(state->running.totalTime());
		//记录这个退出的线程生命期内运行的总时间(包括等待IO及运行IO任务的时间)
        _pastThreadsSpentExecuting.addAndFetch(state->executing.totalTime());

        {
            stdx::lock_guard<stdx::mutex> lk(_threadsMutex);
            _threads.erase(state);
        }
        _deathCondition.notify_one();
    });

    auto jitter = _getThreadJitter();

    while (_isRunning.load()) {
        // We don't want all the threads to start/stop running at exactly the same time, so the
        // jitter setParameter adds/removes a random small amount of time to the runtime.
        Milliseconds runTime = _config->workerThreadRunTime() + jitter;
        dassert(runTime.count() > 0);

        // Reset ticksSpentExecuting timer
        //本次循环执行task的时间，不包括网络IO等待时间
        state->executingCurRun = 0;

        try {
            asio::io_context::work work(*_ioContext);
            // If we're still "pending" only try to run one task, that way the controller will
            // know that it's okay to start adding threads to avoid starvation again.
            state->running.markRunning(); //记录开始时间，也就是任务执行开始时间
            //这里面会调用ServiceExecutorAdaptive::schedule对应的task,线程名也被改为conn线程
            //在该线程异步操作过程中，通过ServiceStateMachine::_sinkCallback  ServiceStateMachine::_sourceCallback把worker线程改为conn线程

			//执行ServiceExecutorAdaptive::schedule中对应的task
			if (stillPending) {
				//在一定时间内处理事件循环，阻塞到任务被完成同时没用其他任务派遣，或者直到io_context调用 stop() 函数停止 或 超时 为止
				//执行一个任务就会返回
				_ioContext->run_one_for(runTime.toSystemDuration());
            } else {  // Otherwise, just run for the full run period
            	//_ioContext对应的所有任务都执行完成后才会返回
                _ioContext->run_for(runTime.toSystemDuration());
            }
			
            // _ioContext->run_one() will return when all the scheduled handlers are completed, and
            // you must call restart() to call run_one() again or else it will return immediately.
            // In the case where the server has just started and there has been no work yet, this
            // means this loop will spin until the first client connect. Thsi call to restart avoids
            // that.
            if (_ioContext->stopped()) //保证该worker线程继续其他网络处理，否则本次异步操作处理完后该线程会退出
                _ioContext->restart();
            // If an exceptione escaped from ASIO, then break from this thread and start a new one.
        } catch (std::exception& e) {
        	//网络IO处理过程中异常了，则该线程退出销毁，同时则创建一个新线程
            log() << "Exception escaped worker thread: " << e.what()
                  << " Starting new worker thread.";
            _startWorkerThread();
            break;
        } catch (...) {
            log() << "Unknown exception escaped worker thread. Starting new worker thread.";
            _startWorkerThread();
            break;
        }
		//本线程本次循环消耗的时间，包括IO等待和执行对应网络事件对应task的时间
        auto spentRunning = state->running.markStopped();

        // If we're still pending, let the controller know and go back around for another go
        //
        // Otherwise we can think about exiting if the last call to run_for() wasn't very
        // productive
        
        if (stillPending) { //该线程第一次执行while中的任务的时候为ture，后面都是false
            _threadsPending.subtractAndFetch(1);
            stillPending = false;
        } else if (_threadsRunning.load() > _config->reservedThreads()) { //
        	//最后一次调用run_for()的效率不是很高，我们可以考虑退出
            // If we spent less than our idle threshold actually running tasks then exit the thread.
            // This time measurement doesn't include time spent running network callbacks, so the
            // threshold is lower than you'd expect.
            dassert(spentRunning < std::numeric_limits<double>::max());

            // First get the ratio of ticks spent executing to ticks spent running. We expect this
            // to be <= 1.0
            //代表本次循环该线程真正处理任务的时间与本次循环总时间(总时间包括IO等待和IO任务处理时间)
            double executingToRunning = state->executingCurRun / static_cast<double>(spentRunning);

            // Multiply that by 100 to get the percentage of time spent executing tasks. We expect
            // this to be <= 100.
            executingToRunning *= 100;
            dassert(executingToRunning <= 100);

            int pctExecuting = static_cast<int>(executingToRunning);
			//线程很多，超过了指定配置，并且满足这个条件，该worker线程会退出，线程比较空闲，退出
			//如果线程真正处理任务执行时间占比小于该值，则说明本线程比较空闲，可以退出。
            if (pctExecuting < _config->idlePctThreshold()) {
                log() << "Thread was only executing tasks " << pctExecuting << "% over the last "
                      << runTime << ". Exiting thread.";
                break;
            }
        }
    }
}

//db.serverStatus().network.serviceExecutorTaskStats  //db.serverStatus().network获取
void ServiceExecutorAdaptive::appendStats(BSONObjBuilder* bob) const {
	//下面的第一列是名,第二列是对应的值，如"totalTimeRunningMicros" : NumberLong("69222621699"),
    BSONObjBuilder section(bob->subobjStart("serviceExecutorTaskStats"));
	//kTotalQueued:总的入队任务数,也就是调用ServiceStateMachine::_scheduleNextWithGuard->ServiceExecutorAdaptive::schedule的次数
    //kExecutorName：adaptive
    //kTotalExecuted: 总执行的任务数
    //kTasksQueued: 当前入队还没执行的task数
    //_deferredTasksQueued: 当前入队还没执行的deferredTask数
    //kThreadsInUse: 当前正在执行task的线程
    //kTotalQueued=kDeferredTasksQueued(deferred task)+kTasksQueued(普通task)
    //kThreadsPending代表当前刚创建或者正在启动的线程总数，也就是创建起来还没有执行task的线程数
    //kThreadsRunning代表已经执行过task的线程总数，也就是这些线程不是刚刚创建起来的
	//kTotalTimeRunningUs:记录这个退出的线程生命期内执行任务的总时间
	//kTotalTimeExecutingUs：记录这个退出的线程生命期内运行的总时间(包括等待IO及运行IO任务的时间)
	//kTotalTimeQueuedUs: 从任务被调度入队，到真正被执行这段过程的时间，也就是等待被调度的时间

    section << kExecutorLabel << kExecutorName                                             //
            << kTotalQueued << _totalQueued.load()                                         //
            << kTotalExecuted << _totalExecuted.load()                                     //
            << kTasksQueued << _tasksQueued.load()                                         //
            << kDeferredTasksQueued << _deferredTasksQueued.load()                         //
            << kThreadsInUse << _threadsInUse.load()                                       //
            << kTotalTimeRunningUs                                                         //
            << ticksToMicros(_getThreadTimerTotal(ThreadTimer::Running), _tickSource)      //
            << kTotalTimeExecutingUs                                                       //
            << ticksToMicros(_getThreadTimerTotal(ThreadTimer::Executing), _tickSource)    //
            << kTotalTimeQueuedUs << ticksToMicros(_totalSpentQueued.load(), _tickSource)  //
            << kThreadsRunning << _threadsRunning.load()                                   //
            << kThreadsPending << _threadsPending.load();
    section.doneFast();
}

}  // namespace transport
}  // namespace mongo
