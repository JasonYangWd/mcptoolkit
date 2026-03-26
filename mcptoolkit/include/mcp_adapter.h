#pragma once

#include <string>
#include <vector>
#include "json/json_msg.h"

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

protected:
    // Override to advertise the tools this server provides.
    virtual std::vector<ToolDefinition> list_tools() { return {}; }

    // Override to handle a tool invocation.
    //   name      — tool name from "name" field
    //   args_json — raw JSON object from "arguments" field (at least "{}")
    virtual ToolResult call_tool(const std::string& /*name*/,
                                 const std::string& /*args_json*/) {
        return {"unknown tool", /*is_error=*/true};
    }

private:
    void dispatch(const MCPMessage& msg);
    void handle_initialize(const MCPMessage& msg);
    void handle_tools_list(const MCPMessage& msg);
    void handle_tools_call(const MCPMessage& msg);

    void send_response(int id, const std::string& result_json);
    void send_error(int id, int code, const char* message);

    // Extract a quoted-string value by key from a flat JSON object span.
    static std::string extract_string(const char* json, size_t len, const char* key);

    // Locate the raw value span for a key in a JSON object (handles nested).
    static bool extract_value_span(const char* json, size_t len, const char* key,
                                   const char** out_start, size_t* out_len);

    std::string _server_name      = "mcptoolkit";
    std::string _server_version   = "0.1.0";
    std::string _protocol_version = "2024-11-05";
};

} // namespace mcptoolkit