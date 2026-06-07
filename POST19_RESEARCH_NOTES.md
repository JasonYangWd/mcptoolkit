# Post 19 Research: Advanced Input Validation in MCP

**Topic:** Input validation, injection attacks, untrusted input handling  
**Series Position:** Post 19 (after Configuration Security)  
**Reading Time Target:** 10-11 minutes  
**Difficulty:** Intermediate+

---

## Topic Overview

The progression so far:
- Post 15: "Who are you?" (Authentication)
- Post 16: "What can you do?" (Authorization)
- Post 17: "What did you do?" (Audit Logging)
- Post 18: "Where are your secrets?" (Configuration)
- **Post 19: "Can I trust this input?" (Input Validation)**

---

## Key Areas to Cover

### 1. Input Validation in MCP Context

**Untrusted Sources:**
- LLM client input (tool names, parameters)
- Tool responses (returned to LLM)
- User data passed through tools
- Configuration from environment/files
- API responses from external services

### 2. Injection Attack Types (MCP-Specific)

**Prompt Injection via Tool Responses**
- Tool returns malicious JSON structure
- Tool returns embedded instructions to LLM
- Tool returns code that looks like legitimate output

**Command Injection in Tool Parameters**
- LLM passes shell commands as file paths
- LLM passes code as parameters
- URL parameters with encoded payloads

**JSON/Structured Data Injection**
- Malformed JSON structures crash parsers
- Nested structures exceed depth limits
- Large payloads cause DoS

**Path Traversal & SSRF**
- LLM requests access to files outside bounds
- LLM requests URLs to internal services
- LLM constructs URLs with special characters

---

## Real CVEs to Research

### Category 1: Prompt Injection via Tool Responses
- OpenAI API prompt injection research
- Tool output injection attacks
- LLM jailbreak patterns

### Category 2: JSON/Parser Vulnerabilities (C++)
- CVE-2021-21233 (JSON library DoS)
- CWE-1025 (Comparison Using Wrong Factors)
- Integer overflow in array sizes

### Category 3: Command Injection
- Shell metacharacter injection
- Path traversal in file tools
- URL manipulation attacks

### Category 4: C++ Memory Issues
- Buffer overflow via large inputs
- Integer overflow in size calculations
- Stack exhaustion from recursion

---

## Structure Plan

### Section 1: Introduction
- The problem: untrusted input from both directions
- LLM as untrusted client
- Tools as untrusted servers
- Why validation matters

### Section 2: Why Input Validation Fails
- 5 common mistakes in MCP

### Section 3: Real CVEs / Case Studies
- 3-4 real injection attacks
- MCP-specific scenarios

### Section 4: Attack Vectors
- 5 ways to exploit input validation gaps

### Section 5: Defense Strategy
- Multi-layer input validation
- Parser hardening
- C++ safety patterns

### Section 6: mcptoolkit Integration
- Current validation capabilities
- Best practices

### Section 7: Testing Checklist

### Section 8: Next Post Preview (Post 20)

---

## MCP-Specific Attack Scenarios

### Scenario 1: Prompt Injection via Tool Response
```
Tool: web_fetch(url)
Returns: {"status": 200, "body": "...and here are your secret keys: ..."}
LLM sees: Tool output → Trust it → Accidentally reveals secrets
```

### Scenario 2: Command Injection in File Tool
```
LLM: file_read(path="/app/data.json")
LLM actually passes: file_read(path="/app/data.json; cat /etc/passwd")
```

### Scenario 3: JSON Parsing DoS
```
LLM: Call tool with massive nested JSON structure
Tool parser recurses 1000 levels deep → Stack overflow
```

### Scenario 4: SSRF via URL Tool
```
LLM: fetch_url("http://localhost:8080/admin")
Tool: Fetches internal service, returns admin panel HTML to LLM
```

### Scenario 5: Path Traversal
```
LLM: file_read("../../../etc/passwd")
Tool: Reads file outside intended directory
```

---

## Key Vulnerabilities (C++ Focus)

### 1. JSON Parser Depth Limits
- C++ recursive JSON parsing can exhaust stack
- Need iterative parsing or depth limits
- mcptoolkit already has depth limit (64)

### 2. String Handling in C++
- Buffer overflows if not careful with sizes
- Integer overflow in string allocation
- NULL bytes causing truncation

### 3. Regular Expression DoS
- ReDoS attacks with complex patterns
- Need to validate regex patterns
- Set timeout on regex matching

### 4. Memory Exhaustion
- Large input arrays cause memory spike
- Deep recursion causes stack exhaust
- Need to limit input sizes

---

## Real CVE Research Candidates

### For Prompt Injection:
- OpenAI security research (tool response injection)
- Anthropic prompt injection research
- Academic papers on LLM safety

### For JSON Parsing:
- CVE-2020-27751 (JSON library DoS)
- CVE-2021-21233 (multiple JSON libraries)
- CWE-776 (Improper Restriction of Recursive Entity References)

### For Command Injection:
- Classic shell injection examples
- Modern examples in MCP context
- URL parameter encoding attacks

---

## Defense Strategies

### Layer 1: Input Type Validation
- Validate that input is the expected type
- Reject unexpected types early

### Layer 2: Size Limits
- Maximum string length
- Maximum array size
- Maximum object depth

### Layer 3: Content Validation
- Whitelist acceptable characters
- Reject control characters
- Validate against regex patterns

### Layer 4: Parsing Safety
- Use safe JSON parsers (with limits)
- Iterative parsing (avoid recursion)
- Timeout on parsing operations

### Layer 5: Semantic Validation
- Validate business logic
- Check against whitelists
- Verify expected relationships

### Layer 6: Output Encoding
- Escape special characters for context
- Encode for safe transmission
- Sanitize before returning to LLM

---

## mcptoolkit Current Capabilities

Check if toolkit has:
- ✅ JSON depth limit (yes, 64)
- ✅ Shell metacharacter validation (yes)
- ✅ Path validation (yes, PathValidator)
- ✅ Size limits (yes, 1MB message limit)
- ? Regex DoS protection (need to check)
- ? Output encoding (need to check)
- ? Tool response validation (need to check)

---

## Key Statistics to Research
- How common are prompt injection attacks?
- What % of tool integrations have injection vulnerabilities?
- Cost of prompt injection vs. other attacks
- Detection rate of input validation bypasses

---

## Teaching Goals
- Understand input validation importance in MCP
- Know common injection attack patterns
- Implement safe input handling
- Validate tool responses
- Test for injection vulnerabilities

---

## Tone & Style
- Match Posts 16-18: Real CVEs, practical code
- Include C++ memory safety examples
- Focus on MCP protocol layer
- Practical defense patterns
- Actionable testing checklist

---

## Content Structure

```
1. Introduction (1-2 paragraphs)
   - Why input validation matters in MCP
   - Both directions: LLM→Server, Tool→LLM
   
2. Why Input Validation Fails (5 paragraphs)
   - Common mistakes developers make
   
3. Real Attack Examples (3-4 sections)
   - Prompt injection via tool responses
   - Command injection in parameters
   - JSON parser DoS
   - One more real CVE
   
4. Attack Vectors (5 paragraphs)
   - Ways to exploit gaps
   
5. Defense Layers (6+ sections)
   - Type validation
   - Size limits
   - Content validation
   - Parser safety
   - Semantic validation
   - Output encoding
   
6. mcptoolkit Integration
   - Current capabilities
   - Best practices
   
7. Testing Checklist
   - 10+ items to test
   
8. What's Next
   - Post 20 preview
```

---

## Outline Summary

**Strengths:**
- Two-directional validation (LLM→Server + Tool→LLM)
- MCP-specific injection attacks
- C++ memory safety focus
- Comprehensive defense layers
- Practical code examples

**Research Needed:**
- Real CVE examples for prompt injection
- JSON parser vulnerabilities in C++
- Rate of injection attacks in LLM tools
- Current best practices for MCP validation

**Next Steps:**
1. Research and verify real CVEs
2. Compile MCP-specific attack examples
3. Check mcptoolkit current validation
4. Draft full Post 19
5. Review for accuracy
6. Publish

---

## Series Alignment

Post 19 completes the "input, process, log" trilogy:
- Post 17: Log what happened (Audit Logging)
- Post 18: Protect secrets (Configuration)
- **Post 19: Validate before processing (Input Validation)**

Together they create comprehensive security foundation for MCP servers.

