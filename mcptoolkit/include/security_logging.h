#pragma once

#include <string>
#include <chrono>

namespace mcptoolkit {

    // Security event categories
    enum class SecurityEventCategory {
        PARSE_ERROR,
        VALIDATION_ERROR,
        DISPATCH_ERROR,
        TIMEOUT,
        SESSION_CLOSED
    };

    // Log a security event to stderr with timestamp
    void log_security_event(SecurityEventCategory category,
                           const std::string& detail,
                           const std::string& context = "");

    // Internal helper to format timestamp
    std::string get_timestamp();

} // namespace mcptoolkit
