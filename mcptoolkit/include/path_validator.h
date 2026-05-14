#pragma once

#include <string>
#include <filesystem>

namespace mcptoolkit {

// Configuration for path validation
struct PathValidationConfig {
    std::string base_directory;           // Allowed root directory (e.g., "/documents/")
    bool allow_absolute_paths = false;    // If false, reject absolute paths (e.g., /etc/passwd)
    bool allow_symlinks = false;          // If false, reject symlinks escaping base directory
};

// Validates file paths to prevent directory traversal attacks (CWE-22)
class PathValidator {
public:
    PathValidator();

    // Configure the validator with a base directory and policy
    void configure(const PathValidationConfig& config);

    // Check if a user-supplied path is safe within the configured base directory.
    // Returns true if path is safe, false otherwise.
    // error_msg contains details if validation fails.
    bool is_safe_path(const std::string& user_path, std::string& error_msg) const;

    // Static checks: can be used independently without configuration

    // Check for .. sequences and Windows-style .. sequences
    static bool contains_traversal_sequences(const std::string& path);

    // Check for URL-encoded traversal sequences (%2e, %2f, %5c)
    static bool contains_encoded_traversal(const std::string& path);

    // Check for null bytes that could truncate paths
    static bool contains_null_byte(const std::string& path);

private:
    PathValidationConfig config_;
    bool configured_ = false;

    // Helper to validate canonical path is within base directory
    bool validate_within_base(const std::filesystem::path& canonical_path) const;
};

} // namespace mcptoolkit
