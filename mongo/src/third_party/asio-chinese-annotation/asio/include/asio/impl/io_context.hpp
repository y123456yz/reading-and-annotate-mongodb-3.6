//
// impl/io_context.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_IMPL_IO_CONTEXT_HPP
#define ASIO_IMPL_IO_CONTEXT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/completion_handler.hpp"
#include "asio/detail/executor_op.hpp"
#include "asio/detail/fenced_block.hpp"
#include "asio/detail/handler_type_requirements.hpp"
#include "asio/detail/recycling_allocator.hpp"
#include "asio/detail/service_registry.hpp"
#include "asio/detail/throw_error.hpp"
#include "asio/detail/type_traits.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {

template <typename Service>
inline Service& use_service(io_context& ioc)
{
  // Check that Service meets the necessary type requirements.
  (void)static_cast<execution_context::service*>(static_cast<Service*>(0));
  (void)static_cast<const execution_context::id*>(&Service::id);

  return ioc.service_registry_->template use_service<Service>(ioc);
}

template <>
inline detail::io_context_impl& use_service<detail::io_context_impl>(
    io_context& ioc)
{
  return ioc.impl_;
}

} // namespace asio

#include "asio/detail/pop_options.hpp"

#if defined(ASIO_HAS_IOCP)
# include "asio/detail/win_iocp_io_context.hpp"
#else
# include "asio/detail/scheduler.hpp"
#endif

#include "asio/detail/push_options.hpp"

namespace asio {

inline io_context::executor_type
io_context::get_executor() ASIO_NOEXCEPT
{
  return executor_type(*this);
}

#if defined(ASIO_HAS_CHRONO)

//mongo::transport::ServiceExecutorAdaptive::_workerThreadRoutine调用走到这里
template <typename Rep, typename Period>
std::size_t io_context::run_for(
    const chrono::duration<Rep, Period>& rel_time)
{
  return this->run_until(chrono::steady_clock::now() + rel_time);
}

template <typename Clock, typename Duration>
//mongo::transport::ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_for
std::size_t io_context::run_until(
    const chrono::time_point<Clock, Duration>& abs_time)
{
  std::size_t n = 0;
  while (this->run_one_until(abs_time))
    if (n != (std::numeric_limits<std::size_t>::max)())
      ++n;
  return n;
}

template <typename Rep, typename Period>
//mongodb中ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_one_for->
std::size_t io_context::run_one_for(
    const chrono::duration<Rep, Period>& rel_time)
{
  return this->run_one_until(chrono::steady_clock::now() + rel_time);
}

template <typename Clock, typename Duration>
//mongo::transport::ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_for->io_context::run_one_until

//mongodb中ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_one_for->io_context::run_one_until
//->schedule::wait_one
//调度运行schedule::wait_one的时间最长持续到abs_time这个时间点
std::size_t io_context::run_one_until(
    const chrono::time_point<Clock, Duration>& abs_time) //abs_time表示运行该函数到期的绝对事件点
{
  typename Clock::time_point now = Clock::now();
  while (now < abs_time)
  {
    //在本循环中还需要运行多久
    typename Clock::duration rel_time = abs_time - now;
	//最多一次循环执行1s，也就是schedule::wait_one最多单次wait 1s
    if (rel_time > chrono::seconds(1))
      rel_time = chrono::seconds(1);

    asio::error_code ec;
	//schedule::wait_one 从操作队列中获取对应的operation执行，如果获取到operation并执行成功则返回1，否则返回0
    std::size_t s = impl_.wait_one(
        static_cast<long>(chrono::duration_cast<
          chrono::microseconds>(rel_time).count()), ec);
    asio::detail::throw_error(ec);

	//已经获取执行完一个operation，则直接返回，否则继续循环调度schedule::wait_one获取operation执行
    if (s || impl_.stopped())
      return s;

	//重新计算当前时间
    now = Clock::now();
  }

  return 0;
}

#endif // defined(ASIO_HAS_CHRONO)

#if !defined(ASIO_NO_DEPRECATED)

inline void io_context::reset()
{
  restart();
}

//mongodb的ServiceExecutorAdaptive::schedule调用->io_context::dispatch(ASIO_MOVE_ARG(CompletionHandler) handler)
template <typename CompletionHandler>
ASIO_INITFN_RESULT_TYPE(CompletionHandler, void ())
io_context::dispatch(ASIO_MOVE_ARG(CompletionHandler) handler)
{
  // If you get an error on the following line it means that your handler does
  // not meet the documented type requirements for a CompletionHandler.
  ASIO_COMPLETION_HANDLER_CHECK(CompletionHandler, handler) type_check;

  async_completion<CompletionHandler, void ()> init(handler);

  //scheduler::can_dispatch  本线程已经加入到了工作线程队列，直接执行handler
  if (impl_.can_dispatch()) //本线程继续执行该handler
  {
    detail::fenced_block b(detail::fenced_block::full);
	//直接运行handler
    asio_handler_invoke_helpers::invoke(
        init.completion_handler, init.completion_handler);
  }
  else
  {
    // Allocate and construct an operation to wrap the handler.
    //构造complete handler,completion_handler继承基础operation 
    typedef detail::completion_handler<
      typename handler_type<CompletionHandler, void ()>::type> op;
    typename op::ptr p = { detail::addressof(init.completion_handler),
      op::ptr::allocate(init.completion_handler), 0 };
    p.p = new (p.v) op(init.completion_handler); //获取op操作，也就是handler回调

    ASIO_HANDLER_CREATION((*this, *p.p,
          "io_context", this, 0, "dispatch"));

	
	//mongodb的ServiceExecutorAdaptive::schedule调用->io_context::dispatch(ASIO_MOVE_ARG(CompletionHandler) handler)
	//->scheduler::do_dispatch
    impl_.do_dispatch(p.p); //scheduler::do_dispatch op入队
    p.v = p.p = 0;
  }

  return init.result.get();
}

template <typename CompletionHandler>
ASIO_INITFN_RESULT_TYPE(CompletionHandler, void ())
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
		|1.先进行状态机任务调度(也就是mongodb中TransportLayerASIO._workerIOContext	TransportLayerASIO._acceptorIOContext相关的任务)
		|2.在执行步骤1对应调度任务过程中最终调用TransportLayerASIO::_acceptConnection、TransportLayerASIO::ASIOSourceTicket::fillImpl和
		|  TransportLayerASIO::ASIOSinkTicket::fillImpl进行新连接处理、数据读写事件epoll注册(下面箭头部分)及其对应回调
		|
		\|/
//accept对应的新链接epoll事件注册流程:reactive_socket_service_base::start_accept_op->reactive_socket_service_base::start_op
//读数据epoll事件注册流程:reactive_descriptor_service::async_read_some->reactive_descriptor_service::start_op->epoll_reactor::start_op
//写数据epoll事件注册流程:reactive_descriptor_service::async_write_some->reactive_descriptor_service::start_op->epoll_reactor::start_op
*/

//reactive_socket_service_base::start_accept_op	
//mongodb accept异步接收链接流程: 
//TransportLayerASIO::_acceptConnection->basic_socket_acceptor::async_accept->reactive_socket_service::async_accept(这里构造reactive_socket_accept_op_base，后续得epoll获取新链接得handler回调也在这里得do_complete中执行)
//->reactive_socket_service_base::start_accept_op->reactive_socket_service_base::start_op中进行accept注册

//mongodb异步读取流程:
 //mongodb通过TransportLayerASIO::ASIOSession::opportunisticRead->asio::async_read->start_read_buffer_sequence_op->read_op::operator
 //->basic_stream_socket::async_read_some->reactive_socket_service_base::async_receive(这里构造reactive_socket_recv_op，后续得epoll读数据及其读取到一个完整mongo报文得handler回调也在这里得do_complete中执行)
 //->reactive_socket_service_base::start_op中进行EPOLL事件注册
//mongodb同步读取流程:
 //mongodb中opportunisticRead->asio:read->detail::read_buffer_sequence->basic_stream_socket::read_some->basic_stream_socket::read_some
 //reactive_socket_service_base::receive->socket_ops::sync_recv(这里直接读取数据)

//write发送异步数据流程: 
 //mongodb中通过opportunisticWrite->asio::async_write->start_write_buffer_sequence_op->detail::write_op() 
 //->basic_stream_socket::async_write_some->reactive_socket_service_base::async_send(这里构造reactive_socket_send_op_base，后续得epoll写数据及其读取到一个完整mongo报文得handler回调也在这里得do_complete中执行)
 //->reactive_socket_service_base::start_op->reactive_socket_service_base::start_op中进行EPOLL事件注册
//write同步发送数据流程:
 //同步写流程asio::write->write_buffer_sequence->basic_stream_socket::write_some->reactive_socket_service_base::send->socket_ops::sync_send(这里是真正得同步发送)



//mongodb的ServiceExecutorAdaptive::schedule调用->io_context::post(ASIO_MOVE_ARG(CompletionHandler) handler)
//->scheduler::post_immediate_completion
io_context::post(ASIO_MOVE_ARG(CompletionHandler) handler)
{
  // If you get an error on the following line it means that your handler does
  // not meet the documented type requirements for a CompletionHandler.
  ASIO_COMPLETION_HANDLER_CHECK(CompletionHandler, handler) type_check;

  async_completion<CompletionHandler, void ()> init(handler);

  bool is_continuation =   //mongodb  asio::async_read(), asio::async_write(), asio::async_connect(), 默认返回true
    asio_handler_cont_helpers::is_continuation(init.completion_handler);

  // Allocate and construct an operation to wrap the handler.
  //构造complete handler,completion_handler继承基础operation 
  typedef detail::completion_handler<
    typename handler_type<CompletionHandler, void ()>::type> op;
  typename op::ptr p = { detail::addressof(init.completion_handler),
      op::ptr::allocate(init.completion_handler), 0 };
  p.p = new (p.v) op(init.completion_handler);

  ASIO_HANDLER_CREATION((*this, *p.p,
        "io_context", this, 0, "post"));

  //scheduler::post_immediate_completion
  impl_.post_immediate_completion(p.p, is_continuation);
  p.v = p.p = 0;

  return init.result.get();
}

template <typename Handler>
#if defined(GENERATING_DOCUMENTATION)
unspecified
#else
inline detail::wrapped_handler<io_context&, Handler>
#endif
io_context::wrap(Handler handler)
{
  return detail::wrapped_handler<io_context&, Handler>(*this, handler);
}

#endif // !defined(ASIO_NO_DEPRECATED)

inline io_context&
io_context::executor_type::context() const ASIO_NOEXCEPT
{
  return io_context_;
}

inline void
io_context::executor_type::on_work_started() const ASIO_NOEXCEPT
{
  io_context_.impl_.work_started();//scheduler::work_started
}

inline void
io_context::executor_type::on_work_finished() const ASIO_NOEXCEPT
{
  io_context_.impl_.work_finished(); //scheduler::work_finished
}

template <typename Function, typename Allocator>
void io_context::executor_type::dispatch(
    ASIO_MOVE_ARG(Function) f, const Allocator& a) const
{
  typedef typename decay<Function>::type function_type;

  // Invoke immediately if we are already inside the thread pool.
  if (io_context_.impl_.can_dispatch())
  {
    // Make a local, non-const copy of the function.
    function_type tmp(ASIO_MOVE_CAST(Function)(f));

    detail::fenced_block b(detail::fenced_block::full);
    asio_handler_invoke_helpers::invoke(tmp, tmp);
    return;
  }

  // Allocate and construct an operation to wrap the function.
  typedef detail::executor_op<function_type, Allocator, detail::operation> op;
  typename op::ptr p = { detail::addressof(a), op::ptr::allocate(a), 0 };
  p.p = new (p.v) op(ASIO_MOVE_CAST(Function)(f), a);

  ASIO_HANDLER_CREATION((this->context(), *p.p,
        "io_context", &this->context(), 0, "post"));

  io_context_.impl_.post_immediate_completion(p.p, false);
  p.v = p.p = 0;
}

template <typename Function, typename Allocator>
void io_context::executor_type::post(
    ASIO_MOVE_ARG(Function) f, const Allocator& a) const
{
  typedef typename decay<Function>::type function_type;

  // Allocate and construct an operation to wrap the function.
  typedef detail::executor_op<function_type, Allocator, detail::operation> op;
  typename op::ptr p = { detail::addressof(a), op::ptr::allocate(a), 0 };
  p.p = new (p.v) op(ASIO_MOVE_CAST(Function)(f), a);

  ASIO_HANDLER_CREATION((this->context(), *p.p,
        "io_context", &this->context(), 0, "post"));

  io_context_.impl_.post_immediate_completion(p.p, false);
  p.v = p.p = 0;
}

template <typename Function, typename Allocator>
void io_context::executor_type::defer(
    ASIO_MOVE_ARG(Function) f, const Allocator& a) const
{
  typedef typename decay<Function>::type function_type;

  // Allocate and construct an operation to wrap the function.
  typedef detail::executor_op<function_type, Allocator, detail::operation> op;
  typename op::ptr p = { detail::addressof(a), op::ptr::allocate(a), 0 };
  p.p = new (p.v) op(ASIO_MOVE_CAST(Function)(f), a);

  ASIO_HANDLER_CREATION((this->context(), *p.p,
        "io_context", &this->context(), 0, "defer"));

  io_context_.impl_.post_immediate_completion(p.p, true);
  p.v = p.p = 0;
}

inline bool
io_context::executor_type::running_in_this_thread() const ASIO_NOEXCEPT
{
  return io_context_.impl_.can_dispatch();
}

#if !defined(ASIO_NO_DEPRECATED)
//mongodb中的TransportLayerASIO::start()调用，表示listener线程处理io_context上面的accept事件
inline io_context::work::work(asio::io_context& io_context)
  : io_context_impl_(io_context.impl_)
{
  io_context_impl_.work_started();
}

inline io_context::work::work(const work& other)
  : io_context_impl_(other.io_context_impl_)
{
  io_context_impl_.work_started();
}

inline io_context::work::~work()
{//scheduler::post_immediate_completion
  io_context_impl_.work_finished();
}

inline asio::io_context& io_context::work::get_io_context()
{
  return static_cast<asio::io_context&>(io_context_impl_.context());
}

inline asio::io_context& io_context::work::get_io_service()
{
  return static_cast<asio::io_context&>(io_context_impl_.context());
}
#endif // !defined(ASIO_NO_DEPRECATED)

inline asio::io_context& io_context::service::get_io_context()
{
  return static_cast<asio::io_context&>(context());
}

#if !defined(ASIO_NO_DEPRECATED)
inline asio::io_context& io_context::service::get_io_service()
{
  return static_cast<asio::io_context&>(context());
}
#endif // !defined(ASIO_NO_DEPRECATED)

} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_IMPL_IO_CONTEXT_HPP
