#include "../pch.h"

#include <ostream>
#include <sstream>

int main()
{

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

  while (true) {
    auto str = std::stringstream();
    std::unique_ptr<RdKafka::Message> msg;
    int64_t block_offset {};
    int64_t block_timestamp {};
    bool block_is_running { false };

    for (int i = 0; i < 200;) {
      msg = std::unique_ptr<RdKafka::Message> { consumer->consume(1000) };
      switch (msg->err()) {
      case RdKafka::ERR__TIMED_OUT:
        break;
      case RdKafka::ERR_NO_ERROR: {
        str << static_cast<const char*>(msg->payload()) << "\n";
        ++i;

        if (!block_is_running) {
          block_offset = msg->offset();

          RdKafka::MessageTimestamp ts = msg->timestamp();
          block_timestamp = ts.timestamp;

          block_is_running = true;

          spdlog::info("New block [{}] {}", block_timestamp, block_offset);
        }

        break;

        // spdlog::info("Message [{}] {} - {}", ts.timestamp, *message->key(), static_cast<const char*>(message->payload()));
      }
      default:
        spdlog::critical("Consume failed: {}", msg->errstr());
        return 0;
      }
    }

    {
      std::string filename { std::to_string(block_timestamp) + "_" + std::to_string(block_offset) + ".txt" };
      std::ofstream file { filename };
      if (file.is_open()) {
        file << str.rdbuf();
        file.close();
        consumer->commitSync(msg.get());
        str = std::stringstream();

        block_offset = 0;
        block_timestamp = 0;
        block_is_running = false;
      } else {
        spdlog::critical("Failed to create file {}", filename);
        return 1;
      }
    }
  }

  return 0;
}
