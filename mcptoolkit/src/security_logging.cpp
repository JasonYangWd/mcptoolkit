#include "../pch.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include "security_logging.h"

namespace mcptoolkit {

    std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    const char* category_to_string(SecurityEventCategory category) {
        switch (category) {
            case SecurityEventCategory::PARSE_ERROR:     return "parse_error";
            case SecurityEventCategory::VALIDATION_ERROR: return "validation_error";
            case SecurityEventCategory::DISPATCH_ERROR:   return "dispatch_error";
            case SecurityEventCategory::TIMEOUT:          return "timeout";
            case SecurityEventCategory::SESSION_CLOSED:   return "session_closed";
            default:                                       return "unknown";
        }
    }

    void log_security_event(SecurityEventCategory category,
                           const std::string& detail,
                           const std::string& context) {
        std::string timestamp = get_timestamp();
        std::cerr << "[" << timestamp << "] SECURITY | "
                  << category_to_string(category) << " | "
                  << detail;
        if (!context.empty()) {
            std::cerr << " | " << context;
        }
        std::cerr << '\n';
        std::cerr.flush();
    }

} // namespace mcptoolkit
