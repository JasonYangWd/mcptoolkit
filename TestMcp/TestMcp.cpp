// TestMcp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <iostream>
#include <chrono>
#include "json_parser.h"
#include "json_builder.h"

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
int main()
{
    //MCPAdapter mcpAdapter;// = new MCPAdapter();
    //mcpAdapter.init();
    //mcpAdapter.run();

    test_parse_initialize_request();
    test_parse_tools_list();
    test_parse_response_result();
    test_parse_error_response();
    test_complex_params();
    test_build_response();
    test_build_error();
    test_round_trip();
    test_array_building();
    test_performance();
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
