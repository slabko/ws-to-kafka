#include "pch.h"

#include <utility>

#include "wsclient.hpp"



const int kPingIntervalSec = 10;
const char* const kUserAgent = "websocket-to-kafka-connector";

namespace slabko::wskafka {

namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
namespace chrono = std::chrono;

using tcp = net::ip::tcp;

using http_field = boost::beast::http::field;

void SetSniHostname(SSL* ssl, const char* hostname)
{
  if (!SSL_set_tlsext_host_name(ssl, hostname)) {
    throw beast::system_error(
      beast::error_code(static_cast<int>(::ERR_get_error()),
        net::error::get_ssl_category()),
      "Failed to set SNI Hostname");
  }
}

template <class SocketType>
void SetupUserAgent(websocket::stream<SocketType>& ws)
{
  ws.set_option(
    websocket::stream_base::decorator([](websocket::request_type& req) {
      req.set(http_field::user_agent, kUserAgent);
    }));
};

template <class SocketType>
std::unique_ptr<websocket::stream<SocketType>> SetupWebsocket(
  net::io_context& ioc,
  tcp::resolver::results_type const& records,
  const std::string& path);

using plain_socket = tcp::socket;
using plain_stream_ptr = std::unique_ptr<websocket::stream<plain_socket>>;

template <>
plain_stream_ptr SetupWebsocket<plain_socket>(
  net::io_context& ioc,
  tcp::resolver::results_type const& records,
  const std::string& path)
{
  auto host = records->host_name();
  auto ws = std::make_unique<websocket::stream<tcp::socket>>(ioc);

  SetupUserAgent(*ws);

  auto endpoint = net::connect(beast::get_lowest_layer(*ws), records);
  std::string host_port = host + ":" + std::to_string(endpoint.port());
  ws->handshake(host_port, path);

  return ws;
}

using ssl_socket = beast::ssl_stream<tcp::socket>;
using ssl_stream_ptr = std::unique_ptr<websocket::stream<ssl_socket>>;

template <>
ssl_stream_ptr SetupWebsocket<ssl_socket>(
  net::io_context& ioc,
  tcp::resolver::results_type const& records,
  const std::string& path)
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
  , ping_timer_ { ioc_ }
  , keep_running_ { true }
{
}

template <class SocketType>
void WsClient<SocketType>::SetCallback(std::function<void(const char*, size_t)> callback)
{
  callback_ = std::move(callback);
}

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
void WsClient<SocketType>::Start()
{
  while (keep_running_) {
    try {
      last_ping_ = chrono::steady_clock::time_point();
      last_pong_ = chrono::steady_clock::time_point();

      tcp::resolver resolver(ioc_);
      auto const lookup_result = resolver.resolve(host_, port_);

      ws_ = SetupWebsocket<SocketType>(ioc_, lookup_result, path_);

      ws_->control_callback([this](websocket::frame_type cf, beast::string_view) {
        OnControlFrame(cf);
      });

      ws_->write(net::buffer(init_write_));

      DoRead();
      DoPing();

      if (ioc_.stopped()) {
        ioc_.restart();
      }

      if (!callback_) {
        spdlog::warn("callback for the websocket events is not set, nobody will consume the websocket output");
      }

      ioc_.run();

    } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      // TODO: put delay on server error, such as "Connection refused"
    }
  }
}

template <class SocketType>
void WsClient<SocketType>::CloseWs()
{
  ioc_.post([this]() {
    // TODO: probably `async_close` is the way to go
    ws_->close(websocket::close_code::normal);
    ws_->async_close(websocket::close_code::normal, [](beast::error_code /*ec*/) {
      spdlog::info("connection is closed");
    });
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

  ws_->async_read(
    buffer_,
    [this](beast::error_code ec, std::size_t n_bytes) {
      OnRead(ec, n_bytes);
    });
}

template <class SocketType>
void WsClient<SocketType>::OnRead(beast::error_code ec, std::size_t /*n_bytes*/)
{
  if (ec && (ec == websocket::error::closed || ec == beast::errc::operation_canceled)) {
    ping_timer_.cancel();
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

template <class SocketType>
void WsClient<SocketType>::DoPing(beast::error_code ec)
{

  using std::chrono_literals::operator""s;
  if (ec && ec == beast::errc::operation_canceled) {
    return;
  }

  if (ec) {
    throw beast::system_error { ec };
  }

  if (last_pong_ - last_ping_ < 0s) {
    std::cout << "no pongs received for a while" << std::endl;
    CloseWs();
    return;
  }

  ws_->async_ping("", [this](beast::error_code ec) {
    OnPing(ec);
  });
}

template <class SocketType>
void WsClient<SocketType>::OnPing(beast::error_code ec)
{
  if (ec && ec == beast::errc::operation_canceled) {
    return;
  }

  if (ec) {
    throw beast::system_error { ec };
  }

  last_ping_ = chrono::steady_clock::now();

  ping_timer_.expires_from_now(boost::posix_time::seconds(kPingIntervalSec));

  ping_timer_.async_wait([this](beast::error_code ec) {
    DoPing(ec);
  });
}

template <class SocketType>
void WsClient<SocketType>::OnControlFrame(websocket::frame_type cf)
{
  if (cf == websocket::frame_type::pong) {
    auto timestamp = chrono::steady_clock::now();
    last_pong_ = timestamp;
  }
};

template class WsClient<tcp::socket>;
template class WsClient<beast::ssl_stream<tcp::socket>>;
}
