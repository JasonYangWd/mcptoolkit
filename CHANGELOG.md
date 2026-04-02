# Changelog

All notable changes to mcptoolkit are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned for v0.2
- Input size limit enforcement (`MAX_MESSAGE_BYTES` inside `parse()`)
- `std::optional<int>` for ID field (fix JSON-RPC -1 sentinel issue)
- String unescape for `\uXXXX` sequences
- TLS/mTLS transport
- Binary length-prefixed framing (stdio)
- `ToolArgs` with `require_str()` / `require_dbl()` helpers
- Result size cap (64KB hard limit)
- Audit log (stderr or syslog)

---

## [0.1.0] - 2026-04-02

### Added
- **Zero-copy JSON-RPC 2.0 parser** (`MCPMessage`)
  - Single-pass recursive descent parsing
  - No heap allocations for field data — parsed fields reference input buffer directly
  - Support for all JSON types: objects, arrays, strings, numbers, booleans, null
  - Depth limit (64) to prevent stack overflow
  
- **MCPAdapter base class** — functional MCP server framework
  - Dispatch pipeline (non-virtual, sealed for security)
  - Handles `initialize`, `tools/list`, `tools/call` methods
  - Session state machine (Uninitialized → Initializing → Ready)
  - Virtual overrides: `list_tools()`, `call_tool()`
  - Stdio transport with length-independent message framing
  
- **JsonBuilder** for constructing JSON-RPC responses
  - Incremental message building with methods for all JSON types
  - String escaping for quotes and backslashes
  - Pre-allocated 4KB buffer (typical MCP message size)
  
- **Test suite** (39 tests passing)
  - Request parsing (initialize, tools/list, tools/call)
  - Response parsing
  - Error responses
  - Complex nested parameters
  - Notifications (no response)
  - Malformed input handling
  
- **Examples**
  - `echo-adapter.cpp` — minimal MCP server
  - `calc-adapter.cpp` — multi-tool dispatch and argument parsing
  - `file-adapter.cpp` — secure file operations with path validation
  - `weather-adapter.cpp` — structured data and API backend pattern
  
- **Cross-platform support**
  - Windows (MSVC 2022)
  - Linux (GCC 13, Clang)
  - macOS (Clang)
  - Builds as static library (`.a`, `.lib`) or shared library (`.so`, `.dll`)

### Changed
- **Eliminated heap allocations in MCPAdapter** (April 2, 2026)
  - `dispatch()`: Use `memcmp` on method span instead of copying to `std::string`
  - `extract_string()`: Return pointer+length span instead of allocating `std::string`
  - `extract_value_span()`: Manual `memchr` scan instead of `std::string needle`
  - Result: Parser → adapter → handler path is now truly zero-copy
  - Only 2 boundary copies remain at `call_tool()` virtual interface

### Known Limitations
- Parsed string fields not unescaped (`\uXXXX` returned as-is)
- ID field uses `-1` sentinel for absent IDs (violates JSON-RPC spec)
- No input size limit (caller responsible for validation)
- Stdio transport only (TLS/mTLS planned for v0.2)

---

## Version History

| Version | Release Date | Status | Notes |
|---------|--------------|--------|-------|
| v0.1.0 | 2026-04-02 | Current | Parser + MCPAdapter functional, zero-copy |
| v0.2 | Q2 2026 | Planned | TLS, input limits, unescape, structured args |
| v0.3 | Q3 2026 | Planned | Tamper-evident audit log, signed manifests |

---

## Migration Guide

### From previous versions
This is the first public release. No migration needed.

---

## Reporting Issues

Security issues: See [SECURITY.md](SECURITY.md) (coming v0.2)

Other issues: [GitHub Issues](https://github.com/JasonYangWd/mcptoolkit/issues)
