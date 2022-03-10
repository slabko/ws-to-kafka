#include "pch.h"
#include "wsclient.hpp"

namespace slabko::wskafka {

namespace ssl = net::ssl;

void SetSniHostname(SSL* ssl, const char* hostname)
{
  if (!SSL_set_tlsext_host_name(ssl, hostname)) {
    throw beast::system_error(beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()),
      "Failed to set SNI Hostname");
  }
}

template <>
PlainStreamPtr SetupWebsocket<PlainSocket>(
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
SSLStreamPtr SetupWebsocket<SSLSocket>(
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

}
