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

#pragma once

#include <vector>

#include "mongo/db/service_context.h"
#include "mongo/platform/atomic_word.h"
#include "mongo/stdx/condition_variable.h"
#include "mongo/stdx/list.h"
#include "mongo/stdx/thread.h"
#include "mongo/transport/service_executor.h"
#include "mongo/util/tick_source.h"

#include <asio.hpp>

namespace mongo {
namespace transport {

/**
 * This is an ASIO-based adaptive ServiceExecutor. It guarantees that threads will not become stuck
 * or deadlocked longer that its configured timeout and that idle threads will terminate themselves
 * if they spend more than its configure idle threshold idle.
 * 这是基于ASIO的异步自适应。它保证线程不会被卡住或死锁，如果配置超时和空闲线程超过空闲配置时间，线程将终止
 * createWithConfig中构造使用
 */
/*
"adaptive") : <ServiceExecutorAdaptive>( 引入了boost.asio库实现网络接口的异步调用并作为默认配置，同时还把线程模型调整
为线程池，动态根据workload压力情况调整线程数量，在大量连接情况下可以避免产生大量的处理线程，降低线程切换开销，以获得
更稳定的性能表现。

"synchronous"): <ServiceExecutorSynchronous>(ctx));  每个网络连接创建一个专用线程，并同步调用网络接口recv/send收发包
}
*/

//构建使用见TransportLayerManager::createWithConfig,最终赋值给ServiceContext._serviceExecutor
//ServiceExecutorSynchronous对应线程池同步模式，ServiceExecutorAdaptive对应线程池异步自适应模式，他们的作用是处理链接相关的线程模型
class ServiceExecutorAdaptive : public ServiceExecutor {
public:
    struct Options {
        virtual ~Options() = default;
        // The minimum number of threads the executor will keep running to service tasks.
        virtual int reservedThreads() const = 0;

        // The amount of time each worker thread runs before considering exiting because of
        // idleness.
        virtual Milliseconds workerThreadRunTime() const = 0;

        // workerThreadRuntime() is offset by a random value between -jitter and +jitter to prevent
        // thundering herds
        virtual int runTimeJitter() const = 0;

        // The amount of time the controller thread will wait before checking for stuck threads
        // to guarantee forward progress
        virtual Milliseconds stuckThreadTimeout() const = 0;

        // The maximum allowed latency between when a task is scheduled and a thread is started to
        // service it.
        virtual Microseconds maxQueueLatency() const = 0;

        // Threads that spend less than this threshold doing work during their workerThreadRunTime
        // period will exit
        virtual int idlePctThreshold() const = 0;

        // The maximum allowable depth of recursion for tasks scheduled with the MayRecurse flag
        // before stack unwinding is forced.
        virtual int recursionLimit() const = 0;
    };

    explicit ServiceExecutorAdaptive(ServiceContext* ctx, std::shared_ptr<asio::io_context> ioCtx);
    explicit ServiceExecutorAdaptive(ServiceContext* ctx,
                                     std::shared_ptr<asio::io_context> ioCtx,
                                     std::unique_ptr<Options> config);

    ServiceExecutorAdaptive(ServiceExecutorAdaptive&&) = default;
    ServiceExecutorAdaptive& operator=(ServiceExecutorAdaptive&&) = default;
    virtual ~ServiceExecutorAdaptive();

    Status start() final;
    Status shutdown(Milliseconds timeout) final;
    Status schedule(Task task, ScheduleFlags flags) final;

    Mode transportMode() const final {
        return Mode::kAsynchronous;
    }

    void appendStats(BSONObjBuilder* bob) const final;

    int threadsRunning() {
        return _threadsRunning.load();
    }

private:
    //计算ticks差值，也就是时间差值
    class TickTimer {
    public:
        explicit TickTimer(TickSource* tickSource)
            : _tickSource(tickSource),
              //1000，也就是1s钟包含1000个ticks，也就是1个ticks代表1ms
              _ticksPerMillisecond(_tickSource->getTicksPerSecond() / 1000),
              //初始化获取时间ticks
              _start(_tickSource->getTicks()) {
            invariant(_ticksPerMillisecond > 0);
        }

        //start到当前的时间差
        TickSource::Tick sinceStartTicks() const {
            return _tickSource->getTicks() - _start.load();
        }

        //以ms为精度，两个时间差值对应的ticks数，也就是相差ticks ms
        Milliseconds sinceStart() const {
            return Milliseconds{sinceStartTicks() / _ticksPerMillisecond};
        }

        //初始时间tick
        void reset() {
            _start.store(_tickSource->getTicks());
        }

    private:
        TickSource* const _tickSource;
        const TickSource::Tick _ticksPerMillisecond;
        AtomicWord<TickSource::Tick> _start;
    };

    class CumulativeTickTimer {
    public:
        CumulativeTickTimer(TickSource* ts) : _timer(ts) {}

        //总的时间记录到_accumulator
        TickSource::Tick markStopped() {
            stdx::lock_guard<stdx::mutex> lk(_mutex);
            invariant(_running);
            _running = false;
            auto curTime = _timer.sinceStartTicks();
            _accumulator += curTime;
            return curTime;
        }

        //记录开始时间
        void markRunning() {
            stdx::lock_guard<stdx::mutex> lk(_mutex);
            invariant(!_running);
            _timer.reset();
            _running = true;
        }

        //获取总的时间记录
        TickSource::Tick totalTime() const {
            stdx::lock_guard<stdx::mutex> lk(_mutex);
            if (!_running)
                return _accumulator;
            return _timer.sinceStartTicks() + _accumulator;
        }

    private:
        //用于计算时间差值
        TickTimer _timer;
        mutable stdx::mutex _mutex;
        //总时间tick
        TickSource::Tick _accumulator = 0;
        bool _running = false;
    };

    struct ThreadState {
        ThreadState(TickSource* ts) : running(ts), executing(ts) {}

        //本线程本次循环消耗的时间，包括IO等待和执行对应网络事件对应task的时间,参考ServiceExecutorAdaptive::_workerThreadRoutine 
        CumulativeTickTimer running;
        //记录本线程执行task消耗的总时间，因为worker线程的一次循环里面可能会递归多次执行ServiceStateMachine::_scheduleNextWithGuard
        TickSource::Tick executingCurRun;
        //记录单次task被执行的时间，参考ServiceExecutorAdaptive::schedule
        CumulativeTickTimer executing;
        int recursionDepth = 0;
    };

    using ThreadList = stdx::list<ThreadState>;

    void _startWorkerThread();
    void _workerThreadRoutine(int threadId, ThreadList::iterator it);
    void _controllerThreadRoutine();
    bool _isStarved() const;
    Milliseconds _getThreadJitter() const;

    enum class ThreadTimer { Running, Executing };
    TickSource::Tick _getThreadTimerTotal(ThreadTimer which) const;

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

    //TransportLayerManager::createWithConfig赋值，链接上的数据读写生效见ServiceExecutorAdaptive::schedule 
    //也就是TransportLayerASIO._workerIOContext  adaptive模式，所有线程共用所有accept新链接对应的网络IO上下文
    std::shared_ptr<asio::io_context> _ioContext; //早期ASIO中叫io_service 
    //TransportLayerManager::createWithConfig赋值调用
    std::unique_ptr<Options> _config;

    mutable stdx::mutex _threadsMutex;
    ThreadList _threads;
    //worker-controller thread   ServiceExecutorAdaptive::start中创建
    stdx::thread _controllerThread;

    //TransportLayerManager::createWithConfig赋值调用
    TickSource* const _tickSource;
    AtomicWord<bool> _isRunning{false};

    // These counters are used to detect stuck threads and high task queuing.
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
    AtomicWord<int> _threadsRunning{0};
    AtomicWord<int> _threadsPending{0};
    AtomicWord<int> _threadsInUse{0};
    AtomicWord<int> _tasksQueued{0};
    AtomicWord<int> _deferredTasksQueued{0};
    //TransportLayerManager::createWithConfig赋值调用
    TickTimer _lastScheduleTimer;
    AtomicWord<TickSource::Tick> _pastThreadsSpentExecuting{0};
    AtomicWord<TickSource::Tick> _pastThreadsSpentRunning{0};
    static thread_local ThreadState* _localThreadState;

    // These counters are only used for reporting in serverStatus.
    AtomicWord<int64_t> _totalQueued{0};
    AtomicWord<int64_t> _totalExecuted{0};
    AtomicWord<TickSource::Tick> _totalSpentQueued{0};

    // Threads signal this condition variable when they exit so we can gracefully shutdown
    // the executor.
    stdx::condition_variable _deathCondition;

    // Tasks should signal this condition variable if they want the thread controller to
    // track their progress and do fast stuck detection
    stdx::condition_variable _scheduleCondition;
};

}  // namespace transport
}  // namespace mongo
