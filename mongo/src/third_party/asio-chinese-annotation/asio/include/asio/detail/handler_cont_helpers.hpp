//
// detail/handler_cont_helpers.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_HANDLER_CONT_HELPERS_HPP
#define ASIO_DETAIL_HANDLER_CONT_HELPERS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"
#include "asio/detail/memory.hpp"
#include "asio/handler_continuation_hook.hpp"

#include "asio/detail/push_options.hpp"


/*
Add a new handler hook called asio_handler_is_continuation.

Asynchronous operations may represent a continuation of the asynchronous
control flow associated with the current handler. Asio's implementation
can use this knowledge to optimise scheduling of the handler.

The asio_handler_is_continuation hook returns true to indicate whether a
completion handler represents a continuation of the current call
context. The default implementation of the hook returns false, and
applications may customise the hook when necessary. The hook has already
been customised within Asio to return true for the following cases:
//下列情况，返回true

- Handlers returned by strand.wrap(), when the corresponding
  asynchronous operation is being initiated from within the strand.

- The internal handlers used to implement the asio::spawn() function's
  stackful coroutines.

- When an intermediate handler of a composed operation (e.g.
  asio::async_read(), asio::async_write(), asio::async_connect(),
  ssl::stream<>, etc.) starts a new asynchronous operation due to the
  composed operation not being complete.    mongodb读写满足这个条件返回true

To support this optimisation, a new running_in_this_thread() member
function has been added to the io_service::strand class. This function
returns true when called from within a strand.

参考 https://github.com/chriskohlhoff/asio/commit/a80c14cdbbed9340bf00ced1329fcc1c935f7bdd
*/

// Calls to asio_handler_is_continuation must be made from a namespace that
// does not contain overloads of this function. This namespace is defined here
// for that purpose.
namespace asio_handler_cont_helpers {

//加is_continuation目的，例如可以保证一次读取read数据不能满足一个mongodb协议的时候，下次调度继续执行该op对应的读操作
template <typename Context> //mongodb  asio::async_read(), asio::async_write(), asio::async_connect(), 默认返回true
inline bool is_continuation(Context& context)
{
#if !defined(ASIO_HAS_HANDLER_HOOKS)
  return false;
#else
  using asio::asio_handler_is_continuation;
  return asio_handler_is_continuation(
      asio::detail::addressof(context));
#endif
}

} // namespace asio_handler_cont_helpers

#include "asio/detail/pop_options.hpp"

#endif // ASIO_DETAIL_HANDLER_CONT_HELPERS_HPP
