//
// detail/impl/epoll_reactor.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_IMPL_EPOLL_REACTOR_IPP
#define ASIO_DETAIL_IMPL_EPOLL_REACTOR_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"

#if defined(ASIO_HAS_EPOLL)

#include <cstddef>
#include <sys/epoll.h>
#include "asio/detail/epoll_reactor.hpp"
#include "asio/detail/throw_error.hpp"
#include "asio/error.hpp"

#if defined(ASIO_HAS_TIMERFD)
# include <sys/timerfd.h>
#endif // defined(ASIO_HAS_TIMERFD)

#include "asio/detail/push_options.hpp"

namespace asio {
namespace detail {

epoll_reactor::epoll_reactor(asio::execution_context& ctx)
  : execution_context_service_base<epoll_reactor>(ctx),
    scheduler_(use_service<scheduler>(ctx)),
    mutex_(ASIO_CONCURRENCY_HINT_IS_LOCKING(
          REACTOR_REGISTRATION, scheduler_.concurrency_hint())),
    interrupter_(),
    epoll_fd_(do_epoll_create()),
    timer_fd_(do_timerfd_create()),
    shutdown_(false),
    registered_descriptors_mutex_(mutex_.enabled())
{
  // Add the interrupter's descriptor to epoll.
  epoll_event ev = { 0, { 0 } };
  ev.events = EPOLLIN | EPOLLERR | EPOLLET;
  ev.data.ptr = &interrupter_;
  epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, interrupter_.read_descriptor(), &ev);
  interrupter_.interrupt();

  // Add the timer descriptor to epoll.
  if (timer_fd_ != -1)
  {
    ev.events = EPOLLIN | EPOLLERR;
    ev.data.ptr = &timer_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, timer_fd_, &ev);
  }
}

epoll_reactor::~epoll_reactor()
{
  if (epoll_fd_ != -1)
    close(epoll_fd_);
  if (timer_fd_ != -1)
    close(timer_fd_);
}

void epoll_reactor::shutdown()
{
  mutex::scoped_lock lock(mutex_);
  shutdown_ = true;
  lock.unlock();

  op_queue<operation> ops;

  while (descriptor_state* state = registered_descriptors_.first())
  {
    for (int i = 0; i < max_ops; ++i)
      ops.push(state->op_queue_[i]);
    state->shutdown_ = true;
    registered_descriptors_.free(state);
  }

  timer_queues_.get_all_timers(ops);

  scheduler_.abandon_operations(ops);
}

void epoll_reactor::notify_fork(
    asio::execution_context::fork_event fork_ev)
{
  if (fork_ev == asio::execution_context::fork_child)
  {
    if (epoll_fd_ != -1)
      ::close(epoll_fd_);
    epoll_fd_ = -1;
    epoll_fd_ = do_epoll_create();

    if (timer_fd_ != -1)
      ::close(timer_fd_);
    timer_fd_ = -1;
    timer_fd_ = do_timerfd_create();

    interrupter_.recreate();

    // Add the interrupter's descriptor to epoll.
    epoll_event ev = { 0, { 0 } };
    ev.events = EPOLLIN | EPOLLERR | EPOLLET;
    ev.data.ptr = &interrupter_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, interrupter_.read_descriptor(), &ev);
    interrupter_.interrupt();

    // Add the timer descriptor to epoll.
    if (timer_fd_ != -1)
    {
      ev.events = EPOLLIN | EPOLLERR;
      ev.data.ptr = &timer_fd_;
      epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, timer_fd_, &ev);
    }

    update_timeout();

    // Re-register all descriptors with epoll.
    mutex::scoped_lock descriptors_lock(registered_descriptors_mutex_);
    for (descriptor_state* state = registered_descriptors_.first();
        state != 0; state = state->next_)
    {
      ev.events = state->registered_events_;
      ev.data.ptr = state;
      int result = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, state->descriptor_, &ev);
      if (result != 0)
      {
        asio::error_code ec(errno,
            asio::error::get_system_category());
        asio::detail::throw_error(ec, "epoll re-registration");
      }
    }
  }
}

void epoll_reactor::init_task()
{
  scheduler_.init_task();
}

//reactive_socket_service_base::do_assign(新链接fd对应epoll注册)  reactive_socket_service_base::do_open(socket套接字对应的epoll注册)
//把套接字descriptor注册到epoll事件集中
int epoll_reactor::register_descriptor(socket_type descriptor,
    epoll_reactor::per_descriptor_data& descriptor_data) //套接字 回调等相关信息
{
  //获取一个描述符descriptor_state信息，分配对应空间
  descriptor_data = allocate_descriptor_state();

  ASIO_HANDLER_REACTOR_REGISTRATION((
        context(), static_cast<uintmax_t>(descriptor),
        reinterpret_cast<uintmax_t>(descriptor_data)));

  {
    mutex::scoped_lock descriptor_lock(descriptor_data->mutex_);

	//下面对descriptor_data进行相应的赋值
    descriptor_data->reactor_ = this;
    descriptor_data->descriptor_ = descriptor;
    descriptor_data->shutdown_ = false;
    for (int i = 0; i < max_ops; ++i)
      descriptor_data->try_speculative_[i] = true;
  }

  epoll_event ev = { 0, { 0 } };
  //同时把这些事件添加到epoll事件集，表示关注这些事件，注意这里是边沿触发
  ev.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLPRI | EPOLLET; 
  descriptor_data->registered_events_ = ev.events; 
  //赋值记录到ev.data.ptr中，当对应网络事件到底执行回调的时候可以通过该指针获取descriptor_data
  ev.data.ptr = descriptor_data; 
  //通过epoll_ctl把events添加到事件集，当对应事件发生，epoll_wait可以获取对应事件
  int result = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, descriptor, &ev);
  if (result != 0)
  {
    if (errno == EPERM)
    {
      descriptor_data->registered_events_ = 0;
      return 0;
    }
    return errno;
  }

  return 0;
}

//把套接字descriptor添加到epoll事件集中，关注accept read事件
int epoll_reactor::register_internal_descriptor(
    int op_type, socket_type descriptor,
    epoll_reactor::per_descriptor_data& descriptor_data, reactor_op* op)
{
  descriptor_data = allocate_descriptor_state();

  ASIO_HANDLER_REACTOR_REGISTRATION((
        context(), static_cast<uintmax_t>(descriptor),
        reinterpret_cast<uintmax_t>(descriptor_data)));

  {
    mutex::scoped_lock descriptor_lock(descriptor_data->mutex_);

    descriptor_data->reactor_ = this;
    descriptor_data->descriptor_ = descriptor;
    descriptor_data->shutdown_ = false;
    descriptor_data->op_queue_[op_type].push(op);
    for (int i = 0; i < max_ops; ++i)
      descriptor_data->try_speculative_[i] = true;
  }

  epoll_event ev = { 0, { 0 } };
  ev.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLPRI | EPOLLET;
  descriptor_data->registered_events_ = ev.events;
  //descriptor_data记录到该指针，epoll_reactor::run中通过对应事件获取该私有信息
  ev.data.ptr = descriptor_data;
  int result = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, descriptor, &ev);
  if (result != 0)
    return errno;

  return 0;
}

void epoll_reactor::move_descriptor(socket_type,
    epoll_reactor::per_descriptor_data& target_descriptor_data,
    epoll_reactor::per_descriptor_data& source_descriptor_data)
{
  target_descriptor_data = source_descriptor_data;
  source_descriptor_data = 0;
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
 //mongodb中opportunisticRead->asio:read->detail::read_buffer_sequence->basic_stream_socket::read_some->basic_stream_socket::read_some
 //reactive_socket_service_base::receive->socket_ops::sync_recv(这里直接读取数据)

//write发送异步数据流程: 
 //mongodb中通过opportunisticWrite->asio::async_write->start_write_buffer_sequence_op->detail::write_op() 
 //->basic_stream_socket::async_write_some->reactive_socket_service_base::async_send(这里构造reactive_socket_send_op_base，后续得epoll写数据及其读取到一个完整mongo报文得handler回调也在这里得do_complete中执行)
 //->reactive_socket_service_base::start_op->reactive_socket_service_base::start_op中进行EPOLL事件注册
//write同步发送数据流程:
 //同步写流程asio::write->write_buffer_sequence->basic_stream_socket::write_some->reactive_socket_service_base::send->socket_ops::sync_send(这里是真正得同步发送)

//mongodb中通过opportunisticWrite->asio::async_write->start_write_buffer_sequence_op->detail::write_op()
 //->basic_stream_socket::async_write_some->reactive_socket_service_base::async_send



//EPOLL对应网络事件回调：reactive_socket_accept_op_base(新连接) reactive_socket_recv_op_base(读) reactive_socket_send_op_base(写)
//operation分类:reactor_op(网络IO事件处理任务)	completion_handler(全局任务) descriptor_state(reactor_op对应的网络IO事件任务最终加入到该结构中由epoll触发处理)

//reactor_op对应的网络事件回调注册见epoll_reactor::start_op
void epoll_reactor::start_op(int op_type, socket_type descriptor,
    epoll_reactor::per_descriptor_data& descriptor_data, reactor_op* op,
    bool is_continuation, bool allow_speculative)
{
  if (!descriptor_data)
  {
    op->ec_ = asio::error::bad_descriptor;
    post_immediate_completion(op, is_continuation);
    return;
  }

  mutex::scoped_lock descriptor_lock(descriptor_data->mutex_);

  if (descriptor_data->shutdown_)
  {
    post_immediate_completion(op, is_continuation);
    return;
  }

  //如果reactor_op还没有加入对应的op_queue_[i]队列，则EPOLL_CTL_MOD跟新epoll对应事件信息
  if (descriptor_data->op_queue_[op_type].empty())
  {
    if (allow_speculative
        && (op_type != read_op
          || descriptor_data->op_queue_[except_op].empty()))
    {
      if (descriptor_data->try_speculative_[op_type])
      {
        if (reactor_op::status status = op->perform())
        {
          if (status == reactor_op::done_and_exhausted)
            if (descriptor_data->registered_events_ != 0)
              descriptor_data->try_speculative_[op_type] = false;
          descriptor_lock.unlock();
          scheduler_.post_immediate_completion(op, is_continuation);
          return;
        }
      }

      if (descriptor_data->registered_events_ == 0)
      {
        op->ec_ = asio::error::operation_not_supported;
        scheduler_.post_immediate_completion(op, is_continuation);
        return;
      }

      if (op_type == write_op)
      {
        if ((descriptor_data->registered_events_ & EPOLLOUT) == 0)
        {
          epoll_event ev = { 0, { 0 } };
          ev.events = descriptor_data->registered_events_ | EPOLLOUT;
          ev.data.ptr = descriptor_data;
          if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, descriptor, &ev) == 0)
          {
            descriptor_data->registered_events_ |= ev.events;
          }
          else
          {
            op->ec_ = asio::error_code(errno,
                asio::error::get_system_category());
            scheduler_.post_immediate_completion(op, is_continuation);
            return;
          }
        }
      }
    }
    else if (descriptor_data->registered_events_ == 0)
    {
      op->ec_ = asio::error::operation_not_supported;
      scheduler_.post_immediate_completion(op, is_continuation);
      return;
    }
    else
    {
      if (op_type == write_op)
      {
        descriptor_data->registered_events_ |= EPOLLOUT;
      }
 
      epoll_event ev = { 0, { 0 } };
      ev.events = descriptor_data->registered_events_;
	  
      ev.data.ptr = descriptor_data;
	  //再次跟新以下对应的IO事件
      epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, descriptor, &ev);
    }
  }
  //把任务回调opration入队到descriptor_data的对应队列
  descriptor_data->op_queue_[op_type].push(op);
  //scheduler::work_started  实际上就是链接数
  scheduler_.work_started();
}

//取出descriptor_data队列上的op入队到scheduler.op_queue_
//把descriptor_state.op_queue_队列上的op重新入队到scheduler.op_queue_  

void epoll_reactor::cancel_ops(socket_type,
    epoll_reactor::per_descriptor_data& descriptor_data)
{
  if (!descriptor_data)
    return;

  mutex::scoped_lock descriptor_lock(descriptor_data->mutex_);

  op_queue<operation> ops;
  for (int i = 0; i < max_ops; ++i)
  {
    while (reactor_op* op = descriptor_data->op_queue_[i].front())
    {
      op->ec_ = asio::error::operation_aborted;
      descriptor_data->op_queue_[i].pop();
      ops.push(op);
    }
  }

  descriptor_lock.unlock();

  //把descriptor_state.op_queue_队列上的op重新入队到scheduler.op_queue_  
  scheduler_.post_deferred_completions(ops);
}

//从epoll事件集中清除descriptor_data句柄
void epoll_reactor::deregister_descriptor(socket_type descriptor,
    epoll_reactor::per_descriptor_data& descriptor_data, bool closing)
{
  if (!descriptor_data)
    return;

  mutex::scoped_lock descriptor_lock(descriptor_data->mutex_);

  if (!descriptor_data->shutdown_)
  {
    if (closing)
    {
      // The descriptor will be automatically removed from the epoll set when
      // it is closed.
    }
    else if (descriptor_data->registered_events_ != 0)
    {
      epoll_event ev = { 0, { 0 } };
      epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, descriptor, &ev);
    }

    op_queue<operation> ops;
    for (int i = 0; i < max_ops; ++i)
    {
      while (reactor_op* op = descriptor_data->op_queue_[i].front())
      {
        op->ec_ = asio::error::operation_aborted;
        descriptor_data->op_queue_[i].pop();
        ops.push(op);
      }
    }

    descriptor_data->descriptor_ = -1;
    descriptor_data->shutdown_ = true;

    descriptor_lock.unlock();

    ASIO_HANDLER_REACTOR_DEREGISTRATION((
          context(), static_cast<uintmax_t>(descriptor),
          reinterpret_cast<uintmax_t>(descriptor_data)));

    scheduler_.post_deferred_completions(ops);
  }
}

void epoll_reactor::deregister_internal_descriptor(socket_type descriptor,
    epoll_reactor::per_descriptor_data& descriptor_data)
{
  if (!descriptor_data)
    return;

  mutex::scoped_lock descriptor_lock(descriptor_data->mutex_);

  if (!descriptor_data->shutdown_)
  {
    epoll_event ev = { 0, { 0 } };
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, descriptor, &ev);

    op_queue<operation> ops;
    for (int i = 0; i < max_ops; ++i)
      ops.push(descriptor_data->op_queue_[i]);

    descriptor_data->descriptor_ = -1;
    descriptor_data->shutdown_ = true;

    descriptor_lock.unlock();

    ASIO_HANDLER_REACTOR_DEREGISTRATION((
          context(), static_cast<uintmax_t>(descriptor),
          reinterpret_cast<uintmax_t>(descriptor_data)));
  }
}

void epoll_reactor::cleanup_descriptor_data(
    per_descriptor_data& descriptor_data)
{
  if (descriptor_data)
  {
    free_descriptor_state(descriptor_data);
    descriptor_data = 0;
  }
}

//epoll对应的网络回调及定时器处理,获取epoll对应事件得回调入队到ops队列，在外层函数统一处理
//scheduler::do_run_one    如果stopped_=true则永远不会进入该分支
void epoll_reactor::run(long usec, op_queue<operation>& ops) //ops队列内容为descriptor_state
{
  // This code relies on the fact that the scheduler queues the reactor task
  // behind all descriptor operations generated by this function. This means,
  // that by the time we reach this point, any previously returned descriptor
  // operations have already been dequeued. Therefore it is now safe for us to
  // reuse and return them for the scheduler to queue again.

  // Calculate timeout. Check the timer queues only if timerfd is not in use.
  int timeout;
  if (usec == 0)
    timeout = 0;
  else
  {
    timeout = (usec < 0) ? -1 : ((usec - 1) / 1000 + 1);
    if (timer_fd_ == -1)
    {
      mutex::scoped_lock lock(mutex_);
      timeout = get_timeout(timeout);
    }
  }

  // Block on the epoll descriptor.
  epoll_event events[128];
  //epoll_wait获取到IO事件后返回，或者超时事件内没有对应网络IO事件，也返回
  int num_events = epoll_wait(epoll_fd_, events, 128, timeout);
 
#if defined(ASIO_ENABLE_HANDLER_TRACKING)
  // Trace the waiting events.
  //遍历获取对应的事件信息
  for (int i = 0; i < num_events; ++i)
  {
    void* ptr = events[i].data.ptr;
    if (ptr == &interrupter_)
    {
      // Ignore.
    }
# if defined(ASIO_HAS_TIMERFD)
    else if (ptr == &timer_fd_)
    {
      // Ignore.
    }
# endif // defined(ASIO_HAS_TIMERFD)
//等待完成之后，我们开始分发事件:
    else
    {
      unsigned event_mask = 0;
	  //accept事件、网络数据到达事件
      if ((events[i].events & EPOLLIN) != 0)
        event_mask |= ASIO_HANDLER_REACTOR_READ_EVENT;
	  //写事件
      if ((events[i].events & EPOLLOUT))
        event_mask |= ASIO_HANDLER_REACTOR_WRITE_EVENT;
	  //异常事件
      if ((events[i].events & (EPOLLERR | EPOLLHUP)) != 0)
        event_mask |= ASIO_HANDLER_REACTOR_ERROR_EVENT;
      ASIO_HANDLER_REACTOR_EVENTS((context(),
            reinterpret_cast<uintmax_t>(ptr), event_mask));
    }
  }
#endif // defined(ASIO_ENABLE_HANDLER_TRACKING)

#if defined(ASIO_HAS_TIMERFD)
  bool check_timers = (timer_fd_ == -1);
#else // defined(ASIO_HAS_TIMERFD)
  bool check_timers = true;
#endif // defined(ASIO_HAS_TIMERFD)

  // Dispatch the waiting events.
  //IO事件类型有三种:interrupt,timer和普通的IO事件
  for (int i = 0; i < num_events; ++i)
  {
    //该事件对应的私有信息指针，通过该指针就可以获取到对应的descriptor_data
    void* ptr = events[i].data.ptr;
    if (ptr == &interrupter_) //在epoll_reactor::interrupt注册
    {
      // No need to reset the interrupter since we're leaving the descriptor
      // in a ready-to-read state and relying on edge-triggered notifications
      // to make it so that we only get woken up when the descriptor's epoll
      // registration is updated.

#if defined(ASIO_HAS_TIMERFD)
      if (timer_fd_ == -1)
        check_timers = true;
#else // defined(ASIO_HAS_TIMERFD)
      check_timers = true;
#endif // defined(ASIO_HAS_TIMERFD)
    }
#if defined(ASIO_HAS_TIMERFD)
    else if (ptr == &timer_fd_)
    {
      check_timers = true;
    }
#endif // defined(ASIO_HAS_TIMERFD)
    else
    {
      // The descriptor operation doesn't count as work in and of itself, so we
      // don't call work_started() here. This still allows the scheduler to
      // stop if the only remaining operations are descriptor operations.
      //通过ptr获取对应descriptor_data信息
      descriptor_state* descriptor_data = static_cast<descriptor_state*>(ptr);
      if (!ops.is_enqueued(descriptor_data))  //不在队列中，则添加
      {
        //对应事件位图置位
        descriptor_data->set_ready_events(events[i].events);
		//epoll operation对应的回调函数是epoll_reactor::descriptor_state::do_complete
        ops.push(descriptor_data); 
      }
      else  //descriptor_data已经在ops的队列中了，对应事件位图置位
      {
        descriptor_data->add_ready_events(events[i].events);
      }
    }
  }

  if (check_timers)
  {
    mutex::scoped_lock common_lock(mutex_);
	//把定时器到期的timer对应的op回调添加到ops
    timer_queues_.get_ready_timers(ops);

#if defined(ASIO_HAS_TIMERFD)
    if (timer_fd_ != -1)
    {
      itimerspec new_timeout;
      itimerspec old_timeout;
      int flags = get_timeout(new_timeout);
      timerfd_settime(timer_fd_, flags, &new_timeout, &old_timeout);
    }
#endif // defined(ASIO_HAS_TIMERFD)
  }
}

void epoll_reactor::interrupt()
{
  epoll_event ev = { 0, { 0 } };
  ev.events = EPOLLIN | EPOLLERR | EPOLLET; //
  ev.data.ptr = &interrupter_;
  //eventfd_select_interrupter::read_descriptor
  epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, interrupter_.read_descriptor(), &ev);
}

//epoll_reactor::epoll_reactor中调用
int epoll_reactor::do_epoll_create()
{
#if defined(EPOLL_CLOEXEC)
  int fd = epoll_create1(EPOLL_CLOEXEC);
#else // defined(EPOLL_CLOEXEC)
  int fd = -1;
  errno = EINVAL;
#endif // defined(EPOLL_CLOEXEC)

  if (fd == -1 && (errno == EINVAL || errno == ENOSYS))
  {
    fd = epoll_create(epoll_size);
    if (fd != -1)
      ::fcntl(fd, F_SETFD, FD_CLOEXEC);
  }

  if (fd == -1)
  {
    asio::error_code ec(errno,
        asio::error::get_system_category());
    asio::detail::throw_error(ec, "epoll");
  }

  return fd;
}

//epoll_reactor::epoll_reactor中调用
int epoll_reactor::do_timerfd_create()
{
#if defined(ASIO_HAS_TIMERFD)
# if defined(TFD_CLOEXEC)
//它是用来创建一个定时器描述符timerfd
/*
/* 
timerfd_create（）函数创建一个定时器对象，同时返回一个与之关联的文件描述符。
clockid：clockid标识指定的时钟计数器，可选值（CLOCK_REALTIME、CLOCK_MONOTONIC。。。）
CLOCK_REALTIME:系统实时时间,随系统实时时间改变而改变,即从UTC1970-1-1 0:0:0开始计时,中间时刻如果系统时间被用户改成其他,则对应的时间相应改变
CLOCK_MONOTONIC:从系统启动这一刻起开始计时,不受系统时间被用户改变的影响
flags：参数flags（TFD_NONBLOCK(非阻塞模式)/TFD_CLOEXEC（表示当程序执行exec函数时本fd将被系统自动关闭,表示不传递）
*/

*/
  int fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
# else // defined(TFD_CLOEXEC)
  int fd = -1;
  errno = EINVAL;
# endif // defined(TFD_CLOEXEC)

  if (fd == -1 && errno == EINVAL)
  {
    
    fd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (fd != -1)
      ::fcntl(fd, F_SETFD, FD_CLOEXEC);
  }

  return fd;
#else // defined(ASIO_HAS_TIMERFD)
  return -1;
#endif // defined(ASIO_HAS_TIMERFD)
}

//epoll_reactor::register_descriptor
epoll_reactor::descriptor_state* epoll_reactor::allocate_descriptor_state()
{
  mutex::scoped_lock descriptors_lock(registered_descriptors_mutex_);
  return registered_descriptors_.alloc(ASIO_CONCURRENCY_HINT_IS_LOCKING(
        REACTOR_IO, scheduler_.concurrency_hint()));
}

void epoll_reactor::free_descriptor_state(epoll_reactor::descriptor_state* s)
{
  mutex::scoped_lock descriptors_lock(registered_descriptors_mutex_);
  registered_descriptors_.free(s);
}

//epoll_reactor::add_timer_queue中调用
void epoll_reactor::do_add_timer_queue(timer_queue_base& queue)
{
  mutex::scoped_lock lock(mutex_);
  timer_queues_.insert(&queue);
}

//epoll_reactor::remove_timer_queue中调用
void epoll_reactor::do_remove_timer_queue(timer_queue_base& queue)
{
  mutex::scoped_lock lock(mutex_);
  timer_queues_.erase(&queue);
}

void epoll_reactor::update_timeout()
{
#if defined(ASIO_HAS_TIMERFD)
  if (timer_fd_ != -1)
  {
    itimerspec new_timeout;
    itimerspec old_timeout;

	//获取
    int flags = get_timeout(new_timeout);
    timerfd_settime(timer_fd_, flags, &new_timeout, &old_timeout);
    return;
  }
#endif // defined(ASIO_HAS_TIMERFD)
  interrupt();
}


int epoll_reactor::get_timeout(int msec)
{
  // By default we will wait no longer than 5 minutes. This will ensure that
  // any changes to the system clock are detected after no longer than this.
  const int max_msec = 5 * 60 * 1000;
  //timer_queue_set::wait_duration_msec->
  return timer_queues_.wait_duration_msec(
      (msec < 0 || max_msec < msec) ? max_msec : msec);
}

#if defined(ASIO_HAS_TIMERFD)
//获取下一个timer的过期时间点
//获取队列中离当前时间最近的timer对应的时间信息(ms),如果离当前最近的时间超过5分钟，则取5分钟
//epoll_reactor::run中调用
int epoll_reactor::get_timeout(itimerspec& ts)
{
  ts.it_interval.tv_sec = 0;
  ts.it_interval.tv_nsec = 0;

  //执行timer_queue::wait_duration_msec
  long usec = timer_queues_.wait_duration_usec(5 * 60 * 1000 * 1000);
  ts.it_value.tv_sec = usec / 1000000;
  ts.it_value.tv_nsec = usec ? (usec % 1000000) * 1000 : 1;

  return usec ? 0 : TFD_TIMER_ABSTIME;
}
#endif // defined(ASIO_HAS_TIMERFD)

//下面的epoll_reactor::descriptor_state::perform_io中构造使用
struct epoll_reactor::perform_io_cleanup_on_block_exit
{
  explicit perform_io_cleanup_on_block_exit(epoll_reactor* r)
    : reactor_(r), first_op_(0)
  {
  }
 
  ~perform_io_cleanup_on_block_exit()
  { 
    //配合epoll_reactor::descriptor_state::perform_io阅读，可以看出第一个op由本线程获取，其他op放入到勒ops_队列
    //队首的op任务由本线程处理，其他op任务放入全局任务队列，由线程池中线程调度执行
	if (first_op_)
    {
      // Post the remaining completed operations for invocation.
      if (!ops_.empty())
	  	//scheduler::post_deferred_completions，op任务放入全局队列，延迟执行
        reactor_->scheduler_.post_deferred_completions(ops_);

      // A user-initiated operation has completed, but there's no need to
      // explicitly call work_finished() here. Instead, we'll take advantage of
      // the fact that the scheduler will call work_finished() once we return.
    }
    else
    {
      // No user-initiated operations have completed, so we need to compensate
      // for the work_finished() call that the scheduler will make once this
      // operation returns.

	  //队首的线程由本线程处理，其他op任务放入全局任务队列, 见epoll_reactor::descriptor_state::do_complete
      reactor_->scheduler_.compensating_work_started();
    }
  }

  epoll_reactor* reactor_;
  op_queue<operation> ops_;
  operation* first_op_;
};

//allocate_descriptor_state
epoll_reactor::descriptor_state::descriptor_state(bool locking)
  : operation(&epoll_reactor::descriptor_state::do_complete),
    mutex_(locking)
{
}

//epoll_reactor::descriptor_state::do_complete执行
//网络IO相关的任务处理，如accept接收新链接、接收数据、发送数据
operation* epoll_reactor::descriptor_state::perform_io(uint32_t events)
{
  mutex_.lock();
  perform_io_cleanup_on_block_exit io_cleanup(reactor_);
  mutex::scoped_lock descriptor_lock(mutex_, mutex::scoped_lock::adopt_lock);

  // Exception operations must be processed first to ensure that any
  // out-of-band data is read before normal data.
  //分别对应链接到达或者数据来临、可以写数据、有紧急的数据可读(这里应该表示有带外数据到来)
  static const int flag[max_ops] = { EPOLLIN, EPOLLOUT, EPOLLPRI };
  //循环处理各自不同的reactor_op(reactive_socket_accept_op_base   reactive_socket_recv_op_base reactive_socket_send_op_base)
  for (int j = max_ops - 1; j >= 0; --j)
  {
    if (events & (flag[j] | EPOLLERR | EPOLLHUP)) //有读写事件、或者epoll_wait有获取到异常，如链接断开等
    {
      try_speculative_[j] = true;
      while (reactor_op* op = op_queue_[j].front())
      {
       //reactive_socket_accept_op_base::do_perform  reactive_socket_recv_op_base::do_perform
  //reactive_socket_send_op_base::do_perform  分别对应新链接，读取数据，发送数据的底层实现
  	//执行底层数据收发的perform_io，也就是如下：
	//1. accept处理底层实现:reactive_socket_accept_op_base::do_perform 
	//2. 读处理底层实现：reactive_socket_recv_op_base::do_perform
	//3. 写处理底层实现：reactive_socket_send_op_base::do_perform
        if (reactor_op::status status = op->perform()) 
		//status为true表示成功，false表示底层处理失败
        {
          //取出对应的op，入队到临时队列io_cleanup.ops_
          op_queue_[j].pop();
          io_cleanup.ops_.push(op);
          if (status == reactor_op::done_and_exhausted)
          {
            try_speculative_[j] = false;
            break;
          }
        }
		//如果有异常
        else
          break;
      }
    }
  }

  // The first operation will be returned for completion now. The others will
  // be posted for later by the io_cleanup object's destructor.
   //这里只返回了第一个op，其他的op在~perform_io_cleanup_on_block_exit处理
  io_cleanup.first_op_ = io_cleanup.ops_.front();
  //把返回的第一个从队列中清除，剩余的op还在队列中
  io_cleanup.ops_.pop(); 

  //该逻辑的总体思路：队首的op任务由本线程处理，其他op任务放入全局任务队列，由线程池中线程调度执行

  //只返回第一个op,外层的epoll_reactor::descriptor_state::do_complete中执行该op对应的complete
  return io_cleanup.first_op_;
}

//赋值给epoll_reactor::descriptor_state::descriptor_state:operation，见epoll_reactor::descriptor_state::descriptor_state
//scheduler::do_run_one  scheduler::do_wait_one中执行

//网络IO相关的任务处理，如accept接收新链接、接收数据、发送数据及其他们对应回调处理
void epoll_reactor::descriptor_state::do_complete(
    void* owner, operation* base,
    const asio::error_code& ec, std::size_t bytes_transferred)
{
  if (owner)
  {
    descriptor_state* descriptor_data = static_cast<descriptor_state*>(base);
    uint32_t events = static_cast<uint32_t>(bytes_transferred); //事件位图
	//执行底层数据收发的perform_func
    if (operation* op = descriptor_data->perform_io(events)) //
    //注意descriptor_data下有很大perform_func执行了，但是这里只有第一个perform_func对应的complete_func得到了执行
    //其他的complete_func在~perform_io_cleanup_on_block_exit析构函数中入队到scheduler.op_queue_，等待其他线程调度执行
    {
      //执行complete_func, 也就是reactive_socket_accept_op_base  reactive_socket_recv_op_base reactive_socket_send_op_base  
      //这三个IO操作对应的complete_func回调

	  //注意这里只执行了网络IO事件任务中的一个，其他的在perform_io中入队到全局队列中了，等待其他线程执行
      op->complete(owner, ec, 0);
    }
  }
}
} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // defined(ASIO_HAS_EPOLL)

#endif // ASIO_DETAIL_IMPL_EPOLL_REACTOR_IPP
