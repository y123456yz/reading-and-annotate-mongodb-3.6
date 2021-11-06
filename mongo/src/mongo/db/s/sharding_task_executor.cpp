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

#define MONGO_LOG_DEFAULT_COMPONENT mongo::logger::LogComponent::kSharding

#include "mongo/platform/basic.h"

#include "mongo/db/s/sharding_task_executor.h"

#include "mongo/base/disallow_copying.h"
#include "mongo/base/status_with.h"
#include "mongo/bson/timestamp.h"
#include "mongo/db/logical_time.h"
#include "mongo/db/operation_time_tracker.h"
#include "mongo/executor/thread_pool_task_executor.h"
#include "mongo/rpc/get_status_from_command_result.h"
#include "mongo/rpc/metadata/sharding_metadata.h"
#include "mongo/s/client/shard_registry.h"
#include "mongo/s/cluster_last_error_info.h"
#include "mongo/s/grid.h"
#include "mongo/util/log.h"
#include "mongo/util/scopeguard.h"

namespace mongo {
namespace executor {

namespace {
const std::string kOperationTimeField = "operationTime";
}

ShardingTaskExecutor::ShardingTaskExecutor(std::unique_ptr<ThreadPoolTaskExecutor> executor)
    : _executor(std::move(executor)) {}

//initializeGlobalShardingState->TaskExecutorPool::startup->ShardingTaskExecutor::startup->ThreadPoolTaskExecutor::startup
//TaskExecutorPool::startup
void ShardingTaskExecutor::startup() { 
    _executor->startup(); //ThreadPoolTaskExecutor::startup
}

void ShardingTaskExecutor::shutdown() {
    _executor->shutdown();
}

void ShardingTaskExecutor::join() {
    _executor->join();
}

void ShardingTaskExecutor::appendDiagnosticBSON(mongo::BSONObjBuilder* builder) const {
    _executor->appendDiagnosticBSON(builder);
}

Date_t ShardingTaskExecutor::now() {
    return _executor->now();
}

StatusWith<TaskExecutor::EventHandle> ShardingTaskExecutor::makeEvent() {
    return _executor->makeEvent();
}

void ShardingTaskExecutor::signalEvent(const EventHandle& event) {
    return _executor->signalEvent(event);
}

StatusWith<TaskExecutor::CallbackHandle> ShardingTaskExecutor::onEvent(const EventHandle& event,
                                                                       const CallbackFn& work) {
    return _executor->onEvent(event, work);
}

void ShardingTaskExecutor::waitForEvent(const EventHandle& event) {
    _executor->waitForEvent(event);
}

Status ShardingTaskExecutor::waitForEvent(OperationContext* opCtx, const EventHandle& event) {
    return _executor->waitForEvent(opCtx, event);
}

StatusWith<TaskExecutor::CallbackHandle> ShardingTaskExecutor::scheduleWork(
    const CallbackFn& work) {
    return _executor->scheduleWork(work);
}

StatusWith<TaskExecutor::CallbackHandle> ShardingTaskExecutor::scheduleWorkAt(
    Date_t when, const CallbackFn& work) {
    return _executor->scheduleWorkAt(when, work);
}

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
/*
2019-03-06T10:37:06.999+0800 D SHARDING [conn----yangtest1] Command begin db: test msg id: 8
2019-03-06T10:37:07.000+0800 D TRACKING [conn----yangtest1] Cmd: insert, TrackingId: 5c7f32529eb75a31e45d9c02
2019-03-06T10:37:07.000+0800 D EXECUTOR [conn----yangtest1] Scheduling remote command request: RemoteCommand 68 -- target:172.23.240.29:27028 db:config expDate:2019-03-06T10:37:37.000+0800 cmd:{ find: "databases", filter: { _id: "test" }, readConcern: { level: "majority", afterOpTime: { ts: Timestamp(1551839822, 1), t: 7 } }, maxTimeMS: 30000 }  thread id:19561
2019-03-06T10:37:07.000+0800 D ASIO     [conn----yangtest1] startCommand: RemoteCommand 68 -- target:172.23.240.29:27028 db:config expDate:2019-03-06T10:37:37.000+0800 cmd:{ find: "databases", filter: { _id: "test" }, readConcern: { level: "majority", afterOpTime: { ts: Timestamp(1551839822, 1), t: 7 } }, maxTimeMS: 30000 }
2019-03-06T10:37:07.000+0800 D ASIO     [NetworkInterfaceASIO-ShardRegistry-0] Initiating asynchronous command: RemoteCommand 68 -- target:172.23.240.29:27028 db:config expDate:2019-03-06T10:37:37.000+0800 cmd:{ find: "databases", filter: { _id: "test" }, readConcern: { level: "majority", afterOpTime: { ts: Timestamp(1551839822, 1), t: 7 } }, maxTimeMS: 30000 }
2019-03-06T10:37:07.000+0800 D NETWORK  [NetworkInterfaceASIO-ShardRegistry-0] Compressing message with snappy
2019-03-06T10:37:07.000+0800 D ASIO     [NetworkInterfaceASIO-ShardRegistry-0] Starting asynchronous command 68 on host 172.23.240.29:27028
2019-03-06T10:37:07.001+0800 D NETWORK  [NetworkInterfaceASIO-ShardRegistry-0] Decompressing message with snappy
2019-03-06T10:37:07.001+0800 D ASIO     [NetworkInterfaceASIO-ShardRegistry-0] Request 68 finished with response: { cursor: { firstBatch: [ { _id: "test", primary: "featdoc", partitioned: false } ], id: 0, ns: "config.databases" }, ok: 1.0, operationTime: Timestamp(1551839822, 1), $replData: { term: 7, lastOpCommitted: { ts: Timestamp(1551839822, 1), t: 7 }, lastOpVisible: { ts: Timestamp(1551839822, 1), t: 7 }, configVersion: 1, replicaSetId: ObjectId('5c6e1c764e3e991ab8278bd9'), primaryIndex: 0, syncSourceIndex: 0 }, $gleStats: { lastOpTime: Timestamp(0, 0), electionId: ObjectId('000000000000000000000000') }, $clusterTime: { clusterTime: Timestamp(1551839822, 1), signature: { hash: BinData(0, 0000000000000000000000000000000000000000), keyId: 0 } } }
2019-03-06T10:37:07.001+0800 D EXECUTOR [NetworkInterfaceASIO-ShardRegistry-0] Received remote response: RemoteResponse --  cmd:{ cursor: { firstBatch: [ { _id: "test", primary: "featdoc", partitioned: false } ], id: 0, ns: "config.databases" }, ok: 1.0, operationTime: Timestamp(1551839822, 1), $replData: { term: 7, lastOpCommitted: { ts: Timestamp(1551839822, 1), t: 7 }, lastOpVisible: { ts: Timestamp(1551839822, 1), t: 7 }, configVersion: 1, replicaSetId: ObjectId('5c6e1c764e3e991ab8278bd9'), primaryIndex: 0, syncSourceIndex: 0 }, $gleStats: { lastOpTime: Timestamp(0, 0), electionId: ObjectId('000000000000000000000000') }, $clusterTime: { clusterTime: Timestamp(1551839822, 1), signature: { hash: BinData(0, 0000000000000000000000000000000000000000), keyId: 0 } } }
2019-03-06T10:37:07.002+0800 D EXECUTOR [conn----yangtest1] Scheduling remote command request: RemoteCommand 70 -- target:172.23.240.29:27018 db:config expDate:2019-03-06T10:37:37.002+0800 cmd:{ find: "collections", filter: { _id: /^test\./ }, readConcern: { level: "majority", afterOpTime: { ts: Timestamp(1551839822, 1), t: 7 } }, maxTimeMS: 30000 }  thread id:19561
2019-03-06T10:37:07.002+0800 D ASIO     [conn----yangtest1] startCommand: RemoteCommand 70 -- target:172.23.240.29:27018 db:config expDate:2019-03-06T10:37:37.002+0800 cmd:{ find: "collections", filter: { _id: /^test\./ }, readConcern: { level: "majority", afterOpTime: { ts: Timestamp(1551839822, 1), t: 7 } }, maxTimeMS: 30000 }
2019-03-06T10:37:07.002+0800 D ASIO     [NetworkInterfaceASIO-ShardRegistry-0] Initiating asynchronous command: RemoteCommand 70 -- target:172.23.240.29:27018 db:config expDate:2019-03-06T10:37:37.002+0800 cmd:{ find: "collections", filter: { _id: /^test\./ }, readConcern: { level: "majority", afterOpTime: { ts: Timestamp(1551839822, 1), t: 7 } }, maxTimeMS: 30000 }
2019-03-06T10:37:07.002+0800 D NETWORK  [NetworkInterfaceASIO-ShardRegistry-0] Compressing message with snappy
2019-03-06T10:37:07.002+0800 D ASIO     [NetworkInterfaceASIO-ShardRegistry-0] Starting asynchronous command 70 on host 172.23.240.29:27018
2019-03-06T10:37:07.003+0800 D NETWORK  [NetworkInterfaceASIO-ShardRegistry-0] Decompressing message with snappy
2019-03-06T10:37:07.003+0800 D ASIO     [NetworkInterfaceASIO-ShardRegistry-0] Request 70 finished with response: { cursor: { firstBatch: [], id: 0, ns: "config.collections" }, ok: 1.0, operationTime: Timestamp(1551839822, 1), $replData: { term: 7, lastOpCommitted: { ts: Timestamp(1551839822, 1), t: 7 }, lastOpVisible: { ts: Timestamp(1551839822, 1), t: 7 }, configVersion: 1, replicaSetId: ObjectId('5c6e1c764e3e991ab8278bd9'), primaryIndex: 0, syncSourceIndex: -1 }, $gleStats: { lastOpTime: { ts: Timestamp(1551839822, 1), t: 7 }, electionId: ObjectId('7fffffff0000000000000007') }, $clusterTime: { clusterTime: Timestamp(1551839822, 1), signature: { hash: BinData(0, 0000000000000000000000000000000000000000), keyId: 0 } } }
2019-03-06T10:37:07.003+0800 D EXECUTOR [NetworkInterfaceASIO-ShardRegistry-0] Received remote response: RemoteResponse --  cmd:{ cursor: { firstBatch: [], id: 0, ns: "config.collections" }, ok: 1.0, operationTime: Timestamp(1551839822, 1), $replData: { term: 7, lastOpCommitted: { ts: Timestamp(1551839822, 1), t: 7 }, lastOpVisible: { ts: Timestamp(1551839822, 1), t: 7 }, configVersion: 1, replicaSetId: ObjectId('5c6e1c764e3e991ab8278bd9'), primaryIndex: 0, syncSourceIndex: -1 }, $gleStats: { lastOpTime: { ts: Timestamp(1551839822, 1), t: 7 }, electionId: ObjectId('7fffffff0000000000000007') }, $clusterTime: { clusterTime: Timestamp(1551839822, 1), signature: { hash: BinData(0, 0000000000000000000000000000000000000000), keyId: 0 } } }
2019-03-06T10:37:07.003+0800 D EXECUTOR [conn----yangtest1] Scheduling remote command request: RemoteCommand 72 -- target:172.23.240.29:28018 db:test cmd:{ insert: "test", bypassDocumentValidation: false, ordered: true, documents: [ { _id: ObjectId('5c7f3252923427661274b082') } ], shardVersion: [ Timestamp(0, 0), ObjectId('000000000000000000000000') ] }  thread id:19561
2019-03-06T10:37:07.003+0800 D ASIO     [conn----yangtest1] startCommand: RemoteCommand 72 -- target:172.23.240.29:28018 db:test cmd:{ insert: "test", bypassDocumentValidation: false, ordered: true, documents: [ { _id: ObjectId('5c7f3252923427661274b082') } ], shardVersion: [ Timestamp(0, 0), ObjectId('000000000000000000000000') ] }
2019-03-06T10:37:07.004+0800 I ASIO     [NetworkInterfaceASIO-TaskExecutorPool-0-0] Connecting to 172.23.240.29:28018
2019-03-06T10:37:07.004+0800 D NETWORK  [NetworkInterfaceASIO-TaskExecutorPool-0-0] Starting client-side compression negotiation
2019-03-06T10:37:07.004+0800 D NETWORK  [NetworkInterfaceASIO-TaskExecutorPool-0-0] Offering snappy compressor to server
2019-03-06T10:37:07.004+0800 D ASIO     [NetworkInterfaceASIO-TaskExecutorPool-0-0] Starting asynchronous command 73 on host 172.23.240.29:28018
2019-03-06T10:37:07.005+0800 D NETWORK  [NetworkInterfaceASIO-TaskExecutorPool-0-0] Finishing client-side compression negotiation
2019-03-06T10:37:07.005+0800 D NETWORK  [NetworkInterfaceASIO-TaskExecutorPool-0-0] Received message compressors from server
2019-03-06T10:37:07.005+0800 D NETWORK  [NetworkInterfaceASIO-TaskExecutorPool-0-0] Adding compressor snappy
2019-03-06T10:37:07.005+0800 I ASIO     [NetworkInterfaceASIO-TaskExecutorPool-0-0] Successfully connected to 172.23.240.29:28018, took 1ms (1 connections now open to 172.23.240.29:28018)
2019-03-06T10:37:07.005+0800 D ASIO     [NetworkInterfaceASIO-TaskExecutorPool-0-0] Request 73 finished with response: {}
2019-03-06T10:37:07.005+0800 D ASIO     [NetworkInterfaceASIO-TaskExecutorPool-0-0] Initiating asynchronous command: RemoteCommand 72 -- target:172.23.240.29:28018 db:test cmd:{ insert: "test", bypassDocumentValidation: false, ordered: true, documents: [ { _id: ObjectId('5c7f3252923427661274b082') } ], shardVersion: [ Timestamp(0, 0), ObjectId('000000000000000000000000') ] }
2019-03-06T10:37:07.005+0800 D NETWORK  [NetworkInterfaceASIO-TaskExecutorPool-0-0] Compressing message with snappy
2019-03-06T10:37:07.005+0800 D ASIO     [NetworkInterfaceASIO-TaskExecutorPool-0-0] Starting asynchronous command 72 on host 172.23.240.29:28018
2019-03-06T10:37:07.006+0800 D NETWORK  [NetworkInterfaceASIO-TaskExecutorPool-0-0] Decompressing message with snappy
2019-03-06T10:37:07.006+0800 D ASIO     [NetworkInterfaceASIO-TaskExecutorPool-0-0] Request 72 finished with response: { n: 1, opTime: { ts: Timestamp(1551839827, 1), t: 10 }, electionId: ObjectId('7fffffff000000000000000a'), ok: 1.0, operationTime: Timestamp(1551839827, 1), $gleStats: { lastOpTime: { ts: Timestamp(1551839827, 1), t: 10 }, electionId: ObjectId('7fffffff000000000000000a') }, $clusterTime: { clusterTime: Timestamp(1551839827, 1), signature: { hash: BinData(0, 0000000000000000000000000000000000000000), keyId: 0 } }, $configServerState: { opTime: { ts: Timestamp(1551839822, 1), t: 7 } } }
2019-03-06T10:37:07.006+0800 D EXECUTOR [NetworkInterfaceASIO-TaskExecutorPool-0-0] Received remote response: RemoteResponse --  cmd:{ n: 1, opTime: { ts: Timestamp(1551839827, 1), t: 10 }, electionId: ObjectId('7fffffff000000000000000a'), ok: 1.0, operationTime: Timestamp(1551839827, 1), $gleStats: { lastOpTime: { ts: Timestamp(1551839827, 1), t: 10 }, electionId: ObjectId('7fffffff000000000000000a') }, $clusterTime: { clusterTime: Timestamp(1551839827, 1), signature: { hash: BinData(0, 0000000000000000000000000000000000000000), keyId: 0 } }, $configServerState: { opTime: { ts: Timestamp(1551839822, 1), t: 7 } } }
2019-03-06T10:37:07.007+0800 D SHARDING [conn----yangtest1] Command end db: test msg id: 8
*/ //例如插入一条数据:先获取mongo-cfg中database库的，在获取所有collection的。然后在把数据根据NetworkInterfaceASIO线程写入mongod
//获取mongo-config的配置信息
StatusWith<TaskExecutor::CallbackHandle> ShardingTaskExecutor::scheduleRemoteCommand(
    const RemoteCommandRequest& request, const RemoteCommandCallbackFn& cb) {

    // schedule the user's callback if there is not opCtx
    if (!request.opCtx) {
        return _executor->scheduleRemoteCommand(request, cb);
    }

    boost::optional<RemoteCommandRequest> newRequest;

    if (request.opCtx->getLogicalSessionId() && !request.cmdObj.hasField("lsid")) {
        newRequest.emplace(request);
        BSONObjBuilder bob(std::move(newRequest->cmdObj));
        {
            BSONObjBuilder subbob(bob.subobjStart("lsid"));
            request.opCtx->getLogicalSessionId()->serialize(&subbob);
        }

        newRequest->cmdObj = bob.obj();
    }

    std::shared_ptr<OperationTimeTracker> timeTracker = OperationTimeTracker::get(request.opCtx);

    auto clusterGLE = ClusterLastErrorInfo::get(request.opCtx->getClient());

    auto shardingCb = [timeTracker, clusterGLE, cb](
        const TaskExecutor::RemoteCommandCallbackArgs& args) {
        ON_BLOCK_EXIT([&cb, &args]() { cb(args); });

        // Update replica set monitor info.
        auto shard = grid.shardRegistry()->getShardForHostNoReload(args.request.target);
        if (!shard) {
            LOG(1) << "Could not find shard containing host: " << args.request.target.toString();
        }

        if (!args.response.isOK()) {
            if (shard) {
                shard->updateReplSetMonitor(args.request.target, args.response.status);
            }
            LOG(1) << "Error processing the remote request, not updating operationTime or gLE";
            return;
        }

        if (shard) {
            shard->updateReplSetMonitor(args.request.target,
                                        getStatusFromCommandResult(args.response.data));
        }

        // Update the logical clock.
        invariant(timeTracker);
        auto operationTime = args.response.data[kOperationTimeField];
        if (!operationTime.eoo()) {
            invariant(operationTime.type() == BSONType::bsonTimestamp);
            timeTracker->updateOperationTime(LogicalTime(operationTime.timestamp()));
        }

        // Update getLastError info for the client if we're tracking it.
        if (clusterGLE) {
            auto swShardingMetadata =
                rpc::ShardingMetadata::readFromMetadata(args.response.metadata);
            if (swShardingMetadata.isOK()) {
                auto shardingMetadata = std::move(swShardingMetadata.getValue());

                auto shardConn = ConnectionString::parse(args.request.target.toString());
                if (!shardConn.isOK()) {
                    severe() << "got bad host string in saveGLEStats: " << args.request.target;
                }

                clusterGLE->addHostOpTime(shardConn.getValue(),
                                          HostOpTime(shardingMetadata.getLastOpTime(),
                                                     shardingMetadata.getLastElectionId()));
            } else if (swShardingMetadata.getStatus() != ErrorCodes::NoSuchKey) {
                warning() << "Got invalid sharding metadata "
                          << redact(swShardingMetadata.getStatus()) << " metadata object was '"
                          << redact(args.response.metadata) << "'";
            }
        }
    };

    return _executor->scheduleRemoteCommand(newRequest ? *newRequest : request, shardingCb);
}

void ShardingTaskExecutor::cancel(const CallbackHandle& cbHandle) {
    _executor->cancel(cbHandle);
}

void ShardingTaskExecutor::wait(const CallbackHandle& cbHandle) {
    _executor->wait(cbHandle);
}

void ShardingTaskExecutor::appendConnectionStats(ConnectionPoolStats* stats) const {
    _executor->appendConnectionStats(stats);
}

}  // namespace executor
}  // namespace mongo
