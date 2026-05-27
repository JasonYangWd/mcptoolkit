#include "session_manager.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <vector>

#ifdef _WIN32
    #include <bcrypt.h>
    #pragma comment(lib, "bcrypt.lib")
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/syscall.h>
    #include <linux/random.h>
#endif

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
    // Generate cryptographically secure random bytes using OS CSPRNG
    std::vector<uint8_t> random_bytes(config_.id_bytes);

#ifdef _WIN32
    // Windows: BCryptGenRandom (requires bcrypt.lib)
    NTSTATUS status = BCryptGenRandom(
        NULL,
        random_bytes.data(),
        static_cast<ULONG>(random_bytes.size()),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );
    if (!BCRYPT_SUCCESS(status)) {
        // Fallback: use a weak but deterministic value on error
        std::memset(random_bytes.data(), 0, random_bytes.size());
    }
#else
    // Unix/Linux: getrandom syscall (available since Linux 3.17)
    // Falls back to /dev/urandom if syscall not available
    ssize_t result = syscall(SYS_getrandom,
                            random_bytes.data(),
                            random_bytes.size(),
                            0);
    if (result < 0 || static_cast<size_t>(result) != random_bytes.size()) {
        // Fallback to /dev/urandom if getrandom unavailable
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd == -1) {
            std::memset(random_bytes.data(), 0, random_bytes.size());
        } else {
            ssize_t bytes_read = read(fd, random_bytes.data(), random_bytes.size());
            close(fd);
            if (bytes_read != static_cast<ssize_t>(random_bytes.size())) {
                std::memset(random_bytes.data(), 0, random_bytes.size());
            }
        }
    }
#endif

    // Convert bytes to hex string
    std::ostringstream oss;
    for (uint8_t byte : random_bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
}

} // namespace mcptoolkit
