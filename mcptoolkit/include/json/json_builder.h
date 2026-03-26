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
    JsonBuilder() {
        buffer.reserve(4096);  // Pre-allocate typical MCP message size
    }

    void start_object() {
        buffer += '{';
    }

    void end_object() {
        buffer += '}';
    }

    void add_field(const char* key, const char* value) {
        if (!buffer.empty() && buffer.back() != '{') buffer += ',';
        buffer += '"';
        buffer += key;
        buffer += "\":\"";
        append_escaped(buffer, value);
        buffer += '"';
    }

    void add_field_number(const char* key, int value) {
        if (!buffer.empty() && buffer.back() != '{') buffer += ',';
        buffer += '"';
        buffer += key;
        buffer += "\":";
        buffer += std::to_string(value);
    }

    void add_field_double(const char* key, double value) {
        if (!buffer.empty() && buffer.back() != '{') buffer += ',';
        buffer += '"';
        buffer += key;
        buffer += "\":";
        char buf[32];
        snprintf(buf, sizeof(buf), "%.15g", value);
        buffer += buf;
    }

    void add_field_bool(const char* key, bool value) {
        if (!buffer.empty() && buffer.back() != '{') buffer += ',';
        buffer += '"';
        buffer += key;
        buffer += "\":";
        buffer += value ? "true" : "false";
    }

    void add_field_raw(const char* key, const char* value) {
        if (!buffer.empty() && buffer.back() != '{' && buffer.back() != '[') buffer += ',';
        buffer += '"';
        buffer += key;
        buffer += "\":";
        buffer += value;
    }

    void add_object_field(const char* key) {
        if (!buffer.empty() && buffer.back() != '{') buffer += ',';
        buffer += '"';
        buffer += key;
        buffer += "\":{";
    }

    void close_object_field() {
        buffer += '}';
    }

    void add_array_field(const char* key) {
        if (!buffer.empty() && buffer.back() != '{') buffer += ',';
        buffer += '"';
        buffer += key;
        buffer += "\":[";
    }

    void close_array_field() {
        buffer += ']';
    }

    void add_array_element(const char* value) {
        if (!buffer.empty() && buffer.back() != '[') buffer += ',';
        buffer += '"';
        append_escaped(buffer, value);
        buffer += '"';
    }

    void add_array_element_raw(const char* value) {
        if (!buffer.empty() && buffer.back() != '[') buffer += ',';
        buffer += value;
    }

    const std::string& get() const {
        return buffer;
    }

    void reset() {
        buffer.clear();
    }

    const char* c_str() const {
        return buffer.c_str();
    }

    size_t size() const {
        return buffer.size();
    }
};

} // namespace mcptoolkit
