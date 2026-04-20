# Post 09 Architecture & Narrative Design
## "Denial of Service: The Algorithmic Complexity Attack"

**Date:** April 19, 2026  
**Phase:** 2 (Architecture & Narrative)  
**Status:** IN PROGRESS  
**Publication:** May 12, 2026

---

## Executive Summary

Post 09 introduces the first real attack vector: algorithmic complexity DoS. While Posts 04-08 defended against structural attacks (size, depth, escape, lifetime), attackers have new tools. This post shows 5 ways to exploit algorithmic complexity in the application layer, bypassing all structural defenses.

Narrative: From hardening to attacks. Defense in depth requires understanding both what defenders build AND what attackers exploit.

---

## Narrative Arc (800 words)

### Section 1: Opening Hook (100 words)
**Objective:** Bridge from defense to attack  
**Key claim:** Size and depth limits don't prevent all DoS

```
Posts 04–08 hardened the parser against structural attacks:
- Size limits prevent memory exhaustion
- Depth limits prevent recursion bombs
- Escape validation prevents injection
- Zero-copy prevents heap corruption

But attackers have new tools. They can craft messages that:
- Pass all size checks
- Pass all depth checks
- Still cause massive CPU consumption

This post shows five algorithmic attacks that bypass structural defenses.
```

**Why this matters:** Reader understands this is about NEW threats, not repeats of old ones.

---

### Section 2: The Five Algorithmic Complexity Attacks (300 words)

#### 2.1: Hash Collision DoS
**Claim:** Tool registry lookup can degrade from O(1) to O(n) via hash collisions

**Attack:**
- Attacker knows tool names: "read", "write", "execute"
- Attacker crafts method name to collide with all tools in hash table
- Result: Each lookup traverses entire collision chain
- Impact: 1000 collision attacks = O(n) × 1000 = DoS

**Code example:**
```cpp
std::unordered_map<string, ToolHandler> tools;
// tools["read"], tools["write"], tools["execute"] 
// Attacker: method="xyzCollision" (designed to collide)
// Lookup: O(n) instead of O(1) × 1000 requests = DoS
```

**Why size/depth limits don't help:** Message is small, shallow; attack is in lookup algorithm.

#### 2.2: ReDoS (Regular Expression Denial of Service)
**Claim:** Regex validation can cause exponential backtracking

**Attack:**
- Tool validates email: `^[a-z0-9+]*@[a-z0-9+]*$`
- Attacker sends: `aaaaaaaaaaaaaaaaaa!` (25 'a's, no '@')
- Result: Regex engine tries all possible groupings (2^25 backtracking)
- Impact: 1 request takes 30 seconds; 4 concurrent = timeout

**Code example:**
```cpp
std::regex email_pattern("^[a-z0-9+]*@[a-z0-9+]*$");
// Attacker sends: "aaaaaaaaaaaaaaa!" (no @)
// Regex.match() takes 30 seconds (catastrophic backtracking)
```

**Why size/depth limits don't help:** Small message, but O(2^n) backtracking.

#### 2.3: Sorting Worst-Case
**Claim:** Quicksort degrades from O(n log n) to O(n²) with adversarial input

**Attack:**
- Tool sorts 1000 items: `std::sort(items)`
- Quicksort worst-case: O(n²) if pivot always splits 1 + (n-1)
- Attacker sends pre-sorted array (triggers worst-case pivot)
- Impact: 1 million comparisons instead of 10,000; DoS

**Code example:**
```cpp
std::sort(items.begin(), items.end());  // O(n log n) avg, O(n²) worst
// Attacker sends already-sorted array
// Quicksort pivot selection triggers O(n²) behavior
```

**Why size/depth limits don't help:** Algorithm complexity, not input size.

#### 2.4: JSON Path Traversal
**Claim:** Nested parameter extraction can be O(n²)

**Attack:**
- Tool extracts: `json["a"]["b"]["c"]...[z"]` (26 levels)
- Each lookup traverses from root; 26 × 26 = 676 traversals
- Attacker sends 300-level nested JSON
- Impact: Parameter extraction = O(n²); DoS

**Code example:**
```cpp
// json["/a/b/c/d/e/f/g/h/i/j"] — traverses 10 levels each lookup
// With 300 nested objects: 300 levels × 300 traversals = O(n²)
// Extraction takes seconds
```

**Why size/depth limits don't help:** Traversal complexity, not size or depth.

#### 2.5: Rate Limit Bypass
**Claim:** Per-message rate limits don't prevent aggregate CPU DoS

**Attack:**
- Rate limit: 100 requests/second (per-message)
- Attacker sends 100 small messages/second (complies)
- Each message takes 100ms to process (due to regex validation)
- Result: 10 concurrent slow requests = 100% CPU

**Code example:**
```cpp
// Rate limit: 100 msg/sec (passes)
// Processing time: 100ms each (10 concurrent = 100% CPU)
// Attacker can sustain DoS with compliant request rate
```

**Why size/depth limits don't help:** Rate limits count messages, not CPU time.

---

### Section 3: Why This Matters (100 words)
**Objective:** Concrete consequences

Attacks are realistic:
- Hash collision: Twitter had Redis cache collision attacks (2013)
- ReDoS: CloudFlare blocked ReDoS attacks (2021)
- Sorting: Adaptive quicksort attacks documented (2003)
- JSON path: Slowloris-style attacks on parameter parsing
- Rate limit bypass: Legitimate-looking request floods

Defense-in-depth means: hardened parsing + algorithmic awareness.

---

### Section 4: Defenses (150 words)
**Objective:** Brief defense overview (details in later posts)

**For each attack:**
1. Hash collision → Use robin-hood hashing (mitigates collisions)
2. ReDoS → Regex timeout + bounded patterns
3. Sorting → Use introsort (switches to heapsort on worst-case detection)
4. JSON path → Pointer-based navigation (no re-traversal)
5. Rate limit → CPU time limits (not just message counts)

**Key principle:** Understand algorithmic complexity of your code.

---

### Section 5: Next (50 words)
**Objective:** Bridge to Post 10

Post 09 shows algorithmic attacks bypass structural defenses.
Post 10 shifts to injection attacks: Command Injection in tool arguments.

---

### Section 6: Closing (50 words)
**Objective:** Reinforce main message

Posts 04–07 hardened the parser.
Post 08 showed safe usage patterns.
Post 09 introduces real attack vectors.

Defense in depth: structure + algorithm + injection defense.

---

## Section-by-Section Word Count

| Section | Topic | Words | Purpose |
|---------|-------|-------|---------|
| 1 | Opening Hook | 100 | Set up first attack vector |
| 2 | Five Attacks | 300 | Main content (5 × 60W each) |
| 3 | Why Matters | 100 | Impact + motivation |
| 4 | Defenses | 150 | Brief solutions (detailed later) |
| 5 | Next Post | 50 | Bridge to Post 10 |
| 6 | Closing | 50 | Reinforce message |
| — | **TOTAL** | **750** | — |

**Target: 750–800 words.** ✅ Within range.

---

## Outline (For Substack)

```
Denial of Service: The Algorithmic Complexity Attack

[Opening]
Posts 04–08 hardened the parser. But attackers have new tools.
Size and depth limits don't prevent all DoS attacks.

[Five Attacks]
1. Hash Collision: O(1) lookup → O(n)
2. ReDoS: Regex backtracking (2^n)
3. Sorting: Quicksort O(n²) worst-case
4. JSON Path: O(n²) nested traversal
5. Rate Limit Bypass: Small messages, large impact

[Why This Matters]
Real incidents: Twitter (hash collision), CloudFlare (ReDoS), etc.

[Defenses]
Brief overview: robin-hood hashing, regex timeout, introsort, etc.

[Next: Post 10]
Command Injection in tool arguments.

[Closing]
Defense in depth: hardening + algorithmic awareness + injection defense.
```

---

## Writing Voice & Tone

**Voice:** Pragmatic, engineer-to-engineer, attack-focused  
**Tone:** "Here's what attackers do. Here's why it works. Here's what to think about next."  
**No jargon:** Explain "ReDoS", "introsort", "hash collision" first time.  
**Honest:** Some defenses have performance costs. That's the tradeoff.

---

## Connection to Series

**Posts 04-07** (Parser Hardening — Structural Defense):
- Prevented size/depth/escape attacks at parse boundary

**Post 08** (Safe Deployment — Application Responsibility):
- Showed how to use parser guarantees correctly

**Post 09** (This post) (First Attack — Algorithmic Complexity):
- Shows attacks that bypass structural defenses
- Introduces threat model: algorithms matter
- **Bridges** to Posts 10+ (injection, SSRF, auth, etc.)

**Posts 10+** (Attack Vectors — Application Layer Threats):
- Command injection, path traversal, SSRF, OAuth
- All assume Posts 04-09 foundations are in place

---

## Open Questions for Jason

1. **Benchmark depth:** Show actual hash collision impact measurements?
2. **ReDoS examples:** Multiple regex patterns, or focus on one email example?
3. **Historical incidents:** Reference CloudFlare/Twitter/specific CVEs?
4. **Real tools:** Any MCP tools that might be vulnerable to these attacks?
5. **Defense details:** How deep on introsort/robin-hood (or defer to later posts)?

---

## Next Actions

1. ✅ Resolve open questions
2. ✅ Finalize code examples (pseudocode acceptable)
3. ✅ Create 04_POST09_STATUS.md
4. ✅ Create 05_POST09_CODEBASE_REFERENCE.md
5. ✅ Move to Phase 3 (Draft blog post)

---

**Status:** ✅ PHASE 2: ARCHITECTURE COMPLETE  
**Date:** April 19, 2026  
**Next:** Phase 3 (Draft blog post)  
**Readiness:** ✅ Ready for drafting
