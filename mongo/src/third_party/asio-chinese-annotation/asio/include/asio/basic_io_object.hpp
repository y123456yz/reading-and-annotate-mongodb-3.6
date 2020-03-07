//
// basic_io_object.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_BASIC_IO_OBJECT_HPP
#define ASIO_BASIC_IO_OBJECT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"
#include "asio/io_context.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {

#if defined(ASIO_HAS_MOVE)
namespace detail
{
  // Type trait used to determine whether a service supports move.
  template <typename IoObjectService>
  class service_has_move
  {
  private:
    typedef IoObjectService service_type;
    typedef typename service_type::implementation_type implementation_type;

    template <typename T, typename U>
    static auto asio_service_has_move_eval(T* t, U* u)
      -> decltype(t->move_construct(*u, *u), char());
    static char (&asio_service_has_move_eval(...))[2];

  public:
    static const bool value =
      sizeof(asio_service_has_move_eval(
        static_cast<service_type*>(0),
        static_cast<implementation_type*>(0))) == 1;
  };
}
#endif // defined(ASIO_HAS_MOVE)

/// Base class for all I/O objects.
/**
 * @note All I/O objects are non-copyable. However, when using C++0x, certain
 * I/O objects do support move construction and move assignment.
 */
#if !defined(ASIO_HAS_MOVE) || defined(GENERATING_DOCUMENTATION)
template <typename IoObjectService>//对应reactive_socket_service
#else
//template <typename IoObjectService, bool Movable = detail::service_has_move<IoObjectService>::value> yang add
#endif

class basic_io_object //basic_socket_acceptor继承该类
{
public:
  /// The type of the service that will be used to provide I/O operations.
  typedef IoObjectService service_type;

  /// The underlying implementation type of I/O object.  reactive_socket_service::implementation_type
  //对应stream_protocol，见TransportLayerASIO::setup
  typedef typename service_type::implementation_type implementation_type;

#if !defined(ASIO_NO_DEPRECATED)
  /// (Deprecated: Use get_executor().) Get the io_context associated with the
  /// object.
  /**
   * This function may be used to obtain the io_context object that the I/O
   * object uses to dispatch handlers for asynchronous operations.
   *
   * @return A reference to the io_context object that the I/O object will use
   * to dispatch handlers. Ownership is not transferred to the caller.
   */
  asio::io_context& get_io_context()
  {
    return service_.get_io_context();
  }

  /// (Deprecated: Use get_executor().) Get the io_context associated with the
  /// object.
  /**
   * This function may be used to obtain the io_context object that the I/O
   * object uses to dispatch handlers for asynchronous operations.
   *
   * @return A reference to the io_context object that the I/O object will use
   * to dispatch handlers. Ownership is not transferred to the caller.
   */
  asio::io_context& get_io_service()
  {
    return service_.get_io_context();
  }
#endif // !defined(ASIO_NO_DEPRECATED)

  /// The type of the executor associated with the object.
  typedef asio::io_context::executor_type executor_type;

  /// Get the executor associated with the object.
  executor_type get_executor() ASIO_NOEXCEPT
  {
    return service_.get_io_context().get_executor();
  }

protected:
  /// Construct a basic_io_object.
  /**
   * Performs:
   * @code get_service().construct(get_implementation()); @endcode
   */
   //basic_socket_acceptor类构造的时候构造,io_context对应mongodb的_acceptorIOContext
  explicit basic_io_object(asio::io_context& io_context)
    : service_(asio::use_service<IoObjectService>(io_context))
  {
    service_.construct(implementation_);
  }

#if defined(GENERATING_DOCUMENTATION)
  /// Move-construct a basic_io_object.
  /**
   * Performs:
   * @code get_service().move_construct(
   *     get_implementation(), other.get_implementation()); @endcode
   *
   * @note Available only for services that support movability,
   */
  basic_io_object(basic_io_object&& other);

  /// Move-assign a basic_io_object.
  /**
   * Performs:
   * @code get_service().move_assign(get_implementation(),
   *     other.get_service(), other.get_implementation()); @endcode
   *
   * @note Available only for services that support movability,
   */
  basic_io_object& operator=(basic_io_object&& other);

  /// Perform a converting move-construction of a basic_io_object.
  template <typename IoObjectService1>
  basic_io_object(IoObjectService1& other_service,
      typename IoObjectService1::implementation_type& other_implementation);
#endif // defined(GENERATING_DOCUMENTATION)

  /// Protected destructor to prevent deletion through this type.
  /**
   * Performs:
   * @code get_service().destroy(get_implementation()); @endcode
   */
  ~basic_io_object()
  {
    service_.destroy(implementation_);
  }

  /// Get the service associated with the I/O object.
  //basic_socket_acceptor::async_accept中调用
  service_type& get_service()
  { //reactive_socket_service
    return service_;
  }

  /// Get the service associated with the I/O object.
  //basic_socket_acceptor::async_accept中调用
  const service_type& get_service() const
  {//mongodb对应类型reactive_socket_service
    return service_;
  }

  /// Get the underlying implementation of the I/O object.
  //获取网络IO得底层实现
  implementation_type& get_implementation()
  {
    return implementation_; //对应stream_protocol，见TransportLayerASIO::setup
  }

  /// Get the underlying implementation of the I/O object.
  const implementation_type& get_implementation() const
  {
    return implementation_; //对应stream_protocol，见TransportLayerASIO::setup
  }

private:
  basic_io_object(const basic_io_object&);
  basic_io_object& operator=(const basic_io_object&);

  // The service associated with the I/O object.
  
  service_type& service_; //对应reactive_socket_service

  /// The underlying implementation of the I/O object.
  //底层实现 //对应stream_protocol，见TransportLayerASIO::setup
  implementation_type implementation_;
};

#if defined(ASIO_HAS_MOVE)
// Specialisation for movable objects.

/*
从basic_io_object派生的类不少，他们分别负责一些具体的事务，例如basic_socket_acceptor可以作为一个
服务器进行侦听，提供了诸如bind(), listen()等接口；再如basic_socket类是对socket IO 操作的封装，
提供了receive(), async_receive(), read_some(), async_readsome(), write_some(), async_write_some()等接口
*/

//mongodb中构造使用见TransportLayerASIO::setup->basic_socket_acceptor构造函数，
//basic_socket_acceptor构造basic_io_object<ASIO_SVC_T>(io_context)  

template <typename IoObjectService> //mongodb这里的service_type对应define ASIO_SVC_T detail::reactive_socket_service<Protocol>
class basic_io_object<IoObjectService, true>
{
public:
  //mongodb这里的service_type对应define ASIO_SVC_T detail::reactive_socket_service<Protocol>
  typedef IoObjectService service_type;
  typedef typename service_type::implementation_type implementation_type;

#if !defined(ASIO_NO_DEPRECATED)
  asio::io_context& get_io_context()
  {
    return service_->get_io_context();
  }

  asio::io_context& get_io_service()
  {
    return service_->get_io_context();
  }
#endif // !defined(ASIO_NO_DEPRECATED)

  typedef asio::io_context::executor_type executor_type;

  executor_type get_executor() ASIO_NOEXCEPT
  {
    return service_->get_io_context().get_executor();
  }

protected:
	//mongodb中构造使用见TransportLayerASIO::setup->basic_socket_acceptor构造函数，
	//basic_socket_acceptor构造basic_io_object<ASIO_SVC_T>(io_context)  
  explicit basic_io_object(asio::io_context& io_context)
  	// //mongodb  IoObjectService对应reactive_socket_service
    : service_(&asio::use_service<IoObjectService>(io_context))
  { //reactive_socket_service
    service_->construct(implementation_); //io_context::construct
  }

  basic_io_object(basic_io_object&& other)
    : service_(&other.get_service())
  {
    service_->move_construct(implementation_, other.implementation_);
  }

  template <typename IoObjectService1>
  basic_io_object(IoObjectService1& other_service,
      typename IoObjectService1::implementation_type& other_implementation)
    : service_(&asio::use_service<IoObjectService>(
          other_service.get_io_context()))
  {
    service_->converting_move_construct(implementation_,
        other_service, other_implementation);
  }

  ~basic_io_object()
  {
    service_->destroy(implementation_);
  }

  basic_io_object& operator=(basic_io_object&& other)
  {
    service_->move_assign(implementation_,
        *other.service_, other.implementation_);
    service_ = other.service_;
    return *this;
  }

  service_type& get_service()
  {
    return *service_;
  }

  const service_type& get_service() const
  {
    return *service_;
  }

  implementation_type& get_implementation()
  {
    return implementation_;
  }

  const implementation_type& get_implementation() const
  {
    return implementation_;
  }

private:
  basic_io_object(const basic_io_object&);
  void operator=(const basic_io_object&);
  
  //mongodb中构造使用见TransportLayerASIO::setup，
  //basic_io_object构造basic_io_object<ASIO_SVC_T>(io_context)  
  //mongodb 对应reactive_socket_service
  IoObjectService* service_;
  //IO对象的底层实现  对应stream_protocol
  implementation_type implementation_;
};
#endif // defined(ASIO_HAS_MOVE)

} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_BASIC_IO_OBJECT_HPP
