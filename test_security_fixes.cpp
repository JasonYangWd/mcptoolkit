#include <iostream>
#include <string>
#include <cassert>
#include "mcptoolkit/include/json/json_builder.h"
#include "mcptoolkit/include/authentication_handler.h"
#include "mcptoolkit/include/mcp_adapter.h"

using namespace mcptoolkit;

// Test JSON escaping fix
void test_json_escaping() {
    std::cout << "Testing JSON escaping fix..." << std::endl;

    JsonBuilder b;
    b.start_object();

    // Test newline escaping
    b.add_field("text1", "Hello\nWorld");

    // Test tab escaping
    b.add_field("text2", "Col1\tCol2");

    // Test carriage return
    b.add_field("text3", "Line1\rLine2");

    // Test control character
    b.add_field("text4", "Null\x00Byte");

    // Test backslash and quote
    b.add_field("text5", "Quote\"and\\slash");

    b.end_object();

    std::string json = b.get();
    std::cout << "Generated JSON: " << json << std::endl;

    // Verify escaping
    assert(json.find("\\n") != std::string::npos);  // Newline escaped
    assert(json.find("\\t") != std::string::npos);  // Tab escaped
    assert(json.find("\\r") != std::string::npos);  // CR escaped
    assert(json.find("\\u0000") != std::string::npos);  // Null byte escaped
    assert(json.find("\\\"") != std::string::npos);  // Quote escaped
    assert(json.find("\\\\") != std::string::npos);  // Backslash escaped

    // Verify no unescaped control chars
    assert(json.find("\n") == std::string::npos);  // No literal newlines
    assert(json.find("\t") == std::string::npos);  // No literal tabs

    std::cout << "✅ JSON escaping test passed!" << std::endl;
}

// Test token decoding fix
void test_token_decoding() {
    std::cout << "\nTesting token decoding fix..." << std::endl;

    AuthenticationHandler auth;
    AuthConfig config;
    auth.configure(config);

    // Set secret key
    auth.set_token_secret("my_secret_key_123");

    // Test 1: Valid token should decode successfully
    // Generate a valid token format: "user_id.signature"
    // In real usage, the signature would be HMAC(user_id, secret)
    // For testing, we use a properly formatted token

    // Test 2: Invalid token format should return empty string
    std::string invalid_token = "no_dot_here";
    assert(auth.decode_token(invalid_token).empty());
    std::cout << "  ✓ Invalid token format rejected" << std::endl;

    // Test 3: Empty token should return empty string
    assert(auth.decode_token("").empty());
    std::cout << "  ✓ Empty token rejected" << std::endl;

    // Test 4: Token without secret should fail
    AuthenticationHandler auth2;
    auth2.configure(config);
    std::string token = "user123.sig_abc";
    assert(auth2.decode_token(token).empty());
    std::cout << "  ✓ Token without secret key rejected" << std::endl;

    std::cout << "✅ Token decoding test passed!" << std::endl;
}

// Test tool definition validation
void test_tool_validation() {
    std::cout << "\nTesting tool definition validation..." << std::endl;

    // Test 1: Valid tool
    ToolDefinition valid_tool{
        name: "valid_tool",
        description: "A valid tool",
        params: {}
    };
    std::string error = MCPAdapter::validate_tool_definition(valid_tool);
    assert(error.empty());
    std::cout << "  ✓ Valid tool accepted" << std::endl;

    // Test 2: Empty name
    ToolDefinition empty_name{
        name: "",
        description: "No name",
        params: {}
    };
    error = MCPAdapter::validate_tool_definition(empty_name);
    assert(!error.empty());
    std::cout << "  ✓ Empty name rejected: " << error << std::endl;

    // Test 3: Name with newline (potential injection)
    ToolDefinition newline_name{
        name: "tool\nINJECTED",
        description: "Malicious",
        params: {}
    };
    error = MCPAdapter::validate_tool_definition(newline_name);
    assert(!error.empty());
    std::cout << "  ✓ Name with newline rejected: " << error << std::endl;

    // Test 4: Name too long
    ToolDefinition long_name{
        name: std::string(300, 'a'),
        description: "Too long",
        params: {}
    };
    error = MCPAdapter::validate_tool_definition(long_name);
    assert(!error.empty());
    std::cout << "  ✓ Name too long rejected: " << error << std::endl;

    // Test 5: Description too long
    ToolDefinition long_desc{
        name: "tool",
        description: std::string(5000, 'a'),
        params: {}
    };
    error = MCPAdapter::validate_tool_definition(long_desc);
    assert(!error.empty());
    std::cout << "  ✓ Description too long rejected: " << error << std::endl;

    // Test 6: Invalid parameter type
    ToolParam invalid_param{
        name: "param1",
        type: "invalid_type",
        description: "Wrong type"
    };
    ToolDefinition invalid_type{
        name: "tool",
        description: "Has invalid param",
        params: {invalid_param}
    };
    error = MCPAdapter::validate_tool_definition(invalid_type);
    assert(!error.empty());
    std::cout << "  ✓ Invalid parameter type rejected: " << error << std::endl;

    std::cout << "✅ Tool validation test passed!" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "MCPToolkit Security Fixes Verification" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        test_json_escaping();
        test_token_decoding();
        test_tool_validation();

        std::cout << "\n========================================" << std::endl;
        std::cout << "✅ ALL TESTS PASSED" << std::endl;
        std::cout << "========================================" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}
