#include "session_manager.h"
#include <random>
#include <sstream>
#include <iomanip>

namespace mcptoolkit {

SessionManager::SessionManager() {}

void SessionManager::configure(const SessionConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    configured_ = true;
}

SessionResult SessionManager::create_anonymous() {
    if (!configured_) return {false, "", "SessionManager not configured"};

    std::lock_guard<std::mutex> lock(mutex_);
    auto id = generate_id();
    auto now = std::chrono::steady_clock::now();
    sessions_[id] = {"", "", now, now};
    return {true, id, ""};
}

SessionResult SessionManager::login(const std::string& old_session_id,
                                    const std::string& user_id,
                                    const std::string& user_agent) {
    if (!configured_) return {false, "", "SessionManager not configured"};
    if (user_id.empty()) return {false, "", "user_id required"};

    std::lock_guard<std::mutex> lock(mutex_);

    // Invalidate pre-auth session — this is the core fix for CWE-384
    sessions_.erase(old_session_id);

    // Issue a server-generated ID; never reuse or accept client-supplied IDs
    auto new_id = generate_id();
    auto now = std::chrono::steady_clock::now();
    sessions_[new_id] = {user_id, user_agent, now, now};
    return {true, new_id, ""};
}

SessionResult SessionManager::validate(const std::string& session_id,
                                       const std::string& user_agent) {
    if (!configured_) return {false, "", "SessionManager not configured"};

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return {false, "", "Session not found"};

    SessionData& s = it->second;

    if (is_expired(s)) {
        sessions_.erase(it);
        return {false, "", "Session expired"};
    }

    if (config_.bind_user_agent && !s.user_agent.empty() && s.user_agent != user_agent) {
        sessions_.erase(it);
        return {false, "", "Session user-agent mismatch"};
    }

    s.last_active = std::chrono::steady_clock::now();
    return {true, session_id, ""};
}

void SessionManager::logout(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session_id);
}

void SessionManager::purge_expired() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        if (is_expired(it->second))
            it = sessions_.erase(it);
        else
            ++it;
    }
}

size_t SessionManager::active_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

bool SessionManager::is_expired(const SessionData& s) const {
    auto now = std::chrono::steady_clock::now();
    auto idle = now - s.last_active;
    auto age  = now - s.created_at;
    return idle > config_.idle_timeout || age > config_.absolute_ttl;
}

std::string SessionManager::generate_id() const {
    // Use mt19937_64 seeded from random_device — not cryptographic, but
    // sufficient for demonstration; production deployments should use
    // OS CSPRNG (RAND_bytes / getrandom / BCryptGenRandom).
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;

    std::ostringstream oss;
    size_t words = (config_.id_bytes + 7) / 8;
    for (size_t i = 0; i < words; ++i) {
        oss << std::hex << std::setw(16) << std::setfill('0') << dist(gen);
    }
    return oss.str().substr(0, config_.id_bytes * 2);
}

} // namespace mcptoolkit
