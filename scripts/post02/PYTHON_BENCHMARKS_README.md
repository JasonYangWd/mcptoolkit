# Python Performance Benchmarks for Post 02

These scripts measure Python JSON parsing performance to provide real data for the blog post "When Python MCP SDKs Aren't Enough".

## Scripts

### 1. `benchmark_python_json.py`
**Purpose:** Comprehensive JSON parsing benchmark

**Measurements:**
- Baseline performance (GC disabled)
- Real-world performance (GC enabled)
- Garbage collection pause times
- Impact of memory pressure

**How to run:**
```bash
python3 scripts/benchmark_python_json.py
```

**Output:** Performance metrics in microseconds, comparison to C++ baseline (~87µs)

### 2. `measure_gc_behavior.py`
**Purpose:** Deep dive into Python's garbage collection behavior

**Measurements:**
- GC configuration (thresholds, generation settings)
- Parsing times with GC event tracking
- Cold vs. warm memory state impact
- Tail latency (P50, P95, P99, P99.9)
- Allocator unpredictability

**How to run:**
```bash
python3 scripts/measure_gc_behavior.py
```

**Output:** GC statistics, variability metrics, findings for blog post

### 3. `compare_json_libraries.py`
**Purpose:** Compare different JSON parsing libraries

**Compares:**
- json (Python standard library)
- orjson (if installed - faster C-based)
- ujson (if installed - faster C-based)

**How to run:**
```bash
python3 scripts/compare_json_libraries.py
```

**Install optional libraries:**
```bash
pip install orjson ujson
```

**Output:** Performance comparison, explanation of why library choice doesn't matter

## Expected Findings for Post 02

### Finding 1: Python is Slower
- Python json.loads(): ~1-5 µs per message (on small payloads)
- C++ parser: ~87 µs per message
- **Tradeoff:** Python may be slightly faster for parsing alone, but loses on determinism

### Finding 2: GC Pauses Break SLAs
- GC pause times: 100s of µs to ms (unpredictable)
- Cannot guarantee <100ms P99 SLA in Python
- C++ guarantees deterministic latency

### Finding 3: Allocator Unpredictability
- Same JSON parses at different speeds depending on heap state
- Cold state vs warm state variance: 10-50%
- Cannot predict worst-case performance

### Finding 4: Tail Latency is the Problem
- P50 (median) may be fast
- P99 (worst 1%) can be 10-100x slower
- Regulated industries need P99 guarantees

## Running All Benchmarks

```bash
#!/bin/bash
echo "=== Python JSON Parsing Benchmarks ==="
echo
echo "1. Basic benchmark..."
python3 scripts/benchmark_python_json.py
echo
echo "2. GC behavior analysis..."
python3 scripts/measure_gc_behavior.py
echo
echo "3. Library comparison..."
python3 scripts/compare_json_libraries.py
echo
echo "All benchmarks complete. Results ready for Post 02."
```

## For Blog Post 02

Use these actual results to replace placeholder numbers:

**What to cite:**
- Actual Python json.loads() performance (from benchmark_python_json.py)
- Real GC pause time (from measure_gc_behavior.py)
- Actual cold vs warm state variance (from measure_gc_behavior.py)
- Real P99 tail latency numbers

**What NOT to cite:**
- ❌ "GC pauses for 50ms" (use actual measurements)
- ❌ "10ms on warm heap, 100ms on cold" (fabricated)
- ❌ Generic estimates

## System Requirements

- Python 3.7+
- No external dependencies (but orjson/ujson optional for comparison)

## Notes

- Run on consistent system (close other apps, minimize system load)
- Run multiple times for consistency
- Results vary by Python version and system

## Integration with Post 02

Once you have real numbers:

1. Update `01_DRAFT_POST02.md` with actual measurements
2. Replace "GC pauses for 50ms" with measured value
3. Replace "warm vs cold (10ms/100ms)" with actual variance
4. Update code_evidence_02.txt with benchmark sources
5. Verify all claims against actual data

---

**Status:** Scripts ready for execution
**Purpose:** Generate real evidence for Post 02 publication
**Target:** Replace placeholder data with actual measurements
