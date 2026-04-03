#pragma once

#include <string>
#include <cstdio>

namespace mcptoolkit {

// MCP protocol message builder for JSON-RPC 2.0 / MCP protocol.
// Constructs messages with these standard fields:
// {
//   "jsonrpc": "2.0",
//   "id": <number>,
//   "method": "<string>",
//   "params": { /* object */ },
//   "result": { /* value */ },
//   "error": { "code": <int>, "message": "<string>" }
// }
class JsonBuilder {
private:
    std::string buffer;

    // Append a string with JSON escaping for quotes and backslashes.
    static void append_escaped(std::string& buf, const char* s) {
        if (!s) return;
        for (; *s; ++s) {
            if (*s == '"' || *s == '\\') buf += '\\';
            buf += *s;
        }
    }

public:
    JsonBuilder();

    void start_object();
    void end_object();

    void add_field(const char* key, const char* value);
    void add_field_number(const char* key, int value);
    void add_field_double(const char* key, double value);
    void add_field_bool(const char* key, bool value);
    void add_field_raw(const char* key, const char* value);

    void add_object_field(const char* key);
    void close_object_field();

    void add_array_field(const char* key);
    void close_array_field();

    void add_array_element(const char* value);
    void add_array_element_raw(const char* value);

    const std::string& get() const;
    void reset();
    const char* c_str() const;
    size_t size() const;
};

} // namespace mcptoolkit
