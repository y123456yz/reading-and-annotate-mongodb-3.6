/**
 *    Copyright (C) 2016 MongoDB Inc.
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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kNetwork

#include "mongo/platform/basic.h"

#include "mongo/s/service_entry_point_mongos.h"

#include "mongo/db/auth/authorization_session.h"
#include "mongo/db/commands.h"
#include "mongo/db/dbmessage.h"
#include "mongo/db/lasterror.h"
#include "mongo/db/operation_context.h"
#include "mongo/db/service_context.h"
#include "mongo/s/client/shard_connection.h"
#include "mongo/s/cluster_last_error_info.h"
#include "mongo/s/commands/strategy.h"
#include "mongo/util/log.h"
#include "mongo/util/net/message.h"
#include "mongo/util/scopeguard.h"

namespace mongo {

namespace {

BSONObj buildErrReply(const DBException& ex) {
    BSONObjBuilder errB;
    errB.append("$err", ex.what());
    errB.append("code", ex.code());
    return errB.obj();
}

}  // namespace

#include <sys/prctl.h>
/*

Breakpoint 1, mongo::ServiceEntryPointMongos::handleRequest (this=<optimized out>, opCtx=0x7f957f43a640, message=...) at src/mongo/s/service_entry_point_mongos.cpp:76
76              LOG(1) << "yang test ........ ServiceEntryPointMongos::handleRequest thread name:" << StringData(name) << "  op:" << (int)op;
(gdb) bt
#0  mongo::ServiceEntryPointMongos::handleRequest (this=<optimized out>, opCtx=0x7f957f43a640, message=...) at src/mongo/s/service_entry_point_mongos.cpp:76
#1  0x00007f957be86f5a in mongo::ServiceStateMachine::_processMessage (this=this@entry=0x7f957f40a710, guard=...) at src/mongo/transport/service_state_machine.cpp:424
#2  0x00007f957be8209f in mongo::ServiceStateMachine::_runNextInGuard (this=0x7f957f40a710, guard=...) at src/mongo/transport/service_state_machine.cpp:501
#3  0x00007f957be85ade in operator() (__closure=0x7f957f319d80) at src/mongo/transport/service_state_machine.cpp:542
#4  std::_Function_handler<void(), mongo::ServiceStateMachine::_scheduleNextWithGuard(mongo::ServiceStateMachine::ThreadGuard, mongo::transport::ServiceExecutor::ScheduleFlags, mongo::ServiceStateMachine::Ownership)::<lambda()> >::_M_invoke(const std::_Any_data &) (__functor=...) at /usr/local/include/c++/5.4.0/functional:1871
#5  0x00007f957c2d09b9 in operator() (this=0x7f957b995fb8) at /usr/local/include/c++/5.4.0/functional:2267
#6  operator() (__closure=0x7f957b995fb0) at src/mongo/transport/service_executor_adaptive.cpp:224
#7  asio_handler_invoke<mongo::transport::ServiceExecutorAdaptive::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()> > (function=...)
    at src/third_party/asio-master/asio/include/asio/handler_invoke_hook.hpp:68
#8  invoke<mongo::transport::ServiceExecutorAdaptive::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()>, mongo::transport::ServiceExecutorAdaptive::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()> > (context=..., function=...) at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#9  dispatch<mongo::transport::ServiceExecutorAdaptive::schedule(mongo::transport::ServiceExecutor::Task, mongo::transport::ServiceExecutor::ScheduleFlags)::<lambda()> > (this=<optimized out>, 
    handler=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0x876c20f, DIE 0x87a7c0e>) at src/third_party/asio-master/asio/include/asio/impl/io_context.hpp:143
#10 mongo::transport::ServiceExecutorAdaptive::schedule(std::function<void ()>, mongo::transport::ServiceExecutor::ScheduleFlags) (this=this@entry=0x7f957f30b8c0, task=..., 
    flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse) at src/mongo/transport/service_executor_adaptive.cpp:240
#11 0x00007f957be80c8d in mongo::ServiceStateMachine::_scheduleNextWithGuard (this=this@entry=0x7f957f40a710, guard=..., flags=flags@entry=mongo::transport::ServiceExecutor::kMayRecurse, 
    ownershipModel=ownershipModel@entry=mongo::ServiceStateMachine::kOwned) at src/mongo/transport/service_state_machine.cpp:546
#12 0x00007f957be83631 in mongo::ServiceStateMachine::_sourceCallback (this=0x7f957f40a710, status=...) at src/mongo/transport/service_state_machine.cpp:327
#13 0x00007f957be8435d in operator() (status=..., __closure=<optimized out>) at src/mongo/transport/service_state_machine.cpp:287
#14 std::_Function_handler<void(mongo::Status), mongo::ServiceStateMachine::_sourceMessage(mongo::ServiceStateMachine::ThreadGuard)::<lambda(mongo::Status)> >::_M_invoke(const std::_Any_data &, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0x4f5fc0, DIE 0x5481dd>) (__functor=..., __args#0=<optimized out>) at /usr/local/include/c++/5.4.0/functional:1871
#15 0x00007f957c5521f9 in operator() (__args#0=..., this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
#16 operator() (status=..., __closure=<optimized out>) at src/mongo/transport/transport_layer_asio.cpp:123
#17 std::_Function_handler<void(mongo::Status), mongo::transport::TransportLayerASIO::asyncWait(mongo::transport::Ticket&&, mongo::transport::TransportLayer::TicketCallback)::<lambda(mongo::Status)> >::_M_invoke(const std::_Any_data &, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0xb37ad7a, DIE 0xb3d39c7>) (__functor=..., __args#0=<optimized out>) at /usr/local/include/c++/5.4.0/functional:1871
#18 0x00007f957c54ebbc in operator() (__args#0=..., this=0x7f957b996540) at /usr/local/include/c++/5.4.0/functional:2267
#19 mongo::transport::TransportLayerASIO::ASIOTicket::finishFill (this=this@entry=0x7f957f087680, status=...) at src/mongo/transport/ticket_asio.cpp:157
#20 0x00007f957c54ed5f in mongo::transport::TransportLayerASIO::ASIOSourceTicket::_bodyCallback (this=this@entry=0x7f957f087680, ec=..., size=size@entry=171) at src/mongo/transport/ticket_asio.cpp:83
#21 0x00007f957c54fec9 in operator() (size=171, ec=..., __closure=<optimized out>) at src/mongo/transport/ticket_asio.cpp:118
#22 opportunisticRead<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, mongo::transport::TransportLayerASIO::ASIOSourceTicket::_headerCallback(const std::error_code&, size_t)::<lambda(const std::error_code&, size_t)> > (this=<optimized out>, handler=<optimized out>, buffers=..., stream=..., sync=false) at src/mongo/transport/session_asio.h:191
#23 read<asio::mutable_buffers_1, mongo::transport::TransportLayerASIO::ASIOSourceTicket::_headerCallback(const std::error_code&, size_t)::<lambda(const std::error_code&, size_t)> > (handler=<optimized out>, buffers=..., sync=false, 
    this=<optimized out>) at src/mongo/transport/session_asio.h:154
#24 mongo::transport::TransportLayerASIO::ASIOSourceTicket::_headerCallback (this=0x7f957f087680, ec=..., size=<optimized out>) at src/mongo/transport/ticket_asio.cpp:118
#25 0x00007f957c550df2 in operator() (size=<optimized out>, ec=..., __closure=0x7f957b9968c8) at src/mongo/transport/ticket_asio.cpp:131
#26 operator() (start=0, bytes_transferred=<optimized out>, ec=..., this=0x7f957b9968a0) at src/third_party/asio-master/asio/include/asio/impl/read.hpp:284
#27 operator() (this=0x7f957b9968a0) at src/third_party/asio-master/asio/include/asio/detail/bind_handler.hpp:163
#28 asio_handler_invoke<asio::detail::binder2<asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> >, std::error_code, long unsigned int> > (function=...) at src/third_party/asio-master/asio/include/asio/handler_invoke_hook.hpp:68
#29 invoke<asio::detail::binder2<asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> >, std::error_code, long unsigned int>, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> > (
    context=..., function=...) at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#30 asio_handler_invoke<asio::detail::binder2<asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> >, std::error_code, long unsigned int>, asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> > (this_handler=<optimized out>, function=...)
    at src/third_party/asio-master/asio/include/asio/impl/read.hpp:337
#31 invoke<asio::detail::binder2<asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> >, std::error_code, long unsigned int>, asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> > > (context=..., function=...)
    at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#32 complete<asio::detail::binder2<asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> >, std::error_code, long unsigned int> > (this=<synthetic pointer>, handler=..., function=...)
    at src/third_party/asio-master/asio/include/asio/detail/handler_work.hpp:81
#33 asio::detail::reactive_socket_recv_op<asio::mutable_buffers_1, asio::detail::read_op<asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, const asio::mutable_buffer*, asio::detail::transfer_all_t, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::<lambda(const std::error_code&, size_t)> > >::do_complete(void *, asio::detail::operation *, const asio::error_code &, std::size_t) (owner=0x7f957f0bc000, 
    base=<optimized out>) at src/third_party/asio-master/asio/include/asio/detail/reactive_socket_recv_op.hpp:121
#34 0x00007f957c55efd3 in complete (bytes_transferred=<optimized out>, ec=..., owner=0x7f957f0bc000, this=<optimized out>) at src/third_party/asio-master/asio/include/asio/detail/scheduler_operation.hpp:39
#35 asio::detail::scheduler::do_wait_one (this=this@entry=0x7f957f0bc000, lock=..., this_thread=..., usec=<optimized out>, usec@entry=1000000, ec=...) at src/third_party/asio-master/asio/include/asio/detail/impl/scheduler.ipp:480
#36 0x00007f957c55f64a in asio::detail::scheduler::wait_one (this=0x7f957f0bc000, usec=1000000, ec=...) at src/third_party/asio-master/asio/include/asio/detail/impl/scheduler.ipp:192
#37 0x00007f957c2d64c9 in asio::io_context::run_one_until<std::chrono::_V2::steady_clock, std::chrono::duration<long, std::ratio<1l, 1000000000l> > > (this=this@entry=0x7f957f0aecf0, abs_time=...)
    at src/third_party/asio-master/asio/include/asio/impl/io_context.hpp:109
#38 0x00007f957c2d52ff in run_until<std::chrono::_V2::steady_clock, std::chrono::duration<long, std::ratio<1l, 1000000000l> > > (abs_time=..., this=0x7f957f0aecf0) at src/third_party/asio-master/asio/include/asio/impl/io_context.hpp:82
#39 run_for<long, std::ratio<1l, 1000000000l> > (rel_time=..., this=0x7f957f0aecf0) at src/third_party/asio-master/asio/include/asio/impl/io_context.hpp:74
#40 mongo::transport::ServiceExecutorAdaptive::_workerThreadRoutine (this=0x7f957f30b8c0, threadId=<optimized out>, state=...) at src/mongo/transport/service_executor_adaptive.cpp:504
#41 0x00007f957c87a444 in operator() (this=<optimized out>) at /usr/local/include/c++/5.4.0/functional:2267
---Type <return> to continue, or q <return> to quit---
#42 mongo::(anonymous namespace)::runFunc (ctx=0x7f957f1d4d20) at src/mongo/transport/service_entry_point_utils.cpp:55
#43 0x00007f957a6b0e25 in start_thread () from /lib64/libpthread.so.0
#44 0x00007f957a3de34d in clone () from /lib64/libc.so.6
(gdb) 

*/
//mongos和后端mongod交互:mongos和后端mongod的链接处理在NetworkInterfaceASIO::_connect，mongos转发数据到mongod在NetworkInterfaceASIO::_beginCommunication
//mongos和客户端交互:ServiceEntryPointMongos::handleRequest
//conn-xx线程处理解析完客户端请求后，在ASIOConnection::setup-> _impl->strand().dispatch中实现连接的过度，后续连接处理由conn-xx线程交接给Network线程
//conn-xx线程处理解析完客户端请求后，在NetworkInterfaceASIO::startCommand中的op->_strand.post([this, op, getConnectionStartTime]完成数据异步交接，而后数据由Network线程处理
//后端应答后，conn线程在BatchWriteExec::executeBatch->while (!ars.done()) {}等待后端应答后发送应答给客户端

//ServiceEntryPointMongod::handleRequest(mongod网络处理)  ServiceEntryPointMongos::handleRequest mongos网络请求处理

//ServiceStateMachine::_processMessage
DbResponse ServiceEntryPointMongos::handleRequest(OperationContext* opCtx, const Message& message) {
    // Release any cached egress connections for client back to pool before destroying
    auto guard = MakeGuard(ShardConnection::releaseMyConnections);
	
    const int32_t msgId = message.header().getId();
    const NetworkOp op = message.operation();

	/*char name[100];
	memset(name, 0, 100);
	prctl(PR_GET_NAME, name);

	//[conn----yangtest1] yang test ........ ServiceEntryPointMongos::handleRequest thread name:conn---.ngtest1  op:2004
	LOG(1) << "yang test ........ ServiceEntryPointMongos::handleRequest thread name:" << StringData(name) << "  op:" << (int)op;
	*/
	
    // This exception will not be returned to the caller, but will be logged and will close the
    // connection
    uassert(ErrorCodes::IllegalOperation,
            str::stream() << "Message type " << op << " is not supported.",
            isSupportedRequestNetworkOp(op) &&
                op != dbCommand &&    // mongos never implemented OP_COMMAND ingress support.
                op != dbCompressed);  // Decompression should be handled above us.

    // Start a new LastError session. Any exceptions thrown from here onwards will be returned
    // to the caller (if the type of the message permits it).
    auto client = opCtx->getClient();
    if (!ClusterLastErrorInfo::get(client)) {
        ClusterLastErrorInfo::get(client) = std::make_shared<ClusterLastErrorInfo>();
    }
    ClusterLastErrorInfo::get(client)->newRequest();
    LastError::get(client).startRequest();
    AuthorizationSession::get(opCtx->getClient())->startRequest(opCtx);

    DbMessage dbm(message);

    // This is before the try block since it handles all exceptions that should not cause the
    // connection to close.
    //一般走这里面  insert find都是是
    if (op == dbMsg || (op == dbQuery && NamespaceString(dbm.getns()).isCommand())) {
        return Strategy::clientCommand(opCtx, message);
    }

    NamespaceString nss;
    DbResponse dbResponse;
    try {
        if (dbm.messageShouldHaveNs()) {
            nss = NamespaceString(StringData(dbm.getns()));

            uassert(ErrorCodes::InvalidNamespace,
                    str::stream() << "Invalid ns [" << nss.ns() << "]",
                    nss.isValid());

            uassert(ErrorCodes::IllegalOperation,
                    "Can't use 'local' database through mongos",
                    nss.db() != NamespaceString::kLocalDb);
        }


        LOG(3) << "Request::process begin ns: " << nss << " msg id: " << msgId
               << " op: " << networkOpToString(op);

        switch (op) { //mongos请求的命令分类
            case dbQuery:
                // Commands are handled above through Strategy::clientCommand().
                invariant(!nss.isCommand());
                dbResponse = Strategy::queryOp(opCtx, nss, &dbm);
                break;

            case dbGetMore:
                dbResponse = Strategy::getMore(opCtx, nss, &dbm);
                break;

            case dbKillCursors:
                Strategy::killCursors(opCtx, &dbm);  // No Response.
                break;

            case dbInsert:
            case dbUpdate:
            case dbDelete:
                Strategy::writeOp(opCtx, &dbm);  // No Response.
                break;

            default:
                MONGO_UNREACHABLE;
        }

        LOG(3) << "Request::process end ns: " << nss << " msg id: " << msgId
               << " op: " << networkOpToString(op);

    } catch (const DBException& ex) {
        LOG(1) << "Exception thrown while processing " << networkOpToString(op) << " op for "
               << nss.ns() << causedBy(ex);

        if (op == dbQuery || op == dbGetMore) {
            dbResponse = replyToQuery(buildErrReply(ex), ResultFlag_ErrSet);
        } else {
            // No Response.
        }

        // We *always* populate the last error for now
        LastError::get(opCtx->getClient()).setLastError(ex.code(), ex.what());
    }
    return dbResponse;
}

}  // namespace mongo

