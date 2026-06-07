#include "authentication_handler.h"
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
    #include <wincrypt.h>
    #pragma comment(lib, "bcrypt.lib")
    #pragma comment(lib, "crypt32.lib")
#else
    #include <openssl/hmac.h>
    #include <openssl/sha.h>
#endif

namespace mcptoolkit {

// Helper: Base64 encoding (simple implementation for signatures)
static std::string base64_encode(const unsigned char* data, size_t len) {
    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    for (size_t j = 0; j < len; ++j) {
        char_array_3[i++] = data[j];
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; ++i)
                result += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i > 0) {
        for (int j = i; j < 3; ++j)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (int j = 0; j <= i; ++j)
            result += base64_chars[char_array_4[j]];
        while (i++ < 3)
            result += '=';
    }

    return result;
}

// Helper: Base64 decoding
static bool base64_decode(const std::string& encoded, std::string& decoded) {
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    int in_len = encoded.size();
    int i = 0, j = 0, in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];

    decoded.clear();
    for (int n = 0; n < in_len; ++n) {
        if (encoded[n] == '=') break;

        size_t pos = base64_chars.find(encoded[n]);
        if (pos == std::string::npos) return false;  // Invalid character

        char_array_4[i++] = pos;
        if (i == 4) {
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = (((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2));
            char_array_3[2] = (((char_array_4[2] & 0x3) << 6) + char_array_4[3]);

            for (i = 0; i < 3; ++i)
                decoded += char_array_3[i];
            i = 0;
        }
    }

    if (i > 0) {
        for (int j = i; j < 4; ++j)
            char_array_4[j] = 0;

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = (((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2));
        char_array_3[2] = (((char_array_4[2] & 0x3) << 6) + char_array_4[3]);

        for (int j = 0; j < i - 1; ++j)
            decoded += char_array_3[j];
    }

    return true;
}

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

std::string AuthenticationHandler::decode_token(const std::string& auth_token) const {
    if (auth_token.empty() || token_secret_.empty()) {
        return "";  // Invalid: empty token or no secret configured
    }

    // Split token into parts: "user_id.signature"
    size_t dot_pos = auth_token.find('.');
    if (dot_pos == std::string::npos) {
        return "";  // Invalid format: no dot separator
    }

    std::string user_id_part = auth_token.substr(0, dot_pos);
    std::string signature_part = auth_token.substr(dot_pos + 1);

    // Validate user_id is not empty
    if (user_id_part.empty() || signature_part.empty()) {
        return "";  // Invalid: empty user_id or signature
    }

    // Verify signature matches
    if (!verify_token_signature(user_id_part, signature_part)) {
        return "";  // Signature verification failed
    }

    // Check if token is revoked
    if (is_revoked(auth_token)) {
        return "";  // Token has been revoked
    }

    // Check if token is expired
    if (config_.token_ttl.count() > 0 && is_expired(auth_token)) {
        return "";  // Token has expired
    }

    return user_id_part;  // Return validated user_id
}

void AuthenticationHandler::set_token_secret(const std::string& secret_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    token_secret_ = secret_key;
}

std::string AuthenticationHandler::compute_token_signature(const std::string& user_id) const {
    if (token_secret_.empty()) {
        return "";
    }

    // Compute HMAC-SHA256(user_id, secret_key)
    unsigned char digest[32];  // SHA256 produces 32 bytes
    unsigned int digest_len = sizeof(digest);

#ifdef _WIN32
    // Windows: Use CNG for HMAC
    BCRYPT_ALG_HANDLE algo_handle;
    BCRYPT_HASH_HANDLE hash_handle;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algo_handle, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG))) {
        return "";
    }

    NTSTATUS status = BCryptCreateHash(algo_handle, &hash_handle, NULL, 0,
                                      (PUCHAR)token_secret_.data(), (ULONG)token_secret_.size(), 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(algo_handle, 0);
        return "";
    }

    status = BCryptHashData(hash_handle, (PUCHAR)user_id.data(), (ULONG)user_id.size(), 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyHash(hash_handle);
        BCryptCloseAlgorithmProvider(algo_handle, 0);
        return "";
    }

    status = BCryptFinishHash(hash_handle, digest, digest_len, 0);
    BCryptDestroyHash(hash_handle);
    BCryptCloseAlgorithmProvider(algo_handle, 0);

    if (!BCRYPT_SUCCESS(status)) {
        return "";
    }
#else
    // Unix/Linux: Use OpenSSL
    HMAC(EVP_sha256(),
         token_secret_.data(), static_cast<int>(token_secret_.size()),
         reinterpret_cast<const unsigned char*>(user_id.data()), static_cast<int>(user_id.size()),
         digest, &digest_len);
#endif

    // Return base64-encoded signature
    return base64_encode(digest, digest_len);
}

bool AuthenticationHandler::verify_token_signature(const std::string& user_id, const std::string& signature) const {
    std::string computed_sig = compute_token_signature(user_id);
    if (computed_sig.empty()) {
        return false;
    }

    // Constant-time comparison to prevent timing attacks
    if (signature.size() != computed_sig.size()) {
        return false;
    }

    int result = 0;
    for (size_t i = 0; i < signature.size(); ++i) {
        result |= signature[i] ^ computed_sig[i];
    }

    return result == 0;
}

} // namespace mcptoolkit
