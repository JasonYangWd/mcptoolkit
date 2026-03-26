#include "../pch.h"
#include <iostream>
#include <algorithm>
#include "mcp_adapter.h"
#include "json/json_builder.h"

namespace mcptoolkit {

MCPAdapter::MCPAdapter() {}

// =============================================================================
// Main loop
// =============================================================================

void MCPAdapter::run() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        // Strip a trailing '\r' in case of Windows-style CRLF over stdio
        if (!line.empty() && line.back() == '\r') line.pop_back();

        MCPMessage msg = MCPMessage::parse(line.c_str(), line.size());
        if (!msg.valid) {
            send_error(-1, -32700, "Parse error");
            continue;
        }
        dispatch(msg);
    }
}

// =============================================================================
// Dispatch
// =============================================================================

void MCPAdapter::dispatch(const MCPMessage& msg) {
    if (!msg.is_request) return;  // ignore bare responses we didn't expect

    std::string method(msg.method, msg.method_len);

    // Notifications (no id) — handle silently, never send a response
    if (msg.id == -1) {
        // "notifications/initialized" signals the client finished its init
        return;
    }

    if (method == "initialize")  { handle_initialize(msg);  return; }
    if (method == "tools/list")  { handle_tools_list(msg);  return; }
    if (method == "tools/call")  { handle_tools_call(msg);  return; }

    send_error(msg.id, -32601, "Method not found");
}

// =============================================================================
// initialize
// =============================================================================

void MCPAdapter::handle_initialize(const MCPMessage& msg) {
    // Build: {"jsonrpc":"2.0","id":<n>,"result":{"protocolVersion":"...","capabilities":{"tools":{}},"serverInfo":{...}}}
    JsonBuilder b;
    b.start_object();
      b.add_field("jsonrpc", "2.0");
      b.add_field_number("id", msg.id);
      b.add_object_field("result");
        b.add_field("protocolVersion", _protocol_version.c_str());
        b.add_object_field("capabilities");
          b.add_object_field("tools");
          b.close_object_field();
        b.close_object_field();
        b.add_object_field("serverInfo");
          b.add_field("name",    _server_name.c_str());
          b.add_field("version", _server_version.c_str());
        b.close_object_field();
      b.close_object_field();
    b.end_object();

    std::cout << b.get() << '\n';
    std::cout.flush();
}

// =============================================================================
// tools/list
// =============================================================================

void MCPAdapter::handle_tools_list(const MCPMessage& msg) {
    auto tools = list_tools();

    JsonBuilder b;
    b.start_object();
      b.add_field("jsonrpc", "2.0");
      b.add_field_number("id", msg.id);
      b.add_object_field("result");
        b.add_array_field("tools");
        for (const auto& t : tools) {
            // Build each tool descriptor as a raw JSON object
            JsonBuilder tb;
            tb.start_object();
              tb.add_field("name",        t.name.c_str());
              tb.add_field("description", t.description.c_str());
              tb.add_object_field("inputSchema");
                tb.add_field("type", "object");
                tb.add_object_field("properties");
                for (const auto& p : t.params) {
                    tb.add_object_field(p.name.c_str());
                      tb.add_field("type",        p.type.c_str());
                      tb.add_field("description", p.description.c_str());
                    tb.close_object_field();
                }
                tb.close_object_field();
                tb.add_array_field("required");
                for (const auto& p : t.params) {
                    if (p.required) tb.add_array_element(p.name.c_str());
                }
                tb.close_array_field();
              tb.close_object_field();
            tb.end_object();
            b.add_array_element_raw(tb.get().c_str());
        }
        b.close_array_field();
      b.close_object_field();
    b.end_object();

    std::cout << b.get() << '\n';
    std::cout.flush();
}

// =============================================================================
// tools/call
// =============================================================================

void MCPAdapter::handle_tools_call(const MCPMessage& msg) {
    if (!msg.params_start || msg.params_len == 0) {
        send_error(msg.id, -32602, "Invalid params");
        return;
    }

    std::string name = extract_string(msg.params_start, msg.params_len, "name");
    if (name.empty()) {
        send_error(msg.id, -32602, "Missing tool name");
        return;
    }

    // "arguments" is optional — default to empty object
    const char* args_start = nullptr;
    size_t      args_len   = 0;
    std::string args_json  = "{}";
    if (extract_value_span(msg.params_start, msg.params_len,
                           "arguments", &args_start, &args_len)) {
        args_json.assign(args_start, args_len);
    }

    ToolResult result = call_tool(name, args_json);

    // Build: {"jsonrpc":"2.0","id":<n>,"result":{"content":[{"type":"text","text":"..."}],"isError":<bool>}}
    JsonBuilder cb;
    cb.start_object();
      cb.add_field("type", "text");
      cb.add_field("text", result.text.c_str());
    cb.end_object();

    JsonBuilder b;
    b.start_object();
      b.add_field("jsonrpc", "2.0");
      b.add_field_number("id", msg.id);
      b.add_object_field("result");
        b.add_array_field("content");
          b.add_array_element_raw(cb.get().c_str());
        b.close_array_field();
        b.add_field_bool("isError", result.is_error);
      b.close_object_field();
    b.end_object();

    std::cout << b.get() << '\n';
    std::cout.flush();
}

// =============================================================================
// Send helpers
// =============================================================================

void MCPAdapter::send_response(int id, const std::string& result_json) {
    JsonBuilder b;
    b.start_object();
      b.add_field("jsonrpc", "2.0");
      b.add_field_number("id", id);
      b.add_field_raw("result", result_json.c_str());
    b.end_object();
    std::cout << b.get() << '\n';
    std::cout.flush();
}

void MCPAdapter::send_error(int id, int code, const char* message) {
    JsonBuilder b;
    b.start_object();
      b.add_field("jsonrpc", "2.0");
      b.add_field_number("id", id);
      b.add_object_field("error");
        b.add_field_number("code", code);
        b.add_field("message", message);
      b.close_object_field();
    b.end_object();
    std::cout << b.get() << '\n';
    std::cout.flush();
}

// =============================================================================
// JSON extraction helpers
// =============================================================================

// Find "key":"<value>" in a JSON object span and return the unescaped value.
std::string MCPAdapter::extract_string(const char* json, size_t len, const char* key) {
    std::string needle = std::string("\"") + key + "\"";
    const char* p   = json;
    const char* end = json + len;

    while (p < end) {
        const char* found = std::search(p, end,
                                        needle.c_str(), needle.c_str() + needle.size());
        if (found == end) break;
        p = found + needle.size();

        // skip whitespace then ':'
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
        if (p >= end || *p != ':') continue;
        ++p;
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;

        if (p >= end || *p != '"') continue;
        ++p;  // skip opening quote

        std::string value;
        while (p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) ++p;  // minimal unescape: skip backslash
            value += *p++;
        }
        return value;
    }
    return {};
}

// Locate the raw value span for "key" inside a JSON object, handling nesting.
bool MCPAdapter::extract_value_span(const char* json, size_t len, const char* key,
                                    const char** out_start, size_t* out_len) {
    std::string needle = std::string("\"") + key + "\"";
    const char* p   = json;
    const char* end = json + len;

    const char* found = std::search(p, end,
                                    needle.c_str(), needle.c_str() + needle.size());
    if (found == end) return false;
    p = found + needle.size();

    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    if (p >= end || *p != ':') return false;
    ++p;
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    if (p >= end) return false;

    const char* val_start = p;

    if (*p == '{' || *p == '[') {
        char open  = *p;
        char close = (open == '{') ? '}' : ']';
        int  depth   = 0;
        bool in_str  = false;
        while (p < end) {
            if (in_str) {
                if (*p == '\\')      { ++p; }  // skip escaped char
                else if (*p == '"')  { in_str = false; }
            } else {
                if      (*p == '"')   { in_str = true; }
                else if (*p == open)  { ++depth; }
                else if (*p == close) { if (--depth == 0) { ++p; break; } }
            }
            ++p;
        }
    } else if (*p == '"') {
        ++p;
        while (p < end && *p != '"') {
            if (*p == '\\') ++p;
            ++p;
        }
        if (p < end) ++p;  // closing quote
    } else {
        while (p < end && *p != ',' && *p != '}' && *p != ']') ++p;
    }

    *out_start = val_start;
    *out_len   = static_cast<size_t>(p - val_start);
    return true;
}

} // namespace mcptoolkit
