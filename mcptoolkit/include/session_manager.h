#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <functional>

namespace mcptoolkit {

struct SessionConfig {
    std::chrono::seconds idle_timeout{1800};    // 30 min idle
    std::chrono::seconds absolute_ttl{86400};   // 24 hr max lifetime
    size_t id_bytes = 32;                        // 256-bit session ID
    bool bind_user_agent = true;                 // soft client binding
};

struct SessionData {
    std::string user_id;
    std::string user_agent;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point last_active;
};

struct SessionResult {
    bool valid = false;
    std::string session_id;   // new ID after login/regeneration
    std::string error;
};

// Defends against CWE-384 (Session Fixation): regenerates IDs on login,
// rejects client-supplied IDs, enforces expiry and idle timeout.
class SessionManager {
public:
    SessionManager();

    void configure(const SessionConfig& config);

    // Create a pre-auth session (no user bound yet)
    SessionResult create_anonymous();

    // Authenticate: invalidate old session, issue new ID bound to user
    SessionResult login(const std::string& old_session_id,
                        const std::string& user_id,
                        const std::string& user_agent = "");

    // Validate an existing session; refreshes idle timer on success
    SessionResult validate(const std::string& session_id,
                           const std::string& user_agent = "");

    // Invalidate session server-side (logout)
    void logout(const std::string& session_id);

    // Purge all expired sessions
    void purge_expired();

    size_t active_count() const;

private:
    SessionConfig config_;
    bool configured_ = false;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, SessionData> sessions_;

    std::string generate_id() const;
    bool is_expired(const SessionData& s) const;
};

} // namespace mcptoolkit
