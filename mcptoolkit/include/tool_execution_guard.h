#pragma once

#include <string>
#include <chrono>
#include "timeout_manager.h"
#include "security_logging.h"

namespace mcptoolkit {

// Guard for safe tool execution with timeout and audit logging
class ToolExecutionGuard {
public:
    struct ExecutionConfig {
        std::chrono::milliseconds timeout = std::chrono::seconds(30);
        bool log_invocation = true;
        bool log_result = true;
        size_t max_output_size = 1024 * 1024;  // 1 MB
    };

    ToolExecutionGuard(const std::string& tool_name,
                      const std::string& user_id,
                      const ExecutionConfig& config = ExecutionConfig());

    ~ToolExecutionGuard();

    // Check if execution has exceeded timeout
    bool is_timeout() const;

    // Log tool invocation start (called by guard constructor)
    void log_start() const;

    // Log tool completion with result
    void log_completion(bool success, const std::string& result) const;

    // Validate output size and encoding before returning to LLM
    static bool validate_output(const std::string& output,
                               size_t max_size,
                               std::string& error_msg);

    // Get remaining time before timeout
    std::chrono::milliseconds get_remaining_time() const;

private:
    std::string tool_name_;
    std::string user_id_;
    ExecutionConfig config_;
    TimeoutGuard timeout_guard_;
    std::chrono::steady_clock::time_point start_time_;
};

}  // namespace mcptoolkit
