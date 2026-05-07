#include "resource_monitor.h"

namespace mcptoolkit {

void ResourceMonitor::record_request(const std::string& tool_name) {
  std::lock_guard<std::mutex> lock(lock_);
  auto it = metrics_.find(tool_name);
  if (it == metrics_.end()) {
    metrics_[tool_name] = ResourceMetrics(tool_name);
  }
  metrics_[tool_name].request_count++;
}

void ResourceMonitor::record_rejection(const std::string& tool_name) {
  std::lock_guard<std::mutex> lock(lock_);
  auto it = metrics_.find(tool_name);
  if (it == metrics_.end()) {
    metrics_[tool_name] = ResourceMetrics(tool_name);
  }
  metrics_[tool_name].rejection_count++;
}

void ResourceMonitor::record_cpu_time(const std::string& tool_name,
                                     double cpu_ms) {
  std::lock_guard<std::mutex> lock(lock_);
  auto it = metrics_.find(tool_name);
  if (it == metrics_.end()) {
    metrics_[tool_name] = ResourceMetrics(tool_name);
  }
  metrics_[tool_name].cpu_time_ms += cpu_ms;
}

void ResourceMonitor::record_memory(const std::string& tool_name,
                                    size_t memory_kb) {
  std::lock_guard<std::mutex> lock(lock_);
  auto it = metrics_.find(tool_name);
  if (it == metrics_.end()) {
    metrics_[tool_name] = ResourceMetrics(tool_name);
  }
  if (memory_kb > metrics_[tool_name].memory_peak_kb) {
    metrics_[tool_name].memory_peak_kb = memory_kb;
  }
}

ResourceMetrics ResourceMonitor::get_metrics(
    const std::string& tool_name) const {
  std::lock_guard<std::mutex> lock(lock_);
  auto it = metrics_.find(tool_name);
  if (it != metrics_.end()) {
    return it->second;
  }
  return ResourceMetrics(tool_name);
}

void ResourceMonitor::reset_metrics(const std::string& tool_name) {
  std::lock_guard<std::mutex> lock(lock_);
  auto it = metrics_.find(tool_name);
  if (it != metrics_.end()) {
    metrics_[tool_name] = ResourceMetrics(tool_name);
  }
}

void ResourceMonitor::reset_all() {
  std::lock_guard<std::mutex> lock(lock_);
  metrics_.clear();
}

}  // namespace mcptoolkit
