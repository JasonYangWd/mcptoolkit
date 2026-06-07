#include "../include/response_sanitizer.h"
#include <algorithm>
#include <cctype>

namespace mcptoolkit {

// Common prompt injection patterns to detect
const char* ResponseSanitizer::INJECTION_PATTERNS[] = {
    "ignore previous",
    "forget about",
    "disregard",
    "override",
    "bypass",
    "instead do this",
    "now you are",
    "you are now"
};

const int ResponseSanitizer::NUM_PATTERNS = 8;

std::string ResponseSanitizer::sanitize(const std::string& response,
                                        const SanitizeConfig& config,
                                        std::string& error_msg) {
    std::string result = response;

    // Step 1: Enforce size limit
    if (result.size() > config.max_response_size) {
        if (config.truncate_on_size_exceeded) {
            result = result.substr(0, config.max_response_size);
            result += "\n[OUTPUT TRUNCATED]";
        } else {
            error_msg = "Tool response exceeds maximum size";
            return "";
        }
    }

    // Step 2: Check for injection patterns
    if (contains_injection_patterns(result)) {
        error_msg = "Tool response contains suspicious patterns";
        return "";
    }

    // Step 3: Redact paths if configured
    if (config.redact_paths) {
        result = redact_paths(result);
    }

    // Step 4: Escape control characters if configured
    if (config.escape_control_chars) {
        result = escape_control_characters(result);
    }

    return result;
}

bool ResponseSanitizer::contains_injection_patterns(
    const std::string& response) {
    std::string lower_response = response;
    std::transform(lower_response.begin(), lower_response.end(),
                  lower_response.begin(),
                  [](unsigned char c) { return std::tolower(c); });

    for (int i = 0; i < NUM_PATTERNS; ++i) {
        if (lower_response.find(INJECTION_PATTERNS[i]) !=
            std::string::npos) {
            return true;
        }
    }

    return false;
}

std::string ResponseSanitizer::redact_paths(const std::string& text) {
    std::string result = text;

    // Redact absolute paths (e.g., /etc/passwd → /.../)
    // Also redact Windows paths (C:\...\file → \...\)
    // This is a simple heuristic; production would use more sophisticated
    // pattern matching

    size_t pos = 0;
    while ((pos = result.find('/', pos)) != std::string::npos) {
        // Found start of path; redact until next space or end
        size_t end = result.find(' ', pos);
        if (end == std::string::npos) {
            end = result.length();
        }

        // Only redact if it looks like a full path (starts with /)
        if (pos == 0 || std::isspace(result[pos - 1])) {
            std::string path = result.substr(pos, end - pos);

            // Count slashes to detect path-like strings
            int slash_count = std::count(path.begin(), path.end(), '/');
            if (slash_count >= 2) {  // At least /foo/bar
                result.replace(pos, end - pos, "/[PATH]");
                pos += 7;  // Length of "/[PATH]"
                continue;
            }
        }

        pos++;
    }

    return result;
}

std::string ResponseSanitizer::escape_control_characters(
    const std::string& text) {
    std::string result;
    result.reserve(text.length());

    for (unsigned char c : text) {
        switch (c) {
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            case '\b':
                result += "\\b";
                break;
            case '\f':
                result += "\\f";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            default:
                if (c < 0x20) {
                    // Other control characters as unicode escapes
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    result += buf;
                } else {
                    result += c;
                }
                break;
        }
    }

    return result;
}

}  // namespace mcptoolkit
