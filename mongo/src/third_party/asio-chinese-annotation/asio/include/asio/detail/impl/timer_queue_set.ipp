//
// detail/impl/timer_queue_set.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_IMPL_TIMER_QUEUE_SET_IPP
#define ASIO_DETAIL_IMPL_TIMER_QUEUE_SET_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"
#include "asio/detail/timer_queue_set.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {
namespace detail {

//timer_queue_set time队列集初始化
timer_queue_set::timer_queue_set()
  : first_(0)
{
}

//q节点插入队列首部
void timer_queue_set::insert(timer_queue_base* q)
{
  q->next_ = first_;
  first_ = q;
}

//从队列移除节点q
void timer_queue_set::erase(timer_queue_base* q)
{
  if (first_)
  {
    if (q == first_)
    {
      first_ = q->next_;
      q->next_ = 0;
      return;
    }

    for (timer_queue_base* p = first_; p->next_; p = p->next_)
    {
      if (p->next_ == q)
      {
        p->next_ = q->next_;
        q->next_ = 0;
        return;
      }
    }
  }
}

//first_链表上面的timer队列集是否全部为空
bool timer_queue_set::all_empty() const
{
  for (timer_queue_base* p = first_; p; p = p->next_)
    if (!p->empty())
      return false;
  return true;
}

//获取first_队列集中所有timer_queue队列中的timer的最小超时时间(ms)
long timer_queue_set::wait_duration_msec(long max_duration) const
{
  long min_duration = max_duration;
  for (timer_queue_base* p = first_; p; p = p->next_)
    min_duration = p->wait_duration_msec(min_duration);
  return min_duration;
}

//获取first_队列集中所有timer_queue队列中的timer的最小超时时间(us)
long timer_queue_set::wait_duration_usec(long max_duration) const
{
  long min_duration = max_duration;
  for (timer_queue_base* p = first_; p; p = p->next_)
    min_duration = p->wait_duration_usec(min_duration);
  return min_duration;
}

//获取first_队列集中所有队列上面已超时的timer对应的op入队到ops队列
void timer_queue_set::get_ready_timers(op_queue<operation>& ops)
{
  for (timer_queue_base* p = first_; p; p = p->next_)
  	//timer_queue::get_ready_timers
  	//获取p这个timer_queue上面的所有已超时timer对应的op回调入队到ops队列
    p->get_ready_timers(ops);
}

//获取first_队列集中所有队列上面timer(包括已超时还未执行+未超时)对应的op入队到ops队列
void timer_queue_set::get_all_timers(op_queue<operation>& ops)
{
  for (timer_queue_base* p = first_; p; p = p->next_)
    p->get_all_timers(ops); //timer_queue::get_all_timers
}

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_DETAIL_IMPL_TIMER_QUEUE_SET_IPP
