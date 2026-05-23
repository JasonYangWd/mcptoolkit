#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "authentication_handler.h"

using namespace mcptoolkit;

class AuthenticationTest : public ::testing::Test {
protected:
    AuthenticationHandler handler;

    void SetUp() override {
        AuthConfig config;
        config.min_token_length = 16;
        config.require_bearer_prefix = true;
        config.max_failed_attempts = 3;
        config.token_ttl = std::chrono::seconds{3600};
        handler.configure(config);
    }

    // 20-char token well above minimum
    const std::string valid_token = "Bearer abcdef1234567890ab";
    const std::string raw_token   = "abcdef1234567890ab";
};

// ============================================================================
// Vector 1: Missing authentication (empty token)
// ============================================================================

TEST_F(AuthenticationTest, EmptyTokenRejected) {
    auto result = handler.validate_request("");
    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.error, "Authentication required");
}

// ============================================================================
// Vector 2: Null/weak credential format
// ============================================================================

TEST_F(AuthenticationTest, NullByteInTokenRejected) {
    std::string malicious("Bearer abcdef1234567890\0admin", 28);
    auto result = handler.validate_request(malicious);
    EXPECT_FALSE(result.allowed);
}

TEST_F(AuthenticationTest, ShortTokenRejected) {
    auto result = handler.validate_request("Bearer short");
    EXPECT_FALSE(result.allowed);
    EXPECT_NE(result.error.find("too short"), std::string::npos);
}

TEST_F(AuthenticationTest, MissingBearerPrefixRejected) {
    auto result = handler.validate_request(raw_token);
    EXPECT_FALSE(result.allowed);
    EXPECT_NE(result.error.find("Bearer"), std::string::npos);
}

// ============================================================================
// Vector 3: Weak credential values (magic strings)
// ============================================================================

TEST_F(AuthenticationTest, TooShortMagicStringRejected) {
    EXPECT_FALSE(handler.validate_request("Bearer admin").allowed);
    EXPECT_FALSE(handler.validate_request("Bearer password").allowed);
}

// ============================================================================
// Vector 4: Token revocation
// ============================================================================

TEST_F(AuthenticationTest, RevokedTokenRejected) {
    handler.register_token(raw_token);
    EXPECT_TRUE(handler.validate_request(valid_token).allowed);

    handler.revoke_token(raw_token);
    auto result = handler.validate_request(valid_token);
    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.error, "Token has been revoked");
}

TEST_F(AuthenticationTest, IsRevokedReflectsRevocation) {
    handler.register_token(raw_token);
    EXPECT_FALSE(handler.is_revoked(raw_token));
    handler.revoke_token(raw_token);
    EXPECT_TRUE(handler.is_revoked(raw_token));
}

// ============================================================================
// Token expiry
// ============================================================================

TEST_F(AuthenticationTest, ExpiredTokenRejected) {
    // Register token as issued 2 hours ago
    auto two_hours_ago = std::chrono::steady_clock::now() - std::chrono::seconds{7200};
    handler.register_token(raw_token, two_hours_ago);

    auto result = handler.validate_request(valid_token);
    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.error, "Token has expired");
}

TEST_F(AuthenticationTest, FreshTokenAccepted) {
    handler.register_token(raw_token);
    EXPECT_TRUE(handler.validate_request(valid_token).allowed);
}

// ============================================================================
// Vector 5: Lockout after repeated failures
// ============================================================================

TEST_F(AuthenticationTest, LockoutAfterMaxFailures) {
    const std::string client = "attacker-ip-1.2.3.4";
    EXPECT_FALSE(handler.is_locked_out(client));

    handler.record_failure(client);
    handler.record_failure(client);
    EXPECT_FALSE(handler.is_locked_out(client));

    handler.record_failure(client);
    EXPECT_TRUE(handler.is_locked_out(client));
}

TEST_F(AuthenticationTest, ResetClearsLockout) {
    const std::string client = "client-a";
    handler.record_failure(client);
    handler.record_failure(client);
    handler.record_failure(client);
    EXPECT_TRUE(handler.is_locked_out(client));

    handler.reset_failures(client);
    EXPECT_FALSE(handler.is_locked_out(client));
}

// ============================================================================
// Valid token (happy path)
// ============================================================================

TEST_F(AuthenticationTest, ValidTokenAccepted) {
    auto result = handler.validate_request(valid_token);
    EXPECT_TRUE(result.allowed);
    EXPECT_TRUE(result.error.empty());
}

TEST_F(AuthenticationTest, UninitializedHandlerRejectsAll) {
    AuthenticationHandler fresh;
    auto result = fresh.validate_request(valid_token);
    EXPECT_FALSE(result.allowed);
    EXPECT_NE(result.error.find("not configured"), std::string::npos);
}
