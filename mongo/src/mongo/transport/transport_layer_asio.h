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

#include <functional>
#include <string>

#include "mongo/config.h"
#include "mongo/db/server_options.h"
#include "mongo/stdx/condition_variable.h"
#include "mongo/stdx/memory.h"
#include "mongo/stdx/mutex.h"
#include "mongo/stdx/thread.h"
#include "mongo/transport/ticket_impl.h"
#include "mongo/transport/transport_layer.h"
#include "mongo/transport/transport_mode.h"
#include "mongo/util/net/hostandport.h"
#include "mongo/util/net/ssl_options.h"
#include "mongo/util/net/ssl_types.h"

//mongodb使用的asio异步网络库的部分核心类型
namespace asio {
class io_context;

template <typename Protocol>
class basic_socket_acceptor;

namespace generic {
class stream_protocol;
}  // namespace generic

namespace ssl {
class context;
}  // namespace ssl
}  // namespace asio

namespace mongo {

class ServiceContext;
class ServiceEntryPoint;

namespace transport {

/**
 * A TransportLayer implementation based on ASIO networking primitives.
 */
//TransportLayerASIO继承TransportLayer，对应Boost.Asio网络框架,和TransportLayerLegacy一起，选择是asio还是legacy配置，参考createWithConfig
class TransportLayerASIO final : public TransportLayer {
    MONGO_DISALLOW_COPYING(TransportLayerASIO);

public:
    struct Options {  //TransportLayerASIO::Options保持asio传输层相关参数信息
        explicit Options(const ServerGlobalParams* params);
        //默认监听端口
        int port = ServerGlobalParams::DefaultDBPort;  // port to bind to
        //ip配置列表，例如bindIp: 127.0.0.1,30.25.x.17，可以绑定多个IP
        std::string ipList;                            // addresses to bind to
#ifndef _WIN32
        bool useUnixSockets = true;  // whether to allow UNIX sockets in ipList
#endif
        bool enableIPv6 = false;                  // whether to allow IPv6 sockets in ipList
        //同步还是异步，赋值见createWithConfig
        Mode transportMode = Mode::kSynchronous;  // whether accepted sockets should be put into
        //默认最大链接数限制，net.maxIncomingConnections配置                                         // non-blocking mode after they're accepted
        size_t maxConns = DEFAULT_MAX_CONN;       // maximum number of active connections
    };

    TransportLayerASIO(const Options& opts, ServiceEntryPoint* sep);

    virtual ~TransportLayerASIO();

    Ticket sourceMessage(const SessionHandle& session,
                         Message* message,
                         Date_t expiration = Ticket::kNoExpirationDate) final;

    Ticket sinkMessage(const SessionHandle& session,
                       const Message& message,
                       Date_t expiration = Ticket::kNoExpirationDate) final;

    Status wait(Ticket&& ticket) final;

    void asyncWait(Ticket&& ticket, TicketCallback callback) final;

    void end(const SessionHandle& session) final;

    Status setup() final;
    Status start() final;

    void shutdown() final;

    const std::shared_ptr<asio::io_context>& getIOContext();

private:
    class ASIOSession;
    class ASIOTicket;
    class ASIOSourceTicket;
    class ASIOSinkTicket;

    using ASIOSessionHandle = std::shared_ptr<ASIOSession>;
    using ConstASIOSessionHandle = std::shared_ptr<const ASIOSession>;
    using GenericAcceptor = asio::basic_socket_acceptor<asio::generic::stream_protocol>;
    
    void _acceptConnection(GenericAcceptor& acceptor);
#ifdef MONGO_CONFIG_SSL
    SSLParams::SSLModes _sslMode() const;
#endif

    stdx::mutex _mutex;

    // There are two IO contexts that are used by TransportLayerASIO. The _workerIOContext
    // contains all the accepted sockets and all normal networking activity. The
    // _acceptorIOContext contains all the sockets in _acceptors.
    //
    // TransportLayerASIO should never call run() on the _workerIOContext.
    // In synchronous mode, this will cause a massive performance degradation due to
    // unnecessary wakeups on the asio thread for sockets we don't intend to interact
    // with asynchronously. The additional IO context avoids registering those sockets
    // with the acceptors epoll set, thus avoiding those wakeups.  Calling run will
    // undo that benefit.
    //
    // TransportLayerASIO should run its own thread that calls run() on the _acceptorIOContext
    // to process calls to async_accept - this is the equivalent of the "listener" thread in
    // other TransportLayers.
    //
    // The underlying problem that caused this is here:
    // https://github.com/chriskohlhoff/asio/issues/240
    //
    // It is important that the io_context be declared before the
    // vector of acceptors (or any other state that is associated with
    // the io_context), so that we destroy any existing acceptors or
    // other io_service associated state before we drop the refcount
    // on the io_context, which may destroy it.

    
    /*
    //accept对应的状态机任务调度流程
    //TransportLayerASIO::_acceptConnection->basic_socket_acceptor::async_accept
    //->start_accept_op->epoll_reactor::post_immediate_completion
    
    //普通read write对应的状态机任务入队流程
    //mongodb的ServiceExecutorAdaptive::schedule调用->io_context::post(ASIO_MOVE_ARG(CompletionHandler) handler)
    //->scheduler::post_immediate_completion
    //mongodb的ServiceExecutorAdaptive::schedule调用->io_context::dispatch(ASIO_MOVE_ARG(CompletionHandler) handler)
    //->scheduler::do_dispatch
    
    //普通读写read write对应的状态机任务出队流程
    //ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_for->scheduler::wait_one
    //->scheduler::do_wait_one调用
    //mongodb中ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_one_for
    //->io_context::run_one_until->schedule::wait_one
            |
            |1.先进行状态机任务调度(也就是mongodb中TransportLayerASIO._workerIOContext  TransportLayerASIO._acceptorIOContext相关的任务)
            |2.在执行步骤1对应调度任务过程中最终调用TransportLayerASIO::_acceptConnection、TransportLayerASIO::ASIOSourceTicket::fillImpl和
            |  TransportLayerASIO::ASIOSinkTicket::fillImpl进行新连接处理、数据读写事件epoll注册(下面箭头部分)
            |
            \|/
    //accept对应的新链接epoll事件注册流程:reactive_socket_service_base::start_accept_op->reactive_socket_service_base::start_op
    //读数据epoll事件注册流程:reactive_descriptor_service::async_read_some->reactive_descriptor_service::start_op->epoll_reactor::start_op
    //写数据epoll事件注册流程:reactive_descriptor_service::async_write_some->reactive_descriptor_service::start_op->epoll_reactor::start_op
    */
    
    //boost::asio::io_context用于网络IO事件循环
    //_acceptorIOContext针对socket()对应的fd1,也就是处理accept事件，accept事件到来会创建一个新的链接fd2
    //_workerIOContext处理后续的新链接fd2上的所有读写事件，fd2网络IO数据收发真正生效见ServiceExecutorAdaptive::_workerThreadRoutine
    
    //TransportLayerASIO::TransportLayerASIO中构造  
    //网络worker IO fd2上下文，在TransportLayerManager::createWithConfig中被赋值给ServiceExecutorAdaptive._ioContext   
    //fd2数据收发见ServiceExecutorAdaptive::schedule, ServiceExecutorSynchronous线程模式不需要_workerIOContext，因为一个线程和一个session对应，而ServiceExecutorAdaptive模式是多个线程复用网络IO，所以需要
    std::shared_ptr<asio::io_context> _workerIOContext; 

    // 真正生效接收新的链接见TransportLayerASIO::start    
    //_acceptorIOContext和_acceptors关联，见TransportLayerASIO::setup 
    std::unique_ptr<asio::io_context> _acceptorIOContext;  
    

#ifdef MONGO_CONFIG_SSL

    std::unique_ptr<asio::ssl::context> _sslContext;
#endif
    //赋值见TransportLayerASIO::setup，创建套接字，然后bind  一台服务器可以bind多个IP地址，所以是vector
    //_acceptorIOContext和_acceptors关联，见TransportLayerASIO::setup
    std::vector<std::pair<SockAddr, GenericAcceptor>> _acceptors;

    // Only used if _listenerOptions.async is false.
    //listener线程，专门负责accept处理
    stdx::thread _listenerThread;
	///服务入口，mongod和mongos有不同的入口点
	//赋值见TransportLayerManager::createWithConfig中构造使用,
	//新的链接处理ServiceEntryPointImpl::startSession也是通过该成员关联
    ServiceEntryPoint* const _sep = nullptr;
	//运行状态标识
    AtomicWord<bool> _running{false};

    //生效使用见TransportLayerASIO::setup，配置来验ServerGlobalParams
    //赋值见TransportLayerManager::createWithConfig中构造使用
    Options _listenerOptions;

};

}  // namespace transport
}  // namespace mongo
