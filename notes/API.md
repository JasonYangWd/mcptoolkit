# mcptoolkit API Reference & Usage Guide

## Table of Contents

1. [Usage Examples](#usage-examples)
   - [Parsing a Message](#parsing-a-message)
   - [Building a Response](#building-a-response)
   - [Building an Error Response](#building-an-error-response)
   - [Building an MCP Adapter](#building-an-mcp-adapter)
2. [API Reference](#api-reference)
   - [MCPMessage](#mcpmessage)
   - [JsonBuilder](#jsonbuilder)
   - [MCPAdapter](#mcpadapter)

---

## Usage Examples

### Parsing a Message

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

### Building a Response

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

### Building an Error Response

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

// {"jsonrpc":"2.0","id":1,"error":{"code":-32601,"message":"Method not found"}}
std::string response = builder.get();
```

### Building an MCP Adapter

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

---

## API Reference

### `MCPMessage`

A parsed JSON-RPC 2.0 / MCP message. All `const char*` fields point directly into the input buffer and are NOT null-terminated.

#### Fields

| Field | Type | Description |
|---|---|---|
| `valid` | `bool` | `true` if parsing succeeded — **check before using any field** |
| `is_request` | `bool` | Message has a `"method"` field |
| `is_response` | `bool` | Message has a `"result"` or `"error"` field |
| `method` / `method_len` | `const char*` / `size_t` | Method name (no quotes, not null-terminated) |
| `id` | `int` | Request/response ID; `-1` if absent (notification) |
| `params_start` / `params_len` | `const char*` / `size_t` | Raw JSON params value |
| `result_start` / `result_len` | `const char*` / `size_t` | Raw JSON result value |
| `error_code` | `int` | Error code (JSON-RPC error code) |
| `error_msg` / `error_msg_len` | `const char*` / `size_t` | Error message (not null-terminated) |
| `raw_input` / `raw_len` | `const char*` / `size_t` | Original input span |

#### Static Factory Methods

```cpp
// Parse from C-style buffer
static MCPMessage parse(const char* json_str, size_t len);

// Parse from std::string
static MCPMessage parse(const std::string& json_str);
```

#### Lifetime Guarantees

⚠️ **Zero-Copy Buffer Lifetime Contract:**
- Input buffer must remain alive while `MCPMessage` is in use
- Buffer must not be modified after parsing
- If you need to free the buffer, make defensive copies of fields:
  ```cpp
  std::string method_copy(msg.method, msg.method_len);
  // Now safe to free original buffer
  ```

---

### `JsonBuilder`

Incrementally construct JSON-RPC response messages with automatic escaping.

#### Methods

| Method | Description |
|---|---|
| `start_object()` | Open a JSON object |
| `end_object()` | Close a JSON object |
| `add_field(key, value)` | Add a string field (value is escaped for quotes/backslashes) |
| `add_field_number(key, int)` | Add an integer field |
| `add_field_double(key, double)` | Add a floating-point field |
| `add_field_bool(key, bool)` | Add a boolean field (`true` / `false`) |
| `add_field_raw(key, value)` | Add a pre-formed JSON value verbatim (no escaping) |
| `add_object_field(key)` | Open a nested object (must close with `close_object_field()`) |
| `close_object_field()` | Close a nested object |
| `add_array_field(key)` | Open a JSON array (must close with `close_array_field()`) |
| `close_array_field()` | Close a JSON array |
| `add_array_element(value)` | Append an escaped string element to an array |
| `add_array_element_raw(value)` | Append a raw JSON element to an array (no escaping) |
| `get()` | Return the built message as `const std::string&` |
| `reset()` | Clear the buffer for reuse |

#### Important Notes

- ⚠️ Use `add_field()` for user input (automatically escaped)
- ✅ Use `add_field_raw()` or `add_array_element_raw()` for pre-formed JSON
- ✅ Always nest objects/arrays properly (open and close in order)
- ⚠️ Never concatenate JSON strings manually; use builder API for safety

---

### `MCPAdapter`

Base class for building MCP servers. Handles JSON-RPC protocol, session state, and method dispatch.

#### Virtual Methods (Override in Subclass)

| Method | Return | Description |
|---|---|---|
| `list_tools()` | `std::vector<ToolDefinition>` | Advertise your server's tools. Return empty vector for no tools. |
| `call_tool(name, args_json)` | `ToolResult` | Handle tool invocation. `name` is the tool name; `args_json` is raw JSON arguments object. |

#### Non-Virtual Methods

| Method | Return | Description |
|---|---|---|
| `run()` | `void` | Start the stdio read/dispatch loop. Blocks until stdin closes. |

#### Dispatch Pipeline (Built-in)

The dispatch pipeline handles incoming messages in this order:

1. **Parse** — Deserialize JSON-RPC message
2. **Validate** — Check message format
3. **Session State** — Rejects tool calls before `initialize` handshake completes
4. **Method Dispatch** — Routes to handler:
   - `"initialize"` → Send initialization response
   - `"tools/list"` → Call your `list_tools()`, build response
   - `"tools/call"` → Call your `call_tool()`, build result response
   - Other methods → Send `-32601 Method not found` error
5. **Error Handling** — Return errors for malformed input
6. **Response** — Send result back over stdio

#### Data Structures

**ToolDefinition:**
```cpp
struct ToolDefinition {
    std::string name;           // Tool name ("echo", "read_file", etc.)
    std::string description;    // Human-readable description
    std::vector<ToolParam> params;  // Parameter list
};

struct ToolParam {
    std::string name;           // Parameter name
    std::string type;           // JSON type ("string", "number", "boolean", etc.)
    std::string description;    // Parameter description
    bool required;              // Whether parameter is required
};
```

**ToolResult:**
```cpp
struct ToolResult {
    std::string text;           // Result text
    bool is_error;              // true = error, false = success
};
```

---

## Build Instructions

See [README.md](README.md#building) for build instructions (CMake, Visual Studio, Make).

---

## Headers to Include

```cpp
// For parsing messages
#include "json_msg.h"

// For building responses
#include "json_builder.h"

// For building MCP servers
#include "mcp_adapter.h"
```

---

## Example: Complete MCP Server

```cpp
#include "mcp_adapter.h"
#include <cstring>
using namespace mcptoolkit;

class MyMCPServer : public MCPAdapter {
protected:
    std::vector<ToolDefinition> list_tools() override {
        return {
            {
                "uppercase",
                "Convert text to uppercase",
                {{"text", "string", "Text to convert", true}}
            },
            {
                "reverse",
                "Reverse text",
                {{"text", "string", "Text to reverse", true}}
            }
        };
    }

    ToolResult call_tool(const std::string& name, const std::string& args_json) override {
        if (name == "uppercase") {
            // In real implementation, parse args_json to extract "text" field
            return ToolResult{"CONVERTED", false};
        }
        if (name == "reverse") {
            return ToolResult{"DESREVER", false};
        }
        return ToolResult{"Unknown tool: " + name, true};
    }
};

int main() {
    MyMCPServer server;
    server.run();  // Blocks, reading from stdin and writing to stdout
    return 0;
}
```

---

**Version:** 0.1.0  
**Last Updated:** April 3, 2026
