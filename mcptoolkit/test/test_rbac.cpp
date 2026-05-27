#include <gtest/gtest.h>
#include "rbac.h"

using namespace mcptoolkit;

class RBACTest : public ::testing::Test {
protected:
    RoleBasedAccessControl rbac;

    void SetUp() override {
        // Define global permissions (role-based)
        rbac.add_permission({Role::GUEST, "read_public"});
        rbac.add_permission({Role::USER, "read_own"});
        rbac.add_permission({Role::USER, "write_own"});
        rbac.add_permission({Role::ADMIN, "delete_any"});
        rbac.add_permission({Role::ADMIN, "read_all"});
    }

    User create_user(const std::string& id, Role role, bool authenticated = true) {
        User user;
        user.user_id = id;
        user.role = role;
        user.authenticated = authenticated;
        return user;
    }

    ResourceAccess create_resource(const std::string& id,
                                   const std::string& owner,
                                   bool is_public = false) {
        ResourceAccess res;
        res.resource_id = id;
        res.owner_id = owner;
        res.is_public = is_public;
        return res;
    }
};

// ============================================================================
// CWE-306 Vector 1: Missing authorization checks
// ============================================================================

TEST_F(RBACTest, UnauthenticatedUserCannotPerformActions) {
    User unauthenticated = create_user("attacker", Role::GUEST, false);
    EXPECT_FALSE(rbac.check_permission(unauthenticated, "read_public"));
}

TEST_F(RBACTest, GuestCanReadPublic) {
    User guest = create_user("guest1", Role::GUEST);
    EXPECT_TRUE(rbac.check_permission(guest, "read_public"));
}

TEST_F(RBACTest, GuestCannotWrite) {
    User guest = create_user("guest1", Role::GUEST);
    EXPECT_FALSE(rbac.check_permission(guest, "write_own"));
}

// ============================================================================
// CWE-306 Vector 2: Trust client-supplied roles
// ============================================================================

TEST_F(RBACTest, RoleEnforcedByServer) {
    // Simulate attacker claiming to be admin
    User attacker = create_user("attacker", Role::GUEST);
    // Even if they claim admin in request, server checks actual role (GUEST)
    EXPECT_FALSE(rbac.check_permission(attacker, "delete_any"));
}

// ============================================================================
// CWE-306 Vector 3: RBAC logic errors
// ============================================================================

TEST_F(RBACTest, RoleHierarchyEnforced) {
    User user = create_user("user1", Role::USER);
    User admin = create_user("admin1", Role::ADMIN);

    // USER can read_own but not delete_any
    EXPECT_TRUE(rbac.check_permission(user, "read_own"));
    EXPECT_FALSE(rbac.check_permission(user, "delete_any"));

    // ADMIN can do both
    EXPECT_TRUE(rbac.check_permission(admin, "read_own"));
    EXPECT_TRUE(rbac.check_permission(admin, "delete_any"));
}

// ============================================================================
// CWE-306 Vector 4: Privilege escalation via self-modification
// ============================================================================

TEST_F(RBACTest, UserCannotEscalateOwnRole) {
    User user = create_user("user1", Role::USER);
    // Even if user tries to modify their own role in a request,
    // server checks actual role from session/database
    EXPECT_FALSE(rbac.check_permission(user, "delete_any"));
}

// ============================================================================
// CWE-306 Vector 5: Resource enumeration
// ============================================================================

TEST_F(RBACTest, UserCannotAccessOthersPrivateResources) {
    User user1 = create_user("user1", Role::USER);
    User user2 = create_user("user2", Role::USER);

    // User1 owns resource, user2 doesn't
    ResourceAccess resource = create_resource("doc1", "user1", false);
    rbac.register_resource(resource);

    // User1 can access their own resource
    auto result1 = rbac.can_access_resource(user1, resource, "read_own");
    EXPECT_TRUE(result1.allowed);

    // User2 cannot access user1's resource
    auto result2 = rbac.can_access_resource(user2, resource, "read_own");
    EXPECT_FALSE(result2.allowed);
}

// ============================================================================
// Resource-level authorization
// ============================================================================

TEST_F(RBACTest, OwnerCanAccessOwnResource) {
    User owner = create_user("alice", Role::USER);
    ResourceAccess resource = create_resource("doc1", "alice");
    rbac.register_resource(resource);

    auto result = rbac.can_access_resource(owner, resource, "read_own");
    EXPECT_TRUE(result.allowed);
}

TEST_F(RBACTest, AdminCanAccessAllResources) {
    User admin = create_user("admin1", Role::ADMIN);
    ResourceAccess resource = create_resource("doc1", "alice");
    rbac.register_resource(resource);

    auto result = rbac.can_access_resource(admin, resource, "delete_any");
    EXPECT_TRUE(result.allowed);
}

TEST_F(RBACTest, PublicResourceAccessibleToAuthenticatedUsers) {
    User user = create_user("user1", Role::USER);
    ResourceAccess public_resource = create_resource("public_doc", "system", true);
    rbac.register_resource(public_resource);

    auto result = rbac.can_access_resource(user, public_resource, "read_own");
    EXPECT_TRUE(result.allowed);
}

TEST_F(RBACTest, GuestCanAccessPublicResource) {
    User guest = create_user("guest1", Role::GUEST);
    ResourceAccess public_resource = create_resource("public_doc", "system", true);
    rbac.register_resource(public_resource);

    auto result = rbac.can_access_resource(guest, public_resource, "read_public");
    EXPECT_TRUE(result.allowed);
}

// ============================================================================
// Group-based access
// ============================================================================

TEST_F(RBACTest, UserInGroupCanAccessGroupResource) {
    User user = create_user("user1", Role::USER);
    user.group_ids.insert("team_a");

    ResourceAccess resource = create_resource("team_doc", "user2");
    resource.group_ids.insert("team_a");
    rbac.register_resource(resource);

    auto result = rbac.can_access_resource(user, resource, "read_own");
    EXPECT_TRUE(result.allowed);
}

TEST_F(RBACTest, UserNotInGroupCannotAccessGroupResource) {
    User user = create_user("user1", Role::USER);
    user.group_ids.insert("team_a");

    ResourceAccess resource = create_resource("team_doc", "user2");
    resource.group_ids.insert("team_b");  // Different group
    rbac.register_resource(resource);

    auto result = rbac.can_access_resource(user, resource, "read_own");
    EXPECT_FALSE(result.allowed);
}

// ============================================================================
// Resource ID-based authorization
// ============================================================================

TEST_F(RBACTest, CanAccessByResourceId) {
    User owner = create_user("alice", Role::USER);
    ResourceAccess resource = create_resource("doc_123", "alice");
    rbac.register_resource(resource);

    auto result = rbac.can_access_resource_by_id(owner, "doc_123", "read_own");
    EXPECT_TRUE(result.allowed);
}

TEST_F(RBACTest, CannotAccessNonexistentResource) {
    User user = create_user("user1", Role::USER);

    auto result = rbac.can_access_resource_by_id(user, "nonexistent", "read_own");
    EXPECT_FALSE(result.allowed);
}

// ============================================================================
// Deny-by-default policy
// ============================================================================

TEST_F(RBACTest, UnknownActionDenied) {
    User user = create_user("user1", Role::ADMIN);
    EXPECT_FALSE(rbac.check_permission(user, "unknown_action"));
}

TEST_F(RBACTest, InvalidRoleActionCombinationDenied) {
    User guest = create_user("guest1", Role::GUEST);
    // Guest role has only "read_public" permission
    EXPECT_FALSE(rbac.check_permission(guest, "read_own"));
    EXPECT_FALSE(rbac.check_permission(guest, "write_own"));
    EXPECT_FALSE(rbac.check_permission(guest, "delete_any"));
}

// ============================================================================
// Authorization decision details
// ============================================================================

TEST_F(RBACTest, AuthzResultContainsReason) {
    User user = create_user("user1", Role::USER);
    ResourceAccess resource = create_resource("doc1", "alice");
    rbac.register_resource(resource);

    auto result = rbac.can_access_resource(user, resource, "read_own");
    EXPECT_FALSE(result.allowed);
    EXPECT_FALSE(result.reason.empty());
}

// ============================================================================
// Resource ownership updates
// ============================================================================

TEST_F(RBACTest, ResourceOwnershipCanBeUpdated) {
    ResourceAccess resource = create_resource("doc1", "alice");
    rbac.register_resource(resource);

    User alice = create_user("alice", Role::USER);
    User bob = create_user("bob", Role::USER);

    // Initially alice owns it
    auto result1 = rbac.can_access_resource(alice, resource, "read_own");
    EXPECT_TRUE(result1.allowed);

    // Transfer ownership to bob
    rbac.update_resource_owner("doc1", "bob");
    ResourceAccess updated = rbac.get_resource("doc1");
    EXPECT_EQ(updated.owner_id, "bob");

    // Now bob owns it
    auto result2 = rbac.can_access_resource(bob, updated, "read_own");
    EXPECT_TRUE(result2.allowed);
}

// ============================================================================
// Uninitialized/unconfigured handler
// ============================================================================

TEST_F(RBACTest, EmptyPolicyDeniesAll) {
    RoleBasedAccessControl empty_rbac;
    User user = create_user("user1", Role::ADMIN);

    // No permissions defined — all actions denied
    EXPECT_FALSE(empty_rbac.check_permission(user, "read_any"));
}
