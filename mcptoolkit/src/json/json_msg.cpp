#include "../../pch.h"
#include <cctype>
#include <cstring>
#include "json_msg.h"
#include "json_parser.h"

namespace mcptoolkit {

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
