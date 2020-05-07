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

#include <utility>

#include "mongo/base/system_error.h"
#include "mongo/config.h"
#include "mongo/transport/asio_utils.h"
#include "mongo/transport/transport_layer_asio.h"
#include "mongo/util/net/sock.h"
#ifdef MONGO_CONFIG_SSL
#include "mongo/util/net/ssl_manager.h"
#include "mongo/util/net/ssl_types.h"
#endif

#include "asio.hpp"
#ifdef MONGO_CONFIG_SSL
#include "asio/ssl.hpp"
#endif

namespace mongo {
namespace transport {

using GenericSocket = asio::generic::stream_protocol::socket;

//createWithConfig  TransportLayerASIO::_acceptConnection中根据配置构造使用
//记录链接相关的信息，如对端地址等，同时数据调用ASIO相关读写相关也在该类中实现
//调用boost-asio库接口实现数据收发，一个session对应一个链接信息

//ServiceStateMachine._sessionHandle  ASIOTicket._session为该成员类型  
class TransportLayerASIO::ASIOSession : public Session {
    MONGO_DISALLOW_COPYING(ASIOSession);

public:
    //初始化构造 TransportLayerASIO::_acceptConnection调用
    ASIOSession(TransportLayerASIO* tl, GenericSocket socket)
        //fd描述符及TL初始化赋值
        : _socket(std::move(socket)), _tl(tl) {
        std::error_code ec;

        //异步方式设置为非阻塞读
        _socket.non_blocking(_tl->_listenerOptions.transportMode == Mode::kAsynchronous, ec);
        fassert(40490, ec.value() == 0);

        //获取套接字的family
        auto family = endpointToSockAddr(_socket.local_endpoint()).getType();
        //
        if (family == AF_INET || family == AF_INET6) {
            //no_delay keep_alive套接字系统参数设置
            _socket.set_option(asio::ip::tcp::no_delay(true));
            _socket.set_option(asio::socket_base::keep_alive(true));
            //KeepAlive系统参数设置
            setSocketKeepAliveParams(_socket.native_handle());
        }

        //获取本端和对端地址
        _local = endpointToHostAndPort(_socket.local_endpoint());
        _remote = endpointToHostAndPort(_socket.remote_endpoint(ec));
        if (ec) {
            LOG(3) << "Unable to get remote endpoint address: " << ec.message();
        }
    }

    //获取该session对应的tl
    TransportLayer* getTransportLayer() const override {
        return _tl;
    }

    //获取远端地址
    const HostAndPort& remote() const override {
        return _remote;
    }

    //获取本端地址
    const HostAndPort& local() const override {
        return _local;
    }

    //获取链接对应描述符信息
    GenericSocket& getSocket() {
#ifdef MONGO_CONFIG_SSL
        if (_sslSocket) {
            return static_cast<GenericSocket&>(_sslSocket->lowest_layer());
        }
#endif
        return _socket;
    }

    //描述符回收处理
    void shutdown() {
        std::error_code ec;
        getSocket().cancel();
        getSocket().shutdown(GenericSocket::shutdown_both, ec);
        if (ec) {
            error() << "Error closing socket: " << ec.message();
        }
    }

    //ASIOTicket::getSession中调用，标识该描述符是否已注册
    bool isOpen() const {
#ifdef MONGO_CONFIG_SSL
        return _sslSocket ? _sslSocket->lowest_layer().is_open() : _socket.is_open();
#else
        return _socket.is_open();
#endif
    }   
    
    //TransportLayerASIO::ASIOSourceTicket::fillImpl调用
    template <typename MutableBufferSequence, typename CompleteHandler>
    //buffers参数里面携带有buffer的size长度
	//读取整个buffers数据，或者发送完成整个buffers数据后，执行handler回调
    void read(bool sync, const MutableBufferSequence& buffers, CompleteHandler&& handler) {
#ifdef MONGO_CONFIG_SSL
        if (_sslSocket) {
            return opportunisticRead(
                sync, *_sslSocket, buffers, std::forward<CompleteHandler>(handler));
        } else if (!_ranHandshake) {
            invariant(asio::buffer_size(buffers) >= sizeof(MSGHEADER::Value));
            auto postHandshakeCb = [this, sync, buffers, handler](Status status, bool needsRead) {
                if (status.isOK()) {
                    if (needsRead) {
                        read(sync, buffers, handler);
                    } else {
                        std::error_code ec;
                        handler(ec, asio::buffer_size(buffers));
                    }
                } else {
                    handler(std::error_code(status.code(), mongoErrorCategory()), 0);
                }
            };

            auto handshakeRecvCb =
                [ this, postHandshakeCb = std::move(postHandshakeCb), sync, buffers ](
                    const std::error_code& ec, size_t size) {
                _ranHandshake = true;
                if (ec) {
                    postHandshakeCb(errorCodeToStatus(ec), size);
                    return;
                }

                maybeHandshakeSSL(sync, buffers, std::move(postHandshakeCb));
            };

            opportunisticRead(sync, _socket, buffers, std::move(handshakeRecvCb));
        } else {

#endif
            //真正的底层读取操作
            opportunisticRead(sync, _socket, buffers, std::forward<CompleteHandler>(handler));
#ifdef MONGO_CONFIG_SSL
        }
#endif
    }

    template <typename ConstBufferSequence, typename CompleteHandler>
    void write(bool sync, const ConstBufferSequence& buffers, CompleteHandler&& handler) {
#ifdef MONGO_CONFIG_SSL
        if (_sslSocket) {
            opportunisticWrite(sync, *_sslSocket, buffers, std::forward<CompleteHandler>(handler));
        } else {
#endif
            //真正的写入操作
            opportunisticWrite(sync, _socket, buffers, std::forward<CompleteHandler>(handler));
#ifdef MONGO_CONFIG_SSL
        }
#endif
    }

//TransportLayerASIO::ASIOSourceTicket::fillImpl调用
private:
    //从stream对应fd读取数据   read和write指定长度的数据后，执行对应的回调handler
    template <typename Stream, typename MutableBufferSequence, typename CompleteHandler>
    //一次性读size字节如果成功，则直接执行handler，否则没写完的数据通过异步方式继续读，写完后执行对应handler
    void opportunisticRead(bool sync,
                           Stream& stream,
                           const MutableBufferSequence& buffers, //buffers有大小size，实际读最多读size字节
                           CompleteHandler&& handler) {
        std::error_code ec;
        //先直接同步方式从协议栈读取数据，直到读取到数据并且把协议栈数据读完
        auto size = asio::read(stream, buffers, ec); //detail::read_buffer_sequence
        //协议栈内容已经读完了，但是还不够size字节，则继续异步读取
        if ((ec == asio::error::would_block || ec == asio::error::try_again) && !sync) {
            // asio::read is a loop internally, so some of buffers may have been read into already.
            // So we need to adjust the buffers passed into async_read to be offset by size, if
            // size is > 0.
            //buffers有大小size，实际读最多读size字节
            MutableBufferSequence asyncBuffers(buffers);
            if (size > 0) {
                asyncBuffers += size; //buffer offset向后移动
            }
            LOG(0) << "yang test ......... opportunisticRead";
            //数据得读取及handler回调执行见asio库得read_op::operator
            asio::async_read(stream, asyncBuffers, std::forward<CompleteHandler>(handler));
        } else { 
            //直接read获取到size字节数据，则直接执行handler 
            handler(ec, size);
        }
        LOG(0) << "yang test ....2..... opportunisticRead:" << size;
    }

    template <typename Stream, typename ConstBufferSequence, typename CompleteHandler>
    void opportunisticWrite(bool sync,
                            Stream& stream,
                            const ConstBufferSequence& buffers, //buffers有大小size，实际读最多读size字节
                            CompleteHandler&& handler) {
        std::error_code ec;
        //先直接写
        auto size = asio::write(stream, buffers, ec); 
        //一次性写size字节如果成功，则直接执行handler，否则没写完的数据通过异步方式继续写，写完后执行对应handler
        if ((ec == asio::error::would_block || ec == asio::error::try_again) && !sync) {
        //一般当内核协议栈buffer写满后，会返回try_again
            // asio::write is a loop internally, so some of buffers may have been read into already.
            // So we need to adjust the buffers passed into async_write to be offset by size, if
            // size is > 0.
            ConstBufferSequence asyncBuffers(buffers);
            if (size > 0) {
                asyncBuffers += size;
                }
            LOG(0) << "yang test ......... opportunisticWrite";
            //数据得读取及handler回调执行见asio库得write_op::operator
            asio::async_write(stream, asyncBuffers, std::forward<CompleteHandler>(handler));
        } else {
            handler(ec, size);
        }
    }

#ifdef MONGO_CONFIG_SSL
    template <typename MutableBufferSequence, typename HandshakeCb>
    void maybeHandshakeSSL(bool sync, const MutableBufferSequence& buffer, HandshakeCb onComplete) {
        invariant(asio::buffer_size(buffer) >= sizeof(MSGHEADER::Value));
        MSGHEADER::ConstView headerView(asio::buffer_cast<char*>(buffer));
        auto responseTo = headerView.getResponseToMsgId();

        // This logic was taken from the old mongo/util/net/sock.cpp.
        //
        // It lets us run both TLS and unencrypted mongo over the same port.
        //
        // The first message received from the client should have the responseTo field of the wire
        // protocol message needs to be 0 or -1. Otherwise the connection is either sending
        // garbage or a TLS Hello packet which will be caught by the TLS handshake.
        if (responseTo != 0 && responseTo != -1) {
            if (!_tl->_sslContext) {
                return onComplete(
                    {ErrorCodes::SSLHandshakeFailed,
                     "SSL handshake received but server is started without SSL support"},
                    false);
            }

            _sslSocket.emplace(std::move(_socket), *_tl->_sslContext);

            auto handshakeCompleteCb = [ this, onComplete = std::move(onComplete) ](
                const std::error_code& ec, size_t size) {
                auto& sslPeerInfo = SSLPeerInfo::forSession(shared_from_this());

                if (!ec && sslPeerInfo.subjectName.empty()) {
                    auto sslManager = getSSLManager();
                    auto swPeerInfo = sslManager->parseAndValidatePeerCertificate(
                        _sslSocket->native_handle(), "");

                    if (swPeerInfo.isOK()) {
                        // The value of swPeerInfo is a bit complicated:
                        //
                        // If !swPeerInfo.isOK(), then there was an error doing the SSL
                        // handshake and we should reject the connection.
                        //
                        // If !sslPeerInfo.getValue(), then the SSL handshake was successful,
                        // but the peer didn't provide a SSL certificate, and we do not require
                        // one. sslPeerInfo should be empty.
                        //
                        // Otherwise the SSL handshake was successful and the peer did provide
                        // a certificate that is valid, and we should store that info on the
                        // session's SSLPeerInfo decoration.
                        if (swPeerInfo.getValue()) {
                            sslPeerInfo = *swPeerInfo.getValue();
                        }
                    } else {
                        return onComplete(swPeerInfo.getStatus(), false);
                    }
                }

                onComplete(ec ? errorCodeToStatus(ec) : Status::OK(), true);
            };

            if (sync) {
                std::error_code ec;
                _sslSocket->handshake(asio::ssl::stream_base::server, buffer, ec);
                handshakeCompleteCb(ec, asio::buffer_size(buffer));
            } else {
                return _sslSocket->async_handshake(
                    asio::ssl::stream_base::server, buffer, handshakeCompleteCb);
            }
        } else if (_tl->_sslMode() == SSLParams::SSLMode_requireSSL) {
            onComplete({ErrorCodes::SSLHandshakeFailed,
                        "The server is configured to only allow SSL connections"},
                       false);
        } else {
            if (_tl->_sslMode() == SSLParams::SSLMode_preferSSL) {
                LOG(0) << "SSL mode is set to 'preferred' and connection " << id() << " to "
                       << remote() << " is not using SSL.";
            }
            onComplete(Status::OK(), false);
        }
    }
#endif
    //远端地址信息
    HostAndPort _remote;
    //本段地址信息
    HostAndPort _local;
    //赋值见TransportLayerASIO::_acceptConnection
    //也就是fd，链接描述符
    GenericSocket _socket;
#ifdef MONGO_CONFIG_SSL
    boost::optional<asio::ssl::stream<decltype(_socket)>> _sslSocket;
    bool _ranHandshake = false;
#endif

    
    TransportLayerASIO* const _tl;
};

}  // namespace transport
}  // namespace mongo
