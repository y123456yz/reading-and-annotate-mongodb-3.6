//
// detail/scheduler_thread_info.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_SCHEDULER_THREAD_INFO_HPP
#define ASIO_DETAIL_SCHEDULER_THREAD_INFO_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/op_queue.hpp"
#include "asio/detail/thread_info_base.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {
namespace detail {

class scheduler;
class scheduler_operation;

struct scheduler_thread_info : public thread_info_base
{
  ////scheduler::do_wait_one->epoll_reactor::run 获取对应op，
  // 最终再通过scheduler::task_cleanup和scheduler::work_cleanup析构函数入队到scheduler::op_queue_
  //epoll相关的网络事件任务首先入队到私有队列private_op_queue，然后再入队到全局op_queue_队列，这样就可以一次性把获取到的网络事件任务入队到全局队列，只需要加锁一次
  //private_op_queue队列成员的op类型为descriptor_state，
  op_queue<scheduler_operation> private_op_queue; 

  //本线程私有private_op_queue队列中op任务数
  long private_outstanding_work;
};

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_DETAIL_SCHEDULER_THREAD_INFO_HPP
