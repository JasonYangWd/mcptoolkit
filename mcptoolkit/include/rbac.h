#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace mcptoolkit {

enum class Role {
    GUEST = 0,   // No tool access
    USER = 1,    // Read-only access to own resources
    ADMIN = 2    // Full access
};

struct Permission {
    Role required_role;
    std::string action;  // "read", "write", "delete", "execute", etc.
};

struct ResourceAccess {
    std::string resource_id;
    std::string owner_id;
    std::unordered_set<std::string> group_ids;  // Groups with access
    bool is_public = false;
};

struct User {
    std::string user_id;
    Role role = Role::GUEST;
    std::unordered_set<std::string> group_ids;
    bool authenticated = false;
};

struct AuthzResult {
    bool allowed = false;
    std::string reason;
};

// Defends against CWE-306 (Missing Authentication for Critical Function):
// Enforces role-based access control with deny-by-default policy.
class RoleBasedAccessControl {
public:
    RoleBasedAccessControl();

    // Define what each role can do globally
    void add_permission(const Permission& perm);

    // Check if user can perform action (role-level check)
    bool check_permission(const User& user, const std::string& action) const;

    // Check if user can access specific resource
    AuthzResult can_access_resource(const User& user,
                                    const ResourceAccess& resource,
                                    const std::string& action) const;

    // Check if user can access resource by ID
    AuthzResult can_access_resource_by_id(const User& user,
                                          const std::string& resource_id,
                                          const std::string& action) const;

    // Register a resource for access control
    void register_resource(const ResourceAccess& resource);

    // Update resource ownership (admin-only in practice)
    void update_resource_owner(const std::string& resource_id,
                              const std::string& new_owner_id);

    // Get resource info (for internal checks)
    ResourceAccess get_resource(const std::string& resource_id) const;

private:
    mutable std::mutex mutex_;
    std::vector<Permission> policy_;
    std::unordered_map<std::string, ResourceAccess> resources_;

    // Helper: Check role hierarchy
    bool has_sufficient_role(const User& user, const Permission& perm) const;

    // Helper: Check resource ownership
    bool owns_resource(const User& user, const ResourceAccess& resource) const;

    // Helper: Check group membership
    bool in_shared_group(const User& user, const ResourceAccess& resource) const;
};

} // namespace mcptoolkit
