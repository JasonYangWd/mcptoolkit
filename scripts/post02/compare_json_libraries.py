#!/usr/bin/env python3
"""
Compare different Python JSON libraries
Show why standard library is a liability for security boundaries

Usage:
    python3 compare_json_libraries.py
"""

import json
import time
import statistics
import sys

SAMPLE_MESSAGE = '{"jsonrpc": "2.0", "id": 1, "method": "test", "params": {"data": "test"}}'


def benchmark_library(library_name: str, parse_func, iterations: int = 1000) -> dict:
    """Benchmark a JSON library."""
    try:
        times_us = []

        for _ in range(iterations):
            start = time.perf_counter()
            obj = parse_func(SAMPLE_MESSAGE)
            end = time.perf_counter()
            times_us.append((end - start) * 1_000_000)

        times_us_sorted = sorted(times_us)
        n = len(times_us_sorted)

        return {
            "library": library_name,
            "available": True,
            "min": times_us_sorted[0],
            "avg": statistics.mean(times_us_sorted),
            "p99": times_us_sorted[int(n * 0.99)],
            "max": times_us_sorted[-1],
        }
    except Exception as e:
        return {
            "library": library_name,
            "available": False,
            "error": str(e),
        }


def main():
    print("=" * 70)
    print("Python JSON Library Comparison")
    print("Compare standard library to alternatives")
    print("=" * 70)
    print()

    results = []

    # Standard library json
    print("Benchmarking json (standard library)...")
    result_json = benchmark_library("json", json.loads, iterations=1000)
    results.append(result_json)

    # Try orjson (faster, C-based)
    print("Benchmarking orjson (if available)...")
    try:
        import orjson
        result_orjson = benchmark_library("orjson", orjson.loads, iterations=1000)
        results.append(result_orjson)
    except ImportError:
        results.append({"library": "orjson", "available": False, "error": "Not installed"})

    # Try ujson (faster, C-based)
    print("Benchmarking ujson (if available)...")
    try:
        import ujson
        result_ujson = benchmark_library("ujson", ujson.loads, iterations=1000)
        results.append(result_ujson)
    except ImportError:
        results.append({"library": "ujson", "available": False, "error": "Not installed"})

    print()
    print("=" * 70)
    print("Results:")
    print("=" * 70)
    print()

    for result in results:
        print(f"{result['library']}:")
        if result.get("available", False):
            print(f"  Min:  {result['min']:8.2f} µs")
            print(f"  Avg:  {result['avg']:8.2f} µs")
            print(f"  P99:  {result['p99']:8.2f} µs")
            print(f"  Max:  {result['max']:8.2f} µs")
        else:
            print(f"  Status: NOT AVAILABLE ({result.get('error', 'Unknown error')})")
        print()

    # Show why this matters
    print("=" * 70)
    print("Why This Matters for Post 02:")
    print("=" * 70)
    print()
    print("Key Point:")
    print("  Even with different JSON libraries, Python's JSON parsing is")
    print("  unpredictable because it shares the Python runtime, which has:")
    print("  - Garbage collection pauses")
    print("  - Unpredictable memory allocations")
    print("  - No deterministic latency guarantees")
    print()
    print("The Problem:")
    print("  You can't choose a \"fast enough\" JSON library in Python and")
    print("  guarantee latency bounds. The runtime itself is the bottleneck.")
    print()
    print("The Solution:")
    print("  C++ parser gives you control over the entire execution,")
    print("  enabling deterministic latency bounds (~87µs measured).")
    print("=" * 70)


if __name__ == "__main__":
    main()
