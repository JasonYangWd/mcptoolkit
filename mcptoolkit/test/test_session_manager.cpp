#include <gtest/gtest.h>
#include <unordered_set>
#include "session_manager.h"

using namespace mcptoolkit;

class SessionManagerTest : public ::testing::Test {
protected:
    SessionManager mgr;

    void SetUp() override {
        SessionConfig config;
        config.idle_timeout   = std::chrono::seconds{3600};
        config.absolute_ttl   = std::chrono::seconds{86400};
        config.id_bytes       = 32;
        config.bind_user_agent = true;
        mgr.configure(config);
    }

    const std::string ua = "TestAgent/1.0";
};

// ============================================================================
// Core CWE-384 fix: session ID must change on login
// ============================================================================

TEST_F(SessionManagerTest, LoginRotatesSessionId) {
    auto anon = mgr.create_anonymous();
    ASSERT_TRUE(anon.valid);

    auto auth = mgr.login(anon.session_id, "user42", ua);
    ASSERT_TRUE(auth.valid);

    // New ID must differ from the pre-auth ID
    EXPECT_NE(anon.session_id, auth.session_id);
}

TEST_F(SessionManagerTest, OldSessionInvalidatedAfterLogin) {
    auto anon = mgr.create_anonymous();
    ASSERT_TRUE(anon.valid);
    mgr.login(anon.session_id, "user42", ua);

    // Old (pre-auth) session ID must no longer be valid
    auto result = mgr.validate(anon.session_id, ua);
    EXPECT_FALSE(result.valid);
}

// ============================================================================
// Server generates IDs — client-supplied IDs must not become sessions
// ============================================================================

TEST_F(SessionManagerTest, UnknownSessionIdRejected) {
    auto result = mgr.validate("attacker-chosen-session-id", ua);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.error, "Session not found");
}

TEST_F(SessionManagerTest, SessionIdsAreUnique) {
    std::unordered_set<std::string> ids;
    for (int i = 0; i < 100; ++i) {
        auto r = mgr.create_anonymous();
        ASSERT_TRUE(r.valid);
        EXPECT_TRUE(ids.insert(r.session_id).second) << "Duplicate session ID generated";
    }
}

// ============================================================================
// Valid session flow
// ============================================================================

TEST_F(SessionManagerTest, ValidSessionAccepted) {
    auto anon = mgr.create_anonymous();
    auto auth = mgr.login(anon.session_id, "user42", ua);
    ASSERT_TRUE(auth.valid);

    auto check = mgr.validate(auth.session_id, ua);
    EXPECT_TRUE(check.valid);
}

// ============================================================================
// User-agent binding (soft client fingerprint)
// ============================================================================

TEST_F(SessionManagerTest, UserAgentMismatchRejected) {
    auto anon = mgr.create_anonymous();
    auto auth = mgr.login(anon.session_id, "user42", ua);
    ASSERT_TRUE(auth.valid);

    auto result = mgr.validate(auth.session_id, "DifferentAgent/2.0");
    EXPECT_FALSE(result.valid);
    EXPECT_NE(result.error.find("mismatch"), std::string::npos);
}

// ============================================================================
// Expiry
// ============================================================================

TEST_F(SessionManagerTest, ExpiredIdleSessionRejected) {
    SessionManager short_mgr;
    SessionConfig cfg;
    cfg.idle_timeout   = std::chrono::seconds{0}; // expire immediately
    cfg.absolute_ttl   = std::chrono::seconds{86400};
    cfg.bind_user_agent = false;
    short_mgr.configure(cfg);

    auto anon = short_mgr.create_anonymous();
    auto auth = short_mgr.login(anon.session_id, "user1");
    ASSERT_TRUE(auth.valid);

    auto result = short_mgr.validate(auth.session_id);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.error, "Session expired");
}

// ============================================================================
// Logout server-side invalidation
// ============================================================================

TEST_F(SessionManagerTest, LogoutInvalidatesSession) {
    auto anon = mgr.create_anonymous();
    auto auth = mgr.login(anon.session_id, "user42", ua);
    ASSERT_TRUE(auth.valid);

    mgr.logout(auth.session_id);

    auto result = mgr.validate(auth.session_id, ua);
    EXPECT_FALSE(result.valid);
}

// ============================================================================
// Unconfigured handler
// ============================================================================

TEST_F(SessionManagerTest, UninitializedManagerRejectsAll) {
    SessionManager fresh;
    auto r = fresh.create_anonymous();
    EXPECT_FALSE(r.valid);
    EXPECT_NE(r.error.find("not configured"), std::string::npos);
}

// ============================================================================
// Active count & purge
// ============================================================================

TEST_F(SessionManagerTest, ActiveCountTracksSessionsCorrectly) {
    EXPECT_EQ(mgr.active_count(), 0u);
    auto a = mgr.create_anonymous();
    auto b = mgr.create_anonymous();
    EXPECT_EQ(mgr.active_count(), 2u);
    mgr.logout(a.session_id);
    EXPECT_EQ(mgr.active_count(), 1u);
}
