#include "pch.h"
#include "kafka_producer.hpp"

namespace slabko::wskafka {

inline const int kFlushTimeoutMs = 1000;
inline const auto kPollSleep = std::chrono::milliseconds(50);

KafkaProducer::KafkaProducer(std::string bootstrap_servers, std::string topic)
  : bootstrap_servers_ { std::move(bootstrap_servers) }
  , topic_ { std::move(topic) }
{
  auto conf = std::unique_ptr<RdKafka::Conf>(
    RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
  std::string errstr;

  if (conf->set("bootstrap.servers", bootstrap_servers_, errstr) != RdKafka::Conf::CONF_OK) {
    spdlog::critical(errstr);
    throw std::runtime_error(errstr);
  }

  if (conf->set("dr_cb", this, errstr) != RdKafka::Conf::CONF_OK) {
    spdlog::critical(errstr);
    throw std::runtime_error(errstr);
  }

  RdKafka::Producer* producer = RdKafka::Producer::create(conf.get(), errstr);
  if (producer == nullptr) {
    spdlog::critical("failed to create producer: {}", errstr);
    throw std::runtime_error(errstr);
  }

  producer_ = std::unique_ptr<RdKafka::Producer>(producer);
}

void KafkaProducer::Publish(
  const char* payload,
  size_t size,
  const std::string& key)
{
  auto now = std::chrono::system_clock::now().time_since_epoch();
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

  SPDLOG_DEBUG("sending [{}] {}", timestamp, std::string(payload, size));

  RdKafka::ErrorCode err = producer_->produce(
    topic_, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY,
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    const_cast<char*>(payload), size,
    key.c_str(), key.length(),
    timestamp,
    nullptr);

  if (err != RdKafka::ERR_NO_ERROR) {
    spdlog::error("failed to produce to topic {}: {}", topic_, RdKafka::err2str(err));
  }
}

void KafkaProducer::Start()
{
  while (keep_running_.load()) {
    std::this_thread::sleep_for(kPollSleep);
    producer_->poll(0);
  }

  spdlog::info("flushing remaining messages");

  auto err = producer_->flush(kFlushTimeoutMs);

  if (err != RdKafka::ERR_NO_ERROR) {
    spdlog::error("failed to flush remaining messages to {}: {}", topic_, RdKafka::err2str(err));
  } else {
    spdlog::info("flushing remaining messages is finished successfully");
  }
}

void KafkaProducer::Shutdown() { keep_running_.store(false); }

void KafkaProducer::dr_cb(RdKafka::Message& message)
{
  if (message.err() != 0) {
    spdlog::error("message delivery failed: {}", message.errstr());
  }

  SPDLOG_DEBUG("message delivered to topic {} [{}] at offset {}",
    message.topic_name, message.partition(), message.offset());
}
}
