#include "my-beast.hpp"

namespace my {

std::shared_ptr<ws_stream> make_ws_ptr(tcp::socket &&sock) {
  auto ws = std::make_shared<ws_stream>(std::move(sock));
  ws->set_option(ws::stream_base::timeout::suggested(beast::role_type::server));
  return ws;
}

void async_ws_accept(ws_stream &ws, const string_request &req,
                     const std::function<void(beast::error_code)> &cb) {
  ws.async_accept(req, cb);
}

void async_ws_read(
    ws_stream &ws, beast::flat_buffer &buf,
    const std::function<void(beast::error_code, std::size_t)> &cb) {
  ws.async_read(buf, cb);
}

void async_ws_write(
    ws_stream &ws, const asio::const_buffer &buf,
    const std::function<void(beast::error_code, std::size_t)> &cb) {
  ws.async_write(buf, cb);
}

} // namespace my
