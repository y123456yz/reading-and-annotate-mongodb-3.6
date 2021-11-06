//
// detail/timer_queue.hpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_TIMER_QUEUE_HPP
#define ASIO_DETAIL_TIMER_QUEUE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"
#include <cstddef>
#include <vector>
#include "asio/detail/cstdint.hpp"
#include "asio/detail/date_time_fwd.hpp"
#include "asio/detail/limits.hpp"
#include "asio/detail/op_queue.hpp"
#include "asio/detail/timer_queue_base.hpp"
#include "asio/detail/wait_op.hpp"
#include "asio/error.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {
namespace detail {

template <typename Time_Traits>
//timer_queue_base接口的实现类  timer_queue_set.first_成员为该类型 
class timer_queue
  : public timer_queue_base
{
public:
  // The time type.
  //posix_time::ptime
  typedef typename Time_Traits::time_type time_type;

  // The duration type.
  typedef typename Time_Traits::duration_type duration_type;

  // Per-timer data.   //timer_queue.timers_
  class per_timer_data
  {
  public:
    per_timer_data() :
      heap_index_((std::numeric_limits<std::size_t>::max)()),
      next_(0), prev_(0)
    {
    }

  private:
    friend class timer_queue;

    // The operations waiting on the timer.
    //该定时器定时时间到的时候需要执行的op操作
    op_queue<wait_op> op_queue_;

    // The index of the timer in the heap.
    std::size_t heap_index_;

    // Pointers to adjacent timers in a linked list.
    //把timer链接在一起组成双向链表，加入到timers_头部
    per_timer_data* next_;
    per_timer_data* prev_;
  };

  // Constructor.
  timer_queue()
    : timers_(),
      heap_()
  {
  }

  // Add a new timer to the queue. Returns true if this is the timer that is
  // earliest in the queue, in which case the reactor's event demultiplexing
  // function call may need to be interrupted and restarted.
  //epoll_reactor::schedule_timer中调用
  bool enqueue_timer(const time_type& time, per_timer_data& timer, wait_op* op)
  {
    // Enqueue the timer object.
    if (timer.prev_ == 0 && &timer != timers_)
    {
      if (this->is_positive_infinity(time))
      {
        // No heap entry is required for timers that never expire.
        timer.heap_index_ = (std::numeric_limits<std::size_t>::max)();
      }
      else
      {
        // Put the new timer at the correct position in the heap. This is done
        // first since push_back() can throw due to allocation failure.
        //当前heap_ 数组大小,也就是当前heap_数组已经加入了多少个heap_entry节点
        timer.heap_index_ = heap_.size();
        heap_entry entry = { time, &timer };

		//新的timer构造一个heap_entry加入到heap_数组
        heap_.push_back(entry);
		//新的entry节点按照time在heap_数组中快速排序
        up_heap(heap_.size() - 1);
      }

      // Insert the new timer into the linked list of active timers.
      //timer添加到timers_队列头部，组成双向链表
      timer.next_ = timers_;
      timer.prev_ = 0;
      if (timers_)
        timers_->prev_ = &timer;
      timers_ = &timer;
    }

    // Enqueue the individual timer operation.
    timer.op_queue_.push(op); //op入队到本timer的op队列

    // Interrupt reactor only if newly added timer is first to expire.
    return timer.heap_index_ == 0 && timer.op_queue_.front() == op;
  }

  // Whether there are no timers in the queue.
  //timers_队列没有timer
  virtual bool empty() const
  {
    return timers_ == 0;
  }

  // Get the time for the timer that is earliest in the queue.
  //epoll_reactor::get_timeout
  virtual long wait_duration_msec(long max_duration) const
  {
    if (heap_.empty())
      return max_duration;

	//获取heap_数组中第一个timer距离当前时间的时间差(ms)，也就是还有多少ms第一个timer到来
    return this->to_msec(
        Time_Traits::to_posix_duration(
          Time_Traits::subtract(heap_[0].time_, Time_Traits::now())),
        max_duration);
  }

  // Get the time for the timer that is earliest in the queue.
  //epoll_reactor::get_timeout中执行
  //获取heap_数组中第一个timer距离当前时间的时间差(us)，也就是还有多少ms第一个timer到来
  virtual long wait_duration_usec(long max_duration) const
  {
    if (heap_.empty())
      return max_duration;

    return this->to_usec(
        Time_Traits::to_posix_duration(
          Time_Traits::subtract(heap_[0].time_, Time_Traits::now())),
        max_duration);
  }

  // Dequeue all timers not later than the current time.
  //获取heap_中已经到期的timer，并取出对应的ops入队到ops队列
  //timer_queue_set::get_ready_timers中调用
  virtual void get_ready_timers(op_queue<operation>& ops)
  {
    if (!heap_.empty())
    {
      const time_type now = Time_Traits::now();
      while (!heap_.empty() && !Time_Traits::less_than(now, heap_[0].time_))
      {
        per_timer_data* timer = heap_[0].timer_;
        ops.push(timer->op_queue_);
        remove_timer(*timer);
      }
    }
  }

  // Dequeue all timers.
  //获取timers_队列中的所有timer添加到ops队列
  //timer_queue_set::get_all_timers中调用
  virtual void get_all_timers(op_queue<operation>& ops)
  {
    while (timers_)
    {
      per_timer_data* timer = timers_;
      timers_ = timers_->next_;
      ops.push(timer->op_queue_);
      timer->next_ = 0;
      timer->prev_ = 0;
    }

    heap_.clear();
  }

  // Cancel and dequeue operations for the given timer.
  //epoll_reactor::cancel_timer中调用
  std::size_t cancel_timer(per_timer_data& timer, op_queue<operation>& ops,
      std::size_t max_cancelled = (std::numeric_limits<std::size_t>::max)())
  {
    std::size_t num_cancelled = 0;
    if (timer.prev_ != 0 || &timer == timers_)
    {
      while (wait_op* op = (num_cancelled != max_cancelled)
          ? timer.op_queue_.front() : 0)
      {
        op->ec_ = asio::error::operation_aborted;
        timer.op_queue_.pop();
        ops.push(op);
        ++num_cancelled;
      }
      if (timer.op_queue_.empty())
        remove_timer(timer);
    }
    return num_cancelled;
  }

  // Move operations from one timer to another, empty timer.
  void move_timer(per_timer_data& target, per_timer_data& source)
  {
    target.op_queue_.push(source.op_queue_);

    target.heap_index_ = source.heap_index_;
    source.heap_index_ = (std::numeric_limits<std::size_t>::max)();

    if (target.heap_index_ < heap_.size())
      heap_[target.heap_index_].timer_ = &target;

    if (timers_ == &source)
      timers_ = &target;
    if (source.prev_)
      source.prev_->next_ = &target;
    if (source.next_)
      source.next_->prev_= &target;
    target.next_ = source.next_;
    target.prev_ = source.prev_;
    source.next_ = 0;
    source.prev_ = 0;
  }

private:
  // Move the item at the given index up the heap to its correct position.
  //对heap_数组中的成员按照time_过期时间快速排序
  void up_heap(std::size_t index)
  {
    while (index > 0)
    {
      std::size_t parent = (index - 1) / 2;
      if (!Time_Traits::less_than(heap_[index].time_, heap_[parent].time_))
        break;
      swap_heap(index, parent);
      index = parent;
    }
  }

  // Move the item at the given index down the heap to its correct position.
  //对heap_数组中的成员按照time_过期时间快速排序
  void down_heap(std::size_t index)
  {
    std::size_t child = index * 2 + 1;
    while (child < heap_.size())
    {
      std::size_t min_child = (child + 1 == heap_.size()
          || Time_Traits::less_than(
            heap_[child].time_, heap_[child + 1].time_))
        ? child : child + 1;
      if (Time_Traits::less_than(heap_[index].time_, heap_[min_child].time_))
        break;
      swap_heap(index, min_child);
      index = min_child;
      child = index * 2 + 1;
    }
  }

  // Swap two entries in the heap.
  void swap_heap(std::size_t index1, std::size_t index2)
  {
    heap_entry tmp = heap_[index1];
    heap_[index1] = heap_[index2];
    heap_[index2] = tmp;
    heap_[index1].timer_->heap_index_ = index1;
    heap_[index2].timer_->heap_index_ = index2;
  }

  // Remove a timer from the heap and list of timers.
  //从heap_中移除timer   timer::queue::get_ready_timers
  void remove_timer(per_timer_data& timer)
  {
    // Remove the timer from the heap.
    std::size_t index = timer.heap_index_;
    if (!heap_.empty() && index < heap_.size())
    {
      if (index == heap_.size() - 1)
      {
        heap_.pop_back();
      }
      else
      {
        swap_heap(index, heap_.size() - 1);
        heap_.pop_back();
        if (index > 0 && Time_Traits::less_than(
              heap_[index].time_, heap_[(index - 1) / 2].time_))
          up_heap(index);
        else
          down_heap(index);
      }
    }

    // Remove the timer from the linked list of active timers.
    //把timer从timers_队列中移除
    if (timers_ == &timer)
      timers_ = timer.next_;
    if (timer.prev_)
      timer.prev_->next_ = timer.next_;
    if (timer.next_)
      timer.next_->prev_= timer.prev_;
    timer.next_ = 0;
    timer.prev_ = 0;
  }

  // Determine if the specified absolute time is positive infinity.
  template <typename Time_Type>
  static bool is_positive_infinity(const Time_Type&)
  {
    return false;
  }

  // Determine if the specified absolute time is positive infinity.
  template <typename T, typename TimeSystem>
  static bool is_positive_infinity(
      const boost::date_time::base_time<T, TimeSystem>& time)
  {
    return time.is_pos_infinity();
  }

  // Helper function to convert a duration into milliseconds.
  //d转换为ms
  template <typename Duration>
  long to_msec(const Duration& d, long max_duration) const
  {
    if (d.ticks() <= 0)
      return 0;
    int64_t msec = d.total_milliseconds();
    if (msec == 0)
      return 1;
    if (msec > max_duration)
      return max_duration;
    return static_cast<long>(msec);
  }

  // Helper function to convert a duration into microseconds.
  //d转换为us
  template <typename Duration>
  long to_usec(const Duration& d, long max_duration) const
  {
    if (d.ticks() <= 0)
      return 0;
    int64_t usec = d.total_microseconds();
    if (usec == 0)
      return 1;
    if (usec > max_duration)
      return max_duration;
    return static_cast<long>(usec);
  }

  // The head of a linked list of all active timers.
  //所有的timer定时器都通过给链表链接在一起(双向链表)
  per_timer_data* timers_;  

  //每个timer有2个成员，一个记录时间，一个记录timer的私有数据
  struct heap_entry
  {
    // The time when the timer should fire.
    //time_traits.hpp中定义typedef boost::posix_time::ptime time_type;  
    //记录定时器某个时间点过期
    time_type time_; //也就是posix_time::ptimet  

    // The associated timer with enqueued operations.
    //记录本timer的一些私有数据
    per_timer_data* timer_;
  };

  // The heap of timers, with the earliest timer at the front.
  //每个timer对应一个heap_entry节点，方便更加heap_entry.heap_进入快速排序，目的是可以快速定位那个timer到期
  std::vector<heap_entry> heap_;
};

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_DETAIL_TIMER_QUEUE_HPP
