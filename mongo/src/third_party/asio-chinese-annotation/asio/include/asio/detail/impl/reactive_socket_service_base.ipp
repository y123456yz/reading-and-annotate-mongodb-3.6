//
// detail/reactive_socket_service_base.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_IMPL_REACTIVE_SOCKET_SERVICE_BASE_IPP
#define ASIO_DETAIL_IMPL_REACTIVE_SOCKET_SERVICE_BASE_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"

#if !defined(ASIO_HAS_IOCP) \
  && !defined(ASIO_WINDOWS_RUNTIME)

#include "asio/detail/reactive_socket_service_base.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {
namespace detail {

reactive_socket_service_base::reactive_socket_service_base(
    asio::io_context& io_context)
  : io_context_(io_context),
    reactor_(use_service<reactor>(io_context))
{
  reactor_.init_task();
}

void reactive_socket_service_base::base_shutdown()
{
}

void reactive_socket_service_base::construct(
    reactive_socket_service_base::base_implementation_type& impl)
{
  impl.socket_ = invalid_socket;
  impl.state_ = 0;
}

void reactive_socket_service_base::base_move_construct(
    reactive_socket_service_base::base_implementation_type& impl,
    reactive_socket_service_base::base_implementation_type& other_impl)
{
  impl.socket_ = other_impl.socket_;
  other_impl.socket_ = invalid_socket;

  impl.state_ = other_impl.state_;
  other_impl.state_ = 0;

  reactor_.move_descriptor(impl.socket_,
      impl.reactor_data_, other_impl.reactor_data_);
}

void reactive_socket_service_base::base_move_assign(
    reactive_socket_service_base::base_implementation_type& impl,
    reactive_socket_service_base& other_service,
    reactive_socket_service_base::base_implementation_type& other_impl)
{
  destroy(impl);

  impl.socket_ = other_impl.socket_;
  other_impl.socket_ = invalid_socket;

  impl.state_ = other_impl.state_;
  other_impl.state_ = 0;

  other_service.reactor_.move_descriptor(impl.socket_,
      impl.reactor_data_, other_impl.reactor_data_);
}

void reactive_socket_service_base::destroy(
    reactive_socket_service_base::base_implementation_type& impl)
{
  if (impl.socket_ != invalid_socket)
  {
    ASIO_HANDLER_OPERATION((reactor_.context(),
          "socket", &impl, impl.socket_, "close"));

    reactor_.deregister_descriptor(impl.socket_, impl.reactor_data_,
        (impl.state_ & socket_ops::possible_dup) == 0);

    asio::error_code ignored_ec;
    socket_ops::close(impl.socket_, impl.state_, true, ignored_ec);

    reactor_.cleanup_descriptor_data(impl.reactor_data_);
  }
}

asio::error_code reactive_socket_service_base::close(
    reactive_socket_service_base::base_implementation_type& impl,
    asio::error_code& ec)
{
  if (is_open(impl))
  {
    ASIO_HANDLER_OPERATION((reactor_.context(),
          "socket", &impl, impl.socket_, "close"));

    reactor_.deregister_descriptor(impl.socket_, impl.reactor_data_,
        (impl.state_ & socket_ops::possible_dup) == 0);

    socket_ops::close(impl.socket_, impl.state_, false, ec);

    reactor_.cleanup_descriptor_data(impl.reactor_data_);
  }
  else
  {
    ec = asio::error_code();
  }

  // The descriptor is closed by the OS even if close() returns an error.
  //
  // (Actually, POSIX says the state of the descriptor is unspecified. On
  // Linux the descriptor is apparently closed anyway; e.g. see
  //   http://lkml.org/lkml/2005/9/10/129
  // We'll just have to assume that other OSes follow the same behaviour. The
  // known exception is when Windows's closesocket() function fails with
  // WSAEWOULDBLOCK, but this case is handled inside socket_ops::close().
  construct(impl);

  return ec;
}

socket_type reactive_socket_service_base::release(
    reactive_socket_service_base::base_implementation_type& impl,
    asio::error_code& ec)
{
  if (!is_open(impl))
  {
    ec = asio::error::bad_descriptor;
    return invalid_socket;
  }

  ASIO_HANDLER_OPERATION((reactor_.context(),
        "socket", &impl, impl.socket_, "release"));

  reactor_.deregister_descriptor(impl.socket_, impl.reactor_data_, false);
  reactor_.cleanup_descriptor_data(impl.reactor_data_);
  socket_type sock = impl.socket_;
  construct(impl);
  ec = asio::error_code();
  return sock;
}

//AsyncTimerASIO::cancel->basic_waitable_timer::cancel->waitable_timer_service::cancel->epoll_reactor::cancel_ops
asio::error_code reactive_socket_service_base::cancel(
    reactive_socket_service_base::base_implementation_type& impl,
    asio::error_code& ec)
{
  if (!is_open(impl))
  {
    ec = asio::error::bad_descriptor;
    return ec;
  }

  ASIO_HANDLER_OPERATION((reactor_.context(),
        "socket", &impl, impl.socket_, "cancel"));

  //epoll_reactor::cancel_ops
  reactor_.cancel_ops(impl.socket_, impl.reactor_data_);
  ec = asio::error_code();
  return ec;
}

//mongodb中的TransportLayerASIO::setup->basic_socket_acceptor::open->reactive_socket_service::open
asio::error_code reactive_socket_service_base::do_open(
    reactive_socket_service_base::base_implementation_type& impl,
    int af, int type, int protocol, asio::error_code& ec)
{
  if (is_open(impl))
  {
    ec = asio::error::already_open;
    return ec;
  }

  socket_holder sock(socket_ops::socket(af, type, protocol, ec));
  if (sock.get() == invalid_socket)
    return ec;

  //epoll_reactor::register_descriptor  epoll注册，也就是fd和epoll关联
  if (int err = reactor_.register_descriptor(sock.get(), impl.reactor_data_))
  {
    ec = asio::error_code(err,
        asio::error::get_system_category());
    return ec;
  }

  impl.socket_ = sock.release();
  switch (type)
  {
  case SOCK_STREAM: impl.state_ = socket_ops::stream_oriented; break;
  case SOCK_DGRAM: impl.state_ = socket_ops::datagram_oriented; break;
  default: impl.state_ = 0; break;
  }
  ec = asio::error_code();
  return ec;
}

////reactive_socket_move_accept_op::do_complete->reactive_socket_accept_op_base::do_assign
//有新链接的时候会调用对应assign把新链接fd注册到epoll事件集
asio::error_code reactive_socket_service_base::do_assign(
    reactive_socket_service_base::base_implementation_type& impl, int type,
    const reactive_socket_service_base::native_handle_type& native_socket,
    asio::error_code& ec)
{
  if (is_open(impl))
  {
    ec = asio::error::already_open;
    return ec;
  }

  if (int err = reactor_.register_descriptor(
        native_socket, impl.reactor_data_))
  {
    ec = asio::error_code(err,
        asio::error::get_system_category());
    return ec;
  }

  impl.socket_ = native_socket;
  switch (type)
  {
  case SOCK_STREAM: impl.state_ = socket_ops::stream_oriented; break;
  case SOCK_DGRAM: impl.state_ = socket_ops::datagram_oriented; break;
  default: impl.state_ = 0; break;
  }
  impl.state_ |= socket_ops::possible_dup;
  ec = asio::error_code();
  return ec;
}

//reactive_socket_service_base::start_accept_op	
//mongodb accept异步接收链接流程: 
//TransportLayerASIO::_acceptConnection->basic_socket_acceptor::async_accept->reactive_socket_service::async_accept(这里构造reactive_socket_accept_op_base，后续得epoll获取新链接得handler回调也在这里得do_complete中执行)
//->reactive_socket_service_base::start_accept_op->reactive_socket_service_base::start_op中进行accept注册

//mongodb异步读取流程:
 //mongodb通过TransportLayerASIO::ASIOSession::opportunisticRead->asio::async_read->start_read_buffer_sequence_op->read_op::operator
 //->basic_stream_socket::async_read_some->reactive_socket_service_base::async_receive(这里构造reactive_socket_recv_op，后续得epoll读数据及其读取到一个完整mongo报文得handler回调也在这里得do_complete中执行)
 //->reactive_socket_service_base::start_op中进行EPOLL事件注册
//mongodb同步读取流程:
 //mongodb中opportunisticRead->asio:read->basic_stream_socket::read_some->basic_stream_socket::read_some
 //reactive_socket_service_base::receive->socket_ops::sync_recv(这里直接读取数据)

//write发送异步数据流程: 
 //mongodb中通过opportunisticWrite->asio::async_write->start_write_buffer_sequence_op->detail::write_op() 
 //->basic_stream_socket::async_write_some->reactive_socket_service_base::async_send(这里构造reactive_socket_send_op_base，后续得epoll写数据及其读取到一个完整mongo报文得handler回调也在这里得do_complete中执行)
 //->reactive_socket_service_base::start_op->reactive_socket_service_base::start_op中进行EPOLL事件注册
//write同步发送数据流程:
 //同步写流程asio::write->write_buffer_sequence->basic_stream_socket::write_some->reactive_socket_service_base::send->socket_ops::sync_send(这里是真正得同步发送)


void reactive_socket_service_base::start_op(
    reactive_socket_service_base::base_implementation_type& impl, //impl对应stream_protocol
    int op_type, reactor_op* op, bool is_continuation,
    bool is_non_blocking, bool noop)
{
  if (!noop)
  {
    if ((impl.state_ & socket_ops::non_blocking)
        || socket_ops::set_internal_non_blocking( //套接字非阻塞设置
          impl.socket_, impl.state_, true, op->ec_))
    {
      //epoll_reactor::start_op
      reactor_.start_op(op_type, impl.socket_,
          impl.reactor_data_, op, is_continuation, is_non_blocking);
      return;
    }
  }
  //epoll_reactor::post_immediate_completion
  reactor_.post_immediate_completion(op, is_continuation);
}

//mongodb accept接收链接流程TransportLayerASIO::_acceptConnection->basic_socket_acceptor::async_accept->reactive_socket_service::async_accept->start_accept_op
void reactive_socket_service_base::start_accept_op(
    reactive_socket_service_base::base_implementation_type& impl, //impl对应stream_protocol
    reactor_op* op, bool is_continuation, bool peer_is_open) //peer_is_open为false
{
  if (!peer_is_open) //走该分支
    				//epoll_reactor::read_op
    start_op(impl, reactor::read_op, op, is_continuation, true, false);
  else
  {
    op->ec_ = asio::error::already_open;
	
	//mongodb accept接收链接流程TransportLayerASIO::_acceptConnection->basic_socket_acceptor::async_accept->reactive_socket_service::async_accept
	//->start_accept_op->epoll_reactor::post_immediate_completion
    reactor_.post_immediate_completion(op, is_continuation);//epoll_reactor::post_immediate_completion
  }
}

//针对客户端发起链接
void reactive_socket_service_base::start_connect_op(
    reactive_socket_service_base::base_implementation_type& impl,
    reactor_op* op, bool is_continuation,
    const socket_addr_type* addr, size_t addrlen)
{
  if ((impl.state_ & socket_ops::non_blocking)
      || socket_ops::set_internal_non_blocking(
        impl.socket_, impl.state_, true, op->ec_))
  {
    if (socket_ops::connect(impl.socket_, addr, addrlen, op->ec_) != 0)
    {
      if (op->ec_ == asio::error::in_progress
          || op->ec_ == asio::error::would_block)
      {
        op->ec_ = asio::error_code();
		//epoll_reactor::read_op
        reactor_.start_op(reactor::connect_op, impl.socket_,
            impl.reactor_data_, op, is_continuation, false);
        return;
      }
    }
  }

  reactor_.post_immediate_completion(op, is_continuation);
}

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // !defined(ASIO_HAS_IOCP)
       //   && !defined(ASIO_WINDOWS_RUNTIME)

#endif // ASIO_DETAIL_IMPL_REACTIVE_SOCKET_SERVICE_BASE_IPP
