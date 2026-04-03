// TestMcp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <iostream>
#include <sstream>
#include <chrono>
#include "json_parser.h"
#include "json_builder.h"
#include "mcp_adapter.h"

using namespace mcptoolkit;

int test_count = 0;
int pass_count = 0;

void assert_eq(const char* name, bool condition) {
    test_count++;
    if (condition) {
        std::cout << "✓ " << name << std::endl;
        pass_count++;
    }
    else {
        std::cout << "✗ " << name << std::endl;
    }
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

inline MCPMessage parse_mcp_json(const char* json_str, size_t len) {
    return MCPMessage::parse(json_str, len);
}

inline MCPMessage parse_mcp_json(const std::string& json_str) {
    return MCPMessage::parse(json_str.c_str(), json_str.size());
}
// =============================================================================
// TEST 1: BASIC REQUEST PARSING
// =============================================================================

void test_parse_initialize_request() {
    std::cout << "\n=== TEST 1: Parse Initialize Request ===" << std::endl;

    const char* json = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
    size_t len = strlen(json);

    MCPMessage msg = MCPMessage::parse(json, len);

    assert_eq("Parse valid",            msg.valid);
    assert_eq("Is marked as request",   msg.is_request);
    assert_eq("ID correctly parsed as 1", msg.id == 1);
    assert_eq("Method pointer not null", msg.method != nullptr);
    assert_eq("Method length correct",  msg.method_len == strlen("initialize"));
    assert_eq("Params start pointer set", msg.params_start != nullptr);
    assert_eq("Params length > 0",      msg.params_len > 0);

    std::cout << "  Method: \"" << std::string(msg.method, msg.method_len) << "\"" << std::endl;
    std::cout << "  Params JSON: \"" << std::string(msg.params_start, msg.params_len) << "\"" << std::endl;
}

// =============================================================================
// TEST 2: TOOLS LIST REQUEST
// =============================================================================

void test_parse_tools_list() {
    std::cout << "\n=== TEST 2: Parse tools/list Request ===" << std::endl;

    const char* json = R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})";
    MCPMessage msg = parse_mcp_json(json, strlen(json));

    assert_eq("Parse valid",        msg.valid);
    assert_eq("Is request",         msg.is_request);
    assert_eq("ID is 2",            msg.id == 2);
    assert_eq("Method pointer set", msg.method != nullptr);
    assert_eq("Method length correct", msg.method_len == strlen("tools/list"));

    std::cout << "  Method: \"" << std::string(msg.method, msg.method_len) << "\"" << std::endl;
}

// =============================================================================
// TEST 3: RESPONSE WITH RESULT
// =============================================================================

void test_parse_response_result() {
    std::cout << "\n=== TEST 3: Parse Response with Result ===" << std::endl;

    const char* json = R"({"jsonrpc":"2.0","id":1,"result":{"status":"ok"}})";
    MCPMessage msg = parse_mcp_json(json, strlen(json));

    assert_eq("Parse valid",             msg.valid);
    assert_eq("Is marked as response",   msg.is_response);
    assert_eq("Has result flag set",     msg.has_result);
    assert_eq("No error flag",           !msg.has_error);
    assert_eq("Result pointer set",      msg.result_start != nullptr);
    assert_eq("Result length > 0",       msg.result_len > 0);
    assert_eq("Result is valid JSON object", msg.result_start[0] == '{');

    std::string result_json(msg.result_start, msg.result_len);
    std::cout << "  Result JSON: " << result_json << std::endl;
}

// =============================================================================
// TEST 4: ERROR RESPONSE
// =============================================================================

void test_parse_error_response() {
    std::cout << "\n=== TEST 4: Parse Error Response ===" << std::endl;

    const char* json = R"({"jsonrpc":"2.0","id":1,"error":{"code":-32601,"message":"Method not found"}})";
    MCPMessage msg = parse_mcp_json(json, strlen(json));

    assert_eq("Parse valid",              msg.valid);
    assert_eq("Is marked as response",    msg.is_response);
    assert_eq("Error flag set",           msg.has_error);
    assert_eq("No result flag",           !msg.has_result);
    assert_eq("Error code is -32601",     msg.error_code == -32601);
    assert_eq("Error message pointer set", msg.error_msg != nullptr);
    assert_eq("Error message length correct", msg.error_msg_len == strlen("Method not found"));

    std::cout << "  Error code: " << msg.error_code << std::endl;
    std::cout << "  Error message: \"" << std::string(msg.error_msg, msg.error_msg_len) << "\"" << std::endl;
}

// =============================================================================
// TEST 5: COMPLEX PARAMETERS
// =============================================================================

void test_complex_params() {
    std::cout << "\n=== TEST 5: Parse Complex Parameters ===" << std::endl;

    const char* json = R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"query_db","arguments":{"sql":"SELECT *","db":"sales"}}})";
    MCPMessage msg = parse_mcp_json(json, strlen(json));

    assert_eq("Parse valid",              msg.valid);
    assert_eq("Request parsed",           msg.is_request);
    assert_eq("Method contains tools/call", msg.method != nullptr);
    assert_eq("Method length correct",    msg.method_len == strlen("tools/call"));
    assert_eq("Params detected",          msg.params_start != nullptr);
    assert_eq("Params length reasonable", msg.params_len > 10);
    assert_eq("Params starts with brace", msg.params_start[0] == '{');

    std::string params_json(msg.params_start, msg.params_len);
    std::cout << "  Params JSON: " << params_json << std::endl;
}

// =============================================================================
// TEST 6: RESPONSE BUILDING
// =============================================================================

void test_build_response() {
    std::cout << "\n=== TEST 6: Build Response ===" << std::endl;

    JsonBuilder builder;
    builder.start_object();
    builder.add_field("jsonrpc", "2.0");
    builder.add_field_number("id", 42);
    builder.add_object_field("result");
    builder.add_field("status", "ok");
    builder.add_field_number("value", 123);
    builder.close_object_field();
    builder.end_object();

    std::string response = builder.get();

    assert_eq("Response built", response.length() > 0);
    assert_eq("Contains jsonrpc", response.find("jsonrpc") != std::string::npos);
    assert_eq("Contains id 42", response.find("\"id\":42") != std::string::npos);
    assert_eq("Contains result", response.find("result") != std::string::npos);
    assert_eq("Contains status", response.find("status") != std::string::npos);

    std::cout << "  Response: " << response << std::endl;
}

// =============================================================================
// TEST 7: ERROR RESPONSE BUILDING
// =============================================================================

void test_build_error() {
    std::cout << "\n=== TEST 7: Build Error Response ===" << std::endl;

    JsonBuilder builder;
    builder.start_object();
    builder.add_field("jsonrpc", "2.0");
    builder.add_field_number("id", 1);
    builder.add_object_field("error");
    builder.add_field_number("code", -32601);
    builder.add_field("message", "Method not found");
    builder.close_object_field();
    builder.end_object();

    std::string response = builder.get();

    assert_eq("Error response built", response.length() > 0);
    assert_eq("Contains error object", response.find("error") != std::string::npos);
    assert_eq("Contains error code", response.find("-32601") != std::string::npos);
    assert_eq("Contains error message", response.find("Method not found") != std::string::npos);

    std::cout << "  Response: " << response << std::endl;
}

// =============================================================================
// TEST 8: ROUND-TRIP (PARSE REQUEST + BUILD RESPONSE)
// =============================================================================

void test_round_trip() {
    std::cout << "\n=== TEST 8: Round-Trip (Parse + Build) ===" << std::endl;

    // Parse incoming request
    const char* request = R"({"jsonrpc":"2.0","id":99,"method":"test","params":{"arg":"value"}})";
    MCPMessage msg = parse_mcp_json(request, strlen(request));

    // Build response with same ID
    JsonBuilder builder;
    builder.start_object();
    builder.add_field("jsonrpc", "2.0");
    builder.add_field_number("id", msg.id);  // Echo back the ID
    builder.add_object_field("result");
    builder.add_field("processed", "yes");
    builder.close_object_field();
    builder.end_object();

    std::string response = builder.get();

    assert_eq("Response ID matches request", response.find("\"id\":99") != std::string::npos);
    assert_eq("Response has result", response.find("result") != std::string::npos);

    std::cout << "  Request ID: " << msg.id << std::endl;
    std::cout << "  Response: " << response << std::endl;
}

// =============================================================================
// TEST 9: ARRAY HANDLING
// =============================================================================

void test_array_building() {
    std::cout << "\n=== TEST 9: Build Array Response ===" << std::endl;

    JsonBuilder builder;
    builder.start_object();
    builder.add_array_field("tools");
    builder.add_array_element_raw(R"({"name":"tool1","description":"First"})");
    builder.add_array_element_raw(R"({"name":"tool2","description":"Second"})");
    builder.close_array_field();
    builder.end_object();

    std::string response = builder.get();

    assert_eq("Array response built", response.length() > 0);
    assert_eq("Contains array", response.find("[") != std::string::npos);
    assert_eq("Contains tool1", response.find("tool1") != std::string::npos);
    assert_eq("Contains tool2", response.find("tool2") != std::string::npos);

    std::cout << "  Response: " << response << std::endl;
}

// =============================================================================
// TEST 11: GENERIC JSON (NON-MCP) PARSING
// =============================================================================

void test_parse_generic_json() {
    std::cout << "\n=== TEST 11: Parse Generic JSON (non-MCP) ===" << std::endl;

    // Plain object — no JSON-RPC fields at all
    {
        const char* json = R"({"name":"Alice","age":30,"active":true})";
        MCPMessage msg = parse_mcp_json(json, strlen(json));

        assert_eq("Generic object: parses without error",  msg.valid);
        assert_eq("Generic object: not flagged as request",  !msg.is_request);
        assert_eq("Generic object: not flagged as response", !msg.is_response);
        assert_eq("Generic object: no result",               !msg.has_result);
        assert_eq("Generic object: no error",                !msg.has_error);
    }

    // Nested object
    {
        const char* json = R"({"user":{"id":42,"role":"admin"},"count":7})";
        MCPMessage msg = parse_mcp_json(json, strlen(json));

        assert_eq("Nested object: parses without error",  msg.valid);
        assert_eq("Nested object: not a request",         !msg.is_request);
        assert_eq("Nested object: not a response",        !msg.is_response);
    }

    // Array values
    {
        const char* json = R"({"tags":["a","b","c"],"scores":[1,2,3]})";
        MCPMessage msg = parse_mcp_json(json, strlen(json));

        assert_eq("Array values: parses without error", msg.valid);
        assert_eq("Array values: not a request",        !msg.is_request);
    }

    // Deeply nested object (within depth limit)
    {
        const char* json = R"({"a":{"b":{"c":{"d":{"e":"deep"}}}}})";
        MCPMessage msg = parse_mcp_json(json, strlen(json));

        assert_eq("Deep nesting: parses without error", msg.valid);
    }

    // Mixed types
    {
        const char* json = R"({"str":"hello","num":123,"flt":3.14,"flag":false,"nothing":null})";
        MCPMessage msg = parse_mcp_json(json, strlen(json));

        assert_eq("Mixed types: parses without error", msg.valid);
    }

    // Empty object
    {
        const char* json = "{}";
        MCPMessage msg = parse_mcp_json(json, strlen(json));

        assert_eq("Empty object: parses without error", msg.valid);
        assert_eq("Empty object: not a request",        !msg.is_request);
        assert_eq("Empty object: not a response",       !msg.is_response);
    }

    // Malformed JSON — should fail gracefully
    {
        const char* json = R"({"key": "unterminated)";
        MCPMessage msg = parse_mcp_json(json, strlen(json));

        assert_eq("Malformed JSON: valid is false", !msg.valid);
    }

    // Not an object (array at root) — should fail gracefully
    {
        const char* json = R"([1,2,3])";
        MCPMessage msg = parse_mcp_json(json, strlen(json));

        assert_eq("Root array: valid is false", !msg.valid);
    }
}

// =============================================================================
// TEST 10: PERFORMANCE BENCHMARK
// =============================================================================

void test_performance() {
    std::cout << "\n=== TEST 10: Performance Benchmark ===" << std::endl;

    const char* json = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
    size_t len = strlen(json);

    // Parse benchmark
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; i++) {
            MCPMessage msg = parse_mcp_json(json, len);
            (void)msg;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        double per_msg = duration / 1000.0;

        std::cout << "  Parse 1000 messages: " << duration << "µs total" << std::endl;
        std::cout << "  Per message: " << per_msg << "µs" << std::endl;
        assert_eq("Parse < 100µs per msg", per_msg < 100);
    }

    // Build benchmark
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; i++) {
            JsonBuilder builder;
            builder.start_object();
            builder.add_field("jsonrpc", "2.0");
            builder.add_field_number("id", i);
            builder.add_field("status", "ok");
            builder.end_object();
            (void)builder.get();
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        double per_msg = duration / 1000.0;

        std::cout << "  Build 1000 responses: " << duration << "µs total" << std::endl;
        std::cout << "  Per message: " << per_msg << "µs" << std::endl;
        assert_eq("Build < 50µs per msg", per_msg < 50);
    }
}
// =============================================================================
// MCP ADAPTER TEST INFRASTRUCTURE
// =============================================================================

// Minimal subclass that exposes one "echo" tool and records what was called.
class TestServer : public MCPAdapter {
public:
    std::string last_tool_name;
    std::string last_tool_args;

protected:
    std::vector<ToolDefinition> list_tools() override {
        return {{
            "echo",
            "Echoes back the input",
            {{ "text", "string", "Text to echo", /*required=*/true }}
        }};
    }

    ToolResult call_tool(const std::string& name,
                         const std::string& args_json) override {
        last_tool_name = name;
        last_tool_args = args_json;
        if (name == "echo") return {"hello from echo"};
        return {"unknown tool", /*is_error=*/true};
    }
};

// Run an adapter against a block of newline-delimited JSON input.
// Returns everything the adapter wrote to stdout.
static std::string run_adapter(MCPAdapter& adapter, const std::string& input) {
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* old_cin  = std::cin.rdbuf(in.rdbuf());
    std::streambuf* old_cout = std::cout.rdbuf(out.rdbuf());
    adapter.run();
    std::cin.rdbuf(old_cin);
    std::cout.rdbuf(old_cout);
    return out.str();
}

// =============================================================================
// TEST A1: initialize
// =============================================================================

void test_adapter_initialize() {
    std::cout << "\n=== TEST A1: initialize handshake ===" << std::endl;

    MCPAdapter adapter;
    std::string out = run_adapter(adapter,
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})" "\n");

    assert_eq("Response non-empty",        !out.empty());
    assert_eq("Contains id 1",             out.find("\"id\":1")             != std::string::npos);
    assert_eq("Contains result",           out.find("\"result\"")           != std::string::npos);
    assert_eq("Contains protocolVersion",  out.find("protocolVersion")      != std::string::npos);
    assert_eq("Protocol is 2024-11-05",    out.find("2024-11-05")           != std::string::npos);
    assert_eq("Contains capabilities",     out.find("capabilities")         != std::string::npos);
    assert_eq("Contains tools capability", out.find("\"tools\"")            != std::string::npos);
    assert_eq("Contains serverInfo",       out.find("serverInfo")           != std::string::npos);
    assert_eq("Server name mcptoolkit",    out.find("mcptoolkit")           != std::string::npos);

    std::cout << "  Response: " << out;
}

// =============================================================================
// TEST A2: tools/list — no tools
// =============================================================================

void test_adapter_tools_list_empty() {
    std::cout << "\n=== TEST A2: tools/list (no tools) ===" << std::endl;

    MCPAdapter adapter;
    std::string out = run_adapter(adapter,
        R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})" "\n");

    assert_eq("Response non-empty",        !out.empty());
    assert_eq("Contains id 2",             out.find("\"id\":2")             != std::string::npos);
    assert_eq("Tools array is empty",      out.find("\"tools\":[]")         != std::string::npos);

    std::cout << "  Response: " << out;
}

// =============================================================================
// TEST A3: tools/list — with tools
// =============================================================================

void test_adapter_tools_list_with_tools() {
    std::cout << "\n=== TEST A3: tools/list (with tools) ===" << std::endl;

    TestServer server;
    std::string out = run_adapter(server,
        R"({"jsonrpc":"2.0","id":3,"method":"tools/list","params":{}})" "\n");

    assert_eq("Response non-empty",        !out.empty());
    assert_eq("Tool name 'echo'",          out.find("\"echo\"")             != std::string::npos);
    assert_eq("Tool description present",  out.find("Echoes back")          != std::string::npos);
    assert_eq("inputSchema present",       out.find("inputSchema")          != std::string::npos);
    assert_eq("Param 'text' present",      out.find("\"text\"")             != std::string::npos);
    assert_eq("Required array has 'text'", out.find("\"required\":[\"text\"]") != std::string::npos);

    std::cout << "  Response: " << out;
}

// =============================================================================
// TEST A4: tools/call — known tool
// =============================================================================

void test_adapter_tools_call() {
    std::cout << "\n=== TEST A4: tools/call (known tool) ===" << std::endl;

    TestServer server;
    std::string out = run_adapter(server,
        R"({"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"echo","arguments":{"text":"hello"}}})" "\n");

    assert_eq("Response non-empty",        !out.empty());
    assert_eq("Contains result",           out.find("\"result\"")           != std::string::npos);
    assert_eq("Contains content array",    out.find("\"content\"")          != std::string::npos);
    assert_eq("Result text present",       out.find("hello from echo")      != std::string::npos);
    assert_eq("isError is false",          out.find("\"isError\":false")    != std::string::npos);
    assert_eq("Tool name dispatched",      server.last_tool_name == "echo");
    assert_eq("Arguments forwarded",       server.last_tool_args.find("hello") != std::string::npos);

    std::cout << "  Response: " << out;
}

// =============================================================================
// TEST A5: tools/call — unknown tool
// =============================================================================

void test_adapter_tools_call_unknown() {
    std::cout << "\n=== TEST A5: tools/call (unknown tool) ===" << std::endl;

    TestServer server;
    std::string out = run_adapter(server,
        R"({"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"no_such","arguments":{}}})" "\n");

    assert_eq("Response non-empty",        !out.empty());
    assert_eq("isError is true",           out.find("\"isError\":true")     != std::string::npos);
    assert_eq("Contains 'unknown tool'",   out.find("unknown tool")         != std::string::npos);

    std::cout << "  Response: " << out;
}

// =============================================================================
// TEST A6: unknown method
// =============================================================================

void test_adapter_unknown_method() {
    std::cout << "\n=== TEST A6: unknown method ===" << std::endl;

    MCPAdapter adapter;
    std::string out = run_adapter(adapter,
        R"({"jsonrpc":"2.0","id":6,"method":"resources/list","params":{}})" "\n");

    assert_eq("Response non-empty",        !out.empty());
    assert_eq("Contains error",            out.find("\"error\"")            != std::string::npos);
    assert_eq("Error code -32601",         out.find("-32601")               != std::string::npos);
    assert_eq("Method not found",          out.find("Method not found")     != std::string::npos);

    std::cout << "  Response: " << out;
}

// =============================================================================
// TEST A7: malformed JSON
// =============================================================================

void test_adapter_parse_error() {
    std::cout << "\n=== TEST A7: malformed JSON ===" << std::endl;

    MCPAdapter adapter;
    std::string out = run_adapter(adapter, "{bad json\n");

    assert_eq("Response non-empty",        !out.empty());
    assert_eq("Contains error",            out.find("\"error\"")            != std::string::npos);
    assert_eq("Parse error code -32700",   out.find("-32700")               != std::string::npos);

    std::cout << "  Response: " << out;
}

// =============================================================================
// TEST A8: notification yields no response
// =============================================================================

void test_adapter_notification_no_response() {
    std::cout << "\n=== TEST A8: notification yields no response ===" << std::endl;

    MCPAdapter adapter;
    std::string out = run_adapter(adapter,
        R"({"jsonrpc":"2.0","method":"notifications/initialized"})" "\n");

    assert_eq("No response emitted",       out.empty());
}

// =============================================================================
// TEST A9: multiple messages in one session
// =============================================================================

void test_adapter_multiple_messages() {
    std::cout << "\n=== TEST A9: multiple messages in sequence ===" << std::endl;

    TestServer server;
    std::string input =
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})"    "\n"
        R"({"jsonrpc":"2.0","method":"notifications/initialized"})"         "\n"
        R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})"    "\n";

    std::string out = run_adapter(server, input);

    // Count lines of output — each response is one line
    size_t lines = 0;
    for (char c : out) if (c == '\n') ++lines;

    assert_eq("Two response lines",            lines == 2);
    assert_eq("initialize response present",   out.find("protocolVersion")  != std::string::npos);
    assert_eq("tools/list response present",   out.find("\"tools\"")        != std::string::npos);

    std::cout << "  Output lines: " << lines << std::endl;
}

// =============================================================================
// TEST RUNNER
// =============================================================================

// =============================================================================
// TEST 12: INVALID JSON STRINGS
// =============================================================================

void test_invalid_json_strings() {
    std::cout << "\n=== TEST 12: Invalid JSON Strings ===" << std::endl;

    // Unterminated string
    {
        const char* json = R"({"key": "unterminated)";
        MCPMessage msg = parse_mcp_json(json, strlen(json));
        assert_eq("Unterminated string: invalid", !msg.valid);
    }

    // Unterminated object
    {
        const char* json = R"({"key": "value")";
        MCPMessage msg = parse_mcp_json(json, strlen(json));
        assert_eq("Unterminated object: invalid", !msg.valid);
    }

    // Invalid escape sequence (parser is lenient — accepts any escape)
    {
        const char* json = R"({"key": "bad\x escape"})";
        MCPMessage msg = parse_mcp_json(json, strlen(json));
        assert_eq("Invalid escape: parser is lenient", msg.valid);
    }

    // Trailing comma in object (parser is lenient — accepts trailing commas)
    {
        const char* json = R"({"key": "value",})";
        MCPMessage msg = parse_mcp_json(json, strlen(json));
        assert_eq("Trailing comma: parser is lenient", msg.valid);
    }

    // Single quotes instead of double quotes
    {
        const char* json = "{'key': 'value'}";
        MCPMessage msg = parse_mcp_json(json, strlen(json));
        assert_eq("Single quotes: invalid", !msg.valid);
    }

    // Missing colon
    {
        const char* json = R"({"key" "value"})";
        MCPMessage msg = parse_mcp_json(json, strlen(json));
        assert_eq("Missing colon: invalid", !msg.valid);
    }

    // Invalid number (parser accepts leading zeros)
    {
        const char* json = R"({"id": 01234})";
        MCPMessage msg = parse_mcp_json(json, strlen(json));
        assert_eq("Leading zero number: parser is lenient", msg.valid);
    }

    // Duplicate keys (should parse but might be implementation-dependent)
    {
        const char* json = R"({"id": 1, "id": 2})";
        MCPMessage msg = parse_mcp_json(json, strlen(json));
        assert_eq("Duplicate keys: valid (implementation-dependent)", msg.valid);
    }

    // Empty string input
    {
        MCPMessage msg = parse_mcp_json("", 0);
        assert_eq("Empty string: invalid", !msg.valid);
    }

    // Null pointer with zero length
    {
        MCPMessage msg = MCPMessage::parse(nullptr, 0);
        assert_eq("Null pointer: invalid", !msg.valid);
    }

    // Random garbage
    {
        const char* json = "!@#$%^&*()";
        MCPMessage msg = parse_mcp_json(json, strlen(json));
        assert_eq("Random garbage: invalid", !msg.valid);
    }

    // Root array instead of object
    {
        const char* json = R"([1, 2, 3])";
        MCPMessage msg = parse_mcp_json(json, strlen(json));
        assert_eq("Root array: invalid", !msg.valid);
    }

    // Root primitive instead of object
    {
        const char* json = "123";
        MCPMessage msg = parse_mcp_json(json, strlen(json));
        assert_eq("Root number: invalid", !msg.valid);
    }

    std::cout << "  Invalid JSON tests completed" << std::endl;
}

void RunParserTests() {

    test_parse_initialize_request();
    test_parse_tools_list();
    test_parse_response_result();
    test_parse_error_response();
    test_complex_params();
    test_build_response();
    test_build_error();
    test_round_trip();
    test_array_building();
    test_parse_generic_json();
    test_performance();
    test_invalid_json_strings();
}
void RunAdapterTests() {

    test_adapter_initialize();
    test_adapter_tools_list_empty();
    test_adapter_tools_list_with_tools();
    test_adapter_tools_call();
    test_adapter_tools_call_unknown();
    test_adapter_unknown_method();
    test_adapter_parse_error();
    test_adapter_notification_no_response();
    test_adapter_multiple_messages();
}
int main()
{
    RunParserTests();
    RunAdapterTests();
    std::cout << "\n" << pass_count << "/" << test_count << " tests passed\n";
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
