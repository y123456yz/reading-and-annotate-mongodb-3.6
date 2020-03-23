//
// detail/impl/scheduler.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_IMPL_SCHEDULER_IPP
#define ASIO_DETAIL_IMPL_SCHEDULER_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"

#include "asio/detail/concurrency_hint.hpp"
#include "asio/detail/event.hpp"
#include "asio/detail/limits.hpp"
#include "asio/detail/reactor.hpp"
#include "asio/detail/scheduler.hpp"
#include "asio/detail/scheduler_thread_info.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {
namespace detail {

struct scheduler::task_cleanup
{
  //scheduler::do_run_one  scheduler::do_poll_one  scheduler::do_wait_one中执行析构
  ////将本线程的私有队列放入全局队列中，然后用task_operation_来标记一个线程私有队列的结束。
  //每次执行epoll_reactor::run去获取epoll对应的事件列表的时候都会执行该cleanup
  ~task_cleanup()
  {
    if (this_thread_->private_outstanding_work > 0)
    {
      asio::detail::increment(
          scheduler_->outstanding_work_,
          this_thread_->private_outstanding_work);
    }
    this_thread_->private_outstanding_work = 0;

    // Enqueue the completed operations and reinsert the task at the end of
    // the operation queue.
    lock_->lock();
	//将本线程的私有队列放入全局队列中，然后用task_operation_来标记一个线程私有队列的结束。
	//task_operation_标记一个线程私有队列的结束。
    scheduler_->task_interrupted_ = true;
    scheduler_->op_queue_.push(this_thread_->private_op_queue);

	//注意这里，加了一个特殊的op task_operation_到全局队列
    scheduler_->op_queue_.push(&scheduler_->task_operation_);
  }

  scheduler* scheduler_;
  //本作用域的锁
  mutex::scoped_lock* lock_;
  thread_info* this_thread_;
};

struct scheduler::work_cleanup
{
  //scheduler::do_run_one  scheduler::do_poll_one  scheduler::do_wait_one中执行析构
  
  ~work_cleanup()
  {
    if (this_thread_->private_outstanding_work > 1)
    {
      asio::detail::increment(
          scheduler_->outstanding_work_,
          this_thread_->private_outstanding_work - 1);
    }
    else if (this_thread_->private_outstanding_work < 1)
    {
      scheduler_->work_finished();
    }
    this_thread_->private_outstanding_work = 0;

	//将本线程的私有队列放入全局队列中 
#if defined(ASIO_HAS_THREADS)
    if (!this_thread_->private_op_queue.empty())
    {
      lock_->lock();
      scheduler_->op_queue_.push(this_thread_->private_op_queue);
    }
#endif // defined(ASIO_HAS_THREADS)
  }

  scheduler* scheduler_;
  mutex::scoped_lock* lock_;
  thread_info* this_thread_;
};

scheduler::scheduler(
    asio::execution_context& ctx, int concurrency_hint)
  : asio::detail::execution_context_service_base<scheduler>(ctx),
    one_thread_(concurrency_hint == 1 //默认-1,所以不会为true
        || !ASIO_CONCURRENCY_HINT_IS_LOCKING(
          SCHEDULER, concurrency_hint)
        || !ASIO_CONCURRENCY_HINT_IS_LOCKING(
          REACTOR_IO, concurrency_hint)),
    mutex_(ASIO_CONCURRENCY_HINT_IS_LOCKING(
          SCHEDULER, concurrency_hint)),
    task_(0),
    task_interrupted_(true),
    outstanding_work_(0),
    stopped_(false),
    shutdown_(false),
    concurrency_hint_(concurrency_hint)
{
  ASIO_HANDLER_TRACKING_INIT;
}

//销毁队列上的op
void scheduler::shutdown()
{
  mutex::scoped_lock lock(mutex_);
  shutdown_ = true;
  lock.unlock();

  // Destroy handler objects.
  while (!op_queue_.empty())
  {
    operation* o = op_queue_.front();
    op_queue_.pop();
    if (o != &task_operation_)
      o->destroy();
  }

  // Reset to initial state.
  task_ = 0;
}

void scheduler::init_task()
{
  mutex::scoped_lock lock(mutex_);
  if (!shutdown_ && !task_)
  {
    task_ = &use_service<reactor>(this->context());
    op_queue_.push(&task_operation_);
    wake_one_thread_and_unlock(lock);
  }
}

//mongodb中的accept新链接过程TransportLayerASIO::start()->io_context::run->scheduler::run中调用
std::size_t scheduler::run(asio::error_code& ec)
{
  ec = asio::error_code();
  if (outstanding_work_ == 0)
  {
    stop();
    return 0;
  }

  thread_info this_thread;
  this_thread.private_outstanding_work = 0;
  //所有的
  thread_call_stack::context ctx(this, this_thread);

  mutex::scoped_lock lock(mutex_);

  std::size_t n = 0;
  for (; do_run_one(lock, this_thread, ec); lock.lock())
    if (n != (std::numeric_limits<std::size_t>::max)())
      ++n;
  
  return n;
}

std::size_t scheduler::run_one(asio::error_code& ec)
{
  ec = asio::error_code();
  if (outstanding_work_ == 0)
  {
    stop();
    return 0;
  }

  thread_info this_thread;
  this_thread.private_outstanding_work = 0;
  thread_call_stack::context ctx(this, this_thread);

  mutex::scoped_lock lock(mutex_);

  return do_run_one(lock, this_thread, ec);
}

//accept流程
//TransportLayerASIO::_acceptConnection->basic_socket_acceptor::async_accept
//->start_accept_op->epoll_reactor::post_immediate_completion

//普通read write op操作入队流程
//mongodb的ServiceExecutorAdaptive::schedule调用->io_context::post(ASIO_MOVE_ARG(CompletionHandler) handler)
//->scheduler::post_immediate_completion
//mongodb的ServiceExecutorAdaptive::schedule调用->io_context::dispatch(ASIO_MOVE_ARG(CompletionHandler) handler)
//->scheduler::do_dispatch

//普通读写read write 从队列获取op执行流程
//ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_for->scheduler::wait_one->scheduler::do_wait_one调用
//mongodb中ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_one_for->io_context::run_one_until->schedule::wait_one


//ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_for->scheduler::wait_one->scheduler::do_wait_one调用
//mongodb中ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_one_for->io_context::run_one_until->schedule::wait_one
std::size_t scheduler::wait_one(long usec, asio::error_code& ec)
{
  ec = asio::error_code();
  if (outstanding_work_ == 0)
  {
    stop();
    return 0;
  }

  thread_info this_thread;
  this_thread.private_outstanding_work = 0;
  //线程入队到top
  thread_call_stack::context ctx(this, this_thread);

  mutex::scoped_lock lock(mutex_);

  return do_wait_one(lock, this_thread, usec, ec);
}

std::size_t scheduler::poll(asio::error_code& ec)
{
  ec = asio::error_code();
  if (outstanding_work_ == 0)
  {
    stop();
    return 0;
  }

  thread_info this_thread;
  this_thread.private_outstanding_work = 0;
  thread_call_stack::context ctx(this, this_thread);

  mutex::scoped_lock lock(mutex_);

#if defined(ASIO_HAS_THREADS)
  // We want to support nested calls to poll() and poll_one(), so any handlers
  // that are already on a thread-private queue need to be put on to the main
  // queue now.
  
  if (one_thread_)
    if (thread_info* outer_info = static_cast<thread_info*>(ctx.next_by_key()))
      op_queue_.push(outer_info->private_op_queue);
#endif // defined(ASIO_HAS_THREADS)

  std::size_t n = 0;
  for (; do_poll_one(lock, this_thread, ec); lock.lock())
    if (n != (std::numeric_limits<std::size_t>::max)())
      ++n;
  return n;
}

std::size_t scheduler::poll_one(asio::error_code& ec)
{
  ec = asio::error_code();
  if (outstanding_work_ == 0)
  {
    stop();
    return 0;
  }

  thread_info this_thread;
  this_thread.private_outstanding_work = 0;
  thread_call_stack::context ctx(this, this_thread);

  mutex::scoped_lock lock(mutex_);

#if defined(ASIO_HAS_THREADS)
  // We want to support nested calls to poll() and poll_one(), so any handlers
  // that are already on a thread-private queue need to be put on to the main
  // queue now.
  if (one_thread_)
    if (thread_info* outer_info = static_cast<thread_info*>(ctx.next_by_key()))
      op_queue_.push(outer_info->private_op_queue);
#endif // defined(ASIO_HAS_THREADS)

  return do_poll_one(lock, this_thread, ec);
}

//scheduler::work_finished
void scheduler::stop()
{
  mutex::scoped_lock lock(mutex_);
  stop_all_threads(lock);
}

bool scheduler::stopped() const
{
  mutex::scoped_lock lock(mutex_);
  return stopped_;
}

void scheduler::restart()
{
  mutex::scoped_lock lock(mutex_);
  stopped_ = false;
}

void scheduler::compensating_work_started()
{
  thread_info_base* this_thread = thread_call_stack::contains(this);
  ++static_cast<thread_info*>(this_thread)->private_outstanding_work;
}

//accept流程
//TransportLayerASIO::_acceptConnection->basic_socket_acceptor::async_accept
//->start_accept_op->epoll_reactor::post_immediate_completion

//普通read write op操作入队流程
//mongodb的ServiceExecutorAdaptive::schedule调用->io_context::post(ASIO_MOVE_ARG(CompletionHandler) handler)
//->scheduler::post_immediate_completion
//mongodb的ServiceExecutorAdaptive::schedule调用->io_context::dispatch(ASIO_MOVE_ARG(CompletionHandler) handler)
//->scheduler::do_dispatch

//普通读写read write 从队列获取op执行流程
//ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_for->scheduler::wait_one
//->scheduler::do_wait_one调用
//mongodb中ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_one_for
//->io_context::run_one_until->schedule::wait_one
void scheduler::post_immediate_completion(
    scheduler::operation* op, bool is_continuation)
{
#if defined(ASIO_HAS_THREADS)
  if (one_thread_ || is_continuation)
  {
    if (thread_info_base* this_thread = thread_call_stack::contains(this))
    {
      ++static_cast<thread_info*>(this_thread)->private_outstanding_work;
      static_cast<thread_info*>(this_thread)->private_op_queue.push(op);
      return;
    }
  }
#else // defined(ASIO_HAS_THREADS)
  (void)is_continuation;
#endif // defined(ASIO_HAS_THREADS)

  work_started();

//默认多线程，所以这里需要对队列加锁
  mutex::scoped_lock lock(mutex_);
  op_queue_.push(op);
  wake_one_thread_and_unlock(lock);
}

void scheduler::post_deferred_completion(scheduler::operation* op)
{
#if defined(ASIO_HAS_THREADS)
  if (one_thread_)
  {
    if (thread_info_base* this_thread = thread_call_stack::contains(this))
    {
      static_cast<thread_info*>(this_thread)->private_op_queue.push(op);
      return;
    }
  }
#endif // defined(ASIO_HAS_THREADS)
  //默认多线程，所以这里需要对队列加锁

  mutex::scoped_lock lock(mutex_);
  op_queue_.push(op);
  wake_one_thread_and_unlock(lock);
}

//epoll_reactor::cancel_ops  ~perform_io_cleanup_on_block_exit()调用
void scheduler::post_deferred_completions(
    op_queue<scheduler::operation>& ops)
{
  if (!ops.empty())
  {
#if defined(ASIO_HAS_THREADS)
    if (one_thread_)
    {
      if (thread_info_base* this_thread = thread_call_stack::contains(this))
      {
        static_cast<thread_info*>(this_thread)->private_op_queue.push(ops);
        return;
      }
    }
#endif // defined(ASIO_HAS_THREADS)
	//默认多线程，所以这里需要对队列加锁

    mutex::scoped_lock lock(mutex_);
    op_queue_.push(ops); //scheduler.op_queue_
    wake_one_thread_and_unlock(lock);
  }
}


//accept流程
//TransportLayerASIO::_acceptConnection->basic_socket_acceptor::async_accept
//->start_accept_op->epoll_reactor::post_immediate_completion

//普通read write op操作入队流程
//mongodb的ServiceExecutorAdaptive::schedule调用->io_context::post(ASIO_MOVE_ARG(CompletionHandler) handler)
//->scheduler::post_immediate_completion
//mongodb的ServiceExecutorAdaptive::schedule调用->io_context::dispatch(ASIO_MOVE_ARG(CompletionHandler) handler)
//->scheduler::do_dispatch

//普通读写read write 从队列获取op执行流程
//ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_for->scheduler::wait_one->scheduler::do_wait_one调用
//mongodb中ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_one_for->io_context::run_one_until->schedule::wait_one


//入队
void scheduler::do_dispatch(
    scheduler::operation* op)
{
  work_started();
  //默认多线程，所以这里需要对队列加锁
  mutex::scoped_lock lock(mutex_);
  op_queue_.push(op);
  wake_one_thread_and_unlock(lock);
}

void scheduler::abandon_operations(
    op_queue<scheduler::operation>& ops)
{
  op_queue<scheduler::operation> ops2;
  ops2.push(ops);
}

//mongodb中的accept新链接过程TransportLayerASIO::start()->io_context::run->scheduler::run中调用

//scheduler::run  scheduler::run_one 调用
//根据epoll获取对应读写事件，然后在队列op_queue_中取出统一执行
std::size_t scheduler::do_run_one(mutex::scoped_lock& lock,
    scheduler::thread_info& this_thread,
    const asio::error_code& ec)
{
  //stop_all_threads中置为true, 为true后，将不再处理epoll相关事件，参考scheduler::do_run_one
  while (!stopped_)
  {
    //每次从op_queue（其实是一个链表）里拿出头节点，然后处理。
    if (!op_queue_.empty())
    { //队列中已经没有可以指向得回调了，则继续epoll_wait等待获取对应事件回调
      // Prepare to execute first handler from queue.
      operation* o = op_queue_.front();
      op_queue_.pop();
      bool more_handlers = (!op_queue_.empty());

      if (o == &task_operation_) //遇到了特殊op，说明需要继续获取一批epoll事件回调
      {
        task_interrupted_ = more_handlers;

		//如果有剩下的任务且是多线程环境，则使用wake_one_thread_and|unlock尝试唤醒可能休眠的线程；
        if (more_handlers && !one_thread_)
          wakeup_event_.unlock_and_signal_one(lock); //提前解锁
        else
          lock.unlock();

        task_cleanup on_exit = { this, &lock, &this_thread };
//本scheduler::do_run_one函数返回前，会调用task_cleanup析构函数，从而把this_thread.private_op_queue入队
//到scheduler.op_queue_
        (void)on_exit;

        // Run the task. May throw an exception. Only block if the operation
        // queue is empty and we're not polling, otherwise we want to return
        // as soon as possible.
        //scheduler::do_run_one->epoll_reactor::run
        //通过epoll获取所有得网络事件op入队到private_op_queue, 最终再通过scheduler::poll_one scheduler::poll入队到op_queue_
		//epoll_reactor::run
		task_->run(more_handlers ? 0 : -1, this_thread.private_op_queue);
		//函数exit的时候执行前面的on_exit析构函数从而把this_thread.private_op_queue入队到scheduler.op_queue_
      }
      else
      { //取出队列上的一个op回调，op是在前面得if中入队得，在外层的scheduler::run循环执行

	    //获取epoll_wait返回的event信息，赋值见set_ready_events add_ready_events
        std::size_t task_result = o->task_result_; 
		

        if (more_handlers && !one_thread_)
          wake_one_thread_and_unlock(lock);
        else
          lock.unlock();

        // Ensure the count of outstanding work is decremented on block exit.
        work_cleanup on_exit = { this, &lock, &this_thread };
		//函数exit的时候执行前面的on_exit析构函数从而把this_thread.private_op_queue入队到scheduler.op_queue_
        (void)on_exit;
		/*
		asio::detail::reactive_socket_recv_op<
		 asio::mutable_buffers_1, asio::detail::read_op
		 <
		  asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, asio::mutable_buffer const*, asio::detail::transfer_all_t, 
		 void mongo::transport::TransportLayerASIO::ASIOSession::opportunisticRead<asio::basic_stream_socket<asio::generic::stream_protocol
		 >, 
		 asio::mutable_buffers_1, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::{lambda(mongo::Status const&, unsigned long)#1}
		>
		 
		  (
		   bool, asio::basic_stream_socket<asio::generic::stream_protocol>&, 
		   asio::mutable_buffers_1 const&, 
		   mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::{lambda(mongo::Status const&, unsigned long)#1}&&
		  )
		::{lambda(std::error_code const&, unsigned long)#1}> >::do_complete(void*, asio::detail::scheduler_operation*, std::error_code const, unsigned long) ()
		*/
        // Complete the operation. May throw an exception. Deletes the object.
        //epoll_reactor::descriptor_state::do_complete
        o->complete(this, ec, task_result); //在外层的scheduler::run循环执行

        return 1;
      }
    }
    else
    { //如果这个队列为空，本线程那么就需要等待。 等待其他线程通过wake_one_thread_and_unlock唤醒
      wakeup_event_.clear(lock);
      wakeup_event_.wait(lock);
    }
  }

  return 0;
}

//accept流程
//TransportLayerASIO::_acceptConnection->basic_socket_acceptor::async_accept
//->start_accept_op->epoll_reactor::post_immediate_completion

//普通read write op操作入队流程
//mongodb的ServiceExecutorAdaptive::schedule调用->io_context::post(ASIO_MOVE_ARG(CompletionHandler) handler)
//->scheduler::post_immediate_completion
//mongodb的ServiceExecutorAdaptive::schedule调用->io_context::dispatch(ASIO_MOVE_ARG(CompletionHandler) handler)
//->scheduler::do_dispatch

//普通读写read write 从队列获取op执行流程
//ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_for->scheduler::wait_one->scheduler::do_wait_one调用
//mongodb中ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_one_for->io_context::run_one_until->schedule::wait_one



//ServiceExecutorAdaptive::_workerThreadRoutine->io_context::run_for->scheduler::wait_one->scheduler::do_wait_one调用
//wait一段时间  
std::size_t scheduler::do_wait_one(mutex::scoped_lock& lock,
    scheduler::thread_info& this_thread, long usec,
    const asio::error_code& ec)
{//取队列中得一个op执行

  //stop_all_threads中置为true, 为true后，将不再处理epoll相关事件，参考scheduler::do_run_one
  if (stopped_)
    return 0;

  operation* o = op_queue_.front();
  if (o == 0)
  {
    //等待被唤醒
    wakeup_event_.clear(lock);
    wakeup_event_.wait_for_usec(lock, usec);
    usec = 0; // Wait at most once.
    o = op_queue_.front();
  }

  if (o == &task_operation_) //op_queue_队列上面没用IO op操作，于是通过epoll_wait获取加入对应队列
  {
    op_queue_.pop();
    bool more_handlers = (!op_queue_.empty());

    task_interrupted_ = more_handlers;

    if (more_handlers && !one_thread_)
      wakeup_event_.unlock_and_signal_one(lock);
    else
      lock.unlock();

    {
      task_cleanup on_exit = { this, &lock, &this_thread };
	  //函数exit的时候执行前面的on_exit析构函数从而把this_thread.private_op_queue入队到scheduler.op_queue_
        
      (void)on_exit;

      // Run the task. May throw an exception. Only block if the operation
      // queue is empty and we're not polling, otherwise we want to return
      // as soon as possible.
      //scheduler::do_run_one->epoll_reactor::run
      //通过epoll获取所有得网络事件op入队到private_op_queue, 最终再通过scheduler::poll_one scheduler::poll入队到op_queue_
      task_->run(more_handlers ? 0 : usec, this_thread.private_op_queue);
    }

    o = op_queue_.front();
    if (o == &task_operation_)
    {
      if (!one_thread_)
        wakeup_event_.maybe_unlock_and_signal_one(lock);
      return 0;
    }
  }

  if (o == 0)
    return 0;

  //下面执行o，这里的o就是reactor_op的complete_func
  op_queue_.pop();
  bool more_handlers = (!op_queue_.empty());

  std::size_t task_result = o->task_result_;

  if (more_handlers && !one_thread_)
    wake_one_thread_and_unlock(lock);
  else
    lock.unlock();

  // Ensure the count of outstanding work is decremented on block exit.
  
  work_cleanup on_exit = { this, &lock, &this_thread };
  //函数exit的时候执行前面的on_exit析构函数从而把this_thread.private_op_queue入队到scheduler.op_queue_
        
  (void)on_exit;

  // Complete the operation. May throw an exception. Deletes the object.
  /*
  asio::detail::reactive_socket_recv_op<
   asio::mutable_buffers_1, asio::detail::read_op
   <
	asio::basic_stream_socket<asio::generic::stream_protocol>, asio::mutable_buffers_1, asio::mutable_buffer const*, asio::detail::transfer_all_t, 
   void mongo::transport::TransportLayerASIO::ASIOSession::opportunisticRead<asio::basic_stream_socket<asio::generic::stream_protocol
   >, 
   asio::mutable_buffers_1, mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::{lambda(mongo::Status const&, unsigned long)#1}
  >
   
	(
	 bool, asio::basic_stream_socket<asio::generic::stream_protocol>&, 
	 asio::mutable_buffers_1 const&, 
	 mongo::transport::TransportLayerASIO::ASIOSourceTicket::fillImpl()::{lambda(mongo::Status const&, unsigned long)#1}&&
	)
  ::{lambda(std::error_code const&, unsigned long)#1}> >::do_complete(void*, asio::detail::scheduler_operation*, std::error_code const, unsigned long) ()
  */ //epoll_reactor::descriptor_state::do_complete
  o->complete(this, ec, task_result);

  return 1;  
}


//scheduler::poll_one  scheduler::poll
std::size_t scheduler::do_poll_one(mutex::scoped_lock& lock,
    scheduler::thread_info& this_thread,
    const asio::error_code& ec)
{
  //stop_all_threads中置为true, 为true后，将不再处理epoll相关事件，参考scheduler::do_run_one
  if (stopped_)
    return 0;

  operation* o = op_queue_.front();
  if (o == &task_operation_)
  {
    op_queue_.pop();
    lock.unlock();

    {
      task_cleanup c = { this, &lock, &this_thread };
	  //函数exit的时候执行前面的on_exit析构函数从而把this_thread.private_op_queue入队到scheduler.op_queue_
        
      (void)c;

      // Run the task. May throw an exception. Only block if the operation
      // queue is empty and we're not polling, otherwise we want to return
      // as soon as possible.
      task_->run(0, this_thread.private_op_queue);
    }

    o = op_queue_.front();
    if (o == &task_operation_)
    {
      wakeup_event_.maybe_unlock_and_signal_one(lock);
      return 0;
    }
  }

  if (o == 0)
    return 0;

  op_queue_.pop();
  bool more_handlers = (!op_queue_.empty());

  std::size_t task_result = o->task_result_;

  if (more_handlers && !one_thread_)
    wake_one_thread_and_unlock(lock);
  else
    lock.unlock();

  // Ensure the count of outstanding work is decremented on block exit.
  work_cleanup on_exit = { this, &lock, &this_thread };
  //函数exit的时候执行前面的on_exit析构函数从而把this_thread.private_op_queue入队到scheduler.op_queue_
  (void)on_exit;

  // Complete the operation. May throw an exception. Deletes the object.
  //epoll_reactor::descriptor_state::do_complete
  o->complete(this, ec, task_result);

  return 1;
}

void scheduler::stop_all_threads(
    mutex::scoped_lock& lock)
{
  stopped_ = true;
  //唤醒所有wakeup_event_.wait休眠等待线程
  wakeup_event_.signal_all(lock);

  if (!task_interrupted_ && task_)
  {
    task_interrupted_ = true;
    task_->interrupt(); //epoll_reactor::interrupt
  }
}

//尝试唤醒可能休眠的线程；否则直接释放锁
void scheduler::wake_one_thread_and_unlock(
    mutex::scoped_lock& lock)
{
  if (!wakeup_event_.maybe_unlock_and_signal_one(lock))
  {
    if (!task_interrupted_ && task_)
    {
      task_interrupted_ = true;
	  //epoll_reactor::interrupt
      task_->interrupt();
    }
    lock.unlock();
  }
}

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_DETAIL_IMPL_SCHEDULER_IPP
