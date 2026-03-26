#include "../../pch.h"
#include <cctype>
#include <cstring>
#include "json_msg.h"
#include "json_parser.h"

namespace mcptoolkit {


// ---------------------------------------------------------------------------
// Primitives
// ---------------------------------------------------------------------------

void JsonParser::skip_ws() {
    while (pos < len && std::isspace(static_cast<unsigned char>(input[pos])))
        ++pos;
}

// Advance past a JSON string. pos must be on the opening '"' on entry;
// leaves pos one past the closing '"'. Returns false on malformed input.
bool JsonParser::skip_string() {
    if (pos >= len || input[pos] != '"') return false;
    ++pos;  // consume opening quote
    while (pos < len && input[pos] != '"') {
        if (input[pos] == '\\') {
            ++pos;                        // skip escape lead
            if (pos >= len) return false; // truncated escape — no char to skip
        }
        ++pos;
    }
    if (pos >= len) return false;  // no closing quote
    ++pos;                          // consume closing quote
    return true;
}

// Advance past a container ({ or [). pos must be on `open` on entry;
// leaves pos one past the matching `close`. Rejects depth > kMaxDepth.
bool JsonParser::skip_container(char open, char close) {
    if (pos >= len || input[pos] != open) return false;
    ++pos;
    int depth = 1;
    while (pos < len && depth > 0) {
        char c = input[pos];
        if (c == '"') {
            if (!skip_string()) return false;
            continue;   // skip_string already advanced pos
        }
        ++pos;
        if (c == open) {
            if (++depth > kMaxDepth) return false;
        } else if (c == close) {
            --depth;
        }
    }
    return depth == 0;  // false if truncated before matching close
}

// Advance past any JSON value. If out_start/out_len are non-null they receive
// the span of the value (pointer into input[], length in bytes).
bool JsonParser::skip_value(const char** out_start, size_t* out_len) {
    skip_ws();
    if (pos >= len) return false;

    const char* const start = input + pos;
    const char c = input[pos];

    if (c == '"') {
        if (!skip_string()) return false;
    } else if (c == '{') {
        if (!skip_container('{', '}')) return false;
    } else if (c == '[') {
        if (!skip_container('[', ']')) return false;
    } else if (c == 't') {
        if (len - pos < 4 || std::memcmp(input + pos, "true",  4) != 0) return false;
        pos += 4;
    } else if (c == 'f') {
        if (len - pos < 5 || std::memcmp(input + pos, "false", 5) != 0) return false;
        pos += 5;
    } else if (c == 'n') {
        if (len - pos < 4 || std::memcmp(input + pos, "null",  4) != 0) return false;
        pos += 4;
    } else if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
        if (c == '-') {
            ++pos;
            if (pos >= len || !std::isdigit(static_cast<unsigned char>(input[pos])))
                return false;  // bare '-' is not a valid number
        }
        while (pos < len && std::isdigit(static_cast<unsigned char>(input[pos]))) ++pos;
        if (pos < len && input[pos] == '.') {
            ++pos;
            while (pos < len && std::isdigit(static_cast<unsigned char>(input[pos]))) ++pos;
        }
        if (pos < len && (input[pos] == 'e' || input[pos] == 'E')) {
            ++pos;
            if (pos < len && (input[pos] == '+' || input[pos] == '-')) ++pos;
            while (pos < len && std::isdigit(static_cast<unsigned char>(input[pos]))) ++pos;
        }
    } else {
        return false;
    }

    if (out_start) *out_start = start;
    if (out_len)   *out_len   = static_cast<size_t>(input + pos - start);
    return true;
}

// Parse a JSON string value into str_start/str_len (content only, no quotes).
// Skips leading whitespace. pos must be before or on the opening '"'.
bool JsonParser::parse_string(const char*& str_start, size_t& str_len) {
    skip_ws();
    if (pos >= len || input[pos] != '"') return false;
    ++pos;  // consume opening quote
    str_start = input + pos;
    while (pos < len && input[pos] != '"') {
        if (input[pos] == '\\') {
            ++pos;                        // skip escape lead
            if (pos >= len) return false; // truncated escape
        }
        ++pos;
    }
    if (pos >= len) return false;  // no closing quote
    str_len = static_cast<size_t>(input + pos - str_start);
    ++pos;  // consume closing quote
    return true;
}

// Parse a JSON integer into num. Validates at least one digit, guards against
// overflow, and rejects bare '-'. pos must be before or on the first digit/'-'.
bool JsonParser::parse_int(int& num) {
    skip_ws();
    if (pos >= len) return false;

    const bool negative = (input[pos] == '-');
    if (negative) {
        ++pos;
        if (pos >= len) return false;
    }
    if (!std::isdigit(static_cast<unsigned char>(input[pos]))) return false;

    long long val = 0;
    while (pos < len && std::isdigit(static_cast<unsigned char>(input[pos]))) {
        val = val * 10 + (input[pos] - '0');
        if (val > 2147483647LL) return false;  // exceeds INT_MAX — reject
        ++pos;
    }
    num = negative ? static_cast<int>(-val) : static_cast<int>(val);
    return true;
}

// ---------------------------------------------------------------------------
// Error object
// ---------------------------------------------------------------------------

bool JsonParser::parse_error_object(MCPMessage& msg) {
    skip_ws();
    if (pos >= len || input[pos] != '{') return false;
    ++pos;

    while (true) {
        skip_ws();
        if (pos >= len) return false;
        if (input[pos] == '}') { ++pos; break; }

        const char* key; size_t klen;
        if (!parse_string(key, klen)) return false;

        skip_ws();
        if (pos >= len || input[pos] != ':') return false;
        ++pos;

        if      (klen == 4 && std::memcmp(key, "code",    4) == 0) {
            if (!parse_int(msg.error_code)) return false;
        } else if (klen == 7 && std::memcmp(key, "message", 7) == 0) {
            if (!parse_string(msg.error_msg, msg.error_msg_len)) return false;
        } else {
            if (!skip_value()) return false;
        }

        skip_ws();
        if (pos >= len) return false;
        if (input[pos] == ',') { ++pos; continue; }
        if (input[pos] != '}') return false;
        // '}' will be consumed at the top of the next iteration
    }

    msg.has_error = true;
    return true;
}

// ---------------------------------------------------------------------------
// Top-level parser
// ---------------------------------------------------------------------------

bool JsonParser::parse(MCPMessage& msg) {
    msg.raw_input = input;
    msg.raw_len   = len;

    skip_ws();
    if (pos >= len || input[pos] != '{') return false;
    ++pos;

    while (true) {
        skip_ws();
        if (pos >= len) return false;
        if (input[pos] == '}') { ++pos; break; }

        const char* key; size_t klen;
        if (!parse_string(key, klen)) return false;

        skip_ws();
        if (pos >= len || input[pos] != ':') return false;
        ++pos;

        if (klen == 7 && std::memcmp(key, "jsonrpc", 7) == 0) {
            if (!skip_value()) return false;
        } else if (klen == 6 && std::memcmp(key, "method", 6) == 0) {
            if (!parse_string(msg.method, msg.method_len)) return false;
            msg.is_request = true;
        } else if (klen == 2 && std::memcmp(key, "id", 2) == 0) {
            if (!parse_int(msg.id)) return false;
        } else if (klen == 6 && std::memcmp(key, "params", 6) == 0) {
            if (!skip_value(&msg.params_start, &msg.params_len)) return false;
        } else if (klen == 6 && std::memcmp(key, "result", 6) == 0) {
            msg.is_response = true;
            msg.has_result  = true;
            if (!skip_value(&msg.result_start, &msg.result_len)) return false;
        } else if (klen == 5 && std::memcmp(key, "error", 5) == 0) {
            if (!parse_error_object(msg)) return false;
            msg.is_response = true;
        } else {
            if (!skip_value()) return false;
        }

        skip_ws();
        if (pos >= len) return false;
        if (input[pos] == ',') { ++pos; continue; }
        if (input[pos] != '}') return false;
        // '}' will be consumed at the top of the next iteration
    }

    return true;
}

// ---------------------------------------------------------------------------
// MCPMessage static factories
// ---------------------------------------------------------------------------

MCPMessage MCPMessage::parse(const char* json_str, size_t json_len) {
    MCPMessage msg;
    if (json_str && json_len > 0) {
        JsonParser parser(json_str, json_len);
        msg.valid = parser.parse(msg);
    }
    return msg;
}

MCPMessage MCPMessage::parse(const std::string& json_str) {
    return parse(json_str.c_str(), json_str.size());
}

} // namespace mcptoolkit
