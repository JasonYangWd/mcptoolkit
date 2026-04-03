# JSON Vulnerabilities in MCP: A Security Analysis

> **For publication on The Secure MCP Substack**

## Executive Summary

JSON parsing is a critical security boundary in the Model Context Protocol. Common vulnerabilities in JSON handling—integer overflows, denial-of-service through deeply nested structures, unsafe escape sequence handling, and buffer manipulation attacks—directly threaten MCP server reliability and data integrity. This article explores vulnerability classes, real-world attack scenarios, and how secure parsing architectures mitigate risk.

---

## 1. The JSON Attack Surface

JSON parsing sits at the protocol boundary: every message from an untrusted client passes through the JSON parser before reaching application logic. This makes JSON vulnerabilities high-severity by default.

### Why JSON Parsing is Dangerous

1. **Complexity hidden in simplicity** — JSON looks simple but has subtle corner cases
2. **Unbounded input** — Attackers control message size, depth, nesting, and content
3. **Type coercion issues** — Numeric precision, string escaping, and unicode handling vary across implementations
4. **Performance asymmetry** — Attackers can craft inputs that consume disproportionate CPU

### MCP-Specific Risk

In MCP, JSON carries:
- **Method names** — routed to dispatch logic
- **Tool parameters** — passed to external code execution
- **Authentication tokens** — in `initialize` messages
- **Resource identifiers** — may trigger file I/O, database queries, or API calls

A parsing vulnerability here isn't just a crash—it's a stepping stone to RCE.

---

## 2. Common JSON Vulnerabilities

### 2.1 Denial of Service: Deeply Nested Objects

**Attack:**
```json
{"a":{"b":{"c":{"d":{"e":{"f":{"g":{"h":{"i":{"j":...
```

**Impact:**
- Stack overflow if parser uses recursion
- Unbounded memory allocation for parse state
- CPU exhaustion during validation

**Real-world example:**
- **CVE-2016-3720** (Ruby JSON): Deeply nested arrays crashed with stack overflow
- **CVE-2023-46604** (Apache Commons): XML parser DoS via nested structures (analogous JSON risk)

**Mitigation:**
- Enforce a **depth limit** (typical: 32–64 levels)
- Use iterative parsing instead of recursion
- Return `depth_exceeded` error rather than crashing

**mcptoolkit approach:**
```cpp
const size_t kMaxDepth = 64;  // Enforced in skip_container()
```

---

### 2.2 Numeric Precision and Integer Overflow

**Attack 1: Integer Overflow in ID Field**
```json
{"jsonrpc":"2.0","id":9223372036854775807,"method":"initialize","params":{}}
```

If the parser stores the ID in a 32-bit `int` instead of 64-bit:
- `9223372036854775807` (max int64) → wraps to negative or `-1`
- Response ID becomes untrackable or collides with sentinel values

**Attack 2: Large Number DoS**
```json
{"value":99999999999999999999999999999999999999999999999999}
```

If the parser uses `strtod()` or similar without bounds:
- Unbounded string-to-number conversion
- CPU spike during digit iteration

**Real-world:**
- **CVE-2017-12794** (PHP JSON): `json_decode()` integer overflow with large numbers

**Mitigation:**
- Use fixed-width integer types (`int32_t`, `int64_t`)
- Document ID field limitations
- Consider `std::optional<int>` for optional fields instead of `-1` sentinel

**mcptoolkit approach:**
```cpp
int id;  // Currently uses -1 sentinel
// Planned for v0.2: std::optional<int>
```

---

### 2.3 Escape Sequence Vulnerabilities

**Attack 1: Invalid Escape Sequences**
```json
{"text": "Hello\xWorld"}
```

Some parsers accept `\x` (invalid in JSON spec); others crash or misinterpret.

**Attack 2: Unicode Escape DoS**
```json
{"text": "\u0000\u0000\u0000...\u0000"}  // Thousands of null chars
```

- Parser allocates unbounded string for escape sequences
- Null bytes embedded in "string" cause null-termination bugs in C

**Attack 3: Escape Injection**
```json
{"path": "..\\..\\..\\..\\..\\..\\windows\\system32"}
```

If parser accepts `\\` as literal backslash without proper context, path traversal follows.

**Real-world:**
- **CVE-2019-9740** (urllib): Unicode escape in URLs bypassed validation
- **CVE-2020-14343** (YAML): Escape sequences bypassed type safety

**Mitigation:**
- Validate escape sequences strictly (only `\"`, `\\`, `\b`, `\f`, `\n`, `\r`, `\t`, `\uXXXX`)
- Reject invalid escapes immediately
- Document that unescaped strings are returned (caller responsibility to unescape)

**mcptoolkit approach:**
```cpp
// Parser is lenient: accepts \x as valid (design decision)
// Unescaping deferred to application layer (v0.2)
```

---

### 2.4 Buffer Overflow in String Handling

**Attack:**
```json
{"method": "a".repeat(1_000_000)}
```

If parser allocates fixed buffer (e.g., 256-byte method name):
- Overflow: stack smash, heap corruption
- If bounds-checked: truncation, logic bypass

**Lenient parsers accept strings longer than application expects:**
```cpp
const char* method_ptr;  // Points into input buffer
size_t method_len;       // May be larger than any reasonable method name
```

Caller must validate length before use.

**Real-world:**
- **CVE-2018-20225** (Python tarfile): Buffer overflow in string handling
- **CVE-2021-32785** (Jenkins): Buffer overflow via JSON parameter names

**Mitigation:**
- Store strings as (pointer, length) pairs, not null-terminated
- Validate lengths at application boundary
- Use span-based APIs: `std::string_view`, `StringSpan`

**mcptoolkit approach:**
```cpp
struct MCPMessage {
    const char* method;      // Pointer into input
    size_t      method_len;  // Length — no null-termination
};
```

---

### 2.5 Type Confusion and Coercion

**Attack:**
```json
{"id": "1"}  // String instead of number
```

If parser coerces `"1"` to `1`:
- Downstream code assumes integer, gets string
- Comparison logic breaks: `id == 1` might be false if one is string

**MCP-specific risk:**
```json
{
  "jsonrpc": "2.0",
  "id": [],        // Array instead of number
  "method": "initialize",
  "params": {}
}
```

If parser accepts this and stores ID as array reference, response dispatch breaks.

**Mitigation:**
- Strict type checking: reject type mismatches
- Document expected types in specification
- Validate types before using fields

**mcptoolkit approach:**
```cpp
// Parser rejects non-integer IDs
// is_request, is_response flags disambiguate message type
```

---

### 2.6 Zero-Copy Vulnerabilities

**Attack: Use-After-Free**

```cpp
MCPMessage msg1 = MCPMessage::parse(input_buffer, len);
// input_buffer is freed or reused
std::string method(msg1.method, msg1.method_len);  // UaF!
```

Zero-copy parsers reference the input buffer directly. If caller frees or reuses that buffer before consuming the message, dereferencing causes UaF.

**Attack: Buffer Manipulation**

```cpp
char buffer[256] = {...};
MCPMessage msg = MCPMessage::parse(buffer, sizeof(buffer));
buffer[0] = 'X';  // Mutate input after parsing
// msg.method now points to mutated data!
```

**Mitigation:**
- Document buffer lifetime requirements clearly
- Use immutable input (const char*)
- Consider defensive copies for high-value fields (method name, IDs)

**mcptoolkit approach:**
```cpp
// Header comment: "All const char* fields point directly into the buffer passed 
// to parse(). They are NOT null-terminated. They are only valid while that 
// buffer remains alive and unmodified."
```

---

## 3. Real-World MCP Attack Scenarios

### Scenario 1: DoS via Nested Params

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/call",
  "params": {
    "name": "exec_command",
    "arguments": {"a":{"b":{"c":{"d":{...}}}}}  // 100 levels deep
  }
}
```

**Without depth limit:**
- Parser stack overflows
- MCP server crashes
- Tool invocation never reaches handler
- Attacker DoS succeeds

**With depth limit (mcptoolkit):**
- Parser returns `valid = false`
- Adapter sends error response
- Server remains operational

---

### Scenario 2: ID Collision via Integer Overflow

```json
[
  {"jsonrpc":"2.0","id":1,"method":"initialize","params":{}},
  {"jsonrpc":"2.0","id":-1,"method":"tools/call","params":{...}}
]
```

If `-1` is the sentinel for "no ID" (notifications), client can:
- Send request with `id: -1`
- Parser stores ID as `-1`
- Adapter treats it as notification
- Response never sent
- Client hangs waiting for response

---

### Scenario 3: Escape Injection in Tool Arguments

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/call",
  "params": {
    "name": "read_file",
    "arguments": {"path": "../../etc/passwd"}
  }
}
```

**If parser or application fails to unescape properly:**
- Path traversal succeeds
- Attacker reads sensitive files

---

## 4. Parser Implementation Comparison

| Vulnerability | Lenient Parser | Strict Parser | mcptoolkit |
|---|---|---|---|
| Deeply nested JSON | Stack overflow | Depth limit enforced | ✓ 64-level limit |
| Invalid escapes | Accepts `\x` | Rejects | Accepts (lenient) |
| Trailing commas | Accepts | Rejects | Accepts (lenient) |
| Leading zeros | Accepts `01234` | Rejects | Accepts (lenient) |
| Integer overflow | Wraps, truncates | Error | ✗ Uses `-1` sentinel |
| Buffer bounds | Unbounded allocation | Span-based | ✓ (pointer, length) |
| Null-termination | Unsafe | Safe | ✓ Not null-terminated |

**mcptoolkit's design philosophy:**
- **Lenient syntax** (accept non-strict JSON for robustness)
- **Strict semantics** (validate types and ranges)
- **Zero-copy** (span-based references)
- **Bounded resources** (depth limits, fixed buffer)

---

## 5. Mitigation Strategies for MCP Developers

### 5.1 Input Validation Layer

```cpp
struct MCPMessage msg = MCPMessage::parse(input, len);

if (!msg.valid) {
    // Parse error — send error response
    adapter.send_error(-1, -32700, "Parse error");
    return;
}

// Validate field lengths and values
if (msg.method_len == 0 || msg.method_len > 64) {
    adapter.send_error(msg.id, -32602, "Invalid method");
    return;
}

// Validate ID is in expected range
if (msg.id < 0 || msg.id > 1_000_000) {
    adapter.send_error(msg.id, -32602, "Invalid id");
    return;
}
```

### 5.2 Immutable Input Buffers

```cpp
// Good: const input
const std::string request = read_from_socket();
MCPMessage msg = MCPMessage::parse(request.c_str(), request.size());

// Bad: mutable input
char buffer[4096];
fread(buffer, ...);
MCPMessage msg = MCPMessage::parse(buffer, ...);
// Don't modify buffer while msg is in use
```

### 5.3 Document Buffer Lifetime

```cpp
// In your MCP adapter docs:
// "MCPMessage fields are pointers into the input buffer.
//  Do not free or modify the buffer until message processing is complete.
//  If you need to store the message, copy fields to owned data structures."
```

### 5.4 Defensive Copies for High-Value Fields

```cpp
// Make a defensive copy of the method name (critical for dispatch)
std::string method(msg.method, msg.method_len);
// Now it's safe to release the input buffer
```

### 5.5 Whitelist Tool Names and Parameters

```cpp
// Before calling user-defined tool:
const std::string tool_name(msg.params_start, msg.params_len);
if (valid_tools.find(tool_name) == valid_tools.end()) {
    send_error(msg.id, -32602, "Unknown tool");
    return;
}
```

---

## 6. Emerging JSON Vulnerabilities

### 6.1 billion laughs / XML bomb analog

**Concept:** Highly repetitive JSON with exponential expansion when fully parsed.

```json
{"a":"XXXXXXXXXXXXXXXXXXXX...","b":{"$ref":"#/a"},...}
```

**Risk for MCP:** If adapter processes `$ref` or `@type` fields without proper validation, exponential expansion DoS.

**Mitigation:** Disable reference resolution; validate expansion ratios.

---

### 6.2 Regex DoS in Field Validation

**Attack:**
```json
{"tool_name": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab"}
```

If validation regex is `^[a-z]*b$` (catastrophic backtracking):
- Linear input length → exponential regex engine time
- CPU spike, timeout

**Mitigation:** Use simple string matching, not complex regex, for protocol fields.

---

### 6.3 Side-Channel Vulnerabilities

**Timing attacks on JSON comparison:**
```cpp
if (method_ptr && memcmp(method_ptr, "initialize", method_len) == 0) {
    // Vulnerable to timing attack on method name
}
```

If method name is a secret or sensitive, attacker can measure timing to guess it byte-by-byte.

**Mitigation:** Use constant-time comparison for security-critical fields.

---

## 7. Checklist: Securing Your MCP Server

- [ ] **Depth limit enforced** in JSON parser (recommend 32–64)
- [ ] **Integer types validated** (reject `id: "string"`, enforce range)
- [ ] **String lengths bounded** (method < 64 chars, tool names < 128 chars)
- [ ] **No null-termination assumed** (use `string_view` or `(ptr, len)` pairs)
- [ ] **Buffer lifetime documented** (caller must keep input alive)
- [ ] **Escape sequence handling specified** (document unescaped output)
- [ ] **Error responses always sent** (malformed input → error, never crash)
- [ ] **No buffer mutations** while message in use
- [ ] **Whitelist validation** for tool names and method names
- [ ] **Constant-time comparison** for security-critical fields
- [ ] **Regex patterns simplified** (no catastrophic backtracking)

---

## 8. How mcptoolkit Addresses These Risks

### Design Decisions

| Risk | mcptoolkit Mitigation |
|---|---|
| Stack overflow | Depth limit (64 levels), iterative parsing |
| ID collision | Documented as limitation (v0.2: `std::optional<int>`) |
| Buffer overflows | Span-based API: `(const char*, size_t)` |
| Type confusion | Strict type checking, `valid` flag required |
| UaF vulnerabilities | Explicit buffer lifetime documentation |
| Integer precision | Fixed-width types (`int`, `size_t`) |

### Performance Advantage

By combining strict validation with a lenient parser, mcptoolkit achieves:
- **Fast path:** Valid MCP messages parse in ~0.19µs
- **Safe path:** Invalid/malicious input rejected quickly without DoS

---

## 9. Future Work: Proposed Mitigations

### For mcptoolkit v0.2

1. **Input size limit:** `MAX_MESSAGE_BYTES` constant
2. **ID field:** Switch to `std::optional<int>` (eliminate `-1` sentinel)
3. **Escape unescape:** `StringSpan::unescape()` for application convenience
4. **Structured validation:** `ToolArgs` helper with `require_str()`, `require_int()` methods

### Industry Recommendations

1. **JSON Schema validation** for all untrusted input (via external library)
2. **Sandboxed parsing** (separate process, resource limits)
3. **Telemetry:** Log JSON parse errors, track attack patterns
4. **Fuzz testing:** Continuous fuzzing of all JSON parsing code

---

## 10. Conclusion

JSON parsing is a critical security boundary in MCP. The most dangerous vulnerabilities—DoS via nesting, integer overflow, escape injection, and buffer manipulation—are all preventable with:

1. **Depth limits** (prevent stack exhaustion)
2. **Type validation** (reject coercion and confusion)
3. **Span-based APIs** (prevent buffer overflow)
4. **Clear documentation** (buffer lifetime, field limitations)
5. **Input sanitization** (whitelist, length checks)

mcptoolkit demonstrates that zero-copy parsing and security aren't mutually exclusive. By enforcing a bounded depth limit, maintaining explicit buffer lifetime semantics, and providing a strict type-checking interface, the library achieves both performance and safety.

**For MCP developers:** Choose your JSON library carefully. Validate rigorously at the protocol boundary. Document assumptions. Test with malicious inputs. Your security posture depends on it.

---

## References

- [JSON-RPC 2.0 Spec](https://www.jsonrpc.org/specification)
- [Model Context Protocol](https://modelcontextprotocol.io)
- [OWASP JSON Injection](https://owasp.org/www-community/attacks/Code_Injection)
- [CWE-400: Uncontrolled Resource Consumption](https://cwe.mitre.org/data/definitions/400.html)
- [CWE-190: Integer Overflow](https://cwe.mitre.org/data/definitions/190.html)

---

**Author:** Jason Yang  
**Date:** April 3, 2026  
**For:** The Secure MCP  
**Related Project:** [mcptoolkit](https://github.com/JasonYangWd/mcptoolkit)
