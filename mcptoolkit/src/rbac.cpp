#include "rbac.h"

namespace mcptoolkit {

RoleBasedAccessControl::RoleBasedAccessControl() {}

void RoleBasedAccessControl::add_permission(const Permission& perm) {
    std::lock_guard<std::mutex> lock(mutex_);
    policy_.push_back(perm);
}

bool RoleBasedAccessControl::check_permission(const User& user,
                                              const std::string& action) const {
    // Unauthenticated users have no permissions
    if (!user.authenticated) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Check policy: find matching action with sufficient role
    for (const auto& perm : policy_) {
        if (perm.action == action && has_sufficient_role(user, perm)) {
            return true;
        }
    }

    // No matching permission found — deny by default
    return false;
}

AuthzResult RoleBasedAccessControl::can_access_resource(const User& user,
                                                        const ResourceAccess& resource,
                                                        const std::string& action) const {
    // Step 1: Verify user is authenticated
    if (!user.authenticated) {
        return {false, "Not authenticated"};
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Step 2: Check role-based permission
    bool has_role_perm = false;
    for (const auto& perm : policy_) {
        if (perm.action == action && has_sufficient_role(user, perm)) {
            has_role_perm = true;
            break;
        }
    }

    if (!has_role_perm) {
        return {false, "Insufficient permissions for action"};
    }

    // Step 3: Admin can always access any resource for admin-level actions
    // (e.g., "delete_any" implies admin can delete anything)
    if (user.role >= Role::ADMIN && action.find("any") != std::string::npos) {
        return {true, "Admin access to resource granted"};
    }

    // Step 4: Check resource-level access
    if (resource.is_public) {
        // Public resources accessible to authenticated users with role permission
        return {true, "Public resource access granted"};
    }

    // Private resource: check ownership or group membership
    if (owns_resource(user, resource)) {
        return {true, "Resource owner access granted"};
    }

    if (in_shared_group(user, resource)) {
        return {true, "Group access granted"};
    }

    return {false, "No access to this resource"};
}

AuthzResult RoleBasedAccessControl::can_access_resource_by_id(const User& user,
                                                               const std::string& resource_id,
                                                               const std::string& action) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Find resource
    auto it = resources_.find(resource_id);
    if (it == resources_.end()) {
        return {false, "Resource not found"};
    }

    const ResourceAccess& resource = it->second;

    // Check authentication
    if (!user.authenticated) {
        return {false, "Not authenticated"};
    }

    // Check role-based permission
    bool has_role_perm = false;
    for (const auto& perm : policy_) {
        if (perm.action == action && has_sufficient_role(user, perm)) {
            has_role_perm = true;
            break;
        }
    }

    if (!has_role_perm) {
        return {false, "Insufficient permissions for action"};
    }

    // Admin can always access any resource for admin-level actions
    if (user.role >= Role::ADMIN && action.find("any") != std::string::npos) {
        return {true, "Admin access to resource granted"};
    }

    // Check resource-level access
    if (resource.is_public) {
        return {true, "Public resource access granted"};
    }

    if (owns_resource(user, resource)) {
        return {true, "Resource owner access granted"};
    }

    if (in_shared_group(user, resource)) {
        return {true, "Group access granted"};
    }

    return {false, "No access to this resource"};
}

void RoleBasedAccessControl::register_resource(const ResourceAccess& resource) {
    std::lock_guard<std::mutex> lock(mutex_);
    resources_[resource.resource_id] = resource;
}

void RoleBasedAccessControl::update_resource_owner(const std::string& resource_id,
                                                   const std::string& new_owner_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = resources_.find(resource_id);
    if (it != resources_.end()) {
        it->second.owner_id = new_owner_id;
    }
}

ResourceAccess RoleBasedAccessControl::get_resource(const std::string& resource_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = resources_.find(resource_id);
    if (it != resources_.end()) {
        return it->second;
    }
    return ResourceAccess{"", "", {}, false};
}

bool RoleBasedAccessControl::has_sufficient_role(const User& user,
                                                 const Permission& perm) const {
    // Caller must hold mutex_
    return user.role >= perm.required_role;
}

bool RoleBasedAccessControl::owns_resource(const User& user,
                                          const ResourceAccess& resource) const {
    // Caller must hold mutex_
    return resource.owner_id == user.user_id;
}

bool RoleBasedAccessControl::in_shared_group(const User& user,
                                            const ResourceAccess& resource) const {
    // Caller must hold mutex_
    for (const auto& group : resource.group_ids) {
        if (user.group_ids.count(group) > 0) {
            return true;
        }
    }
    return false;
}

} // namespace mcptoolkit
