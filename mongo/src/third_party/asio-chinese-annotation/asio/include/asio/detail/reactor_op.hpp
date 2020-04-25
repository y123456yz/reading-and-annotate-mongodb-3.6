//
// detail/reactor_op.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_REACTOR_OP_HPP
#define ASIO_DETAIL_REACTOR_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"
#include "asio/detail/operation.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {
namespace detail {

//也就是op回调  参考reactive_socket_service::async_accept->start_accept_op  epoll_reactor::register_internal_descriptor
//reactive_socket_move_accept_op::reactive_socket_accept_op_base::reactor_op继承关系
/*
Descriptor_read_op.hpp (include\asio\detail):class descriptor_read_op_base : public reactor_op
Descriptor_write_op.hpp (include\asio\detail):class descriptor_write_op_base : public reactor_op
Reactive_null_buffers_op.hpp (include\asio\detail):class reactive_null_buffers_op : public reactor_op
Reactive_socket_accept_op.hpp (include\asio\detail):class reactive_socket_accept_op_base : public reactor_op
Reactive_socket_connect_op.hpp (include\asio\detail):class reactive_socket_connect_op_base : public reactor_op
Reactive_socket_recvfrom_op.hpp (include\asio\detail):class reactive_socket_recvfrom_op_base : public reactor_op
Reactive_socket_recvmsg_op.hpp (include\asio\detail):class reactive_socket_recvmsg_op_base : public reactor_op
Reactive_socket_recv_op.hpp (include\asio\detail):class reactive_socket_recv_op_base : public reactor_op
Reactive_socket_sendto_op.hpp (include\asio\detail):class reactive_socket_sendto_op_base : public reactor_op
Reactive_socket_send_op.hpp (include\asio\detail):class reactive_socket_send_op_base : public reactor_op
Reactive_wait_op.hpp (include\asio\detail):class reactive_wait_op : public reactor_op
Signal_set_service.ipp (include\asio\detail\impl):class signal_set_service::pipe_read_op : public reactor_op
Signal_set_service.ipp (src\asio-srccode-yyzadd\detail\impl):class signal_set_service::pipe_read_op : public reactor_op
*/ 

// reactor_op(网络IO事件处理任务)  completion_handler(全局任务)继承该类 descriptor_state(reactor_op对应的网络IO事件任务最终加入到该结构中由epoll触发处理) 

//mongodb使用了reactive_socket_accept_op_base   reactive_socket_recv_op_base reactive_socket_send_op_base
//reactive_socket_accept_op_base继承该类,accept对应的op为reactive_socket_accept_op_base   

////网络IO相关真正执行见epoll_reactor::descriptor_state::do_complete，然后调用reactor_op类的相应接口
class reactor_op  //reactor_op对应的网络事件回调注册见epoll_reactor::start_op
//descriptor_state.op_queue_[]队列是该类型，真正执行通过epoll_wait后和获取descriptor_state : operation回调信息    
  : public operation //也就是scheduler_operation
{
public:
  // The error code to be passed to the completion handler.
  asio::error_code ec_;

  // The number of bytes transferred, to be passed to the completion handler.
  //do_perform进行底层fd数据读写的字节数
  std::size_t bytes_transferred_;

  // Status returned by perform function. May be used to decide whether it is
  // worth performing more operations on the descriptor immediately.
  //读取数据或者写数据
  enum status { not_done, done, done_and_exhausted };

  // Perform the operation. Returns true if it is finished.
  status perform() //执行perform_func_
  { //也就是reactive_socket_accept_op_base  reactive_socket_recv_op_base  
  //reactive_socket_send_op_base对应的do_perform

  //epoll_reactor::descriptor_state::perform_io中调度执行
    return perform_func_(this);
  }

protected:
  typedef status (*perform_func_type)(reactor_op*);
  //例如accept操作过程回调过程，一个执行accept操作，一个执行后续的complete_func操作(mongodb中的task)
  //例如recvmsg操作过程，一个执行reactive_socket_recv_op_base::do_perform(最终recvmsg)，一个执行后续complete_func操作(mongodb中的task)
  reactor_op(perform_func_type perform_func, func_type complete_func)
  //例如接受数据的complete_func为reactive_socket_recv_op::do_complete
    : operation(complete_func),  //complete_func赋值给operation，在operation中执行
      bytes_transferred_(0),
      perform_func_(perform_func)
  {
  }

private:
  //perform_func也就是fd数据收发底层实现，赋值给reactor_op.perform_func_, complete_func赋值给父类operation的func,见reactor_op构造函数
  perform_func_type perform_func_;
};

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_DETAIL_REACTOR_OP_HPP
