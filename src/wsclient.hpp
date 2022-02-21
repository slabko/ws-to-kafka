#include "pch.h"

#ifndef WSCLIENT_H_INCLUDED
#define WSCLIENT_H_INCLUDED

namespace slabko::wskafka {

using PlainSocket = boost::asio::ip::tcp::socket;
using SSLSocket = boost::beast::ssl_stream<boost::asio::ip::tcp::socket>;

template <class SocketType>
class WsClient final {

public:
  using CallbackType = std::function<void(const char*, size_t)>;

  WsClient(
    std::string host,
    std::string port,
    std::string path,
    std::string init_write);

  WsClient(WsClient& wsclient) = delete;
  WsClient(WsClient&& wsclient) = delete;

  ~WsClient() = default;

  WsClient<SocketType>& operator=(WsClient<SocketType>) = delete;
  WsClient<SocketType>& operator=(WsClient<SocketType>&&) = delete;

  void SetCallback(CallbackType callback);
  void Start();
  void Shutdown();

private:
  using resolver_result = boost::asio::ip::tcp::resolver::results_type;
  using time_point = std::chrono::steady_clock::time_point;
  using buffer_type = boost::beast::flat_buffer;

  using websocket_stream = boost::beast::websocket::stream<SocketType>;
  using control_frame = boost::beast::websocket::frame_type;
  using error_code = boost::beast::error_code;
  using io_context = boost::asio::io_context;
  using deadline_timer = boost::asio::deadline_timer;

  io_context ioc_;

  // `websocket::stream` doesn't support move semantics,
  // which we use to replace our `ws_` on every restart
  std::unique_ptr<websocket_stream> ws_;

  std::string host_;
  std::string port_;
  std::string path_;
  std::string init_write_;

  buffer_type buffer_;

  bool keep_running_;

  CallbackType callback_;

  void CloseWs();

  void DoRead();
  void OnRead(error_code ec, std::size_t n_bytes);
};

}

#endif
