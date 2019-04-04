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

#define MONGO_LOG_DEFAULT_COMPONENT mongo::logger::LogComponent::kExecutor

#include "mongo/platform/basic.h"

#include "mongo/executor/thread_pool_task_executor.h"

#include <boost/optional.hpp>
#include <iterator>
#include <utility>

#include "mongo/base/checked_cast.h"
#include "mongo/base/disallow_copying.h"
#include "mongo/base/status_with.h"
#include "mongo/db/operation_context.h"
#include "mongo/executor/connection_pool_stats.h"
#include "mongo/executor/network_interface.h"
#include "mongo/platform/atomic_word.h"
#include "mongo/util/concurrency/thread_pool_interface.h"
#include "mongo/util/fail_point_service.h"
#include "mongo/util/log.h"
#include "mongo/util/time_support.h"

namespace mongo {
namespace executor {

namespace {
MONGO_FP_DECLARE(scheduleIntoPoolSpinsUntilThreadPoolShutsDown);
}

class ThreadPoolTaskExecutor::CallbackState : public TaskExecutor::CallbackState {
    MONGO_DISALLOW_COPYING(CallbackState);

public:
    static std::shared_ptr<CallbackState> make(CallbackFn&& cb, Date_t readyDate) {
        return std::make_shared<CallbackState>(std::move(cb), readyDate);
    }

    /**
     * Do not call directly. Use make.
     */
    CallbackState(CallbackFn&& cb, Date_t theReadyDate)
        : callback(std::move(cb)), readyDate(theReadyDate) {}

    virtual ~CallbackState() = default;

    bool isCanceled() const override {
        return canceled.load() > 0;
    }

    void cancel() override {
        MONGO_UNREACHABLE;
    }

    void waitForCompletion() override {
        MONGO_UNREACHABLE;
    }

    // All fields except for "canceled" are guarded by the owning task executor's _mutex. The
    // "canceled" field may be observed without holding _mutex, but may only be set while holding
    // _mutex.

    CallbackFn callback;
    AtomicUInt32 canceled{0U};
    WorkQueue::iterator iter;
    Date_t readyDate;
    bool isNetworkOperation = false;
    AtomicWord<bool> isFinished{false};
    boost::optional<stdx::condition_variable> finishedCondition;
};

class ThreadPoolTaskExecutor::EventState : public TaskExecutor::EventState {
    MONGO_DISALLOW_COPYING(EventState);

public:
    static std::shared_ptr<EventState> make() {
        return std::make_shared<EventState>();
    }

    EventState() = default;

    void signal() override {
        MONGO_UNREACHABLE;
    }
    void waitUntilSignaled() override {
        MONGO_UNREACHABLE;
    }
    bool isSignaled() override {
        MONGO_UNREACHABLE;
    }

    // All fields guarded by the owning task executor's _mutex.

    bool isSignaledFlag = false;
    stdx::condition_variable isSignaledCondition;
    EventList::iterator iter;
    WorkQueue waiters;
};

////赋值见makeShardingTaskExecutor
ThreadPoolTaskExecutor::ThreadPoolTaskExecutor(std::unique_ptr<ThreadPoolInterface> pool,
                                               std::unique_ptr<NetworkInterface> net)
    : _net(std::move(net)), _pool(std::move(pool)) {} //net对应的是NetworkInterfaceASIO  pool对应NetworkInterfaceThreadPool

ThreadPoolTaskExecutor::~ThreadPoolTaskExecutor() {
    shutdown();
    auto lk = _join(stdx::unique_lock<stdx::mutex>(_mutex));
    invariant(_state == shutdownComplete);
}

//ShardingTaskExecutor::startup
//initializeGlobalShardingState->TaskExecutorPool::startup->ShardingTaskExecutor::startup->ThreadPoolTaskExecutor::startup
void ThreadPoolTaskExecutor::startup() {
    _net->startup(); ////对应NetworkInterfaceASIO::startup  创建NetworkInterfaceASIO-TaskExecutorPool-0-1线程
    stdx::lock_guard<stdx::mutex> lk(_mutex);
    if (_inShutdown_inlock()) {
        return;
    }
    invariant(_state == preStart);
    _setState_inlock(running);
    _pool->startup(); //NetworkInterfaceThreadPool::startup
}

void ThreadPoolTaskExecutor::shutdown() {
    stdx::unique_lock<stdx::mutex> lk(_mutex);
    if (_inShutdown_inlock()) {
        invariant(_networkInProgressQueue.empty());
        invariant(_sleepersQueue.empty());
        return;
    }
    _setState_inlock(joinRequired);
    WorkQueue pending;
    pending.splice(pending.end(), _networkInProgressQueue);
    pending.splice(pending.end(), _sleepersQueue);
    for (auto&& eventState : _unsignaledEvents) {
        pending.splice(pending.end(), eventState->waiters);
    }
    for (auto&& cbState : pending) {
        cbState->canceled.store(1);
    }
    for (auto&& cbState : _poolInProgressQueue) {
        cbState->canceled.store(1);
    }
    scheduleIntoPool_inlock(&pending, std::move(lk));
    _pool->shutdown();
}

void ThreadPoolTaskExecutor::join() {
    _join(stdx::unique_lock<stdx::mutex>(_mutex));
}

stdx::unique_lock<stdx::mutex> ThreadPoolTaskExecutor::_join(stdx::unique_lock<stdx::mutex> lk) {
    _stateChange.wait(lk, [this] {
        switch (_state) {
            case preStart:
                return false;
            case running:
                return false;
            case joinRequired:
                return true;
            case joining:
                return false;
            case shutdownComplete:
                return true;
        }
        MONGO_UNREACHABLE;
    });
    if (_state == shutdownComplete) {
        return lk;
    }
    invariant(_state == joinRequired);
    _setState_inlock(joining);
    lk.unlock();
    _pool->join();
    lk.lock();
    while (!_unsignaledEvents.empty()) {
        auto eventState = _unsignaledEvents.front();
        invariant(eventState->waiters.empty());
        EventHandle event;
        setEventForHandle(&event, std::move(eventState));
        signalEvent_inlock(event, std::move(lk));
        lk = stdx::unique_lock<stdx::mutex>(_mutex);
    }
    lk.unlock();
    _net->shutdown();

    lk.lock();
    // The _poolInProgressQueue may not be empty if the network interface attempted to schedule work
    // into _pool after _pool->shutdown(). Because _pool->join() has returned, we know that any
    // items left in _poolInProgressQueue will never be processed by another thread, so we process
    // them now.
    while (!_poolInProgressQueue.empty()) {
        auto cbState = _poolInProgressQueue.front();
        lk.unlock();
        runCallback(std::move(cbState));
        lk.lock();
    }
    invariant(_networkInProgressQueue.empty());
    invariant(_sleepersQueue.empty());
    invariant(_unsignaledEvents.empty());
    _setState_inlock(shutdownComplete);
    return lk;
}

void ThreadPoolTaskExecutor::appendDiagnosticBSON(BSONObjBuilder* b) const {
    stdx::lock_guard<stdx::mutex> lk(_mutex);

    // ThreadPool details
    // TODO: fill in
    BSONObjBuilder poolCounters(b->subobjStart("pool"));
    poolCounters.appendIntOrLL("inProgressCount", _poolInProgressQueue.size());
    poolCounters.done();

    // Queues
    BSONObjBuilder queues(b->subobjStart("queues"));
    queues.appendIntOrLL("networkInProgress", _networkInProgressQueue.size());
    queues.appendIntOrLL("sleepers", _sleepersQueue.size());
    queues.done();

    b->appendIntOrLL("unsignaledEvents", _unsignaledEvents.size());
    b->append("shuttingDown", _inShutdown_inlock());
    b->append("networkInterface", _net->getDiagnosticString());
}

Date_t ThreadPoolTaskExecutor::now() {
    return _net->now();
}

StatusWith<TaskExecutor::EventHandle> ThreadPoolTaskExecutor::makeEvent() {
    auto el = makeSingletonEventList();
    EventHandle event;
    setEventForHandle(&event, el.front());
    stdx::lock_guard<stdx::mutex> lk(_mutex);
    if (_inShutdown_inlock()) {
        return {ErrorCodes::ShutdownInProgress, "Shutdown in progress"};
    }
    _unsignaledEvents.splice(_unsignaledEvents.end(), el);
    return event;
}

void ThreadPoolTaskExecutor::signalEvent(const EventHandle& event) {
    stdx::unique_lock<stdx::mutex> lk(_mutex);
    signalEvent_inlock(event, std::move(lk));
}

StatusWith<TaskExecutor::CallbackHandle> ThreadPoolTaskExecutor::onEvent(const EventHandle& event,
                                                                         const CallbackFn& work) {
    if (!event.isValid()) {
        return {ErrorCodes::BadValue, "Passed invalid event handle to onEvent"};
    }
    auto wq = makeSingletonWorkQueue(work);
    stdx::unique_lock<stdx::mutex> lk(_mutex);
    auto eventState = checked_cast<EventState*>(getEventFromHandle(event));
    auto cbHandle = enqueueCallbackState_inlock(&eventState->waiters, &wq);
    if (!cbHandle.isOK()) {
        return cbHandle;
    }
    if (eventState->isSignaledFlag) {
        scheduleIntoPool_inlock(&eventState->waiters, std::move(lk));
    }
    return cbHandle;
}

Status ThreadPoolTaskExecutor::waitForEvent(OperationContext* opCtx, const EventHandle& event) {
    invariant(opCtx);
    invariant(event.isValid());
    auto eventState = checked_cast<EventState*>(getEventFromHandle(event));
    stdx::unique_lock<stdx::mutex> lk(_mutex);

    try {
        // std::condition_variable::wait() can wake up spuriously, so provide a callback to detect
        // when that happens and go back to waiting.
        opCtx->waitForConditionOrInterrupt(eventState->isSignaledCondition, lk, [&eventState]() {
            return eventState->isSignaledFlag;
        });
    } catch (const DBException& e) {
        return e.toStatus();
    }
    return Status::OK();
}

void ThreadPoolTaskExecutor::waitForEvent(const EventHandle& event) {
    invariant(event.isValid());
    auto eventState = checked_cast<EventState*>(getEventFromHandle(event));
    stdx::unique_lock<stdx::mutex> lk(_mutex);

    while (!eventState->isSignaledFlag) {
        eventState->isSignaledCondition.wait(lk);
    }
}

StatusWith<TaskExecutor::CallbackHandle> ThreadPoolTaskExecutor::scheduleWork(
    const CallbackFn& work) {
    auto wq = makeSingletonWorkQueue(work);
    WorkQueue temp;
    stdx::unique_lock<stdx::mutex> lk(_mutex);
    auto cbHandle = enqueueCallbackState_inlock(&temp, &wq);
    if (!cbHandle.isOK()) {
        return cbHandle;
    }
    scheduleIntoPool_inlock(&temp, std::move(lk));
    return cbHandle;
}

StatusWith<TaskExecutor::CallbackHandle> ThreadPoolTaskExecutor::scheduleWorkAt(
    Date_t when, const CallbackFn& work) {
    if (when <= now()) {
        return scheduleWork(work);
    }
    auto wq = makeSingletonWorkQueue(work, when);
    stdx::unique_lock<stdx::mutex> lk(_mutex);
    auto cbHandle = enqueueCallbackState_inlock(&_sleepersQueue, &wq);
    if (!cbHandle.isOK()) {
        return cbHandle;
    }
    lk.unlock();
    _net->setAlarm(when,
                   [this, when, cbHandle] {
                       auto cbState =
                           checked_cast<CallbackState*>(getCallbackFromHandle(cbHandle.getValue()));
                       if (cbState->canceled.load()) {
                           return;
                       }
                       stdx::unique_lock<stdx::mutex> lk(_mutex);
                       if (cbState->canceled.load()) {
                           return;
                       }
                       scheduleIntoPool_inlock(&_sleepersQueue, cbState->iter, std::move(lk));
                   })
        .transitional_ignore();

    return cbHandle;
}

namespace {

using ResponseStatus = TaskExecutor::ResponseStatus;

// If the request received a connection from the pool but failed in its execution,
// convert the raw Status in cbData to a RemoteCommandResponse so that the callback,
// which expects a RemoteCommandResponse as part of RemoteCommandCallbackArgs,
// can be run despite a RemoteCommandResponse never having been created.
//NetworkInterfaceThreadPool::consumeTasks中执行  赋值见ThreadPoolTaskExecutor::scheduleRemoteCommand
void remoteCommandFinished(const TaskExecutor::CallbackArgs& cbData,
                           const TaskExecutor::RemoteCommandCallbackFn& cb,
                           const RemoteCommandRequest& request,
                           const ResponseStatus& rs) {
    cb(TaskExecutor::RemoteCommandCallbackArgs(cbData.executor, cbData.myHandle, request, rs));
}

// If the request failed to receive a connection from the pool,
// convert the raw Status in cbData to a RemoteCommandResponse so that the callback,
// which expects a RemoteCommandResponse as part of RemoteCommandCallbackArgs,
// can be run despite a RemoteCommandResponse never having been created.
void remoteCommandFailedEarly(const TaskExecutor::CallbackArgs& cbData,
                              const TaskExecutor::RemoteCommandCallbackFn& cb,
                              const RemoteCommandRequest& request) {
    invariant(!cbData.status.isOK());
    cb(TaskExecutor::RemoteCommandCallbackArgs(
        cbData.executor, cbData.myHandle, request, {cbData.status}));
}
}  // namespace

/*
调用栈
Breakpoint 1, mongo::executor::ThreadPoolTaskExecutor::scheduleRemoteCommand(mongo::executor::RemoteCommandRequest const&, std::function<void (mongo::executor::TaskExecutor::RemoteCommandCallbackArgs const&)> const&) (
    this=0x7f54273d00e0, request=..., cb=...) at src/mongo/executor/thread_pool_task_executor.cpp:412
412         LOG(3) << "Scheduling remote command request: " << redact(scheduledRequest.toString()) << "  thread id:" << syscall(SYS_gettid);
(gdb) bt
#0  mongo::executor::ThreadPoolTaskExecutor::scheduleRemoteCommand(mongo::executor::RemoteCommandRequest const&, std::function<void (mongo::executor::TaskExecutor::RemoteCommandCallbackArgs const&)> const&) (this=0x7f54273d00e0, 
    request=..., cb=...) at src/mongo/executor/thread_pool_task_executor.cpp:412
#1  0x00007f5423c0d146 in mongo::executor::ShardingTaskExecutor::scheduleRemoteCommand(mongo::executor::RemoteCommandRequest const&, std::function<void (mongo::executor::TaskExecutor::RemoteCommandCallbackArgs const&)> const&) (
    this=0x7f54272d5cd0, request=..., cb=...) at src/mongo/db/s/sharding_task_executor.cpp:189
#2  0x00007f5423e006ce in mongo::AsyncRequestsSender::_scheduleRequest (this=this@entry=0x7f54236fd110, remoteIndex=remoteIndex@entry=0) at src/mongo/s/async_requests_sender.cpp:246
#3  0x00007f5423e00c1f in mongo::AsyncRequestsSender::_scheduleRequests (this=this@entry=0x7f54236fd110, lk=...) at src/mongo/s/async_requests_sender.cpp:215
#4  0x00007f5423e0589a in mongo::AsyncRequestsSender::AsyncRequestsSender (this=0x7f54236fd110, opCtx=<optimized out>, executor=<optimized out>, db=..., requests=..., readPreference=..., retryPolicy=mongo::Shard::kNoRetry)
    at src/mongo/s/async_requests_sender.cpp:80
#5  0x00007f5423cc545c in mongo::BatchWriteExec::executeBatch (opCtx=opCtx@entry=0x7f54279eb540, targeter=..., clientRequest=..., clientResponse=clientResponse@entry=0x7f54236fd960, stats=stats@entry=0x7f54236fd8a0)
    at src/mongo/s/write_ops/batch_write_exec.cpp:214
#6  0x00007f5423cd11f6 in mongo::ClusterWriter::write (opCtx=opCtx@entry=0x7f54279eb540, request=..., stats=stats@entry=0x7f54236fd8a0, response=response@entry=0x7f54236fd960) at src/mongo/s/commands/cluster_write.cpp:234
#7  0x00007f5423c91c5a in mongo::(anonymous namespace)::ClusterWriteCmd::enhancedRun (this=0x7f542517a6a0 <mongo::(anonymous namespace)::clusterInsertCmd>, opCtx=0x7f54279eb540, request=..., result=...)
    at src/mongo/s/commands/cluster_write_cmd.cpp:204
#8  0x00007f54240c84ff in mongo::Command::publicRun (this=this@entry=0x7f542517a6a0 <mongo::(anonymous namespace)::clusterInsertCmd>, opCtx=0x7f54279eb540, request=..., result=...) at src/mongo/db/commands.cpp:357
#9  0x00007f5423cb055d in execCommandClient (result=..., request=..., c=0x7f542517a6a0 <mongo::(anonymous namespace)::clusterInsertCmd>, opCtx=0x7f54279eb540) at src/mongo/s/commands/strategy.cpp:227
#10 mongo::(anonymous namespace)::runCommand(mongo::OperationContext *, const mongo::OpMsgRequest &, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0x2847e1d, DIE 0x298cf31>) (opCtx=0x7f54279eb540, 
    request=..., builder=builder@entry=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0x2847e1d, DIE 0x298cf31>) at src/mongo/s/commands/strategy.cpp:267
#11 0x00007f5423cb127c in mongo::Strategy::<lambda()>::operator()(void) const (__closure=__closure@entry=0x7f54236fe610) at src/mongo/s/commands/strategy.cpp:425
#12 0x00007f5423cb1939 in mongo::Strategy::clientCommand (opCtx=opCtx@entry=0x7f54279eb540, m=...) at src/mongo/s/commands/strategy.cpp:436
#13 0x00007f5423bd2a21 in mongo::ServiceEntryPointMongos::handleRequest (this=<optimized out>, opCtx=0x7f54279eb540, message=...) at src/mongo/s/service_entry_point_mongos.cpp:167
#14 0x00007f5423bf00ca in mongo::ServiceStateMachine::_processMessage (this=this@entry=0x7f5427410ef0, guard=...) at src/mongo/transport/service_state_machine.cpp:455
#15 0x00007f5423beb00f in mongo::ServiceStateMachine::_runNextInGuard (this=0x7f5427410ef0, guard=...) at src/mongo/transport/service_state_machine.cpp:532
#16 0x00007f5423beeaed in operator() (__closure=0x7f5427315bc0) at src/mongo/transport/service_state_machine.cpp:573
#17 std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#18 0x00007f5424039b29 in operator() (this=0x7f54236feee8) at /usr/local/include/c++/5.4.0/functional:2267
#19 operator() (__closure=0x7f54236feee0) at src/mongo/transport/service_executor_adaptive.cpp:224
#20 asio_handler_invoke<mongo::transport::ServiceExecutorAdaptive::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()> > (function=...)
    at src/third_party/asio-master/asio/include/asio/handler_invoke_hook.hpp:68
#21 invoke<mongo::transport::ServiceExecutorAdaptive::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()>, mongo::transport::ServiceExecutorAdaptive::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()> > (context=..., function=...) at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#22 dispatch<mongo::transport::ServiceExecutorAdaptive::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()> > (this=<optimized out>, 
    handler=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0x876b520, DIE 0x87a6f1f>) at src/third_party/asio-master/asio/include/asio/impl/io_context.hpp:143
#23 mongo::transport::ServiceExecutorAdaptive::schedule(std::function<void ()>, mongo::transport::ServiceExecutor::ScheduleFlags) (this=this@entry=0x7f54273078c0, task=..., 
    flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse) at src/mongo/transport/service_executor_adaptive.cpp:240
#24 0x00007f5423be9b05 in mongo::ServiceStateMachine::_scheduleNextWithGuard (this=this@entry=0x7f5427410ef0, guard=..., flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse, 
    ownershipModel=ownershipModel@entry=mongo::ServiceStateMachine::kOwned) at src/mongo/transport/service_state_machine.cpp:577
#25 0x00007f5423bec702 in mongo::ServiceStateMachine::_sourceCallback (this=0x7f5427410ef0, status=...) at src/mongo/transport/service_state_machine.cpp:358
#26 0x00007f5423bed48d in operator() (status=..., __closure=<optimized out>) at src/mongo/transport/service_state_machine.cpp:317
#27 std::_Function_handler<void(mongo::Status), mongo::ServiceStateMachine::_sourceMessage(mongo::ServiceStateMachine::ThreadGuard)::<lambda(mongo::Status)> >::_M_invoke(const std::_Any_data &, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0x4f5bfb, DIE 0x5481eb>) (__functor=..., __args#0=<optimized out>) at /usr/local/include/c++/5.4.0/functional:1871
#28 0x00007f54242bb439 in operator() (__args#0=..., this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#29 operator() (status=..., __closure=<optimized out>) at src/mongo/transport/transport_layer_asio.cpp:123
#30 std::_Function_handler<void(mongo::Status), mongo::transport::TransportLayerASIO::asyncWait(mongo::transport::Ticket&&, mongo::transport::TransportLayer::TicketCallback)::<lambda(mongo::Status)> >::_M_invoke(const std::_Any_data &, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0xb37a29b, DIE 0xb3d2ee8>) (__functor=..., __args#0=<optimized out>) at /usr/local/include/c++/5.4.0/functional:1871
#31 0x00007f54242b7dfc in operator() (__args#0=..., this=0x7f54236ff540) at /usr/local/include/c++/5.4.0/functional:2267
#32 mongo::transport::TransportLayerASIO::ASIOTicket::finishFill (this=this@entry=0x7f54271ce960, status=...) at src/mongo/transport/ticket_asio.cpp:158
#33 0x00007f54242b7f9f in mongo::transport::TransportLayerASIO::ASIOSourceTicket::_bodyCallback (this=this@entry=0x7f54271ce960, ec=..., size=size@entry=190) at src/mongo/transport/ticket_asio.cpp:83
#34 0x00007f54242b9109 in operator() (size=190, ec=..., __closure=<optimized out>) at src/mongo/transport/ticket_asio.cpp:119
#35 opportunisticRead<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, mongo::transport::TransportLayerASIO::ASIOSourceTicket::_headerCallback(const std::error_code&, size_t)::<lambda(const std::error_code&, size_t)> > (this=<optimized out>, handler=<optimized out>, buffers=..., stream=..., sync=false) at src/mongo/transport/session_asio.h:191
#36 read<asio::mutable_buffers_1, mongo::transport::TransportLayerASIO::ASIOSourceTicket::_headerCallback(const std::error_code&, size_t)::<lambda(const std::error_code&, size_t)> > (handler=<optimized out>, buffers=..., sync=false, 
    this=<optimized out>) at src/mongo/transport/session_asio.h:154
#37 mongo::transport::TransportLayerASIO::ASIOSourceTicket::_headerCallback (this=0x7f54271ce960, ec=..., size=<optimized out>) at src/mongo/transport/ticket_asio.cpp:119
#38 0x00007f54242ba032 in operator() (size=<optimized out>, ec=..., __closure=0x7f54236ff8c8) at src/mongo/transport/ticket_asio.cpp:132
#39 operator() (start=0, bytes_transferred=<optimized out>, ec=..., this=0x7f54236ff8a0) at src/third_party/asio-master/asio/include/asio/impl/read.hpp:284
#40 operator() (this=0x7f54236ff8a0) at src/third_party/asio-master/asio/include/asio/detail/bind_handler.hpp:163
#41 asio_handler_invoke<asio::detail::binder2<asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> >, std::error_code, long unsigned int> > (function=...) at src/third_party/asio-master/asio/include/asio/handler_invoke_hook.hpp:68
#42 invoke<asio::detail::binder2<asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> >, std::error_code, long unsigned int>, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> > (
    context=..., function=...) at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#43 asio_handler_invoke<asio::detail::binder2<asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> >, std::error_code, long unsigned int>, asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> > (this_handler=<optimized out>, function=...)
    at src/third_party/asio-master/asio/include/asio/impl/read.hpp:337
---Type <return> to continue, or q <return> to quit---
#44 invoke<asio::detail::binder2<asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> >, std::error_code, long unsigned int>, asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> > > (context=..., function=...)
    at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#45 complete<asio::detail::binder2<asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> >, std::error_code, long unsigned int> > (this=<synthetic pointer>, handler=..., function=...)
    at src/third_party/asio-master/asio/include/asio/detail/handler_work.hpp:81
#46 asio::detail::reactive_socket_recv_op<asio::mutable_buffers_1, asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> > >::do_complete(void *, asio::detail::operation *, const asio::error_code &, std::size_t) (owner=0x7f54270b8000, 
    base=<optimized out>) at src/third_party/asio-master/asio/include/asio/detail/reactive_socket_recv_op.hpp:121
#47 0x00007f54242c8213 in complete (bytes_transferred=<optimized out>, ec=..., owner=0x7f54270b8000, this=<optimized out>) at src/third_party/asio-master/asio/include/asio/detail/scheduler_operation.hpp:39
#48 asio::detail::scheduler::do_wait_one (this=this@entry=0x7f54270b8000, lock=..., this_thread=..., usec=<optimized out>, usec@entry=1000000, ec=...) at src/third_party/asio-master/asio/include/asio/detail/impl/scheduler.ipp:480
#49 0x00007f54242c888a in asio::detail::scheduler::wait_one (this=0x7f54270b8000, usec=1000000, ec=...) at src/third_party/asio-master/asio/include/asio/detail/impl/scheduler.ipp:192
#50 0x00007f542403f639 in asio::io_context::run_one_until<std::chrono::_V2::steady_clock, std::chrono::duration<long, std::ratio<1l, 1000000000l> > > (this=this@entry=0x7f54270aacf0, abs_time=...)
    at src/third_party/asio-master/asio/include/asio/impl/io_context.hpp:109
#51 0x00007f542403e46f in run_until<std::chrono::_V2::steady_clock, std::chrono::duration<long, std::ratio<1l, 1000000000l> > > (abs_time=..., this=0x7f54270aacf0) at src/third_party/asio-master/asio/include/asio/impl/io_context.hpp:82
#52 run_for<long, std::ratio<1l, 1000000000l> > (rel_time=..., this=0x7f54270aacf0) at src/third_party/asio-master/asio/include/asio/impl/io_context.hpp:74
#53 mongo::transport::ServiceExecutorAdaptive::_workerThreadRoutine (this=0x7f54273078c0, threadId=<optimized out>, state=...) at src/mongo/transport/service_executor_adaptive.cpp:510
#54 0x00007f54245e3684 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#55 mongo::(anonymous namespace)::runFunc (ctx=0x7f54271d0e60) at src/mongo/transport/service_entry_point_utils.cpp:55

*/
//由conn-xx线程处理，实际上是worker线程，只是改名而已
//AsyncRequestsSender::_scheduleRequest调用执行
StatusWith<TaskExecutor::CallbackHandle> ThreadPoolTaskExecutor::scheduleRemoteCommand(
    const RemoteCommandRequest& request, const RemoteCommandCallbackFn& cb) {
    RemoteCommandRequest scheduledRequest = request;
    if (request.timeout == RemoteCommandRequest::kNoTimeout) {
        scheduledRequest.expirationDate = RemoteCommandRequest::kNoExpirationDate;
    } else {
        scheduledRequest.expirationDate = _net->now() + scheduledRequest.timeout;
    }

    // In case the request fails to even get a connection from the pool,
    // we wrap the callback in a method that prepares its input parameters.
    auto wq = makeSingletonWorkQueue([scheduledRequest, cb](const CallbackArgs& cbData) {
        remoteCommandFailedEarly(cbData, cb, scheduledRequest);
    });
    wq.front()->isNetworkOperation = true;
    stdx::unique_lock<stdx::mutex> lk(_mutex);
    auto cbHandle = enqueueCallbackState_inlock(&_networkInProgressQueue, &wq);
    if (!cbHandle.isOK())
        return cbHandle;
    const auto cbState = _networkInProgressQueue.back();

	#include <sys/types.h>
	#include <sys/syscall.h>
	//2019-03-05T16:57:53.368+0800 D EXECUTOR [conn----yangtest1] Scheduling remote command request: 
	//RemoteCommand 150 -- target:172.23.240.29:28018 db:test cmd:{ insert: "test", bypassDocumentValidation: false, ordered: true, documents: [ { _id: ObjectId('5c7e3a11ea9e07c79ca3abdf') } ], shardVersion: [ Timestamp(0, 0), ObjectId('000000000000000000000000') ] }  thread id:6222
    LOG(3) << "Scheduling remote command request: " << redact(scheduledRequest.toString()) << "  thread id:" << syscall(SYS_gettid);

    lk.unlock();
	//NetworkInterfaceASIO::startCommand
    _net->startCommand(
            cbHandle.getValue(),
            scheduledRequest,
            //后端应答后会调用该函数，真正执行在NetworkInterfaceASIO::AsyncOp::finish
            //赋值给NetworkInterfaceASIO::startCommand，后端应答数据后会调用
            //该用法为c++11的Lambda,可以参考https://www.cnblogs.com/DswCnblog/p/5629165.html
            [this, scheduledRequest, cbState, cb](const ResponseStatus& response) { //response里面包含状态 时延 msg等
                using std::swap;
                CallbackFn newCb = [cb, scheduledRequest, response](const CallbackArgs& cbData) {
                    remoteCommandFinished(cbData, cb, scheduledRequest, response);
                };
                stdx::unique_lock<stdx::mutex> lk(_mutex);
                if (_inShutdown_inlock()) {
                    return;
                }
				/*
				D EXECUTOR [NetworkInterfaceASIO-TaskExecutorPool-yang-0-0] Received remote response: RemoteResponse --  
				cmd:{ n: 1, opTime: { ts: Timestamp(1552619801, 1), t: 13 }, electionId: ObjectId('7fffffff000000000000000d'), 
				ok: 1.0, operationTime: Timestamp(1552619801, 1), $gleStats: { lastOpTime: { ts: Timestamp(1552619801, 1), 
				t: 13 }, electionId: ObjectId('7fffffff000000000000000d') }, $clusterTime: { clusterTime: Timestamp(1552619801,
				1), signature: { hash: BinData(0, 0000000000000000000000000000000000000000), keyId: 0 } }, $configServerState:
				{ opTime: { ts: Timestamp(1552619796, 1), t: 9 } } }
				*/
                LOG(3) << "Received remote response: "
                       << redact(response.isOK() ? response.toString()
                                                 : response.status.toString());
                swap(cbState->callback, newCb);
                scheduleIntoPool_inlock(&_networkInProgressQueue, cbState->iter, std::move(lk));
            })
        .transitional_ignore();
    return cbHandle;
}

void ThreadPoolTaskExecutor::cancel(const CallbackHandle& cbHandle) {
    invariant(cbHandle.isValid());
    auto cbState = checked_cast<CallbackState*>(getCallbackFromHandle(cbHandle));
    stdx::unique_lock<stdx::mutex> lk(_mutex);
    if (_inShutdown_inlock()) {
        return;
    }
    cbState->canceled.store(1);
    if (cbState->isNetworkOperation) {
        lk.unlock();
        _net->cancelCommand(cbHandle);
        return;
    }
    if (cbState->readyDate != Date_t{}) {
        // This callback might still be in the sleeper queue; if it is, schedule it now
        // rather than when the alarm fires.
        auto iter = std::find_if(_sleepersQueue.begin(),
                                 _sleepersQueue.end(),
                                 [cbState](const std::shared_ptr<CallbackState>& other) {
                                     return cbState == other.get();
                                 });
        if (iter != _sleepersQueue.end()) {
            invariant(iter == cbState->iter);
            scheduleIntoPool_inlock(&_sleepersQueue, cbState->iter, std::move(lk));
        }
    }
}

void ThreadPoolTaskExecutor::wait(const CallbackHandle& cbHandle) {
    invariant(cbHandle.isValid());
    auto cbState = checked_cast<CallbackState*>(getCallbackFromHandle(cbHandle));
    if (cbState->isFinished.load()) {
        return;
    }
    stdx::unique_lock<stdx::mutex> lk(_mutex);
    if (!cbState->finishedCondition) {
        cbState->finishedCondition.emplace();
    }
    while (!cbState->isFinished.load()) {
        cbState->finishedCondition->wait(lk);
    }
}

void ThreadPoolTaskExecutor::appendConnectionStats(ConnectionPoolStats* stats) const {
    _net->appendConnectionStats(stats);
}

StatusWith<TaskExecutor::CallbackHandle> ThreadPoolTaskExecutor::enqueueCallbackState_inlock(
    WorkQueue* queue, WorkQueue* wq) {
    if (_inShutdown_inlock()) {
        return {ErrorCodes::ShutdownInProgress, "Shutdown in progress"};
    }
    invariant(!wq->empty());
    queue->splice(queue->end(), *wq, wq->begin());
    invariant(wq->empty());
    CallbackHandle cbHandle;
    setCallbackForHandle(&cbHandle, queue->back());
    return cbHandle;
}

ThreadPoolTaskExecutor::WorkQueue ThreadPoolTaskExecutor::makeSingletonWorkQueue(CallbackFn work,
                                                                                 Date_t when) {
    WorkQueue result;
    result.emplace_front(CallbackState::make(std::move(work), when));
    result.front()->iter = result.begin();
    return result;
}

ThreadPoolTaskExecutor::EventList ThreadPoolTaskExecutor::makeSingletonEventList() {
    EventList result;
    result.emplace_front(EventState::make());
    result.front()->iter = result.begin();
    return result;
}

void ThreadPoolTaskExecutor::signalEvent_inlock(const EventHandle& event,
                                                stdx::unique_lock<stdx::mutex> lk) {
    invariant(event.isValid());
    auto eventState = checked_cast<EventState*>(getEventFromHandle(event));
    invariant(!eventState->isSignaledFlag);
    eventState->isSignaledFlag = true;
    eventState->isSignaledCondition.notify_all();
    _unsignaledEvents.erase(eventState->iter);
    scheduleIntoPool_inlock(&eventState->waiters, std::move(lk));
}

//ThreadPoolTaskExecutor::scheduleRemoteCommand
void ThreadPoolTaskExecutor::scheduleIntoPool_inlock(WorkQueue* fromQueue,
                                                     stdx::unique_lock<stdx::mutex> lk) {
    scheduleIntoPool_inlock(fromQueue, fromQueue->begin(), fromQueue->end(), std::move(lk));
}

//ThreadPoolTaskExecutor::scheduleRemoteCommand
void ThreadPoolTaskExecutor::scheduleIntoPool_inlock(WorkQueue* fromQueue,
                                                     const WorkQueue::iterator& iter,
                                                     stdx::unique_lock<stdx::mutex> lk) {
    scheduleIntoPool_inlock(fromQueue, iter, std::next(iter), std::move(lk));
}

//ThreadPoolTaskExecutor::scheduleIntoPool_inlock
void ThreadPoolTaskExecutor::scheduleIntoPool_inlock(WorkQueue* fromQueue,
                                                     const WorkQueue::iterator& begin,
                                                     const WorkQueue::iterator& end,
                                                     stdx::unique_lock<stdx::mutex> lk) {
    dassert(fromQueue != &_poolInProgressQueue);
    std::vector<std::shared_ptr<CallbackState>> todo(begin, end); //赋值见
    _poolInProgressQueue.splice(_poolInProgressQueue.end(), *fromQueue, begin, end);

    lk.unlock();

    if (MONGO_FAIL_POINT(scheduleIntoPoolSpinsUntilThreadPoolShutsDown)) {
        scheduleIntoPoolSpinsUntilThreadPoolShutsDown.setMode(FailPoint::off);
        while (_pool->schedule([] {}) != ErrorCodes::ShutdownInProgress) {
            sleepmillis(100);
        }
    }

    for (const auto& cbState : todo) { //todo赋值见ThreadPoolTaskExecutor::scheduleRemoteCommand
    	//NetworkInterfaceThreadPool::schedule
        const auto status = _pool->schedule([this, cbState] { runCallback(std::move(cbState)); });
        if (status == ErrorCodes::ShutdownInProgress)
            break;
        fassert(28735, status);
    }
    _net->signalWorkAvailable();
}

void ThreadPoolTaskExecutor::runCallback(std::shared_ptr<CallbackState> cbStateArg) {
    CallbackHandle cbHandle;
    setCallbackForHandle(&cbHandle, cbStateArg);
    CallbackArgs args(this,
                      std::move(cbHandle),
                      cbStateArg->canceled.load()
                          ? Status({ErrorCodes::CallbackCanceled, "Callback canceled"})
                          : Status::OK());
    invariant(!cbStateArg->isFinished.load());
    {
        // After running callback function, clear 'cbStateArg->callback' to release any resources
        // that might be held by this function object.
        // Swap 'cbStateArg->callback' with temporary copy before running callback for exception
        // safety.
        TaskExecutor::CallbackFn callback;
        std::swap(cbStateArg->callback, callback);
        callback(std::move(args));
    }
    cbStateArg->isFinished.store(true);
    stdx::lock_guard<stdx::mutex> lk(_mutex);
    _poolInProgressQueue.erase(cbStateArg->iter);
    if (cbStateArg->finishedCondition) {
        cbStateArg->finishedCondition->notify_all();
    }
}

bool ThreadPoolTaskExecutor::_inShutdown_inlock() const {
    return _state >= joinRequired;
}

void ThreadPoolTaskExecutor::_setState_inlock(State newState) {
    if (newState == _state) {
        return;
    }
    _state = newState;
    _stateChange.notify_all();
}

void ThreadPoolTaskExecutor::dropConnections(const HostAndPort& hostAndPort) {
    _net->dropConnections(hostAndPort);
}

}  // namespace executor
}  // namespace mongo
