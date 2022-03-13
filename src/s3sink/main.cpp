#include "../pch.h"

#include "message_store.hpp"
#include <ostream>
#include <sstream>
#include <aws/core/Aws.h>

using slabko::s3sink::MessageStore;

int main()
{
  Aws::SDKOptions options;
  options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Debug;
  Aws::InitAPI(options);

  auto conf = std::unique_ptr<RdKafka::Conf>(
    RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
  std::string errstr;

  if (conf->set("metadata.broker.list", "localhost:9092", errstr) != RdKafka::Conf::CONF_OK) {
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

  std::vector<std::string> topics { "panda-1" };
  RdKafka::ErrorCode err = consumer->subscribe(topics);
  if (err) {
    spdlog::critical("Failed to subscribe to {} topics: {}", topics.size(), RdKafka::err2str(err));
    return 1;
  }

  MessageStore message_store { 10 * 1025 * 1024 };
  while (true) {
    auto msg = std::unique_ptr<RdKafka::Message> { consumer->consume(1000) };
    switch (msg->err()) {
    case RdKafka::ERR__TIMED_OUT:
      break;
    case RdKafka::ERR_NO_ERROR: {
      auto ts = msg->timestamp().timestamp;
      if (!message_store.IsRunningCheckpoint()) {
        auto offset = msg->offset();
        std::string filename { std::to_string(ts) + "_" + std::to_string(offset) + ".txt" };
        message_store.BeginCheckpoint(filename);
      }

      using namespace std::string_literals;
      spdlog::info("Message with size {}", msg->len());
      message_store.Push(std::to_string(ts));
      message_store.Push(" "s);
      message_store.Push(static_cast<const char*>(msg->payload()), msg->len());
      message_store.Push("\n"s);
      spdlog::info("Store size is {}", message_store.Size());

      if (message_store.Size() > (1024 * 1024)) {
        spdlog::info("Commiting checkpoint");
        message_store.CommitCheckpoint();
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
