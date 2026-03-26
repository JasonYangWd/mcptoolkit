#include "../pch.h"
#include <iostream>
#include "mcp_adapter.h"

namespace mcptoolkit {

MCPAdapter::MCPAdapter() {
}

void MCPAdapter::init() {
    std::cout << "MCP: initialized!\n";
}

void MCPAdapter::run() {
    std::cout << "MCP: Running!\n";
}

} // namespace mcptoolkit

