#pragma once

#include "json_msg.h"

namespace mcptoolkit {

    // Internal zero-copy parser. Use MCPMessage::parse() instead.
    class JsonParser {
    public:
        JsonParser(const char* json_str, size_t length)
            : input(json_str), len(length), pos(0) {}

        bool parse(MCPMessage& msg);

    private:
        const char* const input;
        const size_t      len;
        size_t            pos;

        // Maximum nesting depth for objects and arrays.
        static constexpr int kMaxDepth = 64;

        void skip_ws();
        bool skip_string();
        bool skip_container(char open, char close);
        bool skip_value(const char** out_start = nullptr, size_t* out_len = nullptr);
        bool parse_string(const char*& str_start, size_t& str_len);
        bool parse_int(int& num);
        bool parse_error_object(MCPMessage& msg);
    };

} // namespace mcptoolkit
