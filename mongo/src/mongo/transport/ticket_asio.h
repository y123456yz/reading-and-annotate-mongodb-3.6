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

#include "mongo/transport/transport_layer_asio.h"

#include "asio.hpp"

namespace mongo {
namespace transport {

//TransportLayerASIO类的相关接口使用  
//下面的ASIOSinkTicket和ASIOSourceTicket继承该类,用于控制数据的发送和接收
class TransportLayerASIO::ASIOTicket : public TicketImpl {
    MONGO_DISALLOW_COPYING(ASIOTicket);

public:
    //初始化构造
    explicit ASIOTicket(const ASIOSessionHandle& session, Date_t expiration);
    //获取sessionId
    SessionId sessionId() const final {
        return _sessionId;
    }
    //asio模式没用，针对legacy模型
    Date_t expiration() const final {
        return _expiration;
    }

    /**
     * Run this ticket's work item.
     */
    void fill(bool sync, TicketCallback&& cb);

protected:
    void finishFill(Status status);
    std::shared_ptr<ASIOSession> getSession();
    bool isSync() const;

    // This must be implemented by the Source/Sink subclasses as the actual implementation
    // of filling the ticket.
    virtual void fillImpl() = 0;

private:
    //会话信息
    std::weak_ptr<ASIOSession> _session;
    //每个session有一个唯一id
    const SessionId _sessionId;
    //asio模型没用，针对legacy生效
    const Date_t _expiration;
    //一个完整mongodb协议报文发送或者接收完成后的回调处理
    //cb赋值给fill回调，cb接收数据过程对应ServiceStateMachine::_sourceCallback
	//，cb发送数据过程对应ServiceStateMachine::_sinkCallback
    TicketCallback _fillCallback;
    //同步方式还是异步方式进行数据处理，默认异步
    bool _fillSync;
};

/*
Ticket_asio.h (src\mongo\transport):class TransportLayerASIO::ASIOSourceTicket : public TransportLayerASIO::ASIOTicket {
Ticket_asio.h (src\mongo\transport):class TransportLayerASIO::ASIOSinkTicket : public TransportLayerASIO::ASIOTicket {
*/
//TransportLayerASIO类的相关接口使用   TransportLayerASIO::sourceMessage构造使用
//数据接收的ticket
class TransportLayerASIO::ASIOSourceTicket : public TransportLayerASIO::ASIOTicket {
public:
    //初始化构造
    ASIOSourceTicket(const ASIOSessionHandle& session, Date_t expiration, Message* msg);

protected:
    void fillImpl() final;

private:
    void _headerCallback(const std::error_code& ec, size_t size);
    void _bodyCallback(const std::error_code& ec, size_t size);

    //存储数据的buffer，最后数据获取完毕后，会转存到_target中
    SharedBuffer _buffer;
    //数据赋值见TransportLayerASIO::ASIOSourceTicket::_bodyCallback
    //初始空间赋值见ServiceStateMachine::_sourceMessage->Session::sourceMessage->TransportLayerASIO::sourceMessage
    Message* _target; 
};

//TransportLayerASIO类的相关接口使用
//数据发送的ticket
class TransportLayerASIO::ASIOSinkTicket : public TransportLayerASIO::ASIOTicket {
public:
    //初始化构造
    ASIOSinkTicket(const ASIOSessionHandle& session, Date_t expiration, const Message& msg);

protected:
    void fillImpl() final;

private:
    void _sinkCallback(const std::error_code& ec, size_t size);
    //需要发送的数据message信息
    Message _msgToSend;
};

}  // namespace transport
}  // namespace mongo

