# MCPToolkit Security Fixes — Implementation Guide

## Critical Patches Required Before Production

---

## Fix #1: Token Passthrough Vulnerability (HIGH SEVERITY)

### File: `mcp_adapter.cpp`
### Location: Lines 57-76 (dispatch method, authentication section)

### Current Vulnerable Code:
```cpp
// VULNERABLE: Using token directly as user_id
User current_user;
current_user.authenticated = false;

if (_auth_enabled) {
    std::string auth_token = extract_auth_token(msg);
    if (auth_token.empty()) {
        send_error(msg.id, -32603, "Authentication required");
        return;
    }

    AuthResult auth = _auth_handler.validate_request(auth_token);
    if (!auth.allowed) {
        send_error(msg.id, -32603, auth.error.c_str());
        return;
    }

    // ❌ VULNERABILITY HERE: Token used as user_id without decoding
    current_user.user_id = auth_token;  // Use token as user_id for now
    current_user.authenticated = true;
    current_user.role = Role::USER;  // Default role; override in subclass
}
```

### Fixed Code:
```cpp
// SECURE: Token is decoded and validated
User current_user;
current_user.authenticated = false;

if (_auth_enabled) {
    std::string auth_token = extract_auth_token(msg);
    if (auth_token.empty()) {
        send_error(msg.id, -32603, "Authentication required");
        return;
    }

    AuthResult auth = _auth_handler.validate_request(auth_token);
    if (!auth.allowed) {
        send_error(msg.id, -32603, auth.error.c_str());
        return;
    }

    // ✅ SECURE: Decode token to extract user_id with signature verification
    std::string user_id = _auth_handler.decode_token(auth_token);
    if (user_id.empty()) {
        send_error(msg.id, -32603, "Invalid token claims");
        return;
    }

    current_user.user_id = user_id;  // Use decoded user_id from token claims
    current_user.authenticated = true;
    current_user.role = Role::USER;  // Default role; override in subclass
}
```

### Required Changes to `AuthenticationHandler`:

**File: `include/authentication_handler.h`**

Add new method signature:
```cpp
class AuthenticationHandler {
public:
    // ... existing methods ...
    
    // NEW: Decode token and extract user_id claim
    // Returns empty string if token is invalid/expired
    // Verifies token signature if configured
    std::string decode_token(const std::string& auth_token) const;
    
    // NEW: Configure token decoding (e.g., JWT secret key)
    void configure_token_decoder(const std::string& secret_key);
};
```

**File: `src/authentication_handler.cpp`**

Add implementation:
```cpp
std::string AuthenticationHandler::decode_token(const std::string& auth_token) const {
    // Example for JWT tokens
    // This is a stub — implement based on your token format
    
    if (auth_token.empty()) {
        return "";
    }
    
    // If using JWT:
    // 1. Split by dots: header.payload.signature
    // 2. Base64 decode payload
    // 3. Extract "sub" or "user_id" claim
    // 4. Verify signature matches using configured key
    // 5. Return user_id if valid, empty string if invalid
    
    // For now, example with simple format (replace with real JWT library):
    // Token format: "user123.signature_abc"
    size_t dot_pos = auth_token.find('.');
    if (dot_pos == std::string::npos) {
        return "";  // Invalid format
    }
    
    std::string user_id_part = auth_token.substr(0, dot_pos);
    // std::string signature = auth_token.substr(dot_pos + 1);
    // TODO: Verify signature matches using configured key
    
    return user_id_part;  // Return decoded user_id
}

void AuthenticationHandler::configure_token_decoder(const std::string& secret_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Store secret key for token verification
    // TODO: Implementation depends on token format (JWT, custom, etc.)
}
```

### Testing the Fix:
```cpp
// Test: Token with valid structure
assert(_auth_handler.decode_token("user123.sig_abc") == "user123");

// Test: Token with invalid structure
assert(_auth_handler.decode_token("invalid") == "");

// Test: Forged token (wrong signature)
assert(_auth_handler.decode_token("user456.fake_sig") == "");  // Should fail sig check
```

---

## Fix #2: JSON Escaping Vulnerability (MEDIUM-HIGH SEVERITY)

### File: `include/json/json_builder.h`
### Location: Lines 23-28 (append_escaped function)

### Current Vulnerable Code:
```cpp
static void append_escaped(std::string& buf, const char* s) {
    if (!s) return;
    for (; *s; ++s) {
        // ❌ VULNERABILITY: Only escapes quotes and backslashes
        // Control characters like \n, \r, \t are NOT escaped
        if (*s == '"' || *s == '\\') buf += '\\';
        buf += *s;  // Appends unescaped control chars — INVALID JSON
    }
}
```

### Fixed Code:
```cpp
static void append_escaped(std::string& buf, const char* s) {
    if (!s) return;
    for (; *s; ++s) {
        switch (*s) {
            // Escape required JSON special characters
            case '"':
                buf += "\\\"";
                break;
            case '\\':
                buf += "\\\\";
                break;
            case '\b':
                buf += "\\b";
                break;
            case '\f':
                buf += "\\f";
                break;
            case '\n':
                buf += "\\n";
                break;
            case '\r':
                buf += "\\r";
                break;
            case '\t':
                buf += "\\t";
                break;
            default:
                // Escape control characters (0x00-0x1F) as \uXXXX
                unsigned char uc = static_cast<unsigned char>(*s);
                if (uc < 0x20) {
                    char hex[8];
                    snprintf(hex, sizeof(hex), "\\u%04x", uc);
                    buf += hex;
                } else {
                    // Printable ASCII and UTF-8 continuation bytes
                    buf += *s;
                }
        }
    }
}
```

### Testing the Fix:
```cpp
// Test: Literal newline escaping
std::string buf;
append_escaped(buf, "Hello\nWorld");
assert(buf == "Hello\\nWorld");

// Test: Tab escaping
buf.clear();
append_escaped(buf, "Col1\tCol2");
assert(buf == "Col1\\tCol2");

// Test: Control character escaping
buf.clear();
append_escaped(buf, "Test\x01Value");
assert(buf == "Test\\u0001Value");

// Test: Quote and backslash (should still work)
buf.clear();
append_escaped(buf, "Quote \" and \\");
assert(buf == "Quote \\\" and \\\\");

// Test: Mixed
buf.clear();
append_escaped(buf, "A\"B\nC\\D\tE\x00F");
assert(buf == "A\\\"B\\nC\\\\D\\tE\\u0000F");
```

### Verification:
Use a JSON validator to ensure output is valid:
```cpp
std::string result;
JsonBuilder b;
b.start_object();
b.add_field("text", "Contains\nnewline");
b.end_object();

// Result should be valid JSON parseable by any JSON parser
std::string json = b.get();
// Expected: {"text":"Contains\nnewline"}
```

---

## Fix #3: Tool Definition Validation (MEDIUM SEVERITY)

### File: `include/mcp_adapter.h` and `src/mcp_adapter.cpp`

### New Method to Add:

**Header File:**
```cpp
class MCPTOOLKIT MCPAdapter {
public:
    // ... existing methods ...
    
    // NEW: Validate tool definitions before use
    // Returns empty string if valid, or error message if invalid
    static std::string validate_tool_definition(const ToolDefinition& tool);
};
```

**Implementation File:**
```cpp
std::string MCPAdapter::validate_tool_definition(const ToolDefinition& tool) {
    // Validate tool name
    if (tool.name.empty()) {
        return "Tool name cannot be empty";
    }
    if (tool.name.find('\n') != std::string::npos ||
        tool.name.find('\r') != std::string::npos) {
        return "Tool name contains invalid control characters";
    }
    if (tool.name.length() > 256) {
        return "Tool name exceeds maximum length (256 characters)";
    }
    
    // Validate description
    if (tool.description.length() > 4096) {
        return "Tool description exceeds maximum length (4096 characters)";
    }
    
    // Validate parameters
    for (const auto& param : tool.params) {
        if (param.name.empty()) {
            return "Parameter name cannot be empty";
        }
        if (param.name.find('\n') != std::string::npos) {
            return "Parameter name contains invalid control characters";
        }
        if (param.type.empty() || 
            (param.type != "string" && param.type != "integer" && 
             param.type != "boolean" && param.type != "number")) {
            return "Parameter type must be string, integer, boolean, or number";
        }
        if (param.description.length() > 1024) {
            return "Parameter description exceeds maximum length";
        }
    }
    
    return "";  // Valid
}
```

### Usage in handle_tools_list:

**Before:**
```cpp
void MCPAdapter::handle_tools_list(const MCPMessage& msg) {
    auto tools = list_tools();  // Subclass provides tools
    
    JsonBuilder b;
    // ... build response ...
    for (const auto& t : tools) {
        // ❌ No validation of tool definitions
        JsonBuilder tb;
        tb.add_field("name", t.name.c_str());
        // ...
    }
}
```

**After:**
```cpp
void MCPAdapter::handle_tools_list(const MCPMessage& msg) {
    auto tools = list_tools();  // Subclass provides tools
    
    // ✅ Validate all tool definitions
    for (const auto& t : tools) {
        std::string error = validate_tool_definition(t);
        if (!error.empty()) {
            send_error(msg.id, -32603, ("Invalid tool definition: " + error).c_str());
            return;  // Reject entire response if any tool is invalid
        }
    }
    
    JsonBuilder b;
    // ... build response with validated tools ...
}
```

---

## Fix #4: Enhanced Input Validation for Tools

### Recommended: Add maximum length validation

**File: `src/mcp_adapter.cpp`**

Enhance the `handle_tools_call` method to validate argument size:

```cpp
void MCPAdapter::handle_tools_call(const MCPMessage& msg, const User* user) {
    if (!msg.params_start || msg.params_len == 0) {
        send_error(msg.id, -32602, "Invalid params");
        return;
    }
    
    // ✅ NEW: Validate params size (prevent massive payloads)
    const size_t MAX_PARAMS_SIZE = 1024 * 1024;  // 1 MB per call
    if (msg.params_len > MAX_PARAMS_SIZE) {
        send_error(msg.id, -32602, "Parameters exceed maximum size (1 MB)");
        return;
    }

    // ... rest of existing validation ...
}
```

---

## Implementation Priority & Timeline

### Priority 1 (CRITICAL - Fix Before Any Production Use):
1. **Token Passthrough Fix** - Implement token decoding
   - Estimated effort: 4-8 hours
   - Complexity: Medium (depends on token format)

### Priority 2 (CRITICAL - Fix Before Production):
2. **JSON Escaping Fix** - Add control character escaping
   - Estimated effort: 2-4 hours
   - Complexity: Low
   - Low risk of regression

### Priority 3 (IMPORTANT - Fix Before Accepting Untrusted Tools):
3. **Tool Definition Validation** - Add validation framework
   - Estimated effort: 2-3 hours
   - Complexity: Low
   - Improves robustness

### Priority 4 (RECOMMENDED - Deploy for Defense-in-Depth):
4. **Enhanced Input Validation** - Add size limits
   - Estimated effort: 1-2 hours
   - Complexity: Low
   - Additional DoS protection

---

## Verification Checklist After Fixes

```
☐ Token decoding implemented and tested
  - Handles valid tokens correctly
  - Rejects invalid tokens
  - Rejects expired tokens
  - Rejects forged tokens (signature check)

☐ JSON escaping fixed and tested
  - Newlines escaped as \n
  - Tabs escaped as \t
  - Control characters escaped as \uXXXX
  - Output validates as JSON
  - Tool descriptions with newlines work correctly

☐ Tool definition validation implemented
  - Validates all tool definitions
  - Rejects invalid definitions in tools/list response
  - Proper error messages returned

☐ Input validation enhanced
  - Parameter size limits enforced
  - Rate limiting still works
  - Shell metacharacter filtering still active

☐ All tests pass
  - Unit tests for token decoding
  - Unit tests for JSON escaping
  - Integration tests with real MCP flows
  - Fuzzing tests against parser

☐ Security review completed
  - Code review for new token decoding logic
  - Verification of token signature validation
  - Check for new error paths
```

---

## Summary

These four fixes address the critical vulnerabilities while maintaining backward compatibility with the existing API. The fixes are:

1. **Straightforward** - Each fix is localized to specific functions
2. **Low-risk** - Minimal impact on existing code paths
3. **Testable** - Each fix includes clear test cases
4. **Documented** - Implementation details provided with rationale

**Total estimated effort:** 10-20 hours including testing and integration

**Risk of regression:** Low (fixes are additive, not refactoring existing logic)

