/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#ifndef MY_ASIO_HPP
#define MY_ASIO_HPP

#include "asio-pch.hpp"
#include "async_msgstream.hpp"

namespace my {
namespace asio = boost::asio;

typedef asio::posix::stream_descriptor stream_descriptor;

void async_readn(stream_descriptor &stream,
                 const boost::asio::mutable_buffer &buf, std::size_t n,
                 const std::function<void(std::error_code, std::size_t)> &cb);

void async_msgstream_send(stream_descriptor &stream,
                          const boost::asio::const_buffer &buf,
                          std::size_t msg_size,
                          const std::function<msgstream_handler> &handler);

void async_msgstream_recv(stream_descriptor &stream,
                          const boost::asio::mutable_buffer &buf,
                          const std::function<msgstream_handler> &handler);

} // namespace my

#endif
