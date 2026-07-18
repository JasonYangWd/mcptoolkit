# Post 20: Tool Implementation Security in MCP — How Do I Safely Execute This?

**Reading time:** 11 minutes | **Difficulty:** Advanced  
**Published:** 2026-07-18

---

## The Sixth Question: Execution

Posts 15-19 covered the security perimeter:
- "Who are you?" (Authentication)
- "What can you do?" (Authorization)
- "What did you do?" (Audit Logging)
- "Where are your secrets?" (Configuration)
- "Can I trust this input?" (Input Validation)

But we haven't asked: **"How do I safely execute this?"**

Parameters are validated. Authorization is checked. But then what? The tool executes. **Tool implementation is where security theory meets harsh reality.** A perfectly validated parameter can still be misused. An authorized user can request an expensive operation. A safe parameter can become dangerous in the wrong execution context.

This post answers: **"What happens inside the tool, and how do I keep it safe?"**

---

## Why Tool Implementation Security Fails

Most developers treat tool implementation as straightforward coding:

1. **"The framework validates input"** — Assume parameters are safe because validation passed (false; validation prevents injection, not misuse)
2. **"Tools are user-supplied"** — Trust that tool authors will implement safely (false; tools are often rushed, third-party, or compromised)
3. **"Execution is fast"** — Skip timeouts because operations "shouldn't take long" (false; attackers request expensive operations)
4. **"Error messages help debugging"** — Log full paths and system information in errors (false; leaks internal structure)
5. **"Output is data"** — Return tool output directly to LLM without validation (false; output can inject prompts)

The cost of unsafe tool implementation is high: compromise of the MCP server, lateral movement to connected systems, or supply chain attacks if tools are shared.

---

## Real-World Tool Execution Vulnerabilities

### CVE-2023-20873: Docker CLI Arbitrary Code Execution

**CVSS:** 9.3 | **Impact:** Attacker executes arbitrary code with Docker daemon privileges

The Docker CLI allowed code execution through improper parameter handling in the build context. Parameters passed to the Docker daemon weren't properly validated, allowing attackers to inject malicious operations.

**Why this matters for MCP tools:**
If an MCP tool calls external programs (Docker, kubectl, aws CLI, etc.) without proper parameter validation, attackers can:
- Inject flags/options to change behavior
- Cause unintended operations
- Execute commands with the tool's privileges

**Example MCP scenario:**
```
LLM calls: execute_docker_build(dockerfile="Dockerfile", args="...")
Attacker injects: args="--build-arg MALICIOUS_CODE=..."
Tool executes: docker build with attacker's arguments
Result: Attacker code runs inside Docker build
```

**Root cause:** Tool accepts parameters without validating how they're used in external commands.

**Lesson:** Never concatenate parameters into command strings. Use parameterized APIs.

---

### CVE-2024-24786: Go JSON Unmarshaling Panic (DoS)

**CVSS:** 7.5 | **Impact:** Denial of service, tool crashes

Go's JSON unmarshaling could panic on certain malformed input. A tool that unmarshals untrusted JSON without recovery crashes completely, making the MCP server unresponsive.

**Why this matters for MCP tools:**
If a tool parses untrusted data (JSON parameters, API responses, file contents) without panic recovery, attackers can:
- Crash the tool
- Make MCP server unresponsive
- Cause cascading failures

**Example MCP scenario:**
```
LLM calls: parse_json(data="{...}")
Attacker sends: Deeply nested or malformed JSON
Tool calls: json.Unmarshal(data)  // Panics
Result: MCP server crashes
```

**Root cause:** No panic recovery; tool assumes input is well-formed.

**Lesson:** Recover from panics in tools. Validate data before parsing.

---

### CVE-2023-39615: npm CLI Arbitrary Code Execution

**CVSS:** 8.8 | **Impact:** Supply chain attack, malicious code execution

npm package manager executed arbitrary scripts during installation. A tool that invokes npm or similar package managers can be abused to execute attacker code.

**Why this matters for MCP tools:**
If an MCP tool invokes external tools (npm, pip, cargo, etc.), attackers can:
- Trigger script execution via package managers
- Install malicious packages
- Execute code with tool's permissions

**Example MCP scenario:**
```
LLM calls: install_package(package_name="express")
Attacker supplies: package_name="@attacker/express"
Tool executes: npm install @attacker/express
Result: Attacker's preinstall script runs with tool's privileges
```

**Root cause:** Tool doesn't validate package names or disable script execution.

**Lesson:** Use allowlists for external tools. Disable auto-execution of scripts. Validate all package/dependency names.

---

### MCP-Specific: SQL Injection via Tool Parameter

**Scenario:** A tool exposes SQL query functionality to the LLM:

```
LLM calls: database_query(sql="SELECT * FROM users WHERE id = ?")
Attacker injects: sql="SELECT * FROM users; DROP TABLE users--"

Vulnerable tool code:
    std::string query = "SELECT * FROM users WHERE id = " + user_id;
    execute_query(query);  // ❌ String concatenation

Result: Both SELECT and DROP execute; table deleted
```

**Attack variants:**
- Exfiltrate sensitive data via UNION queries
- Modify or delete data
- Escalate privileges via stored procedures
- Bypass row-level security

**Root cause:** Tool builds SQL strings instead of using parameterized queries.

**Lesson:** Always use parameterized queries. Never concatenate user input into SQL.

---

## Attack Vectors: Five Ways Tool Execution Fails

### Vector 1: Command Injection via External Tools

Tool executes external commands with user-supplied parameters:
```cpp
// ❌ VULNERABLE
std::string cmd = "curl " + user_url;
system(cmd.c_str());

// ✅ SAFE
execve("/usr/bin/curl", [user_url], environ);  // No shell interpretation
```

### Vector 2: Resource Exhaustion

Tool performs expensive operations without limits:
```cpp
// ❌ VULNERABLE
void process_file(size_t size) {
    char* buffer = malloc(size);  // No limit
    // Could allocate 10GB if requested
}

// ✅ SAFE
void process_file(size_t size) {
    if (size > MAX_FILE_SIZE) return error("File too large");
    char* buffer = malloc(size);
}
```

### Vector 3: Information Disclosure in Errors

Tool leaks sensitive information in error messages:
```cpp
// ❌ VULNERABLE
catch (FileNotFound& e) {
    return {"File /home/alice/.ssh/id_rsa not found", true};
}

// ✅ SAFE
catch (FileNotFound& e) {
    return {"File not found", true};  // No path disclosure
}
```

### Vector 4: Race Conditions in File Operations

Tool checks file properties then accesses it (TOCTOU):
```cpp
// ❌ VULNERABLE
if (stat(path, &st) == 0 && st.st_size < MAX) {
    // Between stat() and open(), attacker replaces with symlink
    int fd = open(path, O_RDONLY);
}

// ✅ SAFE
int fd = open(path, O_RDONLY);
if (fstat(fd, &st) == 0 && st.st_size < MAX) {
    // Atomic: can't be modified between open and check
}
```

### Vector 5: Timeout Exhaustion

Tool allows long-running operations without timeout:
```cpp
// ❌ VULNERABLE
while (process_next_item()) {
    // Could run forever if attacker provides infinite data
}

// ✅ SAFE
TimeoutGuard timeout(tool_name, user_id);
while (process_next_item()) {
    if (timeout.is_timeout()) {
        return error("Tool execution timeout");
    }
}
```

---

## Defense Strategy: Tool Implementation Layers

### Layer 1: Parameter Validation

Use input validation from Post 19:

```cpp
class MyTool : public MCPAdapter {
protected:
    ToolResult call_tool(const std::string& name,
                        const std::string& args_json,
                        const User* user) override {
        
        // Already done by framework:
        // - Size limits (1 MB)
        // - Shell metacharacter detection
        // - Path traversal detection
        // - URL-encoded metacharacter detection
        
        // Now validate semantically for your tool
        if (name == "file_read") {
            return execute_file_read(args_json);
        }
        
        return {"unknown tool", true};
    }
};
```

**Key:** Framework handles syntax validation; implement tool-specific validation.

---

### Layer 2: Sandboxing & Timeouts

Use ToolExecutionGuard for timeout enforcement and audit logging:

```cpp
ToolResult MyServer::call_tool(const std::string& name,
                               const std::string& args_json,
                               const User* user) {
    
    // Create execution guard with timeout
    ToolExecutionGuard guard(name, user->user_id);
    
    // Check remaining time while executing
    if (guard.get_remaining_time().count() <= 0) {
        return {"Tool execution timeout", true};
    }
    
    // Execute tool...
    ToolResult result = execute_tool_logic(name, args_json);
    
    // Log completion (automatic on guard destruction)
    guard.log_completion(!result.is_error, result.text);
    
    return result;
}
```

**Features:**
- ✅ Automatic timeout checking
- ✅ Integrated audit logging
- ✅ RAII pattern ensures cleanup
- ✅ Remaining time tracking

---

### Layer 3: Safe Execution Methods

**Use parameterized APIs, never shell interpretation:**

```cpp
// File operations: Use canonical path checking
std::string safe_read_file(const std::string& path) {
    // Get absolute path and verify within bounds
    auto canonical = std::filesystem::canonical(base_dir / path);
    if (!canonical.string().starts_with(base_dir.string())) {
        return error("Path traversal detected");
    }
    
    // Atomic check: stat + open in one syscall
    int fd = open(canonical.c_str(), O_RDONLY);
    if (fd < 0) return error("File not found");
    
    // ... read file ...
    close(fd);
}

// SQL queries: Use parameterized statements
std::string safe_query(const std::string& user_id) {
    // Use prepared statement, not string concatenation
    auto stmt = db.prepare("SELECT * FROM users WHERE id = ?");
    stmt.bind(1, user_id);  // Parameterized: safe
    return stmt.execute();
}

// External tools: Use execve, not system()
void safe_curl(const std::string& url) {
    // execve doesn't use shell; can't chain commands
    execve("/usr/bin/curl", {url}, environ);
    // vs. system("curl " + url) which interprets shell
}
```

---

### Layer 4: Output Validation Before LLM

Use ResponseSanitizer to prevent prompt injection and information disclosure:

```cpp
ToolResult result = execute_tool_logic(name, args_json);

// Validate output before returning to LLM
std::string error_msg;
std::string safe_output = ResponseSanitizer::sanitize(
    result.text,
    {.max_response_size = 1024 * 1024, 
     .redact_paths = true,
     .escape_control_chars = true},
    error_msg);

if (safe_output.empty()) {
    // Sanitization failed
    log_security_event(SecurityEventCategory::VALIDATION_ERROR,
                      "Tool output validation failed: " + error_msg);
    return {error_msg, true};
}

// Safe to return to LLM
return {safe_output, false};
```

**ResponseSanitizer detects:**
- ❌ Prompt injection patterns ("ignore previous instructions")
- ❌ Size violations (> 1 MB)
- ❌ Path information (/etc/passwd → /[PATH])
- ❌ Control characters in output

---

### Layer 5: Audit & Monitoring

ToolExecutionGuard automatically logs:

```cpp
// Logs on construction:
// [timestamp] SECURITY | dispatch_error | Tool execution started: file_read
//     | user=alice,tool=file_read

// ... execution happens ...

// Logs on destruction:
// [timestamp] SECURITY | dispatch_error | Tool execution completed: file_read
//     | user=alice,tool=file_read,success=true,result_size=1024

// Or if timeout:
// [timestamp] SECURITY | timeout | Tool execution timeout: file_read
//     | user=alice,tool=file_read,success=false
```

Link this to Post 17's audit logging and MCPAnomalyDetector for real-time alerts.

---

## Safe Tool Implementation Pattern

Here's a complete example:

```cpp
class MyToolServer : public MCPAdapter {
protected:
    std::vector<ToolDefinition> list_tools() override {
        return {
            {"file_read", "Read file contents", {
                {"path", "string", "File path", true}
            }},
            {"database_query", "Execute query", {
                {"query", "string", "SQL query", true}
            }}
        };
    }

    ToolResult call_tool(const std::string& name,
                        const std::string& args_json,
                        const User* user) override {
        
        // Check user authorization
        if (!user || user->role < Role::USER) {
            return {"Unauthorized", true};
        }
        
        // Check rate limits
        if (!rate_limiter.allow_request(user->user_id)) {
            return {"Rate limit exceeded", true};
        }
        
        // Create execution guard (timeout + logging)
        ToolExecutionGuard guard(name, user->user_id);
        
        ToolResult result;
        if (name == "file_read") {
            result = execute_file_read(args_json, guard);
        } else if (name == "database_query") {
            result = execute_query(args_json, guard);
        } else {
            return {"Unknown tool", true};
        }
        
        // Validate output before returning
        std::string error_msg;
        std::string safe = ResponseSanitizer::sanitize(
            result.text, {}, error_msg);
        
        if (safe.empty()) {
            guard.log_completion(false, error_msg);
            return {error_msg, true};
        }
        
        guard.log_completion(!result.is_error, safe);
        return {safe, result.is_error};
    }

private:
    ToolResult execute_file_read(const std::string& args_json,
                                 ToolExecutionGuard& guard) {
        // 1. Extract and validate parameter
        std::string path;
        if (!extract_path_param(args_json, path)) {
            return {"Invalid parameters", true};
        }
        
        // 2. Check timeout before expensive operation
        if (guard.get_remaining_time().count() <= 0) {
            return {"Operation timeout", true};
        }
        
        // 3. Use safe file access (canonical path)
        auto canonical = std::filesystem::canonical(base_dir / path);
        if (!canonical.string().starts_with(base_dir.string())) {
            return {"Path traversal detected", true};
        }
        
        // 4. Atomic open + check
        int fd = open(canonical.c_str(), O_RDONLY);
        if (fd < 0) {
            return {"File not found", true};
        }
        
        // 5. Read file with size limit
        std::string content;
        const size_t MAX_SIZE = 10 * 1024 * 1024;  // 10 MB
        char buffer[4096];
        ssize_t n;
        while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
            if (content.size() + n > MAX_SIZE) {
                close(fd);
                return {"File too large", true};
            }
            content.append(buffer, n);
        }
        close(fd);
        
        return {content, false};
    }
    
    ToolResult execute_query(const std::string& args_json,
                            ToolExecutionGuard& guard) {
        // Extract parameter
        std::string query_str;
        if (!extract_query_param(args_json, query_str)) {
            return {"Invalid parameters", true};
        }
        
        // Use parameterized query (safe)
        try {
            auto stmt = db.prepare(query_str);
            // Don't concatenate user input; use bound parameters
            std::string result = stmt.execute();
            return {result, false};
        } catch (const std::exception& e) {
            // Don't expose SQL errors; they leak information
            return {"Query failed", true};
        }
    }
    
    RateLimiter rate_limiter;
    std::string base_dir = "/data";
};
```

---

## Testing: Tool Security Checklist

- [ ] Does tool check authorization before executing?
- [ ] Does tool enforce timeout (no infinite operations)?
- [ ] Does tool validate all parameters semantically?
- [ ] Does tool use safe APIs (execve, parameterized queries)?
- [ ] Does tool avoid concatenating parameters into command strings?
- [ ] Does tool limit output size before returning?
- [ ] Does tool redact paths/sensitive data from errors?
- [ ] Does tool sanitize output for prompt injection?
- [ ] Does tool log all invocations for audit?
- [ ] Does tool handle panics/exceptions without crashing?
- [ ] Does tool check resource limits (disk, memory, network)?
- [ ] Have you tested with adversarial parameters?

If you answer "no" to any: tool implementation is incomplete.

---

## What's Next: Cryptographic Signing

Post 21 covers **Secure Inter-Tool Communication** — how tools can verify requests come from the MCP server and not from attackers, using cryptographic signatures.

---

## Learn More

- **CWE-78:** OS Command Injection - https://cwe.mitre.org/data/definitions/78.html
- **CWE-89:** SQL Injection - https://cwe.mitre.org/data/definitions/89.html
- **CWE-377:** Insecure Temporary File - https://cwe.mitre.org/data/definitions/377.html
- **OWASP: Command Injection:** https://owasp.org/www-community/attacks/Command_Injection
- **Docker CVE-2023-20873:** https://www.docker.com/blog/cve-2023-20873-containerd-security-advisory/
- **mcptoolkit ToolExecutionGuard:** Source: `mcptoolkit/include/tool_execution_guard.h`
- **mcptoolkit ResponseSanitizer:** Source: `mcptoolkit/include/response_sanitizer.h`

---

Subscribe to **The Secure MCP** for the next post: a practical code review checklist for MCP security.
