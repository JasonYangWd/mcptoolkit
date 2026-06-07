#include "../include/tool_execution_guard.h"
#include <sstream>

namespace mcptoolkit {

ToolExecutionGuard::ToolExecutionGuard(const std::string& tool_name,
                                       const std::string& user_id,
                                       const ExecutionConfig& config)
    : tool_name_(tool_name),
      user_id_(user_id),
      config_(config),
      timeout_guard_(0, std::chrono::steady_clock::now(), config.timeout),
      start_time_(std::chrono::steady_clock::now()) {
    if (config_.log_invocation) {
        log_start();
    }
}

ToolExecutionGuard::~ToolExecutionGuard() = default;

bool ToolExecutionGuard::is_timeout() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - start_time_);
    return elapsed > config_.timeout;
}

void ToolExecutionGuard::log_start() const {
    std::ostringstream oss;
    oss << "user=" << user_id_ << ",tool=" << tool_name_;

    log_security_event(SecurityEventCategory::DISPATCH_ERROR,
                      "Tool execution started: " + tool_name_,
                      oss.str());
}

void ToolExecutionGuard::log_completion(bool success,
                                       const std::string& result) const {
    if (!config_.log_result) {
        return;
    }

    std::ostringstream oss;
    oss << "user=" << user_id_ << ",tool=" << tool_name_
        << ",success=" << (success ? "true" : "false")
        << ",result_size=" << result.size();

    if (is_timeout()) {
        log_security_event(SecurityEventCategory::TIMEOUT,
                          "Tool execution timeout: " + tool_name_,
                          oss.str());
    } else {
        log_security_event(
            success ? SecurityEventCategory::DISPATCH_ERROR
                    : SecurityEventCategory::VALIDATION_ERROR,
            "Tool execution completed: " + tool_name_,
            oss.str());
    }
}

bool ToolExecutionGuard::validate_output(const std::string& output,
                                         size_t max_size,
                                         std::string& error_msg) {
    // Check size
    if (output.size() > max_size) {
        error_msg = "Tool output exceeds maximum size (" +
                   std::to_string(max_size) + " bytes)";
        return false;
    }

    // Check for control characters that could break JSON
    for (unsigned char c : output) {
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
            error_msg = "Tool output contains control characters";
            return false;
        }
    }

    return true;
}

std::chrono::milliseconds ToolExecutionGuard::get_remaining_time() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - start_time_);

    if (elapsed >= config_.timeout) {
        return std::chrono::milliseconds(0);
    }

    return config_.timeout - elapsed;
}

}  // namespace mcptoolkit
