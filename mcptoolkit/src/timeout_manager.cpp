#include "timeout_manager.h"

namespace mcptoolkit {

TimeoutManager::TimeoutManager()
    : next_id_(0), default_timeout_(5000) {}

TimeoutGuard TimeoutManager::start_timeout(std::chrono::milliseconds duration) {
  std::lock_guard<std::mutex> lock(lock_);

  uint64_t id = next_id_++;
  auto now = std::chrono::steady_clock::now();

  return TimeoutGuard(id, now, duration);
}

bool TimeoutManager::is_timeout(const TimeoutGuard& guard) const {
  auto now = std::chrono::steady_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - guard.start_time);
  return elapsed >= guard.timeout_ms;
}

void TimeoutManager::set_default_timeout(std::chrono::milliseconds ms) {
  std::lock_guard<std::mutex> lock(lock_);
  default_timeout_ = ms;
}

std::chrono::milliseconds TimeoutManager::get_default_timeout() const {
  std::lock_guard<std::mutex> lock(lock_);
  return default_timeout_;
}

}  // namespace mcptoolkit
