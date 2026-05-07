#pragma once

#include <chrono>
#include <cstdint>
#include <atomic>
#include <map>
#include <mutex>

namespace mcptoolkit {

struct TimeoutGuard {
  uint64_t timeout_id;
  std::chrono::steady_clock::time_point start_time;
  std::chrono::milliseconds timeout_ms;

  TimeoutGuard()
      : timeout_id(0),
        start_time(std::chrono::steady_clock::now()),
        timeout_ms(0) {}

  TimeoutGuard(uint64_t id, std::chrono::steady_clock::time_point start,
               std::chrono::milliseconds duration)
      : timeout_id(id), start_time(start), timeout_ms(duration) {}
};

class TimeoutManager {
 public:
  TimeoutManager();
  ~TimeoutManager() = default;

  // Start a timeout for the given duration
  TimeoutGuard start_timeout(std::chrono::milliseconds duration);

  // Check if a timeout has elapsed
  bool is_timeout(const TimeoutGuard& guard) const;

  // Set default timeout duration
  void set_default_timeout(std::chrono::milliseconds ms);

  // Get default timeout
  std::chrono::milliseconds get_default_timeout() const;

 private:
  mutable std::mutex lock_;
  std::atomic<uint64_t> next_id_;
  std::chrono::milliseconds default_timeout_;
};

}  // namespace mcptoolkit
