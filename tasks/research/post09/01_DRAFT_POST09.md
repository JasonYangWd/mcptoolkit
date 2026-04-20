# Denial of Service: The Algorithmic Complexity Attack

Published: May 12, 2026
By: Jason Yang
Series: The Secure MCP — Attack Vectors & Defense

---

## Opening: Attacks Beyond Structure

Posts 04–07 hardened the MCP parser against structural attacks: size limits prevent memory exhaustion, depth limits prevent recursion bombs, escape validation prevents injection, and zero-copy prevents heap corruption. Post 08 showed how to use those guarantees safely in application code.

But attackers have new tools. They can craft messages that pass all size checks, pass all depth checks, yet cause massive CPU consumption. This post shows five ways to exploit algorithmic complexity, bypassing every structural defense.

---

## The Five Algorithmic Complexity Attacks

### 1. Hash Collision DoS

Tool registries use hash tables for fast O(1) lookup. But hash tables have a weakness: collisions. When attacker-controlled method names collide, lookup degrades from O(1) to O(n).

**Attack:** Attacker enumerates tools via API discovery (`read`, `write`, `execute`), then crafts a method name designed to collide with all of them in the hash table. Each request now takes O(n) time to look up. With 1,000 collision requests, the server's CPU saturates.

**Why size/depth limits don't help:** The message is small and shallow. The attack is in the lookup algorithm.

**Real incident:** Hash collision DoS was disclosed in oCERT-2011-003 (2011), affecting multiple languages and frameworks. CVEs include CVE-2011-4858 (Apache Tomcat), CVE-2011-4815 (Ruby), CVE-2011-4885 (PHP), and CVE-2011-5036 (Rack). Attackers enumerated hash function outputs, crafted colliding keys, and caused CPU exhaustion across production systems.

### 2. ReDoS (Regular Expression Denial of Service)

Many tools validate parameters using regex. Regex engines can fail catastrophically: exponential backtracking when patterns don't match.

**Attack:** Tool validates email with `^[a-z0-9+]*@[a-z0-9+]*$`. Attacker sends `aaaaaaaaaaaaaaaaaa!` (25 'a's, no '@'). The regex engine tries all possible groupings—2^25 combinations—and takes 30 seconds to fail. Four concurrent validation requests timeout the server.

**Why size/depth limits don't help:** Small message, but O(2^n) backtracking.

**Real incident:** On July 2, 2019, CloudFlare's Web Application Firewall suffered a 27-minute global outage when a single regex pattern in their WAF caused 100% CPU exhaustion across their entire network. A new rule contained a catastrophic backtracking pattern. Response: CloudFlare rewrote their WAF using the non-backtracking Rust regex library (algorithm similar to RE2) to prevent ReDoS entirely.

### 3. Sorting Worst-Case

Many tools sort parameters or batch items. Quicksort has excellent average performance—O(n log n)—but terrible worst-case: O(n²) when the pivot always splits the array into 1 + (n-1).

**Attack:** Attacker sends a pre-sorted array. Quicksort with naive pivot selection will trigger worst-case behavior. Sorting 1,000 items goes from 10,000 comparisons (average) to 1,000,000 (worst-case). A single request takes 100× longer.

**Why size/depth limits don't help:** Algorithm complexity, not input size.

**Real incident:** Quicksort DoS was formally documented in "Denial of Service via Algorithmic Complexity Attacks" (2003). Tools like `antiqsort` demonstrate how attackers can craft inputs that trigger O(n²) behavior in any quicksort implementation. Modern C++ standard library mitigates this: `std::sort` uses introsort, which detects quadratic behavior and switches to heapsort automatically.

### 4. JSON Path Traversal

Tools often extract nested parameters: `json["a"]["b"]["c"]`. If implemented naively—re-traversing from root on each bracket—this becomes O(n²) in nesting depth.

**Attack:** Attacker sends deeply nested JSON (300 levels). Parameter extraction requires 300 traversals × 300 nesting depth = 90,000 operations. A single parameter extraction takes seconds.

**Why size/depth limits don't help:** Not the size or depth itself—the traversal algorithm.

**Real incidents:** 
- CVE-2020-36518 (Jackson-databind): Deeply nested JSON causes stack overflow (uncontrolled recursion). Vulnerable versions: 2.0–2.12.6.0 and 2.13.0–2.13.2.0. Patched in 2.12.6.1 and 2.13.2.1.
- CVE-2025-53864 (Nimbus JOSE + JWT): Uncontrolled recursion on deeply nested JSON in JWT claim sets. Versions 10.0–10.0.1 vulnerable. Patched in 10.0.2.
- CVE-2023-5123 (Grafana JSON datasource): Path traversal in JSON parameter handling allows bypassing configured sub-path restrictions via `../` sequences. Patched in 1.3.21+.

### 5. Rate Limit Bypass

Rate limits typically count messages: "100 requests/second." But they ignore processing time. Attacker sends 100 small messages per second (complies with limit), each taking 100ms to process (regex validation). With 10 concurrent slow requests, the server's CPU hits 100%.

**Attack:** Attacker sends 100 size-1 messages per second, each triggering expensive validation. Server is compliant with rate limits but CPU-bound. Request queue grows, legitimate requests timeout.

**Why size/depth limits don't help:** Rate limits count messages, not CPU time.

**Real incidents:**
- CVE-2026-34573 (Parse Server): GraphQL query complexity validator bypass. A single crafted query with binary fan-out fragment spreads can block the Node.js event loop for seconds, denying service to all concurrent users despite meeting rate limits.
- CVE-2024-12243 (GnuTLS libtasn1): Inefficient algorithmic complexity in certificate decoding. Specially crafted DER-encoded certificates consume excessive CPU during TLS handshakes, causing DoS while rate limits see only legitimate-looking requests.
- Slowloris (2009): Classic slow HTTP request attack. Sends partial requests at low request-per-second rates, keeping connections open. Evades simple rate limiting because it's below the request-count threshold but ties up server resources.

---

## Why This Matters

Posts 04–07 built structural defenses. Post 08 showed safe application patterns. But attackers have proven that algorithmic awareness is the next layer of defense—not theoretical, but documented in real systems and production exploits.

Here's the critical gap: **size and depth limits are necessary but insufficient.** You can reject messages larger than 1MB, shallower than 64 levels, and with invalid escapes. Then a 500-byte message with 10-level nesting can still DoS your server if the algorithm inside is exploitable.

Developers often miss this. They harden the parsing boundary and assume the work is done. But the application layer—tool registries, parameter validation, batch processing—has algorithmic vulnerabilities that structural defenses don't address.

Each attack above shows a real incident where hardened parsing failed to prevent DoS. Major web frameworks fell to hash collision attacks in 2011–2012 (PHP, Java, Ruby, Python, Tomcat). CloudFlare was hit by regex engines in 2019. Sorting and parameter parsing vulnerabilities affect every framework. The pattern is consistent: attackers exploit the algorithms we trust.

---

## Defenses (Brief Overview)

For each attack, there's a defense. None are free—all have tradeoffs. But ignoring these vulnerabilities is costlier than the performance hit.

**1. Hash Collision:** Use robin-hood hashing or cuckoo hashing instead of chained hashing. These distribute collisions more evenly and guarantee O(log n) worst-case instead of O(n). Overhead: ~10% slower average lookup, but worst-case is bounded. Cost of not doing this: DoS from collision attacks.

**2. ReDoS:** Add regex timeout (reject patterns that take >100ms) and avoid backtracking-prone patterns like `(a+)+`. Validate regex patterns at tool registration time, not at runtime. Some regex engines (like Rust's `regex` crate) are designed to prevent catastrophic backtracking by default. Overhead: regex compilation cost, timeouts on edge cases. Cost of not doing this: Single request takes 30+ seconds; DoS.

**3. Sorting:** Use introsort (C++'s `std::sort` does this automatically), which switches to heapsort when detecting O(n²) behavior. Alternatively, use randomized quicksort with random pivot selection to make the worst case probabilistically unlikely. Overhead: Negligible—introsort is already in standard libraries. Cost of not doing this: Single request takes 100× longer on adversarial input.

**4. JSON Path:** Cache traversal results or use pointer-based navigation instead of re-traversing from root on each bracket. If building parameter extraction, optimize for common cases (shallow paths) and set depth limits on path traversal. Overhead: Memory for caching, but typically minimal. Cost of not doing this: Parameter extraction takes seconds on deeply nested JSON.

**5. Rate Limit:** Track CPU time per connection, not just message count. If a message is slow (due to regex or sorting), count it proportionally. Token bucket algorithms with CPU-aware weighting work well. Overhead: More complex rate limit logic, but prevents slow-request DoS. Cost of not doing this: Attackers send compliant request counts but exhaust CPU.

Detailed implementations and security testing patterns in Posts 19-20 (Fuzzing & Security Testing).

---

## Next: Post 10

With parser hardening (Posts 04-07), safe deployment (Post 08), and algorithmic DoS (Post 09) established, Post 10 shifts to injection attacks: Command Injection in tool arguments. Attackers exploit unvalidated parameters to execute arbitrary commands.

---

**Series:** Posts 04–07 hardened the parser. Post 08 showed safe usage. Post 09 introduced algorithmic attacks. Posts 10+ explore injection, SSRF, and authorization threats. Defense in depth: structure + algorithm + content validation—all matter.

---

## Key Takeaways

- Structural defenses (size, depth limits) don't prevent all DoS
- Algorithmic complexity can be exploited even with small, shallow messages
- Common patterns (hash tables, regex, sorting) have known worst-cases
- Rate limiting must account for processing time, not just message count
- Next layer: injection attacks that bypass all structural and algorithmic defenses
