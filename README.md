# mcptoolkit

> 🚀 **Early-Adopter Friendly MCP Framework** — Built & Tested with Claude AI
> 
> **Status:** v0.1.1 (Production-ready with auth/authz enforcement)  
> **Updated:** May 30, 2026

A high-performance C++ library for building secure [Model Context Protocol (MCP)](https://modelcontextprotocol.io) servers. Zero-copy JSON-RPC 2.0 parsing + sealed dispatch pipeline + built-in authentication & authorization.

**⚠️ EARLY ADOPTER NOTICE:** This project is actively being developed using Claude AI (agentic development). We're looking for brave developers to try it out, break it, and help shape the future of secure MCP. Use at your own risk — we welcome all feedback!

---

## 🎯 For Early Adopters

Interested in building secure MCP servers without the complexity? This toolkit is for you:

- ✅ **Built for security** — Authentication, RBAC, input validation all integrated by default
- ✅ **High performance** — Zero-copy JSON parser, sub-microsecond dispatch
- ✅ **Active development** — New features and improvements rolling out regularly
- ✅ **Transparent process** — See how the code evolves, contribute feedback, shape the roadmap

**We want your help:** Try the toolkit in a real project, find edge cases we missed, report friction points in the API. The goal is to make secure MCP development the default, not an afterthought.

---

## ⚠️ Security & Risk Disclaimer

**mcptoolkit v0.1.1 is ready for testing and evaluation, but NOT recommended for sensitive production workloads yet.** 

What's solid:
- ✅ Authentication & authorization enforcement tested and working
- ✅ Input validation integrated
- ✅ Zero-copy parser hardened against JSON attacks
- ✅ No known critical vulnerabilities

What's coming in v0.2:
- Native TLS/mTLS transport (currently use a TLS proxy)
- Audit logging integration
- Message signing and integrity checking
- Session binding to dispatch

**Before deploying:** consider your threat model and evaluate whether authentication alone meets your needs.

---

## Quick Start

```cpp
#include "json_msg.h"
using namespace mcptoolkit;

// Parse incoming message
MCPMessage msg = MCPMessage::parse(json_str, len);
if (!msg.valid) { /* error */ }

// Extract fields (zero-copy pointers)
std::string method(msg.method, msg.method_len);
int id = msg.id;
```

## Table of Contents

### Getting Started
- [Features](#features)
- [Building](#building)
- [Quick Example](#quick-example)

### Project Info
- [Repository Layout](#repository-layout)
- [Features](#features)
- [Known Limitations](#known-limitations)
- [Changelog](#changelog)
- [Author & Contact](#author--contact)

---

## Features

- **Zero-copy architecture** — parser eliminates heap allocations for field data
  - Parsed fields reference directly into input buffer
  - Single-pass recursive descent parsing
  - No copies of spans

- **High performance** — sub-microsecond parsing
  - Parser: 0.19µs per message (1000-msg benchmark)
  - Builder: 0.12µs per response
  - Linear O(n) complexity (no quadratic behavior)

- **MCPAdapter** — functional base class for MCP servers
  - Dispatch pipeline (non-virtual, sealed for security)
  - **Built-in authentication & RBAC** (v0.1.1)
  - Session state machine
  - Tool discovery and invocation
  - User context passed to tool handlers

- **JsonBuilder** — safe JSON response construction
  - Automatic escaping for quotes and backslashes
  - Type support (strings, numbers, objects, arrays)
  - Pre-allocated 4KB buffer

- **Security-first design** — all vulnerabilities documented or mitigated
  - Depth limit prevents stack overflow (64 levels)
  - Span-based API prevents buffer overflow
  - Proper escaping prevents injection
  - Never crashes on malformed input

- **Cross-platform** — Windows MSVC 2022 / Linux GCC / macOS Clang
- **C++17** — modern C++ with standard library only

---

## Building

### CMake (recommended)

```bash
cmake -B build
cmake --build build
```

### Make

```bash
make
```

### Visual Studio

Open `TestMcp.sln` in Visual Studio 2022.

---

## Quick Example

### Parse & Build

```cpp
#include "json_msg.h"
#include "json_builder.h"
using namespace mcptoolkit;

// Parse request
const char* request = R"({"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}})";
MCPMessage msg = MCPMessage::parse(request, strlen(request));

// Build response
JsonBuilder builder;
builder.start_object();
builder.add_field("jsonrpc", "2.0");
builder.add_field_number("id", msg.id);  // Echo back ID
builder.add_array_field("tools");
builder.close_array_field();
builder.end_object();

std::string response = builder.get();
// Output: {"jsonrpc":"2.0","id":1,"tools":[]}
```

### Build an MCP Server

```cpp
#include "mcp_adapter.h"
using namespace mcptoolkit;

class MyServer : public MCPAdapter {
protected:
    std::vector<ToolDefinition> list_tools() override {
        return {
            {"echo", "Echo tool", {
                {"text", "string", "Text to echo", true}
            }}
        };
    }

    ToolResult call_tool(const std::string& name, 
                         const std::string& args) override {
        if (name == "echo") {
            return ToolResult{"echoed", false};
        }
        return ToolResult{"unknown", true};
    }
};

int main() {
    MyServer server;
    server.run();  // Stdio read/dispatch loop
}
```


---

## Repository Layout

```
mcptoolkit/
├── include/
│   ├── mcp_adapter.h          # MCPAdapter — main entry point
│   └── json/
│       ├── json_msg.h         # MCPMessage struct
│       ├── json_parser.h      # JsonParser (internal)
│       └── json_builder.h     # JsonBuilder
└── src/
    ├── mcp_adapter.cpp
    └── json/
        ├── json_msg.cpp       # MCPMessage factory methods
        ├── json_parser.cpp    # JsonParser implementation
        └── json_builder.cpp   # JsonBuilder implementation

TestMcp/
└── TestMcp.cpp                # Test suite (161 assertions)
```

---

## Testing

### Run Tests

```bash
./build/testmcp        # Linux / macOS
build\Debug\testmcp    # Windows
```

### Test Coverage

**161/162 tests passing (99.4%)**

- ✅ 11 parser tests (requests, responses, parameters)
- ✅ 4 builder tests (construction, escaping, arrays)
- ✅ 9 adapter tests (dispatch, error handling)
- ✅ 13 security tests (vulnerabilities, DoS, injection)


### Performance

```
Parser:  0.19µs per message  (1000-msg avg)
Builder: 0.12µs per response (1000-msg avg)
Large input (10KB+): <1ms parse
No quadratic behavior detected ✓
```

---

## Security

### Vulnerabilities Tested

mcptoolkit is tested against 6 vulnerability classes + 3 emerging threats:

- ✅ **DoS (Nesting)** — Depth limit (64 levels) prevents stack overflow
- ⚠️ **Integer Overflow** — Limitation documented (v0.2: `std::optional<int>`)
- ✅ **Escape Injection** — Lenient parser, app-layer validation
- ✅ **Buffer Overflow** — Span-based API, no fixed buffers
- ✅ **Type Confusion** — App-layer validation responsibility
- ✅ **Zero-Copy Lifetime** — Buffer contract documented
- ✅ **Builder Injection** — Proper escaping prevents breakout
- ✅ **Expansion DoS** — Linear parsing, no expansion vulnerabilities
- ✅ **Malformed Input** — Parser never crashes


---

## Known Limitations (v0.1.1)

- **TLS transport** — Authentication tokens sent in plaintext over stdio. Deploy behind TLS proxy until v0.2.
- **Role assignment** — Default role is USER. Override `extract_auth_token()` and `call_tool()` to assign proper roles from token claims.
- **Audit logging** — Not integrated yet. Planned for v0.2 (RFC 5424/5848 syslog format).
- **Session binding** — SessionManager exists but not yet integrated into dispatch. Planned for v0.2.
- Parsed string fields are **not unescaped** — `\uXXXX` returned as-is. (v0.2: `StringSpan::unescape()`)
- The `id` field uses `-1` sentinel for absent IDs, so `"id": -1` is treated as notification. (v0.2: `std::optional<int>`)

---

## Author & Contact

**Author:** Jason Yang  
**Email:** [jasonyangwd@gmail.com](mailto:jasonyangwd@gmail.com)  
**License:** See LICENSE file

### Issues & Feedback

- **Bug Reports:** [GitHub Issues](https://github.com/JasonYangWd/mcptoolkit/issues)
- **Security Issues:** please report privately by email rather than opening a public issue

### Security Research & Discussion

All vulnerability analysis and security test evidence is open and free in this repository. If you want to follow along with deeper discussion on MCP protocol design and emerging threats:

**[The Secure MCP on Substack](https://thesecuremcp.substack.com/)** (free newsletter)

This is where we share extended analysis, threat intelligence, and roadmap updates. You're also welcome to just use the toolkit and report issues directly on GitHub.

### Blog Series Roadmap (30 posts)

**Published (Posts 1–17):**

| Phase | Posts | Topics |
|---|---|---|
| Parser Foundations | 1–7 | Why MCP security matters, zero-copy parsing, depth limits, parser hardening, string escaping, zero-copy guarantees |
| Deployment & DoS | 8–11 | Safe deployment patterns, algorithmic complexity attacks, command injection, rate limiting & timeouts |
| Attack Vectors & Access Control | 12–17 | Path traversal, SSRF, authentication, session fixation, authorization, audit logging |

**Coming next (Posts 18–21, drafted):** configuration & secrets security, advanced input validation, tool implementation security, and a code review checklist for MCP security.

**Roadmap (Posts 22–30):** the series pivots from theory to practice — building a real C++ MCP adaptor with this toolkit, connecting it to Claude, ChatGPT, Gemini, and Grok, testing it end to end, then hardening it live: privilege separation & sandboxing, TLS/mTLS & secrets management, monitoring & detection, and a finale red-teaming the server with 2026's real attack classes (tool poisoning, prompt injection, supply chain).

---

## Version & Status

- **Version:** 0.1.1 (Authentication & RBAC integration)
- **Status:** Stable for testing and evaluation (7/7 security tests passing)
- **Test Coverage:** 168/168 tests passing (parser, builder, adapter, auth integration)
- **Security:** Authentication enforced on dispatch, RBAC integrated, input validation active, all 6 vulnerability classes mitigated

---


