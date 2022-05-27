#include <atomic>
#include <chrono>
#include <condition_variable>

namespace slabko::wskafka {

template <typename Callback>
class IntervalTimer {
  Callback callback_;
  std::chrono::milliseconds interval_;
  std::condition_variable cvar_;
  std::mutex mutex_;
  std::atomic_bool enabled_;

public:
  explicit IntervalTimer(std::chrono::milliseconds interval, Callback callback)
    : callback_ { std::forward<Callback>(callback) }
    , interval_ { interval }
    , enabled_ { false }
  {
  }

  void Start()
  {
    if (!enabled_) {
      enabled_ = true;
      auto deadline = std::chrono::steady_clock::now() + interval_;
      std::unique_lock<std::mutex> lock(mutex_);
      while (enabled_) {
        if (cvar_.wait_until(lock, deadline) == std::cv_status::timeout) {
          lock.unlock();
          callback_();
          deadline += interval_;
          lock.lock();
        }
      }
    }
  }

  void Stop()
  {
    if (enabled_) {
      enabled_ = false;
      cvar_.notify_one();
    }
  }
};

}
