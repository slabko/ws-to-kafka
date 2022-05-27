#include "pch.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <thread>
#include <utility>

#include "interval_timer.hpp"
#include "kafka_producer.hpp"
#include "uri.hpp"
#include "wsclient.hpp"

using slabko::wskafka::IntervalTimer;
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
  const std::string& key,
  const std::vector<std::string>& interval_messages,
  std::chrono::seconds interval)
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
    kafka_producer.Publish(payload, size, key);
  });

  auto kafka_job = std::async([&kafka_producer]() { kafka_producer.Start(); });

  auto book_snapshot_timer = IntervalTimer(std::chrono::seconds(interval), [&client, &interval_messages]() {
    for (const auto& message : interval_messages) {
      client.Write(message);
    }
  });

  auto book_snapshot_thread = std::thread([&book_snapshot_timer]() {
    book_snapshot_timer.Start();
  });

  client.Start();

  book_snapshot_timer.Stop();
  book_snapshot_thread.join();
}

int main()
{
  std::string uri_string;
  std::string boostrap_servers;
  std::string topic;
  std::string message;
  std::string key;
  int interval_seconds {};
  std::vector<std::string> interval_messages;

  std::ifstream config_file("config.json");
  if (config_file.is_open()) {
    nlohmann::json j;
    config_file >> j;

    uri_string = j["url"].get<std::string>();
    boostrap_servers = j["brokers"].get<std::string>();
    topic = j["topic"].get<std::string>();
    key = j["key"].get<std::string>();
    message = j["message"].dump();
    interval_seconds = j["interval_messages"]["interval"].get<int>();

    for (auto& message : j["interval_messages"]["messages"]) {
      interval_messages.emplace_back(message.dump());
    }

    config_file.close();
  }

  auto interval = std::chrono::seconds(interval_seconds);

  auto uri = Uri::Parse(uri_string);

  if (uri.Protocol == "wss" || uri.Protocol == "https") {
    Run<SSLSocket>(uri, message, boostrap_servers, topic, key, interval_messages, interval);
  } else {
    Run<PlainSocket>(uri, message, boostrap_servers, topic, key, interval_messages, interval);
  }

  return 0;
}
