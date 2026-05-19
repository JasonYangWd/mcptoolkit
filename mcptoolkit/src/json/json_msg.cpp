#include "../../pch.h"
#include <cctype>
#include <cstring>
#include "json_msg.h"
#include "json_parser.h"

namespace mcptoolkit {

// ---------------------------------------------------------------------------
// MCPMessage static factories
// ---------------------------------------------------------------------------

MCPMessage MCPMessage::parse(const char* json_str, size_t json_len, size_t max_bytes) {
    MCPMessage msg;
    if (json_str && json_len > 0) {
        // Check input size limit (CWE-400 mitigation)
        if (json_len > max_bytes) {
            msg.raw_input = json_str;
            msg.raw_len = json_len;
            msg.error_code = -32700;  // Parse error (JSON-RPC 2.0)
            msg.valid = false;
            return msg;
        }
        JsonParser parser(json_str, json_len, max_bytes);
        msg.valid = parser.parse(msg);
    }
    return msg;
}

MCPMessage MCPMessage::parse(const std::string& json_str, size_t max_bytes) {
    return parse(json_str.c_str(), json_str.size(), max_bytes);
}

} // namespace mcptoolkit
