#include "pch.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <thread>
#include <utility>

#include "kafka_producer.hpp"
#include "uri.hpp"
#include "wsclient.hpp"

using slabko::wskafka::KafkaProducer;
using slabko::wskafka::PlainSocket;
using slabko::wskafka::SSLSocket;
using slabko::wskafka::Uri;
using slabko::wskafka::WsClient;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::function<void()> shutdown_handler;
void SignalHandler(int /*signum*/) { shutdown_handler(); }

template <class Socket>
void Run(
  Uri uri,
  const std::string& init_write,
  const std::string& boostrap_servers,
  const std::string& topic,
  const std::string& key)
{
  spdlog::info("starting");

  std::signal(SIGINT, SignalHandler);

  auto client = WsClient<Socket, int>(uri.Host, uri.Port, uri.Path, init_write);

  KafkaProducer kafka_producer(boostrap_servers, topic);

  shutdown_handler = [&]() {
    spdlog::info("shutting down");
    client.Shutdown();
    kafka_producer.Shutdown();
  };

  client.SetCallback([&key, &kafka_producer](const char* payload, size_t size) {
    // TODO: set proper name for the key
    kafka_producer.Publish(payload, size, key);
  });

  auto kafka_job = std::async([&kafka_producer]() { kafka_producer.Start(); });

  client.Start();
}

int main()
{
  std::string uri_string;
  std::string boostrap_servers;
  std::string topic;
  std::string message;
  std::string key;

  std::ifstream config_file("config.json");
  if (config_file.is_open()) {
    nlohmann::json j;
    config_file >> j;

    uri_string = j["url"].get<std::string>();
    boostrap_servers = j["brokers"].get<std::string>();
    topic = j["topic"].get<std::string>();
    key = j["key"].get<std::string>();
    message = j["message"].dump();

    config_file.close();
  }

  auto uri = Uri::Parse(uri_string);

  if (uri.Protocol == "wss" || uri.Protocol == "https") {
    Run<SSLSocket>(uri, message, boostrap_servers, topic, key);
  } else {
    Run<PlainSocket>(uri, message, boostrap_servers, topic, key);
  }

  return 0;
}
