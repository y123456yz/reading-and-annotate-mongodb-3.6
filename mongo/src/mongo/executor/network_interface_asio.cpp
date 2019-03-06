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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kASIO

#include "mongo/platform/basic.h"

#include "mongo/executor/network_interface_asio.h"

#include <asio/system_timer.hpp>

#include <utility>

#include "mongo/executor/async_stream_factory.h"
#include "mongo/executor/async_stream_interface.h"
#include "mongo/executor/async_timer_asio.h"
#include "mongo/executor/async_timer_interface.h"
#include "mongo/executor/async_timer_mock.h"
#include "mongo/executor/connection_pool_asio.h"
#include "mongo/executor/connection_pool_stats.h"
#include "mongo/rpc/metadata/metadata_hook.h"
#include "mongo/stdx/chrono.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/concurrency/idle_thread_block.h"
#include "mongo/util/concurrency/thread_name.h"
#include "mongo/util/log.h"
#include "mongo/util/net/sock.h"
#include "mongo/util/net/ssl_manager.h"
#include "mongo/util/table_formatter.h"
#include "mongo/util/time_support.h"

namespace mongo {
namespace executor {

namespace {
const std::size_t kIOServiceWorkers = 1;
}  // namespace

NetworkInterfaceASIO::Options::Options() = default;

NetworkInterfaceASIO::NetworkInterfaceASIO(Options options)
    : _options(std::move(options)),
      _io_service(),
      _metadataHook(std::move(_options.metadataHook)),
      _hook(std::move(_options.networkConnectionHook)),
      _state(State::kReady),
      _timerFactory(std::move(_options.timerFactory)),
      _streamFactory(std::move(_options.streamFactory)),
      //ConnectionPool _connectionPool
      _connectionPool(stdx::make_unique<connection_pool_asio::ASIOImpl>(this),
                      _options.instanceName, //线程名
                      _options.connectionPoolOptions),
      _isExecutorRunnable(false),
      _strand(_io_service) {
    invariant(_timerFactory);
}

std::string NetworkInterfaceASIO::getDiagnosticString() {
    stdx::lock_guard<stdx::mutex> lk(_inProgressMutex);
    return _getDiagnosticString_inlock(nullptr);
}

std::string NetworkInterfaceASIO::_getDiagnosticString_inlock(AsyncOp* currentOp) {
    str::stream output;
    std::vector<TableRow> rows;

    output << "\nNetworkInterfaceASIO Operations' Diagnostic:\n";
    rows.push_back({"Operation:", "Count:"});
    rows.push_back({"Connecting", std::to_string(_inGetConnection.size())});
    rows.push_back({"In Progress", std::to_string(_inProgress.size())});
    rows.push_back({"Succeeded", std::to_string(getNumSucceededOps())});
    rows.push_back({"Canceled", std::to_string(getNumCanceledOps())});
    rows.push_back({"Failed", std::to_string(getNumFailedOps())});
    rows.push_back({"Timed Out", std::to_string(getNumTimedOutOps())});
    output << toTable(rows);

    if (_inProgress.size() > 0) {
        rows.clear();
        rows.push_back(AsyncOp::kFieldLabels);

        // Push AsyncOps
        for (auto&& kv : _inProgress) {
            auto row = kv.first->getStringFields();
            if (currentOp) {
                // If this is the AsyncOp we blew up on, mark with an asterisk
                if (*currentOp == *(kv.first)) {
                    row[0] = "*";
                }
            }

            rows.push_back(row);
        }

        // Format as a table
        output << "\n" << toTable(rows);
    }

    output << "\n";

    return output;
}

uint64_t NetworkInterfaceASIO::getNumCanceledOps() {
    return _numCanceledOps.load();
}

uint64_t NetworkInterfaceASIO::getNumFailedOps() {
    return _numFailedOps.load();
}

uint64_t NetworkInterfaceASIO::getNumSucceededOps() {
    return _numSucceededOps.load();
}

uint64_t NetworkInterfaceASIO::getNumTimedOutOps() {
    return _numTimedOutOps.load();
}

void NetworkInterfaceASIO::appendConnectionStats(ConnectionPoolStats* stats) const {
    _connectionPool.appendConnectionStats(stats);
}

std::string NetworkInterfaceASIO::getHostName() {
    return getHostNameCached();
}

//initializeGlobalShardingState->TaskExecutorPool::startup->ShardingTaskExecutor::startup->ThreadPoolTaskExecutor::startup
//ThreadPoolTaskExecutor::startup  
void NetworkInterfaceASIO::startup() { 
	LOG(2) << "yang test ............ NetworkInterfaceASIO::startup:" << (int)kIOServiceWorkers;
    _serviceRunners.resize(kIOServiceWorkers);
    for (std::size_t i = 0; i < kIOServiceWorkers; ++i) { 
        _serviceRunners[i] = stdx::thread([this, i]() {
			//instanceName来源makeShardingTaskExecutorPool   NetworkInterfaceASIO-TaskExecutorPool-0-1中0-1的0代表是那个pool，1代表是那个ServiceWorkers
            setThreadName(_options.instanceName + "-" + std::to_string(i));
            try {
                LOG(2) << "The NetworkInterfaceASIO worker thread is spinning up :" <<  _options.instanceName + "-" + std::to_string(i); //工作线程开始工作
                asio::io_service::work work(_io_service);
                std::error_code ec;
                _io_service.run(ec);
                if (ec) {
                    severe() << "Failure in _io_service.run(): " << ec.message();
                    fassertFailed(40335);
                }
            } catch (...) {
                severe() << "Uncaught exception in NetworkInterfaceASIO IO "
                            "worker thread of type: "
                         << exceptionToStatus();
                fassertFailed(28820);
            }
        });
    };
    _state.store(State::kRunning);
}

void NetworkInterfaceASIO::shutdown() {
    _state.store(State::kShutdown);
    _io_service.stop();
    for (auto&& worker : _serviceRunners) {
        worker.join();
    }
    LOG(2) << "NetworkInterfaceASIO shutdown successfully";
}

void NetworkInterfaceASIO::waitForWork() {
    stdx::unique_lock<stdx::mutex> lk(_executorMutex);
    // TODO: This can be restructured with a lambda.
    while (!_isExecutorRunnable) {
        MONGO_IDLE_THREAD_BLOCK;
        _isExecutorRunnableCondition.wait(lk);
    }
    _isExecutorRunnable = false;
}

void NetworkInterfaceASIO::waitForWorkUntil(Date_t when) {
    stdx::unique_lock<stdx::mutex> lk(_executorMutex);
    // TODO: This can be restructured with a lambda.
    while (!_isExecutorRunnable) {
        const Milliseconds waitTime(when - now());
        if (waitTime <= Milliseconds(0)) {
            break;
        }
        MONGO_IDLE_THREAD_BLOCK;
        _isExecutorRunnableCondition.wait_for(lk, waitTime.toSystemDuration());
    }
    _isExecutorRunnable = false;
}

void NetworkInterfaceASIO::signalWorkAvailable() {
    stdx::unique_lock<stdx::mutex> lk(_executorMutex);
    _signalWorkAvailable_inlock();
}

void NetworkInterfaceASIO::_signalWorkAvailable_inlock() {
    if (!_isExecutorRunnable) {
        _isExecutorRunnable = true;
        _isExecutorRunnableCondition.notify_one();
    }
}

Date_t NetworkInterfaceASIO::now() {
    return _timerFactory->now();
}

namespace {

Status attachMetadataIfNeeded(RemoteCommandRequest& request,
                              rpc::EgressMetadataHook* metadataHook) {

    // Append the metadata of the request with metadata from the metadata hook
    // if a hook is installed
    if (metadataHook) {
        BSONObjBuilder augmentedBob(std::move(request.metadata));

        auto writeStatus = callNoexcept(*metadataHook,
                                        &rpc::EgressMetadataHook::writeRequestMetadata,
                                        request.opCtx,
                                        &augmentedBob);
        if (!writeStatus.isOK()) {
            return writeStatus;
        }

        request.metadata = augmentedBob.obj();
    }

    return Status::OK();
}

}  // namespace

/*
调用栈
(gdb) bt
#0  mongo::executor::NetworkInterfaceASIO::startCommand(mongo::executor::TaskExecutor::CallbackHandle const&, mongo::executor::RemoteCommandRequest&, std::function<void (mongo::executor::RemoteCommandResponse const&)> const&) (
    this=this@entry=0x7fb3984abb80, cbHandle=..., request=..., onFinish=...) at src/mongo/executor/network_interface_asio.cpp:426
#1  0x00007fb39591b06b in mongo::executor::ThreadPoolTaskExecutor::scheduleRemoteCommand(mongo::executor::RemoteCommandRequest const&, std::function<void (mongo::executor::TaskExecutor::RemoteCommandCallbackArgs const&)> const&) (
    this=<optimized out>, request=..., cb=...) at src/mongo/executor/thread_pool_task_executor.cpp:502
#2  0x00007fb39530f126 in mongo::executor::ShardingTaskExecutor::scheduleRemoteCommand(mongo::executor::RemoteCommandRequest const&, std::function<void (mongo::executor::TaskExecutor::RemoteCommandCallbackArgs const&)> const&) (
    this=0x7fb39859db90, request=..., cb=...) at src/mongo/db/s/sharding_task_executor.cpp:310
#3  0x00007fb3955026ae in mongo::AsyncRequestsSender::_scheduleRequest (this=this@entry=0x7fb394dff110, remoteIndex=remoteIndex@entry=0) at src/mongo/s/async_requests_sender.cpp:246
#4  0x00007fb395502bff in mongo::AsyncRequestsSender::_scheduleRequests (this=this@entry=0x7fb394dff110, lk=...) at src/mongo/s/async_requests_sender.cpp:215
#5  0x00007fb39550787a in mongo::AsyncRequestsSender::AsyncRequestsSender (this=0x7fb394dff110, opCtx=<optimized out>, executor=<optimized out>, db=..., requests=..., readPreference=..., retryPolicy=mongo::Shard::kNoRetry)
    at src/mongo/s/async_requests_sender.cpp:80
#6  0x00007fb3953c743c in mongo::BatchWriteExec::executeBatch (opCtx=opCtx@entry=0x7fb398950640, targeter=..., clientRequest=..., clientResponse=clientResponse@entry=0x7fb394dff960, stats=stats@entry=0x7fb394dff8a0)
    at src/mongo/s/write_ops/batch_write_exec.cpp:214
#7  0x00007fb3953d31d6 in mongo::ClusterWriter::write (opCtx=opCtx@entry=0x7fb398950640, request=..., stats=stats@entry=0x7fb394dff8a0, response=response@entry=0x7fb394dff960) at src/mongo/s/commands/cluster_write.cpp:234
#8  0x00007fb395393c3a in mongo::(anonymous namespace)::ClusterWriteCmd::enhancedRun (this=0x7fb39687d6a0 <mongo::(anonymous namespace)::clusterInsertCmd>, opCtx=0x7fb398950640, request=..., result=...)
    at src/mongo/s/commands/cluster_write_cmd.cpp:204
#9  0x00007fb3957ca4df in mongo::Command::publicRun (this=this@entry=0x7fb39687d6a0 <mongo::(anonymous namespace)::clusterInsertCmd>, opCtx=0x7fb398950640, request=..., result=...) at src/mongo/db/commands.cpp:357
#10 0x00007fb3953b253d in execCommandClient (result=..., request=..., c=0x7fb39687d6a0 <mongo::(anonymous namespace)::clusterInsertCmd>, opCtx=0x7fb398950640) at src/mongo/s/commands/strategy.cpp:227
#11 mongo::(anonymous namespace)::runCommand(mongo::OperationContext *, const mongo::OpMsgRequest &, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0x2848144, DIE 0x298d258>) (opCtx=0x7fb398950640, 
    request=..., builder=builder@entry=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0x2848144, DIE 0x298d258>) at src/mongo/s/commands/strategy.cpp:267
#12 0x00007fb3953b325c in mongo::Strategy::<lambda()>::operator()(void) const (__closure=__closure@entry=0x7fb394e00610) at src/mongo/s/commands/strategy.cpp:425
#13 0x00007fb3953b3919 in mongo::Strategy::clientCommand (opCtx=opCtx@entry=0x7fb398950640, m=...) at src/mongo/s/commands/strategy.cpp:436
#14 0x00007fb3952d4921 in mongo::ServiceEntryPointMongos::handleRequest (this=<optimized out>, opCtx=0x7fb398950640, message=...) at src/mongo/s/service_entry_point_mongos.cpp:167
#15 0x00007fb3952f1fca in mongo::ServiceStateMachine::_processMessage (this=this@entry=0x7fb3986cdc50, guard=...) at src/mongo/transport/service_state_machine.cpp:455
#16 0x00007fb3952ecf0f in mongo::ServiceStateMachine::_runNextInGuard (this=0x7fb3986cdc50, guard=...) at src/mongo/transport/service_state_machine.cpp:532
#17 0x00007fb3952f09ed in operator() (__closure=0x7fb3985de880) at src/mongo/transport/service_state_machine.cpp:573
#18 std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#19 0x00007fb39573bb09 in operator() (this=0x7fb394e00ee8) at /usr/local/include/c++/5.4.0/functional:2267
#20 operator() (__closure=0x7fb394e00ee0) at src/mongo/transport/service_executor_adaptive.cpp:224
#21 asio_handler_invoke<mongo::transport::ServiceExecutorAdaptive::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()> > (function=...)
    at src/third_party/asio-master/asio/include/asio/handler_invoke_hook.hpp:68
#22 invoke<mongo::transport::ServiceExecutorAdaptive::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()>, mongo::transport::ServiceExecutorAdaptive::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()> > (context=..., function=...) at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#23 dispatch<mongo::transport::ServiceExecutorAdaptive::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()> > (this=<optimized out>, 
    handler=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0x876b848, DIE 0x87a7247>) at src/third_party/asio-master/asio/include/asio/impl/io_context.hpp:143
#24 mongo::transport::ServiceExecutorAdaptive::schedule(std::function<void ()>, mongo::transport::ServiceExecutor::ScheduleFlags) (this=this@entry=0x7fb3985cf8c0, task=..., 
    flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse) at src/mongo/transport/service_executor_adaptive.cpp:240
#25 0x00007fb3952eba05 in mongo::ServiceStateMachine::_scheduleNextWithGuard (this=this@entry=0x7fb3986cdc50, guard=..., flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse, 
    ownershipModel=ownershipModel@entry=mongo::ServiceStateMachine::kOwned) at src/mongo/transport/service_state_machine.cpp:577
#26 0x00007fb3952ee602 in mongo::ServiceStateMachine::_sourceCallback (this=0x7fb3986cdc50, status=...) at src/mongo/transport/service_state_machine.cpp:358
#27 0x00007fb3952ef38d in operator() (status=..., __closure=<optimized out>) at src/mongo/transport/service_state_machine.cpp:317
#28 std::_Function_handler<void(mongo::Status), mongo::ServiceStateMachine::_sourceMessage(mongo::ServiceStateMachine::ThreadGuard)::<lambda(mongo::Status)> >::_M_invoke(const std::_Any_data &, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0x4f5bfb, DIE 0x5481eb>) (__functor=..., __args#0=<optimized out>) at /usr/local/include/c++/5.4.0/functional:1871
#29 0x00007fb3959bd649 in operator() (__args#0=..., this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#30 operator() (status=..., __closure=<optimized out>) at src/mongo/transport/transport_layer_asio.cpp:123
#31 std::_Function_handler<void(mongo::Status), mongo::transport::TransportLayerASIO::asyncWait(mongo::transport::Ticket&&, mongo::transport::TransportLayer::TicketCallback)::<lambda(mongo::Status)> >::_M_invoke(const std::_Any_data &, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0xb37a0d6, DIE 0xb3d2d23>) (__functor=..., __args#0=<optimized out>) at /usr/local/include/c++/5.4.0/functional:1871
#32 0x00007fb3959ba00c in operator() (__args#0=..., this=0x7fb394e01540) at /usr/local/include/c++/5.4.0/functional:2267
#33 mongo::transport::TransportLayerASIO::ASIOTicket::finishFill (this=this@entry=0x7fb398496e40, status=...) at src/mongo/transport/ticket_asio.cpp:158
#34 0x00007fb3959ba1af in mongo::transport::TransportLayerASIO::ASIOSourceTicket::_bodyCallback (this=this@entry=0x7fb398496e40, ec=..., size=size@entry=190) at src/mongo/transport/ticket_asio.cpp:83
#35 0x00007fb3959bb319 in operator() (size=190, ec=..., __closure=<optimized out>) at src/mongo/transport/ticket_asio.cpp:119
#36 opportunisticRead<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, mongo::transport::TransportLayerASIO::ASIOSourceTicket::_headerCallback(const std::error_code&, size_t)::<lambda(const std::error_code&, size_t)> > (this=<optimized out>, handler=<optimized out>, buffers=..., stream=..., sync=false) at src/mongo/transport/session_asio.h:191
#37 read<asio::mutable_buffers_1, mongo::transport::TransportLayerASIO::ASIOSourceTicket::_headerCallback(const std::error_code&, size_t)::<lambda(const std::error_code&, size_t)> > (handler=<optimized out>, buffers=..., sync=false, 
    this=<optimized out>) at src/mongo/transport/session_asio.h:154
#38 mongo::transport::TransportLayerASIO::ASIOSourceTicket::_headerCallback (this=0x7fb398496e40, ec=..., size=<optimized out>) at src/mongo/transport/ticket_asio.cpp:119
#39 0x00007fb3959bc242 in operator() (size=<optimized out>, ec=..., __closure=0x7fb394e018c8) at src/mongo/transport/ticket_asio.cpp:132
#40 operator() (start=0, bytes_transferred=<optimized out>, ec=..., this=0x7fb394e018a0) at src/third_party/asio-master/asio/include/asio/impl/read.hpp:284
#41 operator() (this=0x7fb394e018a0) at src/third_party/asio-master/asio/include/asio/detail/bind_handler.hpp:163
#42 asio_handler_invoke<asio::detail::binder2<asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> >, std::error_code, long unsigned int> > (function=...) at src/third_party/asio-master/asio/include/asio/handler_invoke_hook.hpp:68
#43 invoke<asio::detail::binder2<asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> >, std::error_code, long unsigned int>, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> > (
    context=..., function=...) at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#44 asio_handler_invoke<asio::detail::binder2<asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> >, std::error_code, long unsigned int>, asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*,---Type <return> to continue, or q <return> to quit---
 asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> > (this_handler=<optimized out>, function=...)
    at src/third_party/asio-master/asio/include/asio/impl/read.hpp:337
#45 invoke<asio::detail::binder2<asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> >, std::error_code, long unsigned int>, asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> > > (context=..., function=...)
    at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#46 complete<asio::detail::binder2<asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> >, std::error_code, long unsigned int> > (this=<synthetic pointer>, handler=..., function=...)
    at src/third_party/asio-master/asio/include/asio/detail/handler_work.hpp:81
#47 asio::detail::reactive_socket_recv_op<asio::mutable_buffers_1, asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> > >::do_complete(void *, asio::detail::operation *, const asio::error_code &, std::size_t) (owner=0x7fb398380100, 
    base=<optimized out>) at src/third_party/asio-master/asio/include/asio/detail/reactive_socket_recv_op.hpp:121
#48 0x00007fb3959ca423 in complete (bytes_transferred=<optimized out>, ec=..., owner=0x7fb398380100, this=<optimized out>) at src/third_party/asio-master/asio/include/asio/detail/scheduler_operation.hpp:39
#49 asio::detail::scheduler::do_wait_one (this=this@entry=0x7fb398380100, lock=..., this_thread=..., usec=<optimized out>, usec@entry=1000000, ec=...) at src/third_party/asio-master/asio/include/asio/detail/impl/scheduler.ipp:480
#50 0x00007fb3959caa9a in asio::detail::scheduler::wait_one (this=0x7fb398380100, usec=1000000, ec=...) at src/third_party/asio-master/asio/include/asio/detail/impl/scheduler.ipp:192
#51 0x00007fb395741619 in asio::io_context::run_one_until<std::chrono::_V2::steady_clock, std::chrono::duration<long, std::ratio<1l, 1000000000l> > > (this=this@entry=0x7fb398372ef0, abs_time=...)
    at src/third_party/asio-master/asio/include/asio/impl/io_context.hpp:109
#52 0x00007fb39574044f in run_until<std::chrono::_V2::steady_clock, std::chrono::duration<long, std::ratio<1l, 1000000000l> > > (abs_time=..., this=0x7fb398372ef0) at src/third_party/asio-master/asio/include/asio/impl/io_context.hpp:82
#53 run_for<long, std::ratio<1l, 1000000000l> > (rel_time=..., this=0x7fb398372ef0) at src/third_party/asio-master/asio/include/asio/impl/io_context.hpp:74
#54 mongo::transport::ServiceExecutorAdaptive::_workerThreadRoutine (this=0x7fb3985cf8c0, threadId=<optimized out>, state=...) at src/mongo/transport/service_executor_adaptive.cpp:510
#55 0x00007fb395ce5894 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#56 mongo::(anonymous namespace)::runFunc (ctx=0x7fb398499f60) at src/mongo/transport/service_entry_point_utils.cpp:55
*/ //ThreadPoolTaskExecutor::scheduleRemoteCommand中调用
Status NetworkInterfaceASIO::startCommand(const TaskExecutor::CallbackHandle& cbHandle,
                                          RemoteCommandRequest& request,
                                          const RemoteCommandCompletionFn& onFinish) {
    MONGO_ASIO_INVARIANT(onFinish, "Invalid completion function");
    {
        stdx::lock_guard<stdx::mutex> lk(_inProgressMutex);
        const auto insertResult = _inGetConnection.emplace(cbHandle);
        // We should never see the same CallbackHandle added twice
        MONGO_ASIO_INVARIANT_INLOCK(insertResult.second, "Same CallbackHandle added twice");
    }

    if (inShutdown()) {
        return {ErrorCodes::ShutdownInProgress, "NetworkInterfaceASIO shutdown in progress"};
    }

    LOG(2) << "startCommand: " << redact(request.toString());

    auto getConnectionStartTime = now();

    auto statusMetadata = attachMetadataIfNeeded(request, _metadataHook.get());
    if (!statusMetadata.isOK()) {
        return statusMetadata;
    }

    auto nextStep = [this, getConnectionStartTime, cbHandle, request, onFinish](
        StatusWith<ConnectionPool::ConnectionHandle> swConn) {

        if (!swConn.isOK()) {
            LOG(2) << "Failed to get connection from pool for request " << request.id << ": "
                   << swConn.getStatus();

            bool wasPreviouslyCanceled = false;
            {
                stdx::lock_guard<stdx::mutex> lk(_inProgressMutex);
                wasPreviouslyCanceled = _inGetConnection.erase(cbHandle) == 0;
            }

            Status status = wasPreviouslyCanceled
                ? Status(ErrorCodes::CallbackCanceled, "Callback canceled")
                : swConn.getStatus();
            if (ErrorCodes::isExceededTimeLimitError(status.code())) {
                _numTimedOutOps.fetchAndAdd(1);
            }
            if (status.code() != ErrorCodes::CallbackCanceled) {
                _numFailedOps.fetchAndAdd(1);
            }

            onFinish({status, now() - getConnectionStartTime});
            signalWorkAvailable();
            return;
        }

        auto conn = static_cast<connection_pool_asio::ASIOConnection*>(swConn.getValue().get());

        AsyncOp* op = nullptr;

        stdx::unique_lock<stdx::mutex> lk(_inProgressMutex);

        const auto eraseCount = _inGetConnection.erase(cbHandle);

        // If we didn't find the request, we've been canceled
        if (eraseCount == 0) {
            lk.unlock();

            onFinish({ErrorCodes::CallbackCanceled,
                      "Callback canceled",
                      now() - getConnectionStartTime});

            // Though we were canceled, we know that the stream is fine, so indicate success.
            conn->indicateSuccess();

            signalWorkAvailable();

            return;
        }

        // We can't release the AsyncOp until we know we were not canceled.
        auto ownedOp = conn->releaseAsyncOp();
        op = ownedOp.get();

        // This AsyncOp may be recycled. We expect timeout and canceled to be clean.
        // If this op was most recently used to connect, its state transitions won't have been
        // reset, so we do that here.
        MONGO_ASIO_INVARIANT_INLOCK(!op->canceled(), "AsyncOp has dirty canceled flag", op);
        MONGO_ASIO_INVARIANT_INLOCK(!op->timedOut(), "AsyncOp has dirty timeout flag", op);
        op->clearStateTransitions();

        // Now that we're inProgress, an external cancel can touch our op, but
        // not until we release the inProgressMutex.
        _inProgress.emplace(op, std::move(ownedOp));

        op->_cbHandle = std::move(cbHandle);
        op->_request = std::move(request);
        op->_onFinish = std::move(onFinish);
        op->_connectionPoolHandle = std::move(swConn.getValue());
        op->startProgress(getConnectionStartTime);

        // This ditches the lock and gets us onto the strand (so we're
        // threadsafe)
        op->_strand.post([this, op, getConnectionStartTime] {
            const auto timeout = op->_request.timeout;

            // Set timeout now that we have the correct request object
            if (timeout != RemoteCommandRequest::kNoTimeout) {
                // Subtract the time it took to get the connection from the pool from the request
                // timeout.
                auto getConnectionDuration = now() - getConnectionStartTime;
                if (getConnectionDuration >= timeout) {
                    // We only assume that the request timer is guaranteed to fire *after* the
                    // timeout duration - but make no stronger assumption. It is thus possible that
                    // we have already exceeded the timeout. In this case we timeout the operation
                    // manually.
                    std::stringstream msg;
                    msg << "Remote command timed out while waiting to get a connection from the "
                        << "pool, took " << getConnectionDuration << ", timeout was set to "
                        << timeout;
                    auto rs = ResponseStatus(ErrorCodes::NetworkInterfaceExceededTimeLimit,
                                             msg.str(),
                                             getConnectionDuration);
                    return _completeOperation(op, rs);
                }

                // The above conditional guarantees that the adjusted timeout will never underflow.
                MONGO_ASIO_INVARIANT(timeout > getConnectionDuration, "timeout underflowed", op);
                const auto adjustedTimeout = timeout - getConnectionDuration;
                const auto requestId = op->_request.id;

                try {
                    op->_timeoutAlarm =
                        op->_owner->_timerFactory->make(&op->_strand, adjustedTimeout);
                } catch (std::system_error& e) {
                    severe() << "Failed to construct timer for AsyncOp: " << e.what();
                    fassertFailed(40334);
                }

                std::shared_ptr<AsyncOp::AccessControl> access;
                std::size_t generation;
                {
                    stdx::lock_guard<stdx::mutex> lk(op->_access->mutex);
                    access = op->_access;
                    generation = access->id;
                }

                op->_timeoutAlarm->asyncWait(
                    [this, op, access, generation, requestId, adjustedTimeout](std::error_code ec) {
                        // We must pass a check for safe access before using op inside the
                        // callback or we may attempt access on an invalid pointer.
                        stdx::lock_guard<stdx::mutex> lk(access->mutex);
                        if (generation != access->id) {
                            // The operation has been cleaned up, do not access.
                            return;
                        }

                        if (!ec) {
                            LOG(2) << "Request " << requestId << " timed out"
                                   << ", adjusted timeout after getting connection from pool was "
                                   << adjustedTimeout << ", op was " << redact(op->toString());

                            op->timeOut_inlock();
                        } else {
                            LOG(2) << "Failed to time request " << requestId
                                   << "out: " << ec.message() << ", op was "
                                   << redact(op->toString());
                        }
                    });
            }

            _beginCommunication(op);
        });
    };

	//executor::ConnectionPool::get
    _connectionPool.get(request.target, request.timeout, nextStep);
    return Status::OK();
}

void NetworkInterfaceASIO::cancelCommand(const TaskExecutor::CallbackHandle& cbHandle) {
    stdx::lock_guard<stdx::mutex> lk(_inProgressMutex);

    // If we found a matching cbHandle in _inGetConnection, then
    // simply removing it has the same effect as cancelling it, so we
    // can just return.
    if (_inGetConnection.erase(cbHandle) != 0) {
        _numCanceledOps.fetchAndAdd(1);
        return;
    }

    // TODO: This linear scan is unfortunate. It is here because our
    // primary data structure is to keep the AsyncOps in an
    // unordered_map by pointer, but here we only have the
    // callback. We could keep two data structures at the risk of
    // having them diverge.
    for (auto&& kv : _inProgress) {
        if (kv.first->cbHandle() == cbHandle) {
            kv.first->cancel();
            _numCanceledOps.fetchAndAdd(1);
            break;
        }
    }
}

Status NetworkInterfaceASIO::setAlarm(Date_t when, const stdx::function<void()>& action) {
    if (inShutdown()) {
        return {ErrorCodes::ShutdownInProgress, "NetworkInterfaceASIO shutdown in progress"};
    }

    std::shared_ptr<asio::system_timer> alarm;

    try {
        auto timeLeft = when - now();
        // "alarm" must stay alive until it expires, hence the shared_ptr.
        alarm = std::make_shared<asio::system_timer>(_io_service, timeLeft.toSystemDuration());
    } catch (...) {
        return exceptionToStatus();
    }

    alarm->async_wait([alarm, this, action, when](std::error_code ec) {
        const auto nowValue = now();
        if (nowValue < when) {
            warning() << "ASIO alarm returned early. Expected at: " << when
                      << ", fired at: " << nowValue;
            const auto status = setAlarm(when, action);
            if ((!status.isOK()) && (status.code() != ErrorCodes::ShutdownInProgress)) {
                fassertFailedWithStatus(40383, status);
            }
            return;
        }
        if (!ec) {
            return action();
        } else if (ec != asio::error::operation_aborted) {
            // When the network interface is shut down, it will cancel all pending
            // alarms, raising an "operation_aborted" error here, which we ignore.
            warning() << "setAlarm() received an error: " << ec.message();
        }
    });

    return Status::OK();
};

bool NetworkInterfaceASIO::inShutdown() const {
    return (_state.load() == State::kShutdown);
}

bool NetworkInterfaceASIO::onNetworkThread() {
    auto id = stdx::this_thread::get_id();
    return std::any_of(_serviceRunners.begin(),
                       _serviceRunners.end(),
                       [id](const stdx::thread& thread) { return id == thread.get_id(); });
}

void NetworkInterfaceASIO::_failWithInfo(const char* file,
                                         int line,
                                         std::string error,
                                         AsyncOp* op) {
    stdx::lock_guard<stdx::mutex> lk(_inProgressMutex);
    _failWithInfo_inlock(file, line, error, op);
}

void NetworkInterfaceASIO::_failWithInfo_inlock(const char* file,
                                                int line,
                                                std::string error,
                                                AsyncOp* op) {
    std::stringstream ss;
    ss << "Invariant failure at " << file << ":" << line << ": " << error;
    ss << _getDiagnosticString_inlock(op);
    Status status{ErrorCodes::InternalError, ss.str()};
    fassertFailedWithStatus(34429, status);
}

void NetworkInterfaceASIO::dropConnections(const HostAndPort& hostAndPort) {
    _connectionPool.dropConnections(hostAndPort);
}

}  // namespace executor
}  // namespace mongo
