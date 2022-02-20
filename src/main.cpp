#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <chrono>
#include <iostream>
#include <librdkafka/rdkafkacpp.h>
#include <spdlog/spdlog.h>

using namespace std::chrono_literals;

int main() {
  spdlog::info("starting");

  auto conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
  std::string errstr;

  if (conf->set("bootstrap.servers", "localhost:9092", errstr) !=
      RdKafka::Conf::CONF_OK) {
    throw std::runtime_error(errstr);
  }

  boost::asio::io_context ioc;

  boost::asio::steady_timer timer(ioc);
  timer.expires_from_now(1s);

  timer.async_wait([](boost::system::error_code ec) { spdlog::info("done"); });

  ioc.run();

  return 0;
}
