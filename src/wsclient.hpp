#include "pch.h"

#ifndef WSCLIENT_H_INCLUDED
#define WSCLIENT_H_INCLUDED

namespace slabko::wskafka {

using PlainSocket = boost::asio::ip::tcp::socket;
using SSLSocket = boost::beast::ssl_stream<boost::asio::ip::tcp::socket>;

/******************************************************************************************
       Assumption

    1. Everything is executed in a single thread
    2. `ioc_.run()` stops when the `ws_` is closed.
       i.e. when we close `ws_` it `ioc_` should run out of work
       and stop executing. After that we can safely replace `ws_` with
       a new instance and run `ioc_` again
    3. To make sure that `ioc_` runs out of work on closing the `ws_`,
       we cancel any other work (the ping timer) right when we receive
       a close event from reading `ws_`

  There are two chains are running in parallel: reding the socket and sending the pings.
  When reading stops, it must stop the second chain (sending pings), by canceling the timer.

*******************************************************************************************/

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
  using buffer_type = boost::beast::flat_buffer;

  using websocket_stream = boost::beast::websocket::stream<SocketType>;
  using error_code = boost::beast::error_code;
  using io_context = boost::asio::io_context;

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

namespace {
using namespace std::chrono_literals;

namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
namespace http = boost::beast::http;

using tcp = net::ip::tcp;
using plain_socket = tcp::socket;
using plain_stream_ptr = std::unique_ptr<websocket::stream<plain_socket>>;
using ssl_socket = beast::ssl_stream<tcp::socket>;
using ssl_stream_ptr = std::unique_ptr<websocket::stream<ssl_socket>>;

const auto kIdleTimeout = 10s;
const auto kHandshakeTimeout = 2s;
const auto kErrorDelay = 5s;

const char* const kUserAgent = "websocket-to-kafka-connector";

void SetSniHostname(SSL* ssl, const char* hostname)
{
  if (!SSL_set_tlsext_host_name(ssl, hostname)) {
    throw beast::system_error(beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()),
      "Failed to set SNI Hostname");
  }
}

template <class SocketType>
void SetupUserAgent(websocket::stream<SocketType>& ws)
{
  ws.set_option(websocket::stream_base::decorator(
    [](websocket::request_type& req) { req.set(http::field::user_agent, kUserAgent); }));
};

template <class SocketType>
std::unique_ptr<websocket::stream<SocketType>> SetupWebsocket(
  net::io_context& ioc, tcp::resolver::results_type const& records, const std::string& path);

template <>
plain_stream_ptr SetupWebsocket<plain_socket>(
  net::io_context& ioc, tcp::resolver::results_type const& records, const std::string& path)
{
  auto host = records->host_name();
  auto ws = std::make_unique<websocket::stream<tcp::socket>>(ioc);

  SetupUserAgent(*ws);

  auto endpoint = net::connect(beast::get_lowest_layer(*ws), records);
  std::string host_port = host + ":" + std::to_string(endpoint.port());
  ws->handshake(host_port, path);

  return ws;
}

template <>
ssl_stream_ptr SetupWebsocket<ssl_socket>(
  net::io_context& ioc, tcp::resolver::results_type const& records, const std::string& path)
{
  auto host = records->host_name();
  ssl::context ssl_context(net::ssl::context::tlsv12);
  ssl_context.set_default_verify_paths();
  ssl_context.set_verify_mode(ssl::verify_peer);
  ssl_context.set_verify_callback(ssl::host_name_verification(host));

  auto ws = std::make_unique<websocket::stream<beast::ssl_stream<tcp::socket>>>(ioc, ssl_context);

  SetSniHostname(ws->next_layer().native_handle(), host.c_str());

  SetupUserAgent(*ws);

  auto endpoint = net::connect(beast::get_lowest_layer(*ws), records);
  ws->next_layer().handshake(ssl::stream_base::client);
  std::string host_port = host + ":" + std::to_string(endpoint.port());
  ws->handshake(host_port, path);

  return ws;
}
} // anonymous namespace

template <class SocketType>
WsClient<SocketType>::WsClient(
  std::string host,
  std::string port,
  std::string path,
  std::string init_write)
  : host_ { std::move(host) }
  , port_ { std::move(port) }
  , path_ { std::move(path) }
  , init_write_ { std::move(init_write) }
  , keep_running_ { true }
{
}

template <class SocketType>
void WsClient<SocketType>::SetCallback(std::function<void(const char*, size_t)> callback)
{
  callback_ = std::move(callback);
}

template <class SocketType>
void WsClient<SocketType>::Start()
{
  while (keep_running_) {
    try {
      tcp::resolver resolver(ioc_);
      auto const lookup_result = resolver.resolve(host_, port_);

      ws_ = SetupWebsocket<SocketType>(ioc_, lookup_result, path_);

      websocket::stream_base::timeout options { kHandshakeTimeout, kIdleTimeout, true };
      ws_->set_option(options);

      ws_->write(net::buffer(init_write_));

      DoRead();

      if (ioc_.stopped()) {
        ioc_.restart();
      }

      if (!callback_) {
        spdlog::warn("callback for the websocket events is not set, nobody will consume the websocket output");
      }

      ioc_.run();

    } catch (const beast::system_error& e) {
      spdlog::error("lost connection: {}", e.what());
      if (keep_running_) {
        std::this_thread::sleep_for(kErrorDelay);
      }
    } catch (const std::exception& e) {
      spdlog::critical("unexpected error: {}", e.what());
      break; // Something went terribly wrong
    }
  }
}

template <class SocketType>
void WsClient<SocketType>::CloseWs()
{
  ioc_.post([this]() {
    if (ws_->is_open()) {
      ws_->async_close(websocket::close_code::normal,
        [](error_code ec) {
          spdlog::info("connection is closed with description {}", ec.message());
        });
    }
  });
}

template <class SocketType>
void WsClient<SocketType>::Shutdown()
{
  keep_running_ = false;
  CloseWs();
  callback_ = nullptr;
}

template <class SocketType>
void WsClient<SocketType>::DoRead()
{
  buffer_.clear();

  ws_->async_read(buffer_, [this](error_code ec, std::size_t n_bytes) {
    OnRead(ec, n_bytes);
  });
}

template <class SocketType>
void WsClient<SocketType>::OnRead(error_code ec, std::size_t /*n_bytes*/)
{
  if (ec && (ec == websocket::error::closed || ec == beast::errc::operation_canceled)) {
    return;
  }

  if (ec) {
    throw beast::system_error { ec };
  }

  SPDLOG_DEBUG(beast::buffers_to_string(buffer_));

  if (callback_) {
    callback_(net::buffer_cast<const char*>(buffer_.data()), buffer_.size());
  }

  // At the moment when I received the handler the socket might be already closed
  if (!ws_->is_open()) {
    return;
  }

  DoRead();
}
} // namespace slabko::wskafka

#endif
