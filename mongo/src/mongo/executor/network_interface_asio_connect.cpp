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

#include <utility>

#include "mongo/base/system_error.h"
#include "mongo/config.h"
#include "mongo/db/wire_version.h"
#include "mongo/executor/async_stream.h"
#include "mongo/executor/async_stream_factory.h"
#include "mongo/executor/async_stream_interface.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/log.h"

namespace mongo {
namespace executor {

using asio::ip::tcp;

NetworkInterfaceASIO::AsyncConnection::AsyncConnection(std::unique_ptr<AsyncStreamInterface> stream,
                                                       rpc::ProtocolSet protocols)
    : _stream(std::move(stream)),
      _serverProtocols(protocols),
      _clientProtocols(rpc::computeProtocolSet(WireSpec::instance().outgoing)) {}

AsyncStreamInterface& NetworkInterfaceASIO::AsyncConnection::stream() {
    return *_stream;
}

void NetworkInterfaceASIO::AsyncConnection::cancel() {
    _stream->cancel();
}

rpc::ProtocolSet NetworkInterfaceASIO::AsyncConnection::serverProtocols() const {
    return _serverProtocols;
}

rpc::ProtocolSet NetworkInterfaceASIO::AsyncConnection::clientProtocols() const {
    return _clientProtocols;
}

void NetworkInterfaceASIO::AsyncConnection::setServerProtocols(rpc::ProtocolSet protocols) {
    _serverProtocols = protocols;
}

/*
#0  mongo::executor::NetworkInterfaceASIO::_connect (this=0x7fdf88b2db80, op=0x7fdf89341880) at src/mongo/executor/network_interface_asio_connect.cpp:79
#1  0x00007fdf85a819ec in operator() (__closure=0x7fdf820a8960) at src/mongo/executor/connection_pool_asio.cpp:252
#2  asio_handler_invoke<mongo::executor::connection_pool_asio::ASIOConnection::setup(mongo::Milliseconds, mongo::executor::ConnectionPool::ConnectionInterface::SetupCallback)::<lambda()> > (function=...)
    at src/third_party/asio-master/asio/include/asio/handler_invoke_hook.hpp:68
#3  invoke<mongo::executor::connection_pool_asio::ASIOConnection::setup(mongo::Milliseconds, mongo::executor::ConnectionPool::ConnectionInterface::SetupCallback)::<lambda()>, mongo::executor::connection_pool_asio::ASIOConnection::setup(mongo::Milliseconds, mongo::executor::ConnectionPool::ConnectionInterface::SetupCallback)::<lambda()> > (context=..., function=...) at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#4  complete<mongo::executor::connection_pool_asio::ASIOConnection::setup(mongo::Milliseconds, mongo::executor::ConnectionPool::ConnectionInterface::SetupCallback)::<lambda()> > (this=<synthetic pointer>, handler=..., function=...)
    at src/third_party/asio-master/asio/include/asio/detail/handler_work.hpp:81
#5  asio::detail::completion_handler<mongo::executor::connection_pool_asio::ASIOConnection::setup(mongo::Milliseconds, mongo::executor::ConnectionPool::ConnectionInterface::SetupCallback)::<lambda()> >::do_complete(void *, asio::detail::operation *, const asio::error_code &, std::size_t) (owner=<optimized out>, base=<optimized out>) at src/third_party/asio-master/asio/include/asio/detail/completion_handler.hpp:69
#6  0x00007fdf85b28945 in complete (bytes_transferred=0, ec=..., owner=0x7fdf88c0fe00, this=<optimized out>) at src/third_party/asio-master/asio/include/asio/detail/scheduler_operation.hpp:39
#7  asio::detail::strand_service::do_complete (owner=0x7fdf88c0fe00, base=0x7fdf88d50500, ec=...) at src/third_party/asio-master/asio/include/asio/detail/impl/strand_service.ipp:167
#8  0x00007fdf85b25e89 in complete (bytes_transferred=<optimized out>, ec=..., owner=0x7fdf88c0fe00, this=<optimized out>) at src/third_party/asio-master/asio/include/asio/detail/scheduler_operation.hpp:39
#9  asio::detail::scheduler::do_run_one (this=this@entry=0x7fdf88c0fe00, lock=..., this_thread=..., ec=...) at src/third_party/asio-master/asio/include/asio/detail/impl/scheduler.ipp:400
#10 0x00007fdf85b260d1 in asio::detail::scheduler::run (this=0x7fdf88c0fe00, ec=...) at src/third_party/asio-master/asio/include/asio/detail/impl/scheduler.ipp:153
#11 0x00007fdf85b2626e in asio::io_context::run (this=<optimized out>, ec=...) at src/third_party/asio-master/asio/include/asio/impl/io_context.ipp:69
#12 mongo::executor::NetworkInterfaceASIO::<lambda()>::operator()(void) const (__closure=0x7fdf88c8dee8) at src/mongo/executor/network_interface_asio.cpp:165(NetworkInterfaceASIO::startup(_io_service.run))
#13 0x00007fdf8445b8f0 in std::execute_native_thread_routine (__p=<optimized out>) at ../../../.././libstdc++-v3/src/c++11/thread.cc:84
#14 0x00007fdf83c77e25 in start_thread () from /lib64/libpthread.so.0
#15 0x00007fdf839a534d in clone () from /lib64/libc.so.6



#0  mongo::executor::NetworkInterfaceASIO::_connect (this=0x7fdf88b2db80, op=0x7fdf89390a80) at src/mongo/executor/network_interface_asio_connect.cpp:79
#1  0x00007fdf85a811c7 in operator() (__closure=0x7fdf820a79a0) at src/mongo/executor/connection_pool_asio.cpp:252
#2  asio_handler_invoke<mongo::executor::connection_pool_asio::ASIOConnection::setup(mongo::Milliseconds, mongo::executor::ConnectionPool::ConnectionInterface::SetupCallback)::<lambda()> > (function=...)
    at src/third_party/asio-master/asio/include/asio/handler_invoke_hook.hpp:68
#3  invoke<mongo::executor::connection_pool_asio::ASIOConnection::setup(mongo::Milliseconds, mongo::executor::ConnectionPool::ConnectionInterface::SetupCallback)::<lambda()>, mongo::executor::connection_pool_asio::ASIOConnection::setup(mongo::Milliseconds, mongo::executor::ConnectionPool::ConnectionInterface::SetupCallback)::<lambda()> > (context=..., function=...) at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#4  complete<mongo::executor::connection_pool_asio::ASIOConnection::setup(mongo::Milliseconds, mongo::executor::ConnectionPool::ConnectionInterface::SetupCallback)::<lambda()> > (this=<synthetic pointer>, handler=..., function=...)
    at src/third_party/asio-master/asio/include/asio/detail/handler_work.hpp:81
#5  do_complete (base=<optimized out>, owner=<optimized out>) at src/third_party/asio-master/asio/include/asio/detail/completion_handler.hpp:69
#6  dispatch<mongo::executor::connection_pool_asio::ASIOConnection::setup(mongo::Milliseconds, mongo::executor::ConnectionPool::ConnectionInterface::SetupCallback)::<lambda()> > (handler=..., impl=<optimized out>, this=<optimized out>)
    at src/third_party/asio-master/asio/include/asio/detail/impl/strand_service.hpp:87
#7  dispatch<mongo::executor::connection_pool_asio::ASIOConnection::setup(mongo::Milliseconds, mongo::executor::ConnectionPool::ConnectionInterface::SetupCallback)::<lambda()> > (
    handler=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0xa6cabc4, DIE 0xa71f3ab>, this=<optimized out>) at src/third_party/asio-master/asio/include/asio/io_context_strand.hpp:224
#8  mongo::executor::connection_pool_asio::ASIOConnection::setup(mongo::Duration<std::ratio<1l, 1000l> >, std::function<void (mongo::executor::ConnectionPool::ConnectionInterface*, mongo::Status)>) (this=<optimized out>, timeout=..., 
    cb=...) at src/mongo/executor/connection_pool_asio.cpp:253
#9  0x00007fdf85abc5d6 in mongo::executor::ConnectionPool::SpecificPool::spawnConnections (this=this@entry=0x7fdf8937d000, lk=...) at src/mongo/executor/connection_pool.cpp:683
#10 0x00007fdf85abfd61 in operator() (lk=<error reading variable: access outside bounds of object referenced via synthetic pointer>, __closure=<optimized out>) at src/mongo/executor/connection_pool.cpp:677
#11 runWithActiveClient<mongo::executor::ConnectionPool::SpecificPool::spawnConnections(std::unique_lock<std::mutex>&)::<lambda(mongo::executor::ConnectionPool::ConnectionInterface*, mongo::Status)>::<lambda(std::unique_lock<std::mutex>)> > (cb=<optimized out>, lk=<error reading variable: access outside bounds of object referenced via synthetic pointer>, this=0x7fdf8937d000) at src/mongo/executor/connection_pool.cpp:99
#12 runWithActiveClient<mongo::executor::ConnectionPool::SpecificPool::spawnConnections(std::unique_lock<std::mutex>&)::<lambda(mongo::executor::ConnectionPool::ConnectionInterface*, mongo::Status)>::<lambda(std::unique_lock<std::mutex>)> > (cb=<optimized out>, this=0x7fdf8937d000) at src/mongo/executor/connection_pool.cpp:81
#13 operator() (status=..., connPtr=0x7fdf88d57e00, __closure=<optimized out>) at src/mongo/executor/connection_pool.cpp:682
#14 std::_Function_handler<void(mongo::executor::ConnectionPool::ConnectionInterface*, mongo::Status), mongo::executor::ConnectionPool::SpecificPool::spawnConnections(std::unique_lock<std::mutex>&)::<lambda(mongo::executor::ConnectionPool::ConnectionInterface*, mongo::Status)> >::_M_invoke(const std::_Any_data &, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0xac1514a, DIE 0xac79588>, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0xac1514a, DIE 0xac7958d>) (__functor=..., __args#0=<optimized out>, __args#1=<optimized out>) at /usr/local/include/c++/5.4.0/functional:1871
#15 0x00007fdf85a7f069 in operator() (__args#1=..., __args#0=0x7fdf88d57e00, this=0x7fdf88b22f68) at /usr/local/include/c++/5.4.0/functional:2267
#16 operator() (status=..., ptr=0x7fdf88d57e00, __closure=0x7fdf88b22f60) at src/mongo/executor/connection_pool_asio.cpp:227
#17 std::_Function_handler<void(mongo::executor::ConnectionPool::ConnectionInterface*, mongo::Status), mongo::executor::connection_pool_asio::ASIOConnection::setup(mongo::Milliseconds, mongo::executor::ConnectionPool::ConnectionInterface::SetupCallback)::<lambda()>::<lambda(mongo::executor::ConnectionPool::ConnectionInterface*, mongo::Status)> >::_M_invoke(const std::_Any_data &, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0xa6cabc4, DIE 0xa740023>, <unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0xa6cabc4, DIE 0xa740028>) (__functor=..., __args#0=<optimized out>, __args#1=<optimized out>)
    at /usr/local/include/c++/5.4.0/functional:1871
#18 0x00007fdf85a7a828 in operator() (__args#1=..., __args#0=0x7fdf88d57e00, this=0x7fdf820a7c40) at /usr/local/include/c++/5.4.0/functional:2267
#19 operator() (rs=..., __closure=<optimized out>) at src/mongo/executor/connection_pool_asio.cpp:187
#20 std::_Function_handler<void(const mongo::executor::RemoteCommandResponse&), mongo::executor::connection_pool_asio::ASIOConnection::makeAsyncOp(mongo::executor::connection_pool_asio::ASIOConnection*)::<lambda(const ResponseStatus&)> >::_M_invoke(const std::_Any_data &, const mongo::executor::RemoteCommandResponse &) (__functor=..., __args#0=...) at /usr/local/include/c++/5.4.0/functional:1871
#21 0x00007fdf85aa41ce in operator() (__args#0=..., this=0x7fdf893419d8) at /usr/local/include/c++/5.4.0/functional:2267
#22 mongo::executor::NetworkInterfaceASIO::AsyncOp::finish(mongo::executor::RemoteCommandResponse&&) (this=this@entry=0x7fdf89341880, 
    rs=rs@entry=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0xa966f7c, DIE 0xa9e9f51>) at src/mongo/executor/network_interface_asio_operation.cpp:192
#23 0x00007fdf85a95cf9 in mongo::executor::NetworkInterfaceASIO::_completeOperation (this=this@entry=0x7fdf88b2db80, op=op@entry=0x7fdf89341880, resp=...) at src/mongo/executor/network_interface_asio_command.cpp:315
#24 0x00007fdf85a9f48e in _validateAndRun<mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>::<lambda()> > (
    handler=<optimized out>, ec=..., op=<optimized out>, this=<optimized out>) at src/mongo/executor/network_interface_asio.h:441
#25 operator() (endpoints=..., ec=..., __closure=0x7fdf820a8460) at src/mongo/executor/network_interface_asio_connect.cpp:91
#26 operator() (this=0x7fdf820a8460) at src/third_party/asio-master/asio/include/asio/detail/bind_handler.hpp:163
#27 asio_handler_invoke<asio::detail::binder2<mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> > > (function=...) at src/third_party/asio-master/asio/include/asio/handler_invoke_hook.hpp:68
#28 invoke<asio::detail::binder2<mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> >, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)> > (context=..., 
    function=...) at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#29 asio_handler_invoke<asio::detail::binder2<mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> >, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> > (this_handler=<optimized out>, function=...) at src/third_party/asio-master/asio/include/asio/detail/bind_handler.hpp:206
#30 invoke<asio::detail::binder2<mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> >, asio::detail::binder2<mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> > > (context=..., function=...) at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#31 dispatch<asio::detail::binder2<mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> > > (handler=..., impl=@0x7fdf820a84b8: 0x7fdf88d50500, this=<optimized out>) at src/third_party/asio-master/asio/include/asio/detail/impl/strand_service.hpp:61
#32 dispatch<asio::detail::binder2<mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> > > (handler=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0xa90ccc0, DIE 0xa94c3b5>, this=0x7fdf820a84b0)
    at src/third_party/asio-master/asio/include/asio/io_context_strand.hpp:224
#33 operator()<std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> > (arg2=..., arg1=..., this=0x7fdf820a84b0) at src/third_party/asio-master/asio/include/asio/detail/wrapped_handler.hpp:98
#34 operator() (this=0x7fdf820a84b0) at src/third_party/asio-master/asio/include/asio/detail/bind_handler.hpp:163
#35 operator() (this=0x7fdf820a84a0) at src/third_party/asio-master/asio/include/asio/detail/wrapped_handler.hpp:190
#36 asio_handler_invoke<asio::detail::rewrapped_handler<asio::detail::binder2<asio::detail::wrapped_handler<asio::io_context::strand, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, asio::detail::is_continuation_if_running>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> >, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)> > > (function=...) at src/third_party/asio-master/asio/include/asio/handler_invoke_hook.hpp:68
#37 invoke<asio::detail::rewrapped_handler<asio::detail::binder2<asio::detail::wrapped_handler<asio::io_context::strand, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::erro---Type <return> to continue, or q <return> to quit---
r_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, asio::detail::is_continuation_if_running>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> >, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)> >, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)> > (context=..., function=...) at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#38 asio_handler_invoke<asio::detail::rewrapped_handler<asio::detail::binder2<asio::detail::wrapped_handler<asio::io_context::strand, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, asio::detail::is_continuation_if_running>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> >, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)> >, asio::detail::binder2<asio::detail::wrapped_handler<asio::io_context::strand, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, asio::detail::is_continuation_if_running>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> >, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)> > (this_handler=<optimized out>, 
    function=...) at src/third_party/asio-master/asio/include/asio/detail/wrapped_handler.hpp:274
#39 invoke<asio::detail::rewrapped_handler<asio::detail::binder2<asio::detail::wrapped_handler<asio::io_context::strand, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, asio::detail::is_continuation_if_running>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> >, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)> >, asio::detail::rewrapped_handler<asio::detail::binder2<asio::detail::wrapped_handler<asio::io_context::strand, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, asio::detail::is_continuation_if_running>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> >, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)> > > (context=..., 
    function=...) at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#40 complete<asio::detail::rewrapped_handler<asio::detail::binder2<asio::detail::wrapped_handler<asio::io_context::strand, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, asio::detail::is_continuation_if_running>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> >, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)> > > (this=<synthetic pointer>, handler=..., function=...)
    at src/third_party/asio-master/asio/include/asio/detail/handler_work.hpp:81
#41 asio::detail::completion_handler<asio::detail::rewrapped_handler<asio::detail::binder2<asio::detail::wrapped_handler<asio::io_context::strand, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, asio::detail::is_continuation_if_running>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> >, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)> > >::do_complete(void *, asio::detail::operation *, const asio::error_code &, std::size_t) (
    owner=owner@entry=0x7fdf88c0fe00, base=base@entry=0x7fdf88d1a080) at src/third_party/asio-master/asio/include/asio/detail/completion_handler.hpp:69
#42 0x00007fdf85aa01ff in dispatch<asio::detail::rewrapped_handler<asio::detail::binder2<asio::detail::wrapped_handler<asio::io_context::strand, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, asio::detail::is_continuation_if_running>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> >, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)> > > (handler=..., impl=@0x7fdf820a8768: 0x7fdf88d50500, this=<optimized out>)
    at src/third_party/asio-master/asio/include/asio/detail/impl/strand_service.hpp:87
#43 dispatch<asio::detail::rewrapped_handler<asio::detail::binder2<asio::detail::wrapped_handler<asio::io_context::strand, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, asio::detail::is_continuation_if_running>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> >, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)> > > (handler=<unknown type in /home/yyz/reading-and-annotate-mongodb-3.6.1/mongo/mongos, CU 0xa90ccc0, DIE 0xa94c633>, 
    this=0x7fdf820a8760) at src/third_party/asio-master/asio/include/asio/io_context_strand.hpp:224
#44 asio_handler_invoke<asio::detail::binder2<asio::detail::wrapped_handler<asio::io_context::strand, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, asio::detail::is_continuation_if_running>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> >, asio::io_context::strand, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, asio::detail::is_continuation_if_running> (this_handler=0x7fdf820a8760, function=...)
    at src/third_party/asio-master/asio/include/asio/detail/wrapped_handler.hpp:231
#45 invoke<asio::detail::binder2<asio::detail::wrapped_handler<asio::io_context::strand, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, asio::detail::is_continuation_if_running>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> >, asio::detail::wrapped_handler<asio::io_context::strand, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, asio::detail::is_continuation_if_running> > (context=..., function=...)
    at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#46 complete<asio::detail::binder2<asio::detail::wrapped_handler<asio::io_context::strand, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, asio::detail::is_continuation_if_running>, std::error_code, asio::ip::basic_resolver_results<asio::ip::tcp> > > (this=<synthetic pointer>, handler=..., function=...)
    at src/third_party/asio-master/asio/include/asio/detail/handler_work.hpp:81
#47 asio::detail::resolve_query_op<asio::ip::tcp, asio::detail::wrapped_handler<asio::io_context::strand, mongo::executor::NetworkInterfaceASIO::_connect(mongo::executor::NetworkInterfaceASIO::AsyncOp*)::<lambda(std::error_code, asio::ip::basic_resolver<asio::ip::tcp>::iterator)>, asio::detail::is_continuation_if_running> >::do_complete(void *, asio::detail::operation *, const asio::error_code &, std::size_t) (owner=0x7fdf88c0fe00, base=<optimized out>)
    at src/third_party/asio-master/asio/include/asio/detail/resolve_query_op.hpp:115
#48 0x00007fdf85b25e89 in complete (bytes_transferred=<optimized out>, ec=..., owner=0x7fdf88c0fe00, this=<optimized out>) at src/third_party/asio-master/asio/include/asio/detail/scheduler_operation.hpp:39
#49 asio::detail::scheduler::do_run_one (this=this@entry=0x7fdf88c0fe00, lock=..., this_thread=..., ec=...) at src/third_party/asio-master/asio/include/asio/detail/impl/scheduler.ipp:400
#50 0x00007fdf85b260d1 in asio::detail::scheduler::run (this=0x7fdf88c0fe00, ec=...) at src/third_party/asio-master/asio/include/asio/detail/impl/scheduler.ipp:153
#51 0x00007fdf85b2626e in asio::io_context::run (this=<optimized out>, ec=...) at src/third_party/asio-master/asio/include/asio/impl/io_context.ipp:69
#52 0x00007fdf85a855cc in mongo::executor::NetworkInterfaceASIO::<lambda()>::operator()(void) const (__closure=0x7fdf88c8dee8) at src/mongo/executor/network_interface_asio.cpp:165(NetworkInterfaceASIO::startup(_io_service.run))
#53 0x00007fdf8445b8f0 in std::execute_native_thread_routine (__p=<optimized out>) at ../../../.././libstdc++-v3/src/c++11/thread.cc:84
#54 0x00007fdf83c77e25 in start_thread () from /lib64/libpthread.so.0
*/
//mongos和后端mongod的链接处理在NetworkInterfaceASIO::_connect，mongos转发数据到mongod在NetworkInterfaceASIO::_beginCommunication
//ASIOConnection::setup调用
void NetworkInterfaceASIO::_connect(AsyncOp* op) {
//	2019-03-10T18:19:58.459+0800 I ASIO 	[NetworkInterfaceASIO-TaskExecutorPool-yang-0-0] Connecting to 172.23.240.29:28018

    log() << "Connecting to " << op->request().target.toString();

    tcp::resolver::query query(op->request().target.host(),
                               std::to_string(op->request().target.port()));
    // TODO: Investigate how we might hint or use shortcuts to resolve when possible.
    const auto thenConnect = [this, op](std::error_code ec, tcp::resolver::iterator endpoints) {
        if (endpoints == tcp::resolver::iterator()) {
            // Workaround a bug in ASIO returning an invalid resolver iterator (with a non-error
            // std::error_code) when file descriptors are exhausted.
            ec = make_error_code(ErrorCodes::HostUnreachable);
        }
        _validateAndRun(
            op, ec, [this, op, endpoints]() { _setupSocket(op, std::move(endpoints)); });
    };
    op->resolver().async_resolve(query, op->_strand.wrap(std::move(thenConnect)));
}

void NetworkInterfaceASIO::_setupSocket(AsyncOp* op, tcp::resolver::iterator endpoints) {
    // TODO: Consider moving this call to post-auth so we only assign completed connections.
    {
        auto stream = _streamFactory->makeStream(&op->strand(), op->request().target);
        op->setConnection({std::move(stream), rpc::supports::kOpQueryOnly});
    }

    auto& stream = op->connection().stream();

    stream.connect(std::move(endpoints), [this, op](std::error_code ec) {
        _validateAndRun(op, ec, [this, op]() { _runIsMaster(op); });
    });
}

}  // namespace executor
}  // namespace mongo
