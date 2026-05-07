#pragma once

#include <string>
#include <map>
#include <chrono>
#include <mutex>
#include <memory>

namespace mcptoolkit {

struct TokenBucket {
  double tokens;
  std::chrono::steady_clock::time_point last_refill;
  double refill_rate;
  double max_tokens;

  TokenBucket() = default;
  TokenBucket(double rate, double burst)
      : tokens(burst),
        last_refill(std::chrono::steady_clock::now()),
        refill_rate(rate),
        max_tokens(burst) {}
};

class RateLimiter {
 public:
  RateLimiter();
  ~RateLimiter() = default;

  // Configure: requests_per_second, burst_size
  void configure(double rps, size_t burst);

  // Check if request allowed for user/client
  bool allow_request(const std::string& client_id);

  // Set rate for a specific client (requests per second)
  void set_rate(const std::string& client_id, double rps);

  // Set burst allowance for a specific client
  void set_burst(const std::string& client_id, size_t burst);

  // Reset/clear a client's rate limit state
  void reset_client(const std::string& client_id);

  // Get current token count for a client (for monitoring)
  double get_tokens(const std::string& client_id) const;

 private:
  mutable std::mutex lock_;
  std::map<std::string, TokenBucket> buckets_;
  double default_refill_rate_;  // tokens per second
  double default_max_tokens_;   // burst allowance
};

}  // namespace mcptoolkit
