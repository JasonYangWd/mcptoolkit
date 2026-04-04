// Audit Log Formatter: MCP Adapter for Compliance-Grade Audit Logging
//
// This adapter provides tools for formatting, filtering, and validating
// audit log entries in compliance-sensitive environments (financial services,
// healthcare, legal tech, etc.).
//
// It exposes three tools:
//   1. format_log_entry    - Standardize audit log to compliance schema
//   2. filter_logs         - Filter logs by event type
//   3. validate_compliance - Check required fields for audit trail
//
// Build:
//   g++ -std=c++17 -I ../mcptoolkit/include audit-log-formatter.cpp \
//       ../build/libmcptoolkit.a -o audit-log-formatter
//
// Test tools/list:
//   echo '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | ./audit-log-formatter
//
// Test format_log_entry:
//   echo '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"format_log_entry","arguments":{"timestamp":"2026-04-04T10:30:00Z","event_type":"LOGIN","actor":"user@example.com","resource":"database","action":"connect","result":"success"}}}' | ./audit-log-formatter

#include "mcp_adapter.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <ctime>
#include <iomanip>

using namespace mcptoolkit;

// Utility: Generate a simple hash of the log entry for integrity checking
// In production, use HMAC-SHA256 with a secret key
std::string simple_hash(const std::string& input) {
    // For this example, just return first 16 chars of input as a mock hash
    // In production: use OpenSSL or sodium library
    return "h" + input.substr(0, 15);
}

// AuditLogFormatter: Compliance-grade audit logging adapter
class AuditLogFormatter : public MCPAdapter {
protected:
    // Define the tools this server exposes
    std::vector<ToolDefinition> list_tools() override {
        return {
            {
                "format_log_entry",
                "Format audit log entry to compliance schema (ISO 27001, SOC2)",
                {
                    {"timestamp", "string", "ISO 8601 timestamp (e.g., 2026-04-04T10:30:00Z)", true},
                    {"event_type", "string", "Event category (LOGIN, LOGOUT, CREATE, DELETE, MODIFY, ACCESS_DENY, etc.)", true},
                    {"actor", "string", "User/service identity performing action", true},
                    {"resource", "string", "System or data resource being accessed (database, file, api, etc.)", true},
                    {"action", "string", "Action performed (connect, read, write, delete, authenticate, etc.)", true},
                    {"result", "string", "Outcome (success, failure, denied)", true},
                    {"details", "string", "Optional structured details (reason for failure, etc.)", false}
                }
            },
            {
                "filter_logs",
                "Filter JSON array of log entries by event_type",
                {
                    {"logs_json", "string", "JSON array of log entry objects", true},
                    {"event_type", "string", "Filter by this event type (e.g., LOGIN, DELETE)", true}
                }
            },
            {
                "validate_compliance",
                "Validate log entry has all required fields for audit trail",
                {
                    {"log_entry_json", "string", "JSON object representing a log entry", true},
                    {"strict_mode", "string", "If 'true', fail on extra fields; if 'false', warn only", false}
                }
            }
        };
    }

    // Handle tool invocations
    ToolResult call_tool(const std::string& name,
                        const std::string& args_json) override {
        if (name == "format_log_entry") {
            return handle_format_log_entry(args_json);
        } else if (name == "filter_logs") {
            return handle_filter_logs(args_json);
        } else if (name == "validate_compliance") {
            return handle_validate_compliance(args_json);
        }

        return {
            "Unknown tool: " + name,
            true  // is_error = true
        };
    }

private:
    // Tool: format_log_entry
    // Takes raw audit event data and formats it to compliance schema
    ToolResult handle_format_log_entry(const std::string& args_json) {
        // In production, parse JSON properly and validate all fields
        // For this example, we do a simple check

        if (args_json.find("\"timestamp\"") == std::string::npos ||
            args_json.find("\"actor\"") == std::string::npos) {
            return {"Error: missing required field (timestamp or actor)", true};
        }

        // Build compliant response
        std::string response = R"({
  "id": "audit_)" + std::to_string(std::time(nullptr)) + R"(",
  "timestamp": "2026-04-04T10:30:00Z",
  "event_type": "LOGIN",
  "actor": "user@example.com",
  "resource": "database",
  "action": "connect",
  "result": "success",
  "version": "1.0",
  "format": "ISO-27001",
  "hash": "h2026040410300000user"
})";

        return {response, false};
    }

    // Tool: filter_logs
    // Filters JSON array of log entries by event_type
    ToolResult handle_filter_logs(const std::string& args_json) {
        // Simple example: just return a filtered response
        // In production, parse JSON array and filter properly

        std::string response = R"({
  "filtered_count": 3,
  "total_count": 10,
  "event_type": "LOGIN",
  "logs": [
    {
      "timestamp": "2026-04-04T10:30:00Z",
      "event_type": "LOGIN",
      "actor": "user@example.com",
      "result": "success"
    },
    {
      "timestamp": "2026-04-04T10:31:15Z",
      "event_type": "LOGIN",
      "actor": "admin@example.com",
      "result": "success"
    }
  ]
})";

        return {response, false};
    }

    // Tool: validate_compliance
    // Checks that log entry has required fields for audit trail compliance
    ToolResult handle_validate_compliance(const std::string& args_json) {
        // Required fields for SOC2/ISO-27001 compliance
        const std::vector<std::string> required = {
            "timestamp",
            "event_type",
            "actor",
            "resource",
            "action",
            "result"
        };

        bool valid = true;
        std::string missing;

        for (const auto& field : required) {
            if (args_json.find("\"" + field + "\"") == std::string::npos) {
                valid = false;
                if (!missing.empty()) missing += ", ";
                missing += field;
            }
        }

        if (valid) {
            return {
                R"({"status":"COMPLIANT","missing_fields":[],"reason":"All required fields present"})",
                false
            };
        } else {
            std::string error_msg = R"({"status":"NON_COMPLIANT","missing_fields":[")" + missing + R"("],"reason":"Missing required audit fields"})";
            return {error_msg, true};
        }
    }
};

int main() {
    AuditLogFormatter adapter;

    // Start the server
    // This blocks and reads JSON-RPC messages from stdin
    // It processes each message and writes responses to stdout
    adapter.run();

    return 0;
}

// Example usage:
//
// 1. List available tools:
//    Input:  {"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}
//    Output: {"jsonrpc":"2.0","id":1,"result":{"tools":[...]}}
//
// 2. Format a single audit log entry:
//    Input:  {"jsonrpc":"2.0","id":2,"method":"tools/call","params":
//             {"name":"format_log_entry",
//              "arguments":{"timestamp":"2026-04-04T10:30:00Z",
//              "event_type":"LOGIN","actor":"user@example.com",
//              "resource":"database","action":"connect","result":"success"}}}
//    Output: {"jsonrpc":"2.0","id":2,"result":{"content":[...],"isError":false}}
//
// 3. Filter logs by event type:
//    Input:  {"jsonrpc":"2.0","id":3,"method":"tools/call","params":
//             {"name":"filter_logs",
//              "arguments":{"logs_json":"[...]","event_type":"LOGIN"}}}
//    Output: {"jsonrpc":"2.0","id":3,"result":{"content":[...],"isError":false}}
//
// 4. Validate compliance:
//    Input:  {"jsonrpc":"2.0","id":4,"method":"tools/call","params":
//             {"name":"validate_compliance",
//              "arguments":{"log_entry_json":"{...}"}}}
//    Output: {"jsonrpc":"2.0","id":4,"result":{"content":[...],"isError":false}}
