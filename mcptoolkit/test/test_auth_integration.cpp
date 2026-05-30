#include <gtest/gtest.h>
#include <sstream>
#include <iostream>
#include "../include/mcp_adapter.h"

using namespace mcptoolkit;

// Test server with auth/RBAC enabled
class SecureTestServer : public MCPAdapter {
public:
    std::vector<ToolDefinition> list_tools() override {
        return {
            {
                "read_file",
                "Read a file (requires USER or ADMIN role)",
                {{"path", "string", "File path to read", true}}
            },
            {
                "admin_reset",
                "Reset the server (requires ADMIN role)",
                {}
            }
        };
    }

    ToolResult call_tool(const std::string& name,
                         const std::string& args_json,
                         const User* user = nullptr) override {
        // If auth is enabled, user should not be null
        if (user == nullptr) {
            return {"Internal error: auth not enforced", true};
        }

        if (!user->authenticated) {
            return {"User not authenticated", true};
        }

        if (name == "read_file") {
            if (user->role < Role::USER) {
                return {"Insufficient permissions for read_file", true};
            }
            return {"File contents here..."};
        }

        if (name == "admin_reset") {
            if (user->role < Role::ADMIN) {
                return {"Insufficient permissions for admin_reset", true};
            }
            return {"Server reset complete"};
        }

        return {"unknown tool", true};
    }
};

// Test that auth is enforced
TEST(AuthIntegrationTest, RequestWithoutAuthToken) {
    SecureTestServer server;

    // Enable authentication
    AuthConfig auth_cfg;
    auth_cfg.require_bearer_prefix = true;
    auth_cfg.min_token_length = 5;
    server.configure_auth(auth_cfg);

    // Register a valid token
    server.auth_handler().register_token("valid_token_12345");

    // Send request without auth token (should be rejected)
    std::string input = R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"read_file","arguments":{"path":"/etc/passwd"}}})";

    std::istringstream in(input);
    std::ostringstream out;

    std::streambuf* old_in = std::cin.rdbuf(in.rdbuf());
    std::streambuf* old_out = std::cout.rdbuf(out.rdbuf());

    server.run();

    std::cin.rdbuf(old_in);
    std::cout.rdbuf(old_out);

    std::string result = out.str();

    // Should contain error about authentication required
    EXPECT_TRUE(result.find("Authentication required") != std::string::npos ||
                result.find("-32603") != std::string::npos);
}

// Test that valid auth token is accepted
TEST(AuthIntegrationTest, RequestWithValidAuthToken) {
    SecureTestServer server;

    // Enable authentication
    AuthConfig auth_cfg;
    auth_cfg.require_bearer_prefix = true;
    auth_cfg.min_token_length = 5;
    server.configure_auth(auth_cfg);

    // Register a valid token
    std::string token = "valid_token_12345";
    server.auth_handler().register_token(token);

    // Send request with valid auth token
    std::string input = R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"read_file","arguments":{"path":"/data/file.txt"},"auth_token":"valid_token_12345"}})" "\n";

    std::istringstream in(input);
    std::ostringstream out;

    std::streambuf* old_in = std::cin.rdbuf(in.rdbuf());
    std::streambuf* old_out = std::cout.rdbuf(out.rdbuf());

    server.run();

    std::cin.rdbuf(old_in);
    std::cout.rdbuf(old_out);

    std::string result = out.str();

    // Should contain result, not auth error
    // Note: may contain "File contents" or just "result" field
    EXPECT_FALSE(result.find("Authentication required") != std::string::npos) <<
        "Result should not be authentication error. Got: " << result;
    EXPECT_TRUE(result.find("jsonrpc") != std::string::npos) <<
        "Response should be valid JSON-RPC. Got: " << result;
}

// Test that RBAC is enforced
TEST(AuthIntegrationTest, RBACEnforcementOnAdminTool) {
    SecureTestServer server;

    // Enable authentication
    AuthConfig auth_cfg;
    auth_cfg.require_bearer_prefix = true;
    auth_cfg.min_token_length = 5;
    server.configure_auth(auth_cfg);

    // Register valid token
    std::string token = "user_token_12345";
    server.auth_handler().register_token(token);

    // Configure RBAC: only ADMIN can call admin_reset
    auto& rbac = server.rbac();
    Permission admin_perm;
    admin_perm.required_role = Role::ADMIN;
    admin_perm.action = "admin_reset";
    rbac.add_permission(admin_perm);

    // Attempt to call admin_reset with USER role (should be rejected)
    std::string input = R"({"jsonrpc":"2.0","id":1,"method":"admin_reset","params":{"auth_token":"user_token_12345"}})" "\n";

    std::istringstream in(input);
    std::ostringstream out;

    std::streambuf* old_in = std::cin.rdbuf(in.rdbuf());
    std::streambuf* old_out = std::cout.rdbuf(out.rdbuf());

    server.run();

    std::cin.rdbuf(old_in);
    std::cout.rdbuf(old_out);

    std::string result = out.str();

    // Should contain authorization error
    EXPECT_TRUE(result.find("Unauthorized") != std::string::npos ||
                result.find("-32603") != std::string::npos);
}
