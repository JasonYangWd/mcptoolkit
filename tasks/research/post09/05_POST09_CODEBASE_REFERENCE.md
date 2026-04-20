# Post 09 Code References & Evidence
## Codebase locations and attack examples

**Date:** April 19, 2026  
**Post:** 09 — "Denial of Service: The Algorithmic Complexity Attack"  
**Status:** Code mapping complete

---

## Part 1: Existing Evidence (Posts 04-08)

### Tool Registry (MCPAdapter)
**Reference:** Post 08 (application dispatch patterns)  
**Evidence:** Safe tool dispatch pattern  

```cpp
auto tool = find_tool(msg.method);
if (!tool) {
    log("Tool not found", msg.method);
    return error_response;
}
```

**Citation in Post 09:**
> "Tool registry lookup (from Post 08) uses hash table. 
> Default: unordered_map with O(1) lookup. 
> Attack: hash collision can degrade to O(n)."

---

### Parser Guarantees (Posts 04-07)
**Reference:** Posts 04-08 established size/depth limits  
**Evidence:** Prevents structural attacks

**Why relevant to Post 09:**
> "Size limits (Post 05) prevent oversized messages.
> Depth limits (Post 04) prevent deep nesting.
> But both allow small, shallow messages that are CPU-expensive to process."

---

## Part 2: Attack Examples (Pseudocode)

### Attack 1: Hash Collision DoS
**Type:** Algorithm attack on tool registry  
**Purpose:** Degrade O(1) lookup to O(n)

```cpp
// Tool registry (default C++)
std::unordered_map<string, ToolHandler> tools;
// tools["read"], tools["write"], tools["execute"]

// Attacker sends method name designed to collide with all tools
// In C++ unordered_map, collisions create linked list
// Lookup: O(n) instead of O(1)

// Impact: 
// 1000 requests × O(n) collision chain = DoS
```

**Real scenario:**
- Attacker enumerates tools via API discovery
- Attacker calculates hash colliding method name
- Attacker sends: `method="xyzCollision"` (1000× per second)
- Server: Registry lookup for each request becomes O(n)
- Result: CPU saturated

---

### Attack 2: ReDoS (Regular Expression DoS)
**Type:** Algorithm attack on regex validation  
**Purpose:** Exponential backtracking

```cpp
// Example: Email validation regex (common pattern)
std::regex email_pattern("^[a-z0-9+]*@[a-z0-9+]*$");

// Attacker sends: "aaaaaaaaaaaaaaaaaaaaa!" (25 'a's, no '@')
// Result: Regex engine tries all possible groupings
// Backtracking: 2^25 = 33 million possible matches to try
// Time: 30 seconds per request

// Impact:
// 4 concurrent regex validations = timeout
```

**Real scenario:**
- Tool validates email parameter: `validate_email(method)`
- Attacker sends malformed email in parameter
- Regex engine backtracks exponentially
- Server: Single validation request takes 30+ seconds
- Result: DoS from small messages

---

### Attack 3: Sorting Worst-Case
**Type:** Algorithm attack on sorting  
**Purpose:** Trigger O(n²) behavior in quicksort

```cpp
// Tool: "process_batch" sorts items
std::sort(items.begin(), items.end());

// Quicksort worst-case: O(n²) when pivot always splits into 1 + (n-1)
// Attacker: sends already-sorted array
// Result: Quicksort pivot selection triggers O(n²)

// Example: Sorting 1000 items
// Average: 10,000 comparisons (O(n log n))
// Worst: 1,000,000 comparisons (O(n²))
// 100× slower than average

// Impact:
// 1 request with 1000-item sort takes 1 second instead of 10ms
```

**Real scenario:**
- Tool sorts batch of items
- Attacker sends pre-sorted array
- Quicksort performance degrades to O(n²)
- Server: Single request causes CPU spike
- Result: DoS from algorithmic vulnerability

---

### Attack 4: JSON Path Traversal
**Type:** Algorithm attack on parameter extraction  
**Purpose:** O(n²) nested object access

```cpp
// Tool extracts nested parameter: json["a"]["b"]["c"]...["z"]
// Problem: Each bracket notation traverses from root
// With 26 levels: 26 traversals × 26 levels = 676 operations
// With 300 levels: 300 × 300 = 90,000 operations

// Example pseudocode (not optimal):
auto value = json["a"]["b"]["c"]["d"]["e"];
// Each [] operator traverses from root, not cached

// Attacker: sends deeply nested JSON
// {"a": {"b": {"c": ... (300 levels) ...}}}
// Parameter extraction: O(n²) in nesting depth

// Impact:
// Single parameter extraction takes seconds
```

**Real scenario:**
- Tool extracts value from nested JSON parameter
- Attacker sends 300-level nested structure
- Parameter extraction traverses 300 × 300 times
- Server: Extraction takes seconds
- Result: DoS from parameter parsing

---

### Attack 5: Rate Limit Bypass
**Type:** Algorithm attack on rate limiting  
**Purpose:** Bypass message-count limits

```cpp
// Rate limiting (message count):
// if (requests_per_second > 100) reject();

// Problem: Doesn't account for processing time
// Attacker: sends 100 small (1-byte) messages per second
// - Each message under rate limit (100 allowed)
// - Each message takes 100ms to validate (regex)
// - 100 messages/sec × 100ms = 10 seconds of work

// Result: 10 concurrent slow validations = 100% CPU

// Better rate limiting would track CPU time:
// if (total_cpu_time > limit) reject();
```

**Real scenario:**
- Server has rate limit: 100 requests/second
- Attacker sends 100 small messages/second (complies)
- Each message triggers regex validation (100ms)
- 10 concurrent validations = 100% CPU
- Result: DoS with rate-limit-compliant request rate

---

## Part 3: Evidence Summary — VERIFIED

### From Posts 04-08 (Existing Foundations)
| Defense | File | Purpose |
|---------|------|---------|
| Size limit | json_parser.cpp | Prevents large messages |
| Depth limit | json_parser.h (kMaxDepth=64) | Prevents deep nesting |
| Escape validation | json_parser.cpp | Prevents injection |
| Zero-copy | json_msg.h | Prevents heap corruption |
| Tool dispatch | MCPAdapter | Safe registry lookup |

### Post 09 Attack Examples — Real Incidents

| Attack | Evidence | Source | Status |
|--------|----------|--------|--------|
| **Hash Collision** | oCERT-2011-003 CVE-2011-4858 (Tomcat), CVE-2011-4815 (Ruby), CVE-2011-4885 (PHP), CVE-2011-5036 (Rack) | [oCERT Advisory](https://ocert.org/advisories/ocert-2011-003.html) | ✅ VERIFIED |
| **ReDoS** | CloudFlare WAF outage July 2, 2019 (13:42–14:09 UTC), 27 minutes, regex: `.*(?:.*=.*)` | [CloudFlare Blog](https://blog.cloudflare.com/details-of-the-cloudflare-outage-on-july-2-2019/) | ✅ VERIFIED |
| **Quicksort** | "Denial of Service via Algorithmic Complexity Attacks" Crosby & Wallach (2003), antiqsort tool | [USENIX 2003](https://www.usenix.org/conference/12th-usenix-security-symposium/denial-service-algorithmic-complexity-attacks) | ✅ VERIFIED |
| **JSON Path Traversal** | CVE-2020-36518 (Jackson-databind), CVE-2025-53864 (Nimbus JOSE), CVE-2023-5123 (Grafana) | [Jackson GitHub](https://github.com/FasterXML/jackson-databind/security), [Nimbus Advisory](https://advisories.gitlab.com/pkg/maven/com.nimbusds/nimbus-jose-jwt/CVE-2025-53864/), [Grafana](https://grafana.com/security/security-advisories/cve-2023-5123/) | ✅ VERIFIED |
| **Rate Limit Bypass** | CVE-2026-34573 (Parse Server), CVE-2024-12243 (GnuTLS), Slowloris (2009) | [Parse Server](https://radar.offseq.com/threat/cve-2026-34573), [GnuTLS](https://radar.offseq.com/threat/cve-2024-12243), [Slowloris/CloudFlare](https://www.cloudflare.com/learning/ddos/ddos-attack-tools/slowloris/) | ✅ VERIFIED |

---

## Part 4: Quality Checklist

Before drafting, verify:

- [ ] All five attacks are pseudocode (not production code)
- [ ] Attacks clearly show why size/depth limits don't prevent them
- [ ] Real-world scenarios are realistic
- [ ] Code examples compile (or marked pseudocode)
- [ ] References to Posts 04-08 are accurate
- [ ] Scope rule verified: Only Post 10 mentioned, no full roadmap
- [ ] No overclaiming (defenses deferred to later posts)

---

**Status:** ✅ CODE MAPPING COMPLETE  
**Date:** April 19, 2026  
**Next:** Phase 3 (Draft blog post)
