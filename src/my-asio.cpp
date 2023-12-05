#include "my-asio.hpp"
#include "async_msgstream.hpp"

namespace my {

void async_msgstream_send(stream_descriptor &stream,
                          const boost::asio::const_buffer &buf,
                          std::size_t msg_size,
                          const std::function<msgstream_handler> &handler) {
  return ::async_msgstream_send(stream, buf, msg_size, handler);
}

void async_msgstream_recv(stream_descriptor &stream,
                          const boost::asio::mutable_buffer &buf,
                          const std::function<msgstream_handler> &handler) {
  return ::async_msgstream_recv(stream, buf, handler);
}

} // namespace my
