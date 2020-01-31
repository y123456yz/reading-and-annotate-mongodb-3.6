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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kNetwork

#include "mongo/platform/basic.h"

#include "mongo/transport/service_entry_point_impl.h"

#include <vector>

#include "mongo/db/auth/restriction_environment.h"
#include "mongo/transport/service_state_machine.h"
#include "mongo/transport/session.h"
#include "mongo/util/log.h"
#include "mongo/util/processinfo.h"
#include "mongo/util/scopeguard.h"

#if !defined(_WIN32)
#include <sys/resource.h>
#endif

namespace mongo {
//调整最大可用文件描述符fd，_initAndListen中调用执行
ServiceEntryPointImpl::ServiceEntryPointImpl(ServiceContext* svcCtx) : _svcCtx(svcCtx) {

    const auto supportedMax = [] {
#ifdef _WIN32
        return serverGlobalParams.maxConns;
#else
        struct rlimit limit;
        verify(getrlimit(RLIMIT_NOFILE, &limit) == 0);

        size_t max = (size_t)(limit.rlim_cur * .8);

        LOG(1) << "fd limit 11 "
               << " hard:" << limit.rlim_max << " soft:" << limit.rlim_cur << " max conn: " << max;

        return std::min(max, serverGlobalParams.maxConns);
#endif
    }();

    // If we asked for more connections than supported, inform the user.
    if (supportedMax < serverGlobalParams.maxConns &&
        serverGlobalParams.maxConns != DEFAULT_MAX_CONN) {
        log() << " --maxConns too high, can only handle " << supportedMax;
    }

    _maxNumConnections = supportedMax;
}

/*
#0  mongo::ServiceEntryPointImpl::startSession (this=0x7f1d47b1c320, session=...) at src/mongo/transport/service_entry_point_impl.cpp:85
#1  0x00007f1d459d9487 in operator() (peerSocket=..., ec=..., __closure=0x7f1d375aa1b0) at src/mongo/transport/transport_layer_asio.cpp:321
#2  operator() (this=0x7f1d375aa1b0) at src/third_party/asio-master/asio/include/asio/detail/bind_handler.hpp:308
#3  asio_handler_invoke<asio::detail::move_binder2<mongo::transport::TransportLayerASIO::_acceptConnection(mongo::transport::TransportLayerASIO::GenericAcceptor&)::<lambda(const std::error_code&, mongo::transport::GenericSocket)>, std::error_code, asio::basic_stream_socket<asio::generic::stream_protocol> > > (function=...) at src/third_party/asio-master/asio/include/asio/handler_invoke_hook.hpp:68
#4  invoke<asio::detail::move_binder2<mongo::transport::TransportLayerASIO::_acceptConnection(mongo::transport::TransportLayerASIO::GenericAcceptor&)::<lambda(const std::error_code&, mongo::transport::GenericSocket)>, std::error_code, asio::basic_stream_socket<asio::generic::stream_protocol> >, mongo::transport::TransportLayerASIO::_acceptConnection(mongo::transport::TransportLayerASIO::GenericAcceptor&)::<lambda(const std::error_code&, mongo::transport::GenericSocket)> > (context=..., function=...) at src/third_party/asio-master/asio/include/asio/detail/handler_invoke_helpers.hpp:37
#5  complete<asio::detail::move_binder2<mongo::transport::TransportLayerASIO::_acceptConnection(mongo::transport::TransportLayerASIO::GenericAcceptor&)::<lambda(const std::error_code&, mongo::transport::GenericSocket)>, std::error_code, asio::basic_stream_socket<asio::generic::stream_protocol> > > (this=<synthetic pointer>, handler=..., function=...) at src/third_party/asio-master/asio/include/asio/detail/handler_work.hpp:81
#6  asio::detail::reactive_socket_move_accept_op<asio::generic::stream_protocol, mongo::transport::TransportLayerASIO::_acceptConnection(mongo::transport::TransportLayerASIO::GenericAcceptor&)::<lambda(const std::error_code&, mongo::transport::GenericSocket)> >::do_complete(void *, asio::detail::operation *, const asio::error_code &, std::size_t) (owner=<optimized out>, base=<optimized out>)
    at src/third_party/asio-master/asio/include/asio/detail/reactive_socket_accept_op.hpp:201
#7  0x00007f1d459e37d9 in complete (bytes_transferred=<optimized out>, ec=..., owner=0x7f1d4790e100, this=<optimized out>) at src/third_party/asio-master/asio/include/asio/detail/scheduler_operation.hpp:39
#8  asio::detail::scheduler::do_run_one (this=this@entry=0x7f1d4790e100, lock=..., this_thread=..., ec=...) at src/third_party/asio-master/asio/include/asio/detail/impl/scheduler.ipp:400
#9  0x00007f1d459e3a21 in asio::detail::scheduler::run (this=0x7f1d4790e100, ec=...) at src/third_party/asio-master/asio/include/asio/detail/impl/scheduler.ipp:153
#10 0x00007f1d459edc6e in asio::io_context::run (this=0x7f1d476f0810) at src/third_party/asio-master/asio/include/asio/impl/io_context.ipp:61
#11 0x00007f1d459d740e in operator() (__closure=0x7f1d4b332c88) at src/mongo/transport/transport_layer_asio.cpp:243
#12 _M_invoke<> (this=0x7f1d4b332c88) at /usr/local/include/c++/5.4.0/functional:1531
#13 operator() (this=0x7f1d4b332c88) at /usr/local/include/c++/5.4.0/functional:1520
#14 std::thread::_Impl<std::_Bind_simple<mongo::transport::TransportLayerASIO::start()::<lambda()>()> >::_M_run(void) (this=0x7f1d4b332c70) at /usr/local/include/c++/5.4.0/thread:115
#15 0x00007f1d431f18f0 in std::execute_native_thread_routine (__p=<optimized out>) at ../../../.././libstdc++-v3/src/c++11/thread.cc:84
#16 0x00007f1d42a0de25 in start_thread () from /lib64/libpthread.so.0
*/ 
//新的链接到来或者关闭都要走到这里  ServiceEntryPointImpl::startSession中listen线程执行
//TransportLayerASIO::_acceptConnection调用，每个新链接都会创建一个新的session
//TransportLayerASIO::_acceptConnection(每个新链接都会创建一个新的session) -> ServiceEntryPointImpl::startSession->ServiceStateMachine::create(每个新链接对应一个ServiceStateMachine结构)

void ServiceEntryPointImpl::startSession(transport::SessionHandle session) { //session对应ASIOSession
    // Setup the restriction environment on the Session, if the Session has local/remote Sockaddrs
    const auto& remoteAddr = session->remote().sockAddr();
    const auto& localAddr = session->local().sockAddr();
    invariant(remoteAddr && localAddr);
    auto restrictionEnvironment =
        stdx::make_unique<RestrictionEnvironment>(*remoteAddr, *localAddr);
	//RestrictionEnvironment::set
    RestrictionEnvironment::set(session, std::move(restrictionEnvironment));

    SSMListIterator ssmIt;

    const bool quiet = serverGlobalParams.quiet.load();
    size_t connectionCount;
/*
"adaptive") : <ServiceExecutorAdaptive>( 
"synchronous"): <ServiceExecutorSynchronous>(ctx));
}
*/ //kAsynchronous  kSynchronous
    auto transportMode = _svcCtx->getServiceExecutor()->transportMode();
	
	//ServiceStateMachine::ServiceStateMachine  这里面设置线程名conn+num
    auto ssm = ServiceStateMachine::create(_svcCtx, session, transportMode);
    {
        stdx::lock_guard<decltype(_sessionsMutex)> lk(_sessionsMutex);
        connectionCount = _sessions.size() + 1; //连接数自增
        if (connectionCount <= _maxNumConnections) {
			//新来的链接对应的session保存到_sessions链表
            ssmIt = _sessions.emplace(_sessions.begin(), ssm);
            _currentConnections.store(connectionCount);
            _createdConnections.addAndFetch(1);
        }
    }

    // Checking if we successfully added a connection above. Separated from the lock so we don't log
    // while holding it.
    if (connectionCount > _maxNumConnections) { //链接超限
        if (!quiet) {
            log() << "connection refused because too many open connections: " << connectionCount;
        }
        return;
    }
 
    if (!quiet) { //建链接打印  
    //I NETWORK  [listener] connection accepted from 1 127.0.0.1:42816 #1 (1 connection now open)
        const auto word = (connectionCount == 1 ? " connection"_sd : " connections"_sd);
        log() << "connection accepted from 1 " << session->remote() << " #" << session->id() << " ("
              << connectionCount << word << " now open)";
    }

    ssm->setCleanupHook([ this, ssmIt, session = std::move(session) ] {
        size_t connectionCount;
        auto remote = session->remote();
        {
            stdx::lock_guard<decltype(_sessionsMutex)> lk(_sessionsMutex);
            _sessions.erase(ssmIt);
            connectionCount = _sessions.size();
            _currentConnections.store(connectionCount);
        }
        _shutdownCondition.notify_one();

		//关链接打印
        const auto word = (connectionCount == 1 ? " connection"_sd : " connections"_sd);
        log() << "end connection 1 " << remote << " (" << connectionCount << word << " now open)";

    });

    auto ownership = ServiceStateMachine::Ownership::kOwned;
	//如果是transport::Mode::kSynchronous一个链接一个线程模式，则整个过程中都是同一个线程处理，所以不需要更改线程名
	//如果是async异步线程池模式，则处理链接的过程中会从conn线程变为worker线程
    if (transportMode == transport::Mode::kSynchronous) {
        ownership = ServiceStateMachine::Ownership::kStatic;
    }
	//ServiceStateMachine::start
    ssm->start(ownership);
}

void ServiceEntryPointImpl::endAllSessions(transport::Session::TagMask tags) {
    // While holding the _sesionsMutex, loop over all the current connections, and if their tags
    // do not match the requested tags to skip, terminate the session.
    {
        stdx::unique_lock<decltype(_sessionsMutex)> lk(_sessionsMutex);
        for (auto& ssm : _sessions) {
            ssm->terminateIfTagsDontMatch(tags);
        }
    }
}

bool ServiceEntryPointImpl::shutdown(Milliseconds timeout) {
    using logger::LogComponent;

    stdx::unique_lock<decltype(_sessionsMutex)> lk(_sessionsMutex);

    // Request that all sessions end, while holding the _sesionsMutex, loop over all the current
    // connections and terminate them
    for (auto& ssm : _sessions) {
        ssm->terminate();
    }

    // Close all sockets and then wait for the number of active connections to reach zero with a
    // condition_variable that notifies in the session cleanup hook. If we haven't closed drained
    // all active operations within the deadline, just keep going with shutdown: the OS will do it
    // for us when the process terminates.
    auto timeSpent = Milliseconds(0);
    const auto checkInterval = std::min(Milliseconds(250), timeout);

    auto noWorkersLeft = [this] { return numOpenSessions() == 0; };
    while (timeSpent < timeout &&
           !_shutdownCondition.wait_for(lk, checkInterval.toSystemDuration(), noWorkersLeft)) {
        log(LogComponent::kNetwork) << "shutdown: still waiting on " << numOpenSessions()
                                    << " active workers to drain... ";
        timeSpent += checkInterval;
    }

    bool result = noWorkersLeft();
    if (result) {
        log(LogComponent::kNetwork) << "shutdown: no running workers found...";
    } else {
        log(LogComponent::kNetwork) << "shutdown: exhausted grace period for" << numOpenSessions()
                                    << " active workers to drain; continuing with shutdown... ";
    }
    return result;
}

ServiceEntryPoint::Stats ServiceEntryPointImpl::sessionStats() const {

    size_t sessionCount = _currentConnections.load();

    ServiceEntryPoint::Stats ret;
    ret.numOpenSessions = sessionCount;
    ret.numCreatedSessions = _createdConnections.load();
    ret.numAvailableSessions = _maxNumConnections - sessionCount;
    return ret;
}

}  // namespace mongo
