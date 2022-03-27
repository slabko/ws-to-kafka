#ifndef KAFKA_PRODUCER_H_INCLUDED
#define KAFKA_PRODUCER_H_INCLUDED

#include "../pch.h"
#include <atomic>

namespace slabko::wskafka {

class KafkaProducer final : private RdKafka::DeliveryReportCb {
public:
  KafkaProducer(std::string bootstrap_servers, std::string topic);

  void Publish(const char* payload, size_t size, const std::string& key);
  void Start();
  void Shutdown();

private:
  void dr_cb(RdKafka::Message& message) override;

  std::string bootstrap_servers_;
  std::string topic_;
  std::unique_ptr<RdKafka::Producer> producer_;
  std::atomic<bool> keep_running_ { true };
};

} // namespace slabko::wskafka

#endif
