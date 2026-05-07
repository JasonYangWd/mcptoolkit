#pragma once

#include <string>
#include <map>
#include <mutex>
#include <cstdint>

namespace mcptoolkit {

struct ResourceMetrics {
  std::string tool_name;
  double cpu_time_ms;
  size_t memory_peak_kb;
  uint64_t request_count;
  uint64_t rejection_count;

  ResourceMetrics()
      : tool_name(""),
        cpu_time_ms(0.0),
        memory_peak_kb(0),
        request_count(0),
        rejection_count(0) {}

  ResourceMetrics(const std::string& name)
      : tool_name(name),
        cpu_time_ms(0.0),
        memory_peak_kb(0),
        request_count(0),
        rejection_count(0) {}
};

class ResourceMonitor {
 public:
  ResourceMonitor() = default;
  ~ResourceMonitor() = default;

  // Record a successful request for a tool
  void record_request(const std::string& tool_name);

  // Record a rejected request for a tool
  void record_rejection(const std::string& tool_name);

  // Record CPU time for a tool execution
  void record_cpu_time(const std::string& tool_name, double cpu_ms);

  // Record peak memory for a tool
  void record_memory(const std::string& tool_name, size_t memory_kb);

  // Get metrics for a specific tool
  ResourceMetrics get_metrics(const std::string& tool_name) const;

  // Reset metrics for a specific tool
  void reset_metrics(const std::string& tool_name);

  // Reset all metrics
  void reset_all();

 private:
  mutable std::mutex lock_;
  std::map<std::string, ResourceMetrics> metrics_;
};

}  // namespace mcptoolkit
