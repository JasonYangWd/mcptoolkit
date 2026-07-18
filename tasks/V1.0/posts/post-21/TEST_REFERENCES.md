# Test Suite References

**Post:** 21

## TestMCP Test Suite

All tests are in: `/media/sf_Shared/TestMcp/TestMcp/TestMcp.cpp`

## Test Categories

### Authentication Tests
- Session creation and validation
- Token generation and expiration
- Credential handling
- Multi-factor authentication

### Security Tests
- Input validation and parsing
- Buffer overflow protection
- Path traversal defense
- CSRF/SSRF protection
- Rate limiting and throttling

### Access Control Tests
- Role-based access control (RBAC)
- Permission enforcement
- Resource authorization
- Delegation scenarios

### Integration Tests
- MCP protocol handling
- End-to-end workflows
- Error handling
- Concurrent access

### Edge Cases
- Empty/null input
- Large payloads
- Malformed data
- Boundary conditions
- Unicode and encoding

## Running Specific Tests

```bash
cd /media/sf_Shared/TestMcp/build
./test_mcp_integration     # Full integration tests
./test_validation_demo     # Validation demonstrations
```

## Test Output

Each test produces:
- Pass/Fail status
- Error messages (if any)
- Coverage metrics
- Performance data

## Adding New Tests

To add tests for vulnerabilities discussed in this post:
1. Edit: `/media/sf_Shared/TestMcp/TestMcp/TestMcp.cpp`
2. Add test case
3. Rebuild and run
4. Update this reference if new test categories added

---
*This is a reference document. See actual test code in TestMcp.cpp*
