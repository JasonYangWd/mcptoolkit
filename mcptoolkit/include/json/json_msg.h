#pragma once

#include <string>
#include <cstring>

namespace mcptoolkit {

    // A parsed JSON-RPC 2.0 / MCP message.
    //
    // All const char* fields point directly into the buffer passed to parse().
    // They are NOT null-terminated. They are only valid while that buffer
    // remains alive and unmodified. Always check .valid before using any field.
    struct MCPMessage {

        // Request fields
        const char* method;         // method string content (no enclosing quotes)
        size_t      method_len;

        int         id;             // request/response id; -1 if absent (notification)

        const char* params_start;   // params JSON value (includes braces/brackets)
        size_t      params_len;

        // Response fields
        const char* result_start;   // result JSON value (any type)
        size_t      result_len;

        int         error_code;
        const char* error_msg;      // error.message string content (no quotes)
        size_t      error_msg_len;

        // Classification
        bool        is_request;     // has a "method" field
        bool        is_response;    // has a "result" or "error" field
        bool        has_result;
        bool        has_error;
        bool        valid;          // parse completed without error

        // Original input span
        const char* raw_input;
        size_t      raw_len;

        // Parse a JSON-RPC 2.0 / MCP message. All pointers in the returned struct
        // reference into json_str — keep the buffer alive as long as you use the result.
        static MCPMessage parse(const char* json_str, size_t len);
        static MCPMessage parse(const std::string& json_str);

        MCPMessage()
            : method(nullptr),      method_len(0)
            , id(-1)
            , params_start(nullptr), params_len(0)
            , result_start(nullptr), result_len(0)
            , error_code(0), error_msg(nullptr), error_msg_len(0)
            , is_request(false), is_response(false)
            , has_result(false),    has_error(false)
            , valid(false)
            , raw_input(nullptr),   raw_len(0)
        {}
    };

} // namespace mcptoolkit
