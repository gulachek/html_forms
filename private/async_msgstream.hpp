#ifndef ASYNC_MSGSTREAM_HPP
#define ASYNC_MSGSTREAM_HPP

#include "asio-pch.hpp"
#include <msgstream.h>
#include <system_error>

typedef void msgstream_handler(std::error_condition, size_t);

template <typename AsyncWriteStream> struct async_msgstream_send_impl {
  enum state { header, body, complete };
  using header_array_type = std::array<std::uint8_t, MSGSTREAM_HEADER_BUF_SIZE>;

public:
  async_msgstream_send_impl(AsyncWriteStream &stream,
                            const boost::asio::const_buffer &buf,
                            std::size_t msg_size)
      : stream_{stream}, buf_{buf}, msg_size_{msg_size},
        header_buf_{std::make_unique<header_array_type>()} {}

  template <typename Self>
  void operator()(Self &self, std::error_code error = {}, std::size_t n = 0) {
    if (state_ == header) {
      std::size_t hdr_size;
      int ec = msgstream_header_size(buf_.size(), &hdr_size);
      if (ec) {
        // TODO error
      }

      ec = msgstream_encode_header(msg_size_, hdr_size, header_buf_->data());

      if (ec) {
        // TODO error code
      }

      state_ = body;
      boost::asio::async_write(stream_, boost::asio::buffer(*header_buf_),
                               boost::asio::transfer_exactly(hdr_size),
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
  std::unique_ptr<header_array_type> header_buf_;
  boost::asio::const_buffer buf_;
  std::size_t msg_size_;
  state state_ = header;
};

template <typename AsyncWriteStream,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(msgstream_handler) SendToken>
auto async_msgstream_send(AsyncWriteStream &stream,
                          const boost::asio::const_buffer &buf,
                          std::size_t msg_size, SendToken &&token) {
  return boost::asio::async_compose<SendToken, msgstream_handler>(
      async_msgstream_send_impl<AsyncWriteStream>{stream, buf, msg_size}, token,
      stream);
}

template <typename AsyncReadStream> struct async_msgstream_recv_impl {
  enum state { header, body, complete };
  using header_array_type = std::array<std::uint8_t, MSGSTREAM_HEADER_BUF_SIZE>;

public:
  async_msgstream_recv_impl(AsyncReadStream &stream,
                            const boost::asio::mutable_buffer &buf)
      : stream_{stream}, buf_{buf},
        header_buf_{std::make_unique<header_array_type>()} {}

  template <typename Self>
  void operator()(Self &self, std::error_code error = {}, std::size_t n = 0) {
    if (state_ == header) {
      std::size_t hdr_size;
      int ec = msgstream_header_size(buf_.size(), &hdr_size);
      if (ec) {
        // TODO error code
      }

      state_ = body;
      boost::asio::async_read(stream_, boost::asio::buffer(*header_buf_),
                              boost::asio::transfer_exactly(hdr_size),
                              std::move(self));
    } else if (state_ == body) {
      if (error) {
        self.complete(error.default_error_condition(), 0);
        return;
      }

      std::size_t msg_size;
      int ec = msgstream_decode_header(header_buf_->data(), n, &msg_size);
      if (ec) {
        // TODO error code
      }

      state_ = complete;
      boost::asio::async_read(stream_, buf_,
                              boost::asio::transfer_exactly(msg_size),
                              std::move(self));
    } else {
      if (error) {
        self.complete(error.default_error_condition(), 0);
        return;
      }

      self.complete(std::error_condition{}, n);
    }
  }

private:
  AsyncReadStream &stream_;

  std::unique_ptr<header_array_type> header_buf_;
  boost::asio::mutable_buffer buf_;
  state state_ = header;
};

template <typename AsyncReadStream,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(msgstream_handler) RecvToken>
auto async_msgstream_recv(AsyncReadStream &stream,
                          const boost::asio::mutable_buffer &buf,
                          RecvToken &&token) {
  return boost::asio::async_compose<RecvToken, msgstream_handler>(
      async_msgstream_recv_impl<AsyncReadStream>{stream, buf}, token, stream);
}

#endif
