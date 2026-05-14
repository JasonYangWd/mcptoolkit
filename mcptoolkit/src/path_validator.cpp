#include "path_validator.h"
#include <algorithm>
#include <cctype>

namespace mcptoolkit {

PathValidator::PathValidator() {}

void PathValidator::configure(const PathValidationConfig& config) {
    config_ = config;
    configured_ = true;
}

bool PathValidator::is_safe_path(const std::string& user_path, std::string& error_msg) const {
    if (!configured_) {
        error_msg = "PathValidator not configured";
        return false;
    }

    // Check for null bytes (can truncate paths in C APIs)
    if (contains_null_byte(user_path)) {
        error_msg = "Path contains null byte";
        return false;
    }

    // Check for obvious traversal sequences
    if (contains_traversal_sequences(user_path)) {
        error_msg = "Path contains directory traversal sequences (..)";
        return false;
    }

    // Check for URL-encoded traversal sequences
    if (contains_encoded_traversal(user_path)) {
        error_msg = "Path contains URL-encoded traversal sequences";
        return false;
    }

    // Reject absolute paths if not allowed
    if (!config_.allow_absolute_paths) {
        if (!user_path.empty() && (user_path[0] == '/' || user_path[0] == '\\')) {
            error_msg = "Absolute paths are not allowed";
            return false;
        }
        // Check for Windows drive letters (C:\, D:\, etc.)
        if (user_path.size() >= 2 && std::isalpha(user_path[0]) && user_path[1] == ':') {
            error_msg = "Absolute paths are not allowed";
            return false;
        }
    }

    try {
        // Get canonical path from base directory
        std::filesystem::path base_dir(config_.base_directory);
        std::filesystem::path user_filesystem_path(user_path);
        std::filesystem::path combined = base_dir / user_filesystem_path;

        // Use weakly_canonical to resolve .. and symlinks
        // weakly_canonical doesn't fail if path doesn't exist yet (unlike canonical)
        std::filesystem::path canonical = std::filesystem::weakly_canonical(combined);

        // Ensure canonical path is within base directory
        if (!validate_within_base(canonical)) {
            error_msg = "Path escapes base directory";
            return false;
        }

        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        error_msg = std::string("Filesystem error: ") + e.what();
        return false;
    }
}

bool PathValidator::validate_within_base(const std::filesystem::path& canonical_path) const {
    std::filesystem::path base_dir(config_.base_directory);
    std::filesystem::path canonical_base = std::filesystem::weakly_canonical(base_dir);

    // Get string representations for comparison
    std::string canonical_str = canonical_path.string();
    std::string base_str = canonical_base.string();

    // Ensure base directory ends with separator for clean comparison
    if (!base_str.empty() && base_str.back() != std::filesystem::path::preferred_separator) {
        base_str += std::filesystem::path::preferred_separator;
    }

    // Check if canonical path starts with base directory
    if (canonical_str == base_str || canonical_str.find(base_str) == 0) {
        return true;
    }

    return false;
}

bool PathValidator::contains_traversal_sequences(const std::string& path) {
    // Check for .. (parent directory)
    if (path.find("..") != std::string::npos) {
        return true;
    }

    // Check for Windows-style traversal (..\ sequences)
    if (path.find("..\\") != std::string::npos) {
        return true;
    }

    // Check for ~/ which expands to home directory
    if (path.find("~/") != std::string::npos) {
        return true;
    }

    return false;
}

bool PathValidator::contains_encoded_traversal(const std::string& path) {
    // Patterns to check for URL-encoded traversal
    static const char* encoded_patterns[] = {
        // URL-encoded .. -> %2e%2e
        "%2e%2e", "%2E%2E", "%2e%2E", "%2E%2e",
        // URL-encoded / -> %2f
        "%2f", "%2F",
        // URL-encoded \ -> %5c
        "%5c", "%5C",
        // Double-encoded .. -> %252e
        "%252e", "%252E",
        // Double-encoded / -> %252f
        "%252f", "%252F",
    };

    for (const auto& pattern : encoded_patterns) {
        if (path.find(pattern) != std::string::npos) {
            return true;
        }
    }

    // Also check for mixed case variations of common patterns
    // Check for %2e (dot) used in multiple places
    size_t pos = 0;
    int dot_count = 0;
    while ((pos = path.find("%2e", pos)) != std::string::npos || (pos = path.find("%2E", pos)) != std::string::npos) {
        dot_count++;
        pos++;
        // If we find multiple %2e patterns that could form .. when combined
        if (dot_count >= 2) {
            return true;
        }
    }

    return false;
}

bool PathValidator::contains_null_byte(const std::string& path) {
    return path.find('\0') != std::string::npos;
}

} // namespace mcptoolkit
