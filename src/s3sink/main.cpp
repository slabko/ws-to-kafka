#include "../pch.h"

#include <ostream>
#include <sstream>

#include <aws/core/Aws.h>
#include <nlohmann/json.hpp>

#include "message_store.hpp"

const unsigned int kFileSize = 10 * 1025 * 1024;
const unsigned int kBufferSize = 11 * 1024 * 1024;
const unsigned int kPullTimeoutMs = 1000;

using slabko::s3sink::MessageStore;

int main()
{
  std::string topic;
  std::string boostrap_servers;

  std::ifstream config_file("config.json");
  if (config_file.is_open()) {
    nlohmann::json j;
    config_file >> j;
    topic = j["topic"].get<std::string>();
    boostrap_servers = j["brokers"].get<std::string>();
    config_file.close();
  }

  Aws::SDKOptions options;
  options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Debug;
  Aws::InitAPI(options);

  auto conf = std::unique_ptr<RdKafka::Conf>(
    RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
  std::string errstr;

  if (conf->set("metadata.broker.list", boostrap_servers, errstr) != RdKafka::Conf::CONF_OK) {
    spdlog::critical(errstr);
    return 0;
  }

  if (conf->set("group.id", "group-1", errstr) != RdKafka::Conf::CONF_OK) {
    spdlog::critical(errstr);
    return 0;
  }

  if (conf->set("enable.auto.commit", "false", errstr) != RdKafka::Conf::CONF_OK) {
    spdlog::critical(errstr);
    return 0;
  }

  if (conf->set("auto.offset.reset", "earliest", errstr) != RdKafka::Conf::CONF_OK) {
    spdlog::critical(errstr);
    return 0;
  }

  RdKafka::KafkaConsumer* consumer = RdKafka::KafkaConsumer::create(conf.get(), errstr);
  if (!consumer) {
    spdlog::critical("Failed to create consumer: {}", errstr);
    return 1;
  }

  std::vector<std::string> topics { topic };
  RdKafka::ErrorCode err = consumer->subscribe(topics);
  if (err) {
    spdlog::critical("Failed to subscribe to {} topics: {}", topics.size(), RdKafka::err2str(err));
    return 1;
  }

  MessageStore message_store { kBufferSize };
  while (true) {
    auto msg = std::unique_ptr<RdKafka::Message> { consumer->consume(kPullTimeoutMs) };
    switch (msg->err()) {
    case RdKafka::ERR__TIMED_OUT:
      break;
    case RdKafka::ERR_NO_ERROR: {
      auto ts = msg->timestamp().timestamp;
      if (!message_store.IsRunningCheckpoint()) {
        auto offset = msg->offset();
        std::string filename { topic + "_" + std::to_string(ts) + "_" + std::to_string(offset) + ".txt" };
        message_store.BeginCheckpoint(filename);
      }

      using namespace std::string_literals;
      message_store.Push(std::to_string(ts));
      message_store.Push(" "s);
      message_store.Push(static_cast<const char*>(msg->payload()), static_cast<unsigned int>(msg->len()));
      message_store.Push("\n"s);

      if (message_store.CompressedSize() > (kFileSize)) {
        spdlog::info("Commiting checkpoint");
        bool success = message_store.CommitCheckpoint();
        if (!success) {
          spdlog::critical("Failed to upload checkpoint");
          return 0;
        }
        consumer->commitSync(msg.get());
      }

      break;
    }
    default:
      spdlog::critical("Consume failed: {}", msg->errstr());
      return 0;
    }
  }

  Aws::ShutdownAPI(options);

  return 0;
}
