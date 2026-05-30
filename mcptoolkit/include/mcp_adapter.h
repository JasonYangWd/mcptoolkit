#pragma once

#include <string>
#include <vector>
#include <optional>
#include "json/json_msg.h"
#include "input_validation.h"
#include "authentication_handler.h"
#include "rbac.h"

#if defined(_WIN32) && defined(MCPTOOLKIT_SHARED)
  #ifdef MCPTOOLKIT_EXPORTS
    #define MCPTOOLKIT __declspec(dllexport)
  #else
    #define MCPTOOLKIT __declspec(dllimport)
  #endif
#elif defined(MCPTOOLKIT_SHARED) && defined(__GNUC__)
  #define MCPTOOLKIT __attribute__((visibility("default")))
#else
  #define MCPTOOLKIT
#endif

namespace mcptoolkit {

// One parameter in a tool's input schema.
struct ToolParam {
    std::string name;
    std::string type;         // "string" | "integer" | "boolean" | "number"
    std::string description;
    bool        required = false;
};

// Describes a tool that the server advertises via tools/list.
struct ToolDefinition {
    std::string            name;
    std::string            description;
    std::vector<ToolParam> params;
};

// Return value from a tool handler.
struct ToolResult {
    std::string text;
    bool        is_error = false;
};

class MCPTOOLKIT MCPAdapter {
public:
    MCPAdapter();
    virtual ~MCPAdapter() = default;

    // Start the stdio JSON-RPC read/dispatch loop. Blocks until stdin closes.
    void run();

    // Register input validation rules for a tool
    void register_tool_validation(const ToolValidationRules& rules) {
        _validator.register_tool_rules(rules);
    }

    // Get access to the validator (for subclasses to set up rules)
    InputValidationHandler& validator() { return _validator; }

    // Configure authentication handler
    void configure_auth(const AuthConfig& config) {
        _auth_handler.configure(config);
        _auth_enabled = true;
    }

    // Get access to auth handler (for registering tokens, etc.)
    AuthenticationHandler& auth_handler() { return _auth_handler; }

    // Get access to RBAC (for configuration)
    RoleBasedAccessControl& rbac() {
        _rbac_enabled = true;
        return _rbac;
    }

    // Override to extract auth token from params (optional; default extracts from "auth_token" field)
    virtual std::string extract_auth_token(const MCPMessage& msg) {
        const char* token_ptr = nullptr;
        size_t token_len = 0;
        if (extract_string(msg.params_start, msg.params_len, "auth_token", token_ptr, token_len)) {
            return std::string(token_ptr, token_len);
        }
        return "";
    }

protected:
    // Override to advertise the tools this server provides.
    virtual std::vector<ToolDefinition> list_tools() { return {}; }

    // Override to handle a tool invocation. User is provided for authorization checks.
    //   name      — tool name from "name" field
    //   args_json — raw JSON object from "arguments" field (at least "{}")
    //   user      — authenticated user (contains role, permissions); null if auth disabled
    virtual ToolResult call_tool(const std::string& /*name*/,
                                 const std::string& /*args_json*/,
                                 const User* /*user*/ = nullptr) {
        return {"unknown tool", /*is_error=*/true};
    }

private:
    void dispatch(const MCPMessage& msg);
    void handle_initialize(const MCPMessage& msg);
    void handle_tools_list(const MCPMessage& msg);
    void handle_tools_call(const MCPMessage& msg, const User* user);

    void send_response(std::optional<int> id, const std::string& result_json);
    void send_error(std::optional<int> id, int code, const char* message);

    // Extract a quoted-string value by key from a JSON object span.
    // Returns the string content as a zero-copy span into the input buffer.
    // out_start and out_len receive the pointer and length (content only, no quotes).
    static bool extract_string(const char* json, size_t len, const char* key,
                               const char*& out_start, size_t& out_len);

    // Locate the raw value span for a key in a JSON object (handles nested).
    static bool extract_value_span(const char* json, size_t len, const char* key,
                                   const char** out_start, size_t* out_len);

    std::string _server_name      = "mcptoolkit";
    std::string _server_version   = "0.1.0";
    std::string _protocol_version = "2024-11-05";
    InputValidationHandler _validator;
    AuthenticationHandler _auth_handler;
    RoleBasedAccessControl _rbac;
    bool _auth_enabled = false;
    bool _rbac_enabled = false;
};

} // namespace mcptoolkit