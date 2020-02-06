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

#include "mongo/transport/transport_layer_asio.h"

#include "boost/algorithm/string.hpp"

#include "asio.hpp"

#include "mongo/config.h"
#ifdef MONGO_CONFIG_SSL
#include "asio/ssl.hpp"
#endif

#include "mongo/base/checked_cast.h"
#include "mongo/base/system_error.h"
#include "mongo/db/server_options.h"
#include "mongo/db/service_context.h"
#include "mongo/transport/asio_utils.h"
#include "mongo/transport/service_entry_point.h"
#include "mongo/transport/ticket.h"
#include "mongo/transport/ticket_asio.h"
#include "mongo/util/log.h"
#include "mongo/util/net/hostandport.h"
#include "mongo/util/net/message.h"
#include "mongo/util/net/sock.h"
#include "mongo/util/net/sockaddr.h"
#include "mongo/util/net/ssl_manager.h"
#include "mongo/util/net/ssl_options.h"

// session_asio.h has some header dependencies that require it to be the last header.
#include "mongo/transport/session_asio.h"

namespace mongo {
namespace transport {

//网络模块相关参数
TransportLayerASIO::Options::Options(const ServerGlobalParams* params)
    : port(params->port),
      ipList(params->bind_ip),
#ifndef _WIN32
      useUnixSockets(!params->noUnixSocket),
#endif
      enableIPv6(params->enableIPv6),
      maxConns(params->maxConns) {
}

TransportLayerASIO::TransportLayerASIO(const TransportLayerASIO::Options& opts,
                                       ServiceEntryPoint* sep)
    //boost::asio::io_context用于网络IO事件循环
    //可以参考https://blog.csdn.net/qq_35976351/article/details/90373124
    : _workerIOContext(std::make_shared<asio::io_context>()),
      _acceptorIOContext(stdx::make_unique<asio::io_context>()),
#ifdef MONGO_CONFIG_SSL
      _sslContext(nullptr),
#endif
      _sep(sep),
      _listenerOptions(opts) {
}

TransportLayerASIO::~TransportLayerASIO() = default;

Ticket TransportLayerASIO::sourceMessage(const SessionHandle& session,
                                         Message* message,
                                         Date_t expiration) {
    auto asioSession = checked_pointer_cast<ASIOSession>(session);
    auto ticket = stdx::make_unique<ASIOSourceTicket>(asioSession, expiration, message);
    return {this, std::move(ticket)};
}

Ticket TransportLayerASIO::sinkMessage(const SessionHandle& session,
                                       const Message& message,
                                       Date_t expiration) {
    auto asioSession = checked_pointer_cast<ASIOSession>(session);
    auto ticket = stdx::make_unique<ASIOSinkTicket>(asioSession, expiration, message);
    return {this, std::move(ticket)};
}

//asio中的epoll_wait超时等待处理  ServiceStateMachine::_sourceMessage中会调用
Status TransportLayerASIO::wait(Ticket&& ticket) {
    auto ownedASIOTicket = getOwnedTicketImpl(std::move(ticket));
    auto asioTicket = checked_cast<ASIOTicket*>(ownedASIOTicket.get());

    Status waitStatus = Status::OK();
    asioTicket->fill(true, [&waitStatus](Status result) { waitStatus = result; });

    return waitStatus;
}
//TransportLayerASIO::ASIOTicket::finishFill
void TransportLayerASIO::asyncWait(Ticket&& ticket, TicketCallback callback) {
    auto ownedASIOTicket = std::shared_ptr<TicketImpl>(getOwnedTicketImpl(std::move(ticket)));
    auto asioTicket = checked_cast<ASIOTicket*>(ownedASIOTicket.get());

    asioTicket->fill(
        false,
        [ callback = std::move(callback),
          ownedASIOTicket = std::move(ownedASIOTicket) ](Status status) { callback(status); });
}

// Must not be called while holding the TransportLayerASIO mutex.
void TransportLayerASIO::end(const SessionHandle& session) {
    auto asioSession = checked_pointer_cast<ASIOSession>(session);
    asioSession->shutdown();
}

/*
void TransportLayerASIO::anetSetReuseAddr(int fd) {
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        error() << "setsockopt SO_REUSEADDR failed";
    }
    return;
}*/


//TransportLayerASIO::start  accept处理
//TransportLayerASIO::setup() listen监听
//新链接到来后，在TransportLayerASIO::start中执行

//创建套接字并bind, TransportLayerManager::setup中执行
Status TransportLayerASIO::setup() {
    std::vector<std::string> listenAddrs;
    if (_listenerOptions.ipList.empty()) {
        listenAddrs = {"127.0.0.1"};
        if (_listenerOptions.enableIPv6) {
            listenAddrs.emplace_back("::1");
        }
    } else {
        boost::split(
            listenAddrs, _listenerOptions.ipList, boost::is_any_of(","), boost::token_compress_on);
    }

#ifndef _WIN32
    if (_listenerOptions.useUnixSockets) {
        listenAddrs.emplace_back(makeUnixSockPath(_listenerOptions.port));
    }
#endif
    for (auto& ip : listenAddrs) {
        std::error_code ec;
        if (ip.empty()) {
            warning() << "Skipping empty bind address";
            continue;
        }

		//填充创建套接字时需要的sockaddr_storage结构
        const auto addrs = SockAddr::createAll(
            ip, _listenerOptions.port, _listenerOptions.enableIPv6 ? AF_UNSPEC : AF_INET);
        if (addrs.empty()) {
            warning() << "Found no addresses for " << ip;
            continue;
        }

        for (const auto& addr : addrs) {
            asio::generic::stream_protocol::endpoint endpoint(addr.raw(), addr.addressSize);

#ifndef _WIN32
            if (addr.getType() == AF_UNIX) {
                if (::unlink(ip.c_str()) == -1 && errno != ENOENT) {
                    error() << "Failed to unlink socket file " << ip << " "
                            << errnoWithDescription(errno);
                    fassertFailedNoTrace(40486);
                }
            }
#endif
            if (addr.getType() == AF_INET6 && !_listenerOptions.enableIPv6) {
                error() << "Specified ipv6 bind address, but ipv6 is disabled";
                fassertFailedNoTrace(40488);
            }

			/* boost.asio套接字使用过程
			.acceptor使用方式
				传统使用
				open();bind();listen()
				新的使用
				通过构造函数，传入endpoint，直接完成open(),bind(),listen()
				调用accept()可以接收新的连接

				acceptor用来存储socket相关的信息
			*/
            GenericAcceptor acceptor(*_acceptorIOContext);
            acceptor.open(endpoint.protocol());
			//SO_REUSEADDR配置
            acceptor.set_option(GenericAcceptor::reuse_address(true));

            acceptor.non_blocking(true, ec);
            if (ec) {
                return errorCodeToStatus(ec);
            }

            acceptor.bind(endpoint, ec); //bind绑定
            if (ec) {
                return errorCodeToStatus(ec);
            }

#ifndef _WIN32
            if (addr.getType() == AF_UNIX) {
                if (::chmod(ip.c_str(), serverGlobalParams.unixSocketPermissions) == -1) {
                    error() << "Failed to chmod socket file " << ip << " "
                            << errnoWithDescription(errno);
                    fassertFailedNoTrace(40487);
                }
            }
#endif

			//socket对应得套接字_acceptors相关处理在后续的TransportLayerASIO::start
			////一个acceptors代表bing绑定和监听的地址
            _acceptors.emplace_back(std::make_pair(std::move(addr), std::move(acceptor)));
        }
    }

    if (_acceptors.empty()) {
        return Status(ErrorCodes::SocketException, "No available addresses/ports to bind to");
    }

#ifdef MONGO_CONFIG_SSL
    const auto& sslParams = getSSLGlobalParams();

    if (_sslMode() != SSLParams::SSLMode_disabled) {
        _sslContext = stdx::make_unique<asio::ssl::context>(asio::ssl::context::sslv23);

        const auto sslManager = getSSLManager();
        sslManager
            ->initSSLContext(_sslContext->native_handle(),
                             sslParams,
                             SSLManagerInterface::ConnectionDirection::kOutgoing)
            .transitional_ignore();
    }
#endif

    return Status::OK();
}

//TransportLayerASIO::start  accept处理
//TransportLayerASIO::setup() listen监听
//新链接到来后，在TransportLayerASIO::start中执行


//TransportLayerManager::start中执行   参考boost::ASIO
Status TransportLayerASIO::start() { //listen线程处理
    stdx::lock_guard<stdx::mutex> lk(_mutex);
    _running.store(true);

	
	warning() << "222 yang test  TransportLayerASIO::start";
	//这里专门起一个线程做listen相关的accept事件处理
	//注意套接字的初始化 bind listen操作由initandlisten完成，listen线程只是负责accept事件的循环处理
    _listenerThread = stdx::thread([this] {
        setThreadName("listener"); //新线程为listen线程
        while (_running.load()) {
			
            asio::io_context::work work(*_acceptorIOContext); 
			//_acceptorIOContext和_acceptors是关联的，见TransportLayerASIO::setup
            try {
				warning() << "yang test  TransportLayerASIO::start";
				//异步调度_acceptConnection中的TransportLayerASIO::_acceptConnection->ServiceEntryPointImpl::startSession
                _acceptorIOContext->run(); 
            } catch (...) {
                severe() << "Uncaught exception in the listener: " << exceptionToStatus();
                fassertFailed(40491);
            }
        }
		//db.shutdown的时候，会走到这里
		warning() << "yang test  TransportLayerASIO::start end";
    }); //创建listener线程

	warning() << "111 yang test  TransportLayerASIO::start";
	/*
	现在的默认配置都是该模型:
	mongod为每个连接创建一个线程，创建时做了一定优化，将栈空间设置为1M，减少了线程的内存开销。
	当线程太多时，线程切换的开销也会变大，但因为mongdb后端是持久化的存储，切换开销相比IO的开销还是要小得多。

	如果配置了net  adaptive，则会复用链接
	*/ //一个acceptors代表bing绑定和监听的地址
    for (auto& acceptor : _acceptors) { //bind绑定的时候赋值，见TransportLayerASIO::setup
        acceptor.second.listen(serverGlobalParams.listenBacklog);
        _acceptConnection(acceptor.second);    //异步accept处理在该函数中
    }

    const char* ssl = "";
#ifdef MONGO_CONFIG_SSL
    if (_sslMode() != SSLParams::SSLMode_disabled) {
        ssl = " ssl";
    }
#endif
    log() << "waiting for connections on port " << _listenerOptions.port << ssl;

    return Status::OK();
}

void TransportLayerASIO::shutdown() {
    stdx::lock_guard<stdx::mutex> lk(_mutex);
    _running.store(false);

    // Loop through the acceptors and cancel their calls to async_accept. This will prevent new
    // connections from being opened.
    for (auto& acceptor : _acceptors) {
        acceptor.second.cancel();
        auto& addr = acceptor.first;
        if (addr.getType() == AF_UNIX && !addr.isAnonymousUNIXSocket()) {
            auto path = addr.getAddr();
            log() << "removing socket file: " << path;
            if (::unlink(path.c_str()) != 0) {
                const auto ewd = errnoWithDescription();
                warning() << "Unable to remove UNIX socket " << path << ": " << ewd;
            }
        }
    }

    // If the listener thread is joinable (that is, we created/started a listener thread), then
    // the io_context is owned exclusively by the TransportLayer and we should stop it and join
    // the listener thread.
    //
    // Otherwise the ServiceExecutor may need to continue running the io_context to drain running
    // connections, so we just cancel the acceptors and return.
    if (_listenerThread.joinable()) {
        _acceptorIOContext->stop();
        _listenerThread.join();
    }
}

//TransportLayerManager::createWithConfig
const std::shared_ptr<asio::io_context>& TransportLayerASIO::getIOContext() {
	//网络IO上下文，在TransportLayerManager::createWithConfig中复制给adaptive或者synchronous
    return _workerIOContext; 
}

//TransportLayerASIO::start  这里的acceptor和TransportLayerASIO::start中的_acceptorIOContext是关联的
void TransportLayerASIO::_acceptConnection(GenericAcceptor& acceptor) {
    auto acceptCb = [this, &acceptor](const std::error_code& ec, GenericSocket peerSocket) mutable {
        if (!_running.load())
            return;

        if (ec) {
            log() << "Error accepting new connection on "
                  << endpointToHostAndPort(acceptor.local_endpoint()) << ": " << ec.message();
            _acceptConnection(acceptor);
            return;
        }

		//每个新的链接都会new一个新的ASIOSession
        std::shared_ptr<ASIOSession> session(new ASIOSession(this, std::move(peerSocket)));

		//新的链接处理ServiceEntryPointImpl::startSession
        _sep->startSession(std::move(session));
        _acceptConnection(acceptor); //递归，知道处理完所有的网络accept事件
    };

	//新连接到来，最终的acceptCb是由TransportLayerASIO::start  listen线程来处理
    acceptor.async_accept(*_workerIOContext, std::move(acceptCb)); //异步接收处理，新链接到来listen线程调用acceptCb回调
}

#ifdef MONGO_CONFIG_SSL
SSLParams::SSLModes TransportLayerASIO::_sslMode() const {
    return static_cast<SSLParams::SSLModes>(getSSLGlobalParams().sslMode.load());
}
#endif

}  // namespace transport
}  // namespace mongo
