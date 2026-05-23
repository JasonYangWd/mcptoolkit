#include "authentication_handler.h"

namespace mcptoolkit {

static constexpr const char* kBearerPrefix = "Bearer ";
static constexpr size_t kBearerPrefixLen = 7;

AuthenticationHandler::AuthenticationHandler() {}

void AuthenticationHandler::configure(const AuthConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    configured_ = true;
}

AuthResult AuthenticationHandler::validate_request(const std::string& auth_token) const {
    if (!configured_) {
        return {false, "AuthenticationHandler not configured"};
    }

    if (auth_token.empty()) {
        return {false, "Authentication required"};
    }

    std::string token = auth_token;
    if (config_.require_bearer_prefix) {
        token = extract_bearer(auth_token);
        if (token.empty()) {
            return {false, "Invalid authorization format: expected 'Bearer <token>'"};
        }
    }

    std::string format_error;
    if (!validate_format(token, format_error)) {
        return {false, format_error};
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (revoked_tokens_.count(token)) {
        return {false, "Token has been revoked"};
    }

    if (config_.token_ttl.count() > 0 && is_expired(token)) {
        return {false, "Token has expired"};
    }

    return {true, ""};
}

void AuthenticationHandler::register_token(const std::string& token,
                                           std::chrono::steady_clock::time_point issued_at) {
    std::lock_guard<std::mutex> lock(mutex_);
    token_issue_times_[token] = issued_at;
}

void AuthenticationHandler::revoke_token(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    revoked_tokens_.insert(token);
    token_issue_times_.erase(token);
}

bool AuthenticationHandler::is_revoked(const std::string& token) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return revoked_tokens_.count(token) > 0;
}

bool AuthenticationHandler::is_expired(const std::string& token) const {
    // Caller must hold mutex_
    auto it = token_issue_times_.find(token);
    if (it == token_issue_times_.end()) {
        return false; // Not registered — let validate_request decide
    }
    auto age = std::chrono::steady_clock::now() - it->second;
    return age > config_.token_ttl;
}

void AuthenticationHandler::record_failure(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    failed_attempts_[client_id]++;
}

void AuthenticationHandler::reset_failures(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    failed_attempts_.erase(client_id);
}

bool AuthenticationHandler::is_locked_out(const std::string& client_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = failed_attempts_.find(client_id);
    if (it == failed_attempts_.end()) return false;
    return it->second >= config_.max_failed_attempts;
}

std::string AuthenticationHandler::extract_bearer(const std::string& auth_header) const {
    if (auth_header.size() <= kBearerPrefixLen) return "";
    if (auth_header.substr(0, kBearerPrefixLen) != kBearerPrefix) return "";
    return auth_header.substr(kBearerPrefixLen);
}

bool AuthenticationHandler::validate_format(const std::string& token, std::string& error) const {
    if (token.size() < config_.min_token_length) {
        error = "Token too short (minimum " + std::to_string(config_.min_token_length) + " characters)";
        return false;
    }

    // Reject null bytes — can cause silent truncation in C string APIs
    if (token.find('\0') != std::string::npos) {
        error = "Token contains null byte";
        return false;
    }

    return true;
}

} // namespace mcptoolkit
