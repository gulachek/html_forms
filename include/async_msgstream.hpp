#ifndef ASYNC_MSGSTREAM_HPP
#define ASYNC_MSGSTREAM_HPP

#include "asio-pch.hpp"
#include <msgstream.h>
#include <system_error>

template <typename AsyncWriteStream> struct async_msgstream_send_impl {
  enum state { header, body, complete };

public:
  async_msgstream_send_impl(AsyncWriteStream &stream,
                            const boost::asio::const_buffer &buf,
                            std::size_t msg_size)
      : stream_{stream}, buf_{buf}, msg_size_{msg_size} {}

  template <typename Self>
  void operator()(Self &self, std::error_code error = {}, std::size_t n = 0) {
    if (state_ == header) {
      auto n = msgstream_encode_header(
          header_buf_.data(), header_buf_.size(), buf_.size(),
          static_cast<msgstream_size>(msg_size_), nullptr);

      if (n < 0) {
        // error code
      }

      state_ = body;
      boost::asio::async_write(stream_, boost::asio::buffer(header_buf_),
                               boost::asio::transfer_exactly(n),
                               std::move(self));
    } else if (state_ == body) {
      if (error) {
        self.complete(error.default_error_condition(), 0);
        return;
      }

      state_ = complete;
      boost::asio::async_write(stream_, buf_,
                               boost::asio::transfer_exactly(msg_size_),
                               std::move(self));
    } else {
      if (error) {
        self.complete(error.default_error_condition(), 0);
        return;
      }

      self.complete(std::error_condition{}, msg_size_);
    }
  }

private:
  AsyncWriteStream &stream_;
  std::array<std::uint8_t, MSGSTREAM_HEADER_BUF_SIZE> header_buf_;
  boost::asio::const_buffer buf_;
  std::size_t msg_size_;
  state state_ = header;
};

typedef void msgstream_handler(std::error_condition, msgstream_size);

template <typename AsyncWriteStream,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(msgstream_handler) SendToken>
auto async_msgstream_send(AsyncWriteStream &stream,
                          const boost::asio::const_buffer &buf,
                          std::size_t msg_size, SendToken &&token) {
  return boost::asio::async_compose<SendToken, msgstream_handler>(
      async_msgstream_send_impl<AsyncWriteStream>{stream, buf, msg_size}, token,
      stream);
}

#endif
