//
// detail/scheduler.hpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_SCHEDULER_HPP
#define ASIO_DETAIL_SCHEDULER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"

#include "asio/error_code.hpp"
#include "asio/execution_context.hpp"
#include "asio/detail/atomic_count.hpp"
#include "asio/detail/conditionally_enabled_event.hpp"
#include "asio/detail/conditionally_enabled_mutex.hpp"
#include "asio/detail/op_queue.hpp"
#include "asio/detail/reactor_fwd.hpp"
#include "asio/detail/scheduler_operation.hpp"
#include "asio/detail/thread_context.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {
namespace detail {

//线程私有队列
struct scheduler_thread_info;

//在io_context.hpp中被定义为io_context_impl   typedef class scheduler io_context_impl;
//io_context.impl_  epoll_reactor.scheduler_成员为该类  scheduler.task_成员为epoll_reactor
class scheduler
  : public execution_context_service_base<scheduler>,
    public thread_context
{
public:
  ////reactor_op继承该类
  typedef scheduler_operation operation;

  // Constructor. Specifies the number of concurrent threads that are likely to
  // run the scheduler. If set to 1 certain optimisation are performed.
  ASIO_DECL scheduler(asio::execution_context& ctx,
      int concurrency_hint = 0);

  // Destroy all user-defined handler objects owned by the service.
  ASIO_DECL void shutdown();

  // Initialise the task, if required.
  ASIO_DECL void init_task();

  // Run the event loop until interrupted or no more work.
  ASIO_DECL std::size_t run(asio::error_code& ec);

  // Run until interrupted or one operation is performed.
  ASIO_DECL std::size_t run_one(asio::error_code& ec);

  // Run until timeout, interrupted, or one operation is performed.
  ASIO_DECL std::size_t wait_one(
      long usec, asio::error_code& ec);

  // Poll for operations without blocking.
  ASIO_DECL std::size_t poll(asio::error_code& ec);

  // Poll for one operation without blocking.
  ASIO_DECL std::size_t poll_one(asio::error_code& ec);

  // Interrupt the event processing loop.
  ASIO_DECL void stop();

  // Determine whether the scheduler is stopped.
  ASIO_DECL bool stopped() const;

  // Restart in preparation for a subsequent run invocation.
  ASIO_DECL void restart();

  // Notify that some work has started.
  //scheduler::post_immediate_completion   scheduler::post_immediate_completion
  //epoll_reactor::schedule_timer  epoll_reactor::start_op
  //work_finished和work_started对应
  void work_started() //计数，代表当前有多少个线程正在运行
  {
    ++outstanding_work_;
  }

  // Used to compensate for a forthcoming work_finished call. Must be called
  // from within a scheduler-owned thread.
  ASIO_DECL void compensating_work_started();

  // Notify that some work has finished.
  void work_finished()
  {
    if (--outstanding_work_ == 0)
      stop();
  }

  // Return whether a handler can be dispatched immediately.
  bool can_dispatch()
  {
    return thread_call_stack::contains(this) != 0;
  }

  // Request invocation of the given operation and return immediately. Assumes
  // that work_started() has not yet been called for the operation.
  ASIO_DECL void post_immediate_completion(
      operation* op, bool is_continuation);

  // Request invocation of the given operation and return immediately. Assumes
  // that work_started() was previously called for the operation.
  ASIO_DECL void post_deferred_completion(operation* op);

  // Request invocation of the given operations and return immediately. Assumes
  // that work_started() was previously called for each operation.
  ASIO_DECL void post_deferred_completions(op_queue<operation>& ops);

  // Enqueue the given operation following a failed attempt to dispatch the
  // operation for immediate invocation.
  ASIO_DECL void do_dispatch(operation* op);

  // Process unfinished operations as part of a shutdownoperation. Assumes that
  // work_started() was previously called for the operations.
  ASIO_DECL void abandon_operations(op_queue<operation>& ops);

  // Get the concurrency hint that was used to initialise the scheduler.
  int concurrency_hint() const
  {
    return concurrency_hint_;
  }

private:
  // The mutex type used by this scheduler.
  typedef conditionally_enabled_mutex mutex;

  // The event type used by this scheduler.
  typedef conditionally_enabled_event event;

  // Structure containing thread-specific data.
  //线程私有队列，该结构中包含一个队列成员
  typedef scheduler_thread_info thread_info;

  // Run at most one operation. May block.
  ASIO_DECL std::size_t do_run_one(mutex::scoped_lock& lock,
      thread_info& this_thread, const asio::error_code& ec);

  // Run at most one operation with a timeout. May block.
  ASIO_DECL std::size_t do_wait_one(mutex::scoped_lock& lock,
      thread_info& this_thread, long usec, const asio::error_code& ec);

  // Poll for at most one operation.
  ASIO_DECL std::size_t do_poll_one(mutex::scoped_lock& lock,
      thread_info& this_thread, const asio::error_code& ec);

  // Stop the task and all idle threads.
  ASIO_DECL void stop_all_threads(mutex::scoped_lock& lock);

  // Wake a single idle thread, or the task, and always unlock the mutex.
  ASIO_DECL void wake_one_thread_and_unlock(
      mutex::scoped_lock& lock);

  // Helper class to perform task-related operations on block exit.
  struct task_cleanup;
  friend struct task_cleanup;

  // Helper class to call work-related operations on block exit.
  struct work_cleanup;
  friend struct work_cleanup;

  // Whether to optimise for single-threaded use cases.
  //初始化的时候赋值，见scheduler::scheduler  如果一个线程为true
  //表示本scheduler由一个线程运行还是多个线程运行
  const bool one_thread_;

  // Mutex to protect access to internal data.
  //全局锁，多线程互斥
  mutable mutex mutex_;

  // Event to wake up blocked threads.
  event wakeup_event_; 

  // The task to be run by this service.
  //scheduler包含一个reactor，scheduler通过reactor模拟proactor：用户面对的接口一致，但数据的复制是在用户态而非内核态完成。
  reactor* task_; //epoll_reactor

  // Operation object to represent the position of the task in the queue.
  //初始化添加到op_queue_队列，见init_task  
  //scheduler::task_cleanup也加入到全局op_queue_队列
  struct task_operation : operation //特殊的op
  {
	//这个特殊op的作用是保证从epoll_wait返回值中获取到一批网络事件对应的IO回调op接入全局队列op_queue_后，都加上该特殊op到队列中
    task_operation() : operation(0) {}
  } task_operation_;  

  // Whether the task has been interrupted.
  //将本线程的私有队列放入全局队列中，然后用task_operation_来标记一个线程私有队列的结束。
  bool task_interrupted_;

  // The count of unfinished work.
  atomic_count outstanding_work_;

  // The queue of handlers that are ready to be delivered.
  // //scheduler.op_queue_和descriptor_state.op_queue_的联系见epoll_reactor::cancel_ops
  //操作队列,操作队列用于存放一般性操作   队列头指向op_queue_，见init_task
  //入队scheduler::post_deferred_completions  scheduler::post_immediate_completion  scheduler::poll_one scheduler::poll添加op到该队列
  //出队执行scheduler::do_run_one   scheduler::do_wait_one  scheduler::do_poll_one
  op_queue<operation> op_queue_; ////队列的o就是reactor_op的complete_func

  // Flag to indicate that the dispatcher has been stopped.
  //stop_all_threads中置为true, 为true后，将不再处理epoll相关事件，参考scheduler::do_run_one
  bool stopped_;

  // Flag to indicate that the dispatcher has been shut down.
  bool shutdown_;

  // The concurrency hint used to initialise the scheduler.
  const int concurrency_hint_;
};

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#if defined(ASIO_HEADER_ONLY)
# include "asio/detail/impl/scheduler.ipp"
#endif // defined(ASIO_HEADER_ONLY)

#endif // ASIO_DETAIL_SCHEDULER_HPP
