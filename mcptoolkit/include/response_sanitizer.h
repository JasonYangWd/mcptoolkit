#pragma once

#include <string>

namespace mcptoolkit {

// Sanitize tool responses before returning to LLM
class ResponseSanitizer {
public:
    struct SanitizeConfig {
        size_t max_response_size = 1024 * 1024;  // 1 MB
        bool truncate_on_size_exceeded = true;
        bool redact_paths = true;
        bool escape_control_chars = true;
    };

    // Sanitize a tool response for safe return to LLM
    // Returns sanitized response; sets error_msg if sanitization failed
    static std::string sanitize(const std::string& response,
                               const SanitizeConfig& config,
                               std::string& error_msg);

    // Check if response contains suspicious patterns (prompt injection)
    static bool contains_injection_patterns(const std::string& response);

    // Redact file paths from error messages
    static std::string redact_paths(const std::string& text);

    // Escape control characters for JSON transmission
    static std::string escape_control_characters(const std::string& text);

private:
    // Common prompt injection patterns to detect
    static const char* INJECTION_PATTERNS[];
    static const int NUM_PATTERNS;
};

}  // namespace mcptoolkit
