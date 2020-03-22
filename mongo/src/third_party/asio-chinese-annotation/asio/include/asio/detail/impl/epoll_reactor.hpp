//
// detail/impl/epoll_reactor.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_IMPL_EPOLL_REACTOR_HPP
#define ASIO_DETAIL_IMPL_EPOLL_REACTOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#if defined(ASIO_HAS_EPOLL)

#include "asio/detail/push_options.hpp"

namespace asio {
namespace detail {

template <typename Time_Traits>
void epoll_reactor::add_timer_queue(timer_queue<Time_Traits>& queue)
{
  //调用epoll_reactor::do_add_timer_queue
  do_add_timer_queue(queue);
}

template <typename Time_Traits>
void epoll_reactor::remove_timer_queue(timer_queue<Time_Traits>& queue)
{
  //调用epoll_reactor::do_remove_timer_queue
  do_remove_timer_queue(queue);
}

template <typename Time_Traits>
void epoll_reactor::schedule_timer(timer_queue<Time_Traits>& queue,
    const typename Time_Traits::time_type& time,
    typename timer_queue<Time_Traits>::per_timer_data& timer, wait_op* op)
{
  mutex::scoped_lock lock(mutex_);

  if (shutdown_)
  {//epoll_reactor::post_immediate_completion
    scheduler_.post_immediate_completion(op, false);
    return;
  }

  //执行timer_queue::enqueue_timer
  bool earliest = queue.enqueue_timer(time, timer, op);
  scheduler_.work_started();//epoll_reactor::work_started
  if (earliest)
    update_timeout();
}

template <typename Time_Traits>
std::size_t epoll_reactor::cancel_timer(timer_queue<Time_Traits>& queue,
    typename timer_queue<Time_Traits>::per_timer_data& timer,
    std::size_t max_cancelled)
{
  mutex::scoped_lock lock(mutex_);
  op_queue<operation> ops;
  //执行timer_queue::cancel_timer
  std::size_t n = queue.cancel_timer(timer, ops, max_cancelled);
  lock.unlock();
  //epoll_reactor::post_deferred_completions
  scheduler_.post_deferred_completions(ops);
  return n;
}

template <typename Time_Traits>
void epoll_reactor::move_timer(timer_queue<Time_Traits>& queue,
    typename timer_queue<Time_Traits>::per_timer_data& target,
    typename timer_queue<Time_Traits>::per_timer_data& source)
{
  mutex::scoped_lock lock(mutex_);
  op_queue<operation> ops;

   //执行timer_queue::cancel_timer
  queue.cancel_timer(target, ops); 
   //执行timer_queue::move_timer
  queue.move_timer(target, source);
  lock.unlock();
  //epoll_reactor::post_deferred_completions
  scheduler_.post_deferred_completions(ops);
}

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // defined(ASIO_HAS_EPOLL)

#endif // ASIO_DETAIL_IMPL_EPOLL_REACTOR_HPP
