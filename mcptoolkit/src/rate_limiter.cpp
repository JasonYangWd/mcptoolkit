#include "rate_limiter.h"

namespace mcptoolkit {

RateLimiter::RateLimiter()
    : default_refill_rate_(100.0), default_max_tokens_(10.0) {}

void RateLimiter::configure(double rps, size_t burst) {
  std::lock_guard<std::mutex> lock(lock_);
  default_refill_rate_ = rps;
  default_max_tokens_ = static_cast<double>(burst);
}

bool RateLimiter::allow_request(const std::string& client_id) {
  std::lock_guard<std::mutex> lock(lock_);

  auto now = std::chrono::steady_clock::now();

  // Get or create bucket for this client
  auto it = buckets_.find(client_id);
  if (it == buckets_.end()) {
    // First request from this client - create new bucket
    buckets_[client_id] =
        TokenBucket(default_refill_rate_, default_max_tokens_);
    it = buckets_.find(client_id);
  }

  TokenBucket& bucket = it->second;

  // Calculate elapsed time since last refill
  auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - bucket.last_refill);
  double elapsed_seconds = elapsed.count() / 1000.0;

  // Refill tokens based on elapsed time
  bucket.tokens += elapsed_seconds * bucket.refill_rate;
  if (bucket.tokens > bucket.max_tokens) {
    bucket.tokens = bucket.max_tokens;
  }
  bucket.last_refill = now;

  // Check if request is allowed
  if (bucket.tokens >= 1.0) {
    bucket.tokens -= 1.0;
    return true;
  }
  return false;
}

void RateLimiter::set_rate(const std::string& client_id, double rps) {
  std::lock_guard<std::mutex> lock(lock_);
  auto it = buckets_.find(client_id);
  if (it == buckets_.end()) {
    buckets_[client_id] =
        TokenBucket(rps, default_max_tokens_);
  } else {
    it->second.refill_rate = rps;
  }
}

void RateLimiter::set_burst(const std::string& client_id, size_t burst) {
  std::lock_guard<std::mutex> lock(lock_);
  auto it = buckets_.find(client_id);
  if (it == buckets_.end()) {
    double burst_d = static_cast<double>(burst);
    buckets_[client_id] =
        TokenBucket(default_refill_rate_, burst_d);
  } else {
    double burst_d = static_cast<double>(burst);
    it->second.max_tokens = burst_d;
    if (it->second.tokens > burst_d) {
      it->second.tokens = burst_d;
    }
  }
}

void RateLimiter::reset_client(const std::string& client_id) {
  std::lock_guard<std::mutex> lock(lock_);
  buckets_.erase(client_id);
}

double RateLimiter::get_tokens(const std::string& client_id) const {
  std::lock_guard<std::mutex> lock(lock_);
  auto it = buckets_.find(client_id);
  if (it != buckets_.end()) {
    return it->second.tokens;
  }
  return 0.0;
}

}  // namespace mcptoolkit
