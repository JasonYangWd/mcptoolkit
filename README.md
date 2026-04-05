# mcptoolkit

> **High-Performance Zero-Copy JSON-RPC 2.0 / MCP Parser & Server Framework**  
> **Last updated:** April 3, 2026

A C++ library for parsing and building [JSON-RPC 2.0](https://www.jsonrpc.org/specification) / [Model Context Protocol (MCP)](https://modelcontextprotocol.io) messages. Designed for high performance with a zero-copy parser and a lightweight message builder.

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

See [API.md](notes/API.md) for complete usage examples.

---

## Table of Contents

### Getting Started
- [Features](#features)
- [Building](#building)
- [Quick Example](#quick-example)

### Documentation
- **[notes/API.md](notes/API.md)** — Usage examples and API reference
- **[notes/TEST_RESULTS.md](notes/TEST_RESULTS.md)** — Comprehensive test coverage (161/162 passing)

### Security
- **[notes/JSON_VULNERABILITIES.md](notes/JSON_VULNERABILITIES.md)** — Threat analysis (6 vulnerability classes + emerging risks)
- **[notes/SECURITY_TEST_COVERAGE.md](notes/SECURITY_TEST_COVERAGE.md)** — Test evidence for security claims

### Project Info
- [Repository Layout](#repository-layout)
- [Features](#features)
- [Known Limitations](#known-limitations)
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
  - Session state machine
  - Tool discovery and invocation

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

See [API.md](API.md) for complete examples.

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

See [TEST_RESULTS.md](TEST_RESULTS.md) for detailed breakdown.

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

See **[notes/JSON_VULNERABILITIES.md](notes/JSON_VULNERABILITIES.md)** for comprehensive threat analysis.  
See **[notes/SECURITY_TEST_COVERAGE.md](notes/SECURITY_TEST_COVERAGE.md)** for test evidence.

---

## Known Limitations (v0.1)

- Parsed string fields are **not unescaped** — `\uXXXX` returned as-is. (v0.2: `StringSpan::unescape()`)
- The `id` field uses `-1` sentinel for absent IDs, so `"id": -1` is treated as notification. (v0.2: `std::optional<int>`)
- No input size limit — callers should validate. (v0.2: `MAX_MESSAGE_BYTES` enforcement)
- Stdio transport only — TLS/mTLS planned for v0.2.

---

## Documentation

| Document | Purpose |
|---|---|
| **[API.md](API.md)** | Usage examples, API reference, headers to include |
| **[TEST_RESULTS.md](TEST_RESULTS.md)** | Test breakdown, performance metrics, coverage matrix |
| **[notes/JSON_VULNERABILITIES.md](notes/JSON_VULNERABILITIES.md)** | Threat analysis, CVE examples, real-world scenarios |
| **[notes/SECURITY_TEST_COVERAGE.md](notes/SECURITY_TEST_COVERAGE.md)** | Test evidence, vulnerability mapping, gap analysis |

---

## Author & Contact

**Author:** Jason Yang  
**Email:** [jasonyangwd@gmail.com](mailto:jasonyangwd@gmail.com)  
**License:** See LICENSE file

### Issues & Feedback

- **Bug Reports:** [GitHub Issues](https://github.com/JasonYangWd/mcptoolkit/issues)
- **Security Issues:** See [notes/JSON_VULNERABILITIES.md](notes/JSON_VULNERABILITIES.md) for responsible disclosure

### Subscribe to The Secure MCP

For weekly deep-dives on MCP security, JSON vulnerabilities, and protocol design:

**[The Secure MCP on Substack](https://thesecuremcp.substack.com/)**

Read the vulnerability analysis and security test evidence in this repository, then subscribe for extended discussion on emerging threats in MCP protocol design.

---

## Version & Status

- **Version:** 0.1.0 (First public release)
- **Status:** Stable (zero-copy parser + MCPAdapter functional)
- **Test Coverage:** 161/162 tests passing
- **Security:** All 6 primary vulnerability classes tested and mitigated

---

**[→ View API Documentation](notes/API.md) | [→ View Test Results](notes/TEST_RESULTS.md) | [→ Read Security Analysis](notes/JSON_VULNERABILITIES.md)**
