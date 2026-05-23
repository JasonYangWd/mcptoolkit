#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <chrono>
#include <mutex>

namespace mcptoolkit {

struct AuthConfig {
    size_t min_token_length = 16;         // Minimum token length (reject "admin", "Bearer admin")
    bool require_bearer_prefix = true;    // Enforce "Bearer <token>" format
    int max_failed_attempts = 10;         // Lock out after N consecutive failures
    std::chrono::seconds token_ttl{3600}; // Token lifetime (0 = no expiry check)
};

struct AuthResult {
    bool allowed = false;
    std::string error;
};

// Validates caller identity on every MCP request (CWE-287: Improper Authentication)
class AuthenticationHandler {
public:
    AuthenticationHandler();

    void configure(const AuthConfig& config);

    // Primary validation — call before dispatching any tool
    AuthResult validate_request(const std::string& auth_token) const;

    // Token lifecycle management
    void register_token(const std::string& token,
                        std::chrono::steady_clock::time_point issued_at
                            = std::chrono::steady_clock::now());
    void revoke_token(const std::string& token);
    bool is_revoked(const std::string& token) const;

    // Expiry check (returns false if TTL is 0 or token not registered)
    bool is_expired(const std::string& token) const;

    // Failed-attempt tracking
    void record_failure(const std::string& client_id);
    void reset_failures(const std::string& client_id);
    bool is_locked_out(const std::string& client_id) const;

private:
    AuthConfig config_;
    bool configured_ = false;

    mutable std::mutex mutex_;
    std::unordered_set<std::string> revoked_tokens_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> token_issue_times_;
    std::unordered_map<std::string, int> failed_attempts_;

    std::string extract_bearer(const std::string& auth_header) const;
    bool validate_format(const std::string& token, std::string& error) const;
};

} // namespace mcptoolkit
