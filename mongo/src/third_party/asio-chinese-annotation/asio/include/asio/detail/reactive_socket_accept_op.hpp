//
// detail/reactive_socket_accept_op.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_REACTIVE_SOCKET_ACCEPT_OP_HPP
#define ASIO_DETAIL_REACTIVE_SOCKET_ACCEPT_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"
#include "asio/detail/bind_handler.hpp"
#include "asio/detail/buffer_sequence_adapter.hpp"
#include "asio/detail/fenced_block.hpp"
#include "asio/detail/memory.hpp"
#include "asio/detail/reactor_op.hpp"
#include "asio/detail/socket_holder.hpp"
#include "asio/detail/socket_ops.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {
namespace detail {

//reactive_socket_move_accept_op::reactive_socket_accept_op_base::reactor_op继承关系
template <typename Socket, typename Protocol>
class reactive_socket_accept_op_base : public reactor_op
{
public:
  //reactive_socket_move_accept_op中构造赋值
  reactive_socket_accept_op_base(socket_type socket,
      socket_ops::state_type state, Socket& peer, const Protocol& protocol,
      typename Protocol::endpoint* peer_endpoint, func_type complete_func)
      //accept获取新链接fd，在这里面reactive_socket_accept_op_base::do_perform, 
      //complete_func对应接收到新链接后获取到accept获取到新fd的时候对应的回调，也就是TransportLayerASIO::_acceptConnection中的回调
    : reactor_op(&reactive_socket_accept_op_base::do_perform, complete_func),
      socket_(socket),
      state_(state),
      peer_(peer),
      protocol_(protocol),
      peer_endpoint_(peer_endpoint),
      addrlen_(peer_endpoint ? peer_endpoint->capacity() : 0)
  {
  }

  //reactive_socket_accept_op_base构造函数中赋值给reactor_op
  static status do_perform(reactor_op* base)
  {
    reactive_socket_accept_op_base* o(
        static_cast<reactive_socket_accept_op_base*>(base));

    socket_type new_socket = invalid_socket;
	
    status result = socket_ops::non_blocking_accept(o->socket_,
        o->state_, o->peer_endpoint_ ? o->peer_endpoint_->data() : 0,
        o->peer_endpoint_ ? &o->addrlen_ : 0, o->ec_, new_socket)
    ? done : not_done;

	//新的链接信息赋值给base
    o->new_socket_.reset(new_socket);

    ASIO_HANDLER_REACTOR_OPERATION((*o, "non_blocking_accept", o->ec_));

    return result;
  }

  //在外层继承类中的do_complete中执行
  void do_assign() //do_complete->do_assign->
  {
    if (new_socket_.get() != invalid_socket)
    {
      if (peer_endpoint_)
        peer_endpoint_->resize(addrlen_);
	  //reactive_socket_service_base::do_assign
      peer_.assign(protocol_, new_socket_.get(), ec_);
      if (!ec_)
        new_socket_.release();
    }
  }

private:
  socket_type socket_; //sock套接字sd
  socket_ops::state_type state_;
  //accept获取到的新链接fd，见reactive_socket_accept_op_base::do_perform
  socket_holder new_socket_;
  Socket& peer_; //对应reactive_socket_move_accept_op，见reactive_socket_move_accept_op构造函数
  Protocol protocol_;
  typename Protocol::endpoint* peer_endpoint_;
  std::size_t addrlen_;
};

template <typename Socket, typename Protocol, typename Handler>
class reactive_socket_accept_op :
  public reactive_socket_accept_op_base<Socket, Protocol>
{
public:
  ASIO_DEFINE_HANDLER_PTR(reactive_socket_accept_op);

  reactive_socket_accept_op(socket_type socket,
      socket_ops::state_type state, Socket& peer, const Protocol& protocol,
      typename Protocol::endpoint* peer_endpoint, Handler& handler)
    : reactive_socket_accept_op_base<Socket, Protocol>(socket, state, peer,
        protocol, peer_endpoint, &reactive_socket_accept_op::do_complete),
      handler_(ASIO_MOVE_CAST(Handler)(handler))
  {
    handler_work<Handler>::start(handler_);
  }

  static void do_complete(void* owner, operation* base,
      const asio::error_code& /*ec*/,
      std::size_t /*bytes_transferred*/)
  {
    // Take ownership of the handler object.
    reactive_socket_accept_op* o(static_cast<reactive_socket_accept_op*>(base));
    ptr p = { asio::detail::addressof(o->handler_), o, o };
    handler_work<Handler> w(o->handler_);

    // On success, assign new connection to peer socket object.
    if (owner)
      o->do_assign();

    ASIO_HANDLER_COMPLETION((*o));

    // Make a copy of the handler so that the memory can be deallocated before
    // the upcall is made. Even if we're not about to make an upcall, a
    // sub-object of the handler may be the true owner of the memory associated
    // with the handler. Consequently, a local copy of the handler is required
    // to ensure that any owning sub-object remains valid until after we have
    // deallocated the memory here.
    detail::binder1<Handler, asio::error_code>
      handler(o->handler_, o->ec_);
    p.h = asio::detail::addressof(handler.handler_);
    p.reset();

    // Make the upcall if required.
    if (owner)
    {
      fenced_block b(fenced_block::half);
      ASIO_HANDLER_INVOCATION_BEGIN((handler.arg1_));
      w.complete(handler, handler.handler_);
      ASIO_HANDLER_INVOCATION_END;
    }
  }

private:
  Handler handler_;
};

#if defined(ASIO_HAS_MOVE)

template <typename Protocol, typename Handler>
//reactive_socket_service::async_accept中构造使用 
//reactive_socket_move_accept_op::reactive_socket_accept_op_base::reactor_op继承关系
class reactive_socket_move_accept_op :
  private Protocol::socket,
  public reactive_socket_accept_op_base<typename Protocol::socket, Protocol>
{
public:
  ASIO_DEFINE_HANDLER_PTR(reactive_socket_move_accept_op);

  reactive_socket_move_accept_op(io_context& ioc, socket_type socket,
      socket_ops::state_type state, const Protocol& protocol,
      typename Protocol::endpoint* peer_endpoint, Handler& handler)
    : Protocol::socket(ioc),
      reactive_socket_accept_op_base<typename Protocol::socket, Protocol>(
        socket, state, *this, protocol, peer_endpoint,
        &reactive_socket_move_accept_op::do_complete),
      handler_(ASIO_MOVE_CAST(Handler)(handler))
  {
    handler_work<Handler>::start(handler_);
  }

  static void do_complete(void* owner, operation* base,
      const asio::error_code& /*ec*/,
      std::size_t /*bytes_transferred*/)
  {
    // Take ownership of the handler object.
    reactive_socket_move_accept_op* o(
        static_cast<reactive_socket_move_accept_op*>(base));
    ptr p = { asio::detail::addressof(o->handler_), o, o };
    handler_work<Handler> w(o->handler_);

    // On success, assign new connection to peer socket object.
    if (owner) //reactive_socket_move_accept_op::do_assign
      o->do_assign();

    ASIO_HANDLER_COMPLETION((*o));

    // Make a copy of the handler so that the memory can be deallocated before
    // the upcall is made. Even if we're not about to make an upcall, a
    // sub-object of the handler may be the true owner of the memory associated
    // with the handler. Consequently, a local copy of the handler is required
    // to ensure that any owning sub-object remains valid until after we have
    // deallocated the memory here.
    detail::move_binder2<Handler,
      asio::error_code, typename Protocol::socket>
        handler(0, ASIO_MOVE_CAST(Handler)(o->handler_), o->ec_,
          ASIO_MOVE_CAST(typename Protocol::socket)(*o));
    p.h = asio::detail::addressof(handler.handler_);
    p.reset();

    // Make the upcall if required.
    if (owner)
    {
      fenced_block b(fenced_block::half);
      ASIO_HANDLER_INVOCATION_BEGIN((handler.arg1_, "..."));
      w.complete(handler, handler.handler_);
      ASIO_HANDLER_INVOCATION_END;
    }
  }

private:
  Handler handler_;
};

#endif // defined(ASIO_HAS_MOVE)

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_DETAIL_REACTIVE_SOCKET_ACCEPT_OP_HPP
