# mcptoolkit

A C++ library for parsing and building [JSON-RPC 2.0](https://www.jsonrpc.org/specification) / [Model Context Protocol (MCP)](https://modelcontextprotocol.io) messages. Designed for high performance with a zero-copy parser and a lightweight message builder.

## Features

- **Zero-copy architecture** — parser and adapter eliminate heap allocations for field data
  - Parsed fields reference directly into the input buffer
  - Single-pass recursive descent parsing, no copies of spans
- Single-pass recursive descent parsing
- `MCPAdapter` — functional base class for building MCP servers (dispatch, state machine, audit)
- `JsonBuilder` for incrementally constructing JSON-RPC messages
- C++17, cross-platform (Windows MSVC / Linux GCC / Clang)
- Builds as a static or shared library (DLL / `.so`)

## Repository Layout

```
mcptoolkit/
├── include/
│   ├── mcp_adapter.h          # MCPAdapter — main entry point (work in progress)
│   └── json/
│       ├── json_msg.h         # MCPMessage struct
│       ├── json_parser.h      # JsonParser (internal)
│       └── json_builder.h     # JsonBuilder
└── src/
    ├── mcp_adapter.cpp
    └── json/
        └── json_msg.cpp       # MCPMessage + JsonParser implementation

TestMcp/
└── TestMcp.cpp                # Test suite and benchmarks
```

## Building

### CMake (recommended)

```bash
cmake -B build
cmake --build build
```

Build as a shared library:

```bash
cmake -B build -DBUILD_SHARED_LIBS=ON
cmake --build build
```

### Visual Studio

Open `TestMcp.sln` in Visual Studio 2022 and build the solution.

### Make

```bash
make
```

## Usage

### Parsing a message

```cpp
#include "json_msg.h"
using namespace mcptoolkit;

const char* json = R"({"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}})";
MCPMessage msg = MCPMessage::parse(json, strlen(json));

if (!msg.valid) { /* handle parse error */ }

std::string method(msg.method, msg.method_len);  // "tools/list"
int id = msg.id;                                  // 1
```

> **Important:** All `const char*` fields in `MCPMessage` point directly into the input buffer. The buffer must remain alive and unmodified for as long as the `MCPMessage` is used.

### Building a response

```cpp
#include "json_builder.h"
using namespace mcptoolkit;

JsonBuilder builder;
builder.start_object();
builder.add_field("jsonrpc", "2.0");
builder.add_field_number("id", 1);
builder.add_object_field("result");
builder.add_field("status", "ok");
builder.close_object_field();
builder.end_object();

// {"jsonrpc":"2.0","id":1,"result":{"status":"ok"}}
std::string response = builder.get();
```

### Building an error response

```cpp
JsonBuilder builder;
builder.start_object();
builder.add_field("jsonrpc", "2.0");
builder.add_field_number("id", 1);
builder.add_object_field("error");
builder.add_field_number("code", -32601);
builder.add_field("message", "Method not found");
builder.close_object_field();
builder.end_object();
```

### Building an MCP adapter

```cpp
#include "mcp_adapter.h"
using namespace mcptoolkit;

class EchoAdapter : public MCPAdapter {
protected:
    std::vector<ToolDefinition> list_tools() override {
        return {
            {"echo", "Echoes the input back", {
                {"text", "string", "Text to echo", true}
            }}
        };
    }

    ToolResult call_tool(const std::string& name, const std::string& args_json) override {
        if (name == "echo") {
            // Parse args_json and return result
            return ToolResult{"echo result", false};
        }
        return ToolResult{"unknown tool", true};
    }
};

int main() {
    EchoAdapter adapter;
    adapter.run();  // Start stdio read/dispatch loop
}
```

## API Reference

### `MCPMessage`

| Field | Type | Description |
|---|---|---|
| `valid` | `bool` | `true` if parsing succeeded — check before using any field |
| `is_request` | `bool` | Message has a `"method"` field |
| `is_response` | `bool` | Message has a `"result"` or `"error"` field |
| `method` / `method_len` | `const char*` / `size_t` | Method name (no quotes, not null-terminated) |
| `id` | `int` | Request/response ID; `-1` if absent (notification) |
| `params_start` / `params_len` | `const char*` / `size_t` | Raw JSON params value |
| `result_start` / `result_len` | `const char*` / `size_t` | Raw JSON result value |
| `error_code` | `int` | Error code |
| `error_msg` / `error_msg_len` | `const char*` / `size_t` | Error message (not null-terminated) |
| `raw_input` / `raw_len` | `const char*` / `size_t` | Original input span |

### `JsonBuilder` methods

| Method | Description |
|---|---|
| `start_object()` / `end_object()` | Open / close a JSON object |
| `add_field(key, value)` | Add a string field (value is escaped) |
| `add_field_number(key, int)` | Add an integer field |
| `add_field_double(key, double)` | Add a double field |
| `add_field_bool(key, bool)` | Add a boolean field |
| `add_field_raw(key, value)` | Add a pre-formed JSON value verbatim |
| `add_object_field(key)` / `close_object_field()` | Open / close a nested object |
| `add_array_field(key)` / `close_array_field()` | Open / close a JSON array |
| `add_array_element(value)` | Append an escaped string element to an array |
| `add_array_element_raw(value)` | Append a raw JSON element to an array |
| `get()` | Return the built message as `const std::string&` |
| `reset()` | Clear the buffer for reuse |

### `MCPAdapter` methods

| Method | Return | Description |
|---|---|---|
| `run()` | `void` | Start the stdio read/dispatch loop. Blocks until stdin closes. |
| `list_tools()` | `std::vector<ToolDefinition>` | Override to advertise your server's tools. Return empty vector for no tools. |
| `call_tool(name, args_json)` | `ToolResult` | Override to handle tool invocations. `name` is the tool name; `args_json` is the raw JSON arguments object. |

**Dispatch pipeline (built-in, non-virtual):**
1. Session state check — rejects tool calls before `initialize` handshake completes
2. Method dispatch — routes `initialize`, `tools/list`, `tools/call` to handlers
3. Handler invokes your `list_tools()` or `call_tool()` override
4. Response sent back over stdio

## Running Tests

After building:

```bash
./build/testmcp        # Linux / macOS
build\Debug\testmcp    # Windows
```

The test suite covers request parsing, response parsing, error responses, complex parameters, message building, round-trip parse+build, array construction, and a performance benchmark (target: <100 µs/parse, <50 µs/build).

## Known Limitations (v0.1)

- Parsed string fields are **not unescaped** — `\uXXXX` and other escape sequences are returned as-is in the raw span. (v0.2: will add `StringSpan::unescape()`)
- The `id` field is stored as `int`; `-1` is used as a sentinel for absent IDs, so JSON-RPC messages with `"id": -1` are treated as notifications. (v0.2: will use `std::optional<int>`)
- No input size limit — callers should validate message length before parsing untrusted input. (v0.2: will enforce `MAX_MESSAGE_BYTES` inside `parse()`)
- `MCPAdapter` supports only stdio transport. TLS/mTLS and binary framing planned for v0.2.
