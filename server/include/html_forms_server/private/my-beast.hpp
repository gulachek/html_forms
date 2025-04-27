/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#ifndef MY_BEAST_HPP
#define MY_BEAST_HPP

#include "asio-pch.hpp"

namespace my {
namespace asio = boost::asio;
namespace beast = boost::beast;
using tcp = asio::ip::tcp;
namespace http = beast::http;
namespace ws = beast::websocket;

using string_response = http::response<boost::beast::http::string_body>;
using string_request = http::request<boost::beast::http::string_body>;

using ws_stream = ws::stream<beast::tcp_stream>;

std::shared_ptr<ws_stream> make_ws_ptr(tcp::socket &&sock);

void async_ws_accept(ws_stream &ws, const string_request &req,
                     const std::function<void(beast::error_code)> &cb);

void async_ws_read(
    ws_stream &ws, beast::flat_buffer &buf,
    const std::function<void(beast::error_code, std::size_t)> &cb);

void async_ws_write(
    ws_stream &ws, const asio::const_buffer &buf,
    const std::function<void(beast::error_code, std::size_t)> &cb);

} // namespace my

#endif
