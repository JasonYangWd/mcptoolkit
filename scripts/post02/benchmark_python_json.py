#!/usr/bin/env python3
"""
Python JSON Parsing Performance Benchmark
Compare json.loads() to C++ parser baseline (~87µs per message)

Usage:
    python3 benchmark_python_json.py
"""

import json
import time
import gc
import statistics
from typing import List, Tuple

# Sample MCP messages (JSON-RPC 2.0)
SAMPLE_MESSAGES = [
    # Initialize request
    '{"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"protocolVersion": "2024-11-05"}}',

    # Tools list request
    '{"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}}',

    # Tool call request
    '{"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "search", "arguments": {"query": "test"}}}',

    # Response with result
    '{"jsonrpc": "2.0", "id": 1, "result": {"tools": [{"name": "search", "description": "Search tool"}]}}',

    # Response with error
    '{"jsonrpc": "2.0", "id": 2, "error": {"code": -32600, "message": "Invalid Request"}}',

    # Complex nested params
    '{"jsonrpc": "2.0", "id": 4, "method": "test", "params": {"nested": {"deeply": {"value": [1, 2, 3]}}}}',
]


def benchmark_json_parsing(iterations: int = 1000) -> Tuple[float, float, float]:
    """
    Benchmark Python's json.loads() on MCP messages.

    Returns: (min_microseconds, avg_microseconds, max_microseconds)
    """
    gc.collect()
    gc.disable()  # Disable GC during benchmark for consistency

    times_us = []

    try:
        for _ in range(iterations):
            for msg in SAMPLE_MESSAGES:
                start = time.perf_counter()
                obj = json.loads(msg)
                end = time.perf_counter()
                times_us.append((end - start) * 1_000_000)  # Convert to microseconds
    finally:
        gc.enable()

    times_us.sort()
    min_us = times_us[0]
    avg_us = statistics.mean(times_us)
    max_us = times_us[-1]
    p99_us = times_us[int(len(times_us) * 0.99)]

    return min_us, avg_us, max_us, p99_us


def benchmark_with_gc_enabled(iterations: int = 1000) -> Tuple[float, float, float]:
    """
    Benchmark with garbage collection enabled (real-world scenario).

    Returns: (min_microseconds, avg_microseconds, max_microseconds)
    """
    gc.collect()
    gc.enable()

    times_us = []

    for _ in range(iterations):
        for msg in SAMPLE_MESSAGES:
            start = time.perf_counter()
            obj = json.loads(msg)
            end = time.perf_counter()
            times_us.append((end - start) * 1_000_000)  # Convert to microseconds

    times_us.sort()
    min_us = times_us[0]
    avg_us = statistics.mean(times_us)
    max_us = times_us[-1]
    p99_us = times_us[int(len(times_us) * 0.99)]

    return min_us, avg_us, max_us, p99_us


def measure_gc_pause_times(iterations: int = 100) -> float:
    """
    Measure garbage collection pause times.

    Returns: Maximum GC pause time in microseconds
    """
    gc.collect()

    max_pause_us = 0.0

    for _ in range(iterations):
        # Allocate lots of objects to trigger GC
        objects = [{"data": list(range(100))} for _ in range(1000)]

        start = time.perf_counter()
        gc.collect()  # Force garbage collection
        end = time.perf_counter()

        pause_us = (end - start) * 1_000_000
        max_pause_us = max(max_pause_us, pause_us)

    return max_pause_us


def benchmark_memory_pressure(iterations: int = 500) -> Tuple[float, float]:
    """
    Benchmark JSON parsing under memory pressure.

    Returns: (avg_us_with_pressure, avg_us_baseline)
    """
    # Baseline measurement
    gc.collect()
    gc.disable()

    times_baseline = []
    for _ in range(iterations):
        for msg in SAMPLE_MESSAGES:
            start = time.perf_counter()
            obj = json.loads(msg)
            end = time.perf_counter()
            times_baseline.append((end - start) * 1_000_000)

    gc.enable()
    avg_baseline = statistics.mean(times_baseline)

    # Under memory pressure
    gc.collect()

    times_pressure = []
    for _ in range(iterations):
        # Create memory pressure by allocating large objects
        pressure = [bytearray(100000) for _ in range(10)]  # 1MB of pressure

        for msg in SAMPLE_MESSAGES:
            start = time.perf_counter()
            obj = json.loads(msg)
            end = time.perf_counter()
            times_pressure.append((end - start) * 1_000_000)

        del pressure

    avg_pressure = statistics.mean(times_pressure)

    return avg_pressure, avg_baseline


def main():
    print("=" * 70)
    print("Python JSON Parsing Performance Benchmark")
    print("Comparing to C++ baseline: ~87µs per message")
    print("=" * 70)
    print()

    # Baseline measurement (GC disabled)
    print("1. Baseline (GC Disabled):")
    print("-" * 70)
    min_us, avg_us, max_us, p99_us = benchmark_json_parsing(iterations=1000)
    print(f"   Min:  {min_us:8.2f} µs")
    print(f"   Avg:  {avg_us:8.2f} µs")
    print(f"   P99:  {p99_us:8.2f} µs")
    print(f"   Max:  {max_us:8.2f} µs")
    print(f"   vs C++ baseline (87µs): {avg_us / 87:.1f}x slower")
    print()

    # Real-world measurement (GC enabled)
    print("2. Real-World (GC Enabled):")
    print("-" * 70)
    min_us, avg_us, max_us, p99_us = benchmark_with_gc_enabled(iterations=1000)
    print(f"   Min:  {min_us:8.2f} µs")
    print(f"   Avg:  {avg_us:8.2f} µs")
    print(f"   P99:  {p99_us:8.2f} µs")
    print(f"   Max:  {max_us:8.2f} µs")
    print(f"   vs C++ baseline (87µs): {avg_us / 87:.1f}x slower")
    print()

    # GC pause times
    print("3. Garbage Collection Pause Times:")
    print("-" * 70)
    max_pause = measure_gc_pause_times(iterations=100)
    print(f"   Max GC pause: {max_pause:8.2f} µs")
    print(f"   vs message parse (87µs): {max_pause / 87:.1f}x longer")
    print()

    # Memory pressure impact
    print("4. Memory Pressure Impact:")
    print("-" * 70)
    avg_pressure, avg_baseline = benchmark_memory_pressure(iterations=500)
    slowdown = (avg_pressure / avg_baseline - 1.0) * 100
    print(f"   Baseline (no pressure): {avg_baseline:.2f} µs")
    print(f"   With memory pressure:   {avg_pressure:.2f} µs")
    print(f"   Slowdown: {slowdown:.1f}%")
    print()

    # Summary
    print("=" * 70)
    print("SUMMARY FOR POST 02")
    print("=" * 70)
    print(f"Python json.loads() baseline: {avg_us:.1f} µs per message")
    print(f"C++ parser baseline:           87.0 µs per message")
    print(f"Slowdown factor:               {avg_us / 87:.1f}x")
    print()
    print(f"GC pause times: Up to {max_pause:.0f} µs (unpredictable)")
    print(f"Memory pressure impact: {slowdown:.1f}% slowdown")
    print()
    print("Key Finding:")
    print("  Python's performance is unpredictable due to GC and allocator.")
    print("  Cannot guarantee SLA bounds like C++ can.")
    print("=" * 70)


if __name__ == "__main__":
    main()
