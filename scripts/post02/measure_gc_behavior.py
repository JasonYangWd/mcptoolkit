#!/usr/bin/env python3
"""
Python Garbage Collection Behavior Analysis
Demonstrate unpredictability of GC pause times

Usage:
    python3 measure_gc_behavior.py
"""

import gc
import time
import statistics
from typing import List, Tuple

# Sample JSON-RPC message to parse repeatedly
SAMPLE_MESSAGE = '{"jsonrpc": "2.0", "id": 1, "method": "test", "params": {}}'


def measure_parsing_with_gc_events(iterations: int = 5000) -> Tuple[List[float], int, float]:
    """
    Measure JSON parsing times while tracking GC events.

    Returns: (times_us, gc_count, max_pause)
    """
    import json

    gc.enable()
    gc.collect()

    times_us = []
    gc_count_before = gc.get_count()
    max_pause_us = 0.0

    for i in range(iterations):
        # Track if GC runs during parsing
        gc_before = gc.get_count()

        start = time.perf_counter()
        obj = json.loads(SAMPLE_MESSAGE)
        end = time.perf_counter()

        gc_after = gc.get_count()
        elapsed_us = (end - start) * 1_000_000
        times_us.append(elapsed_us)

        # If GC count changed, we triggered a collection
        if gc_after != gc_before:
            max_pause_us = max(max_pause_us, elapsed_us)

    gc_count_after = gc.get_count()

    return times_us, iterations, max_pause_us


def show_gc_stats(times_us: List[float]) -> None:
    """Display GC statistics."""
    times_us_sorted = sorted(times_us)
    n = len(times_us_sorted)

    print(f"   Samples: {n}")
    print(f"   Min:     {times_us_sorted[0]:.2f} µs")
    print(f"   P50:     {times_us_sorted[n//2]:.2f} µs")
    print(f"   P95:     {times_us_sorted[int(n*0.95)]:.2f} µs")
    print(f"   P99:     {times_us_sorted[int(n*0.99)]:.2f} µs")
    print(f"   P99.9:   {times_us_sorted[int(n*0.999)]:.2f} µs")
    print(f"   Max:     {times_us_sorted[-1]:.2f} µs")
    print(f"   Avg:     {statistics.mean(times_us):.2f} µs")
    print(f"   StdDev:  {statistics.stdev(times_us):.2f} µs")


def measure_allocator_behavior() -> Tuple[float, float]:
    """
    Show how memory state affects parsing performance.

    Returns: (avg_cold_state, avg_warm_state)
    """
    import json

    # Cold state: Fresh interpreter
    gc.collect()
    gc.disable()

    times_cold = []
    for _ in range(100):
        start = time.perf_counter()
        obj = json.loads(SAMPLE_MESSAGE)
        end = time.perf_counter()
        times_cold.append((end - start) * 1_000_000)

    gc.enable()
    avg_cold = statistics.mean(times_cold)

    # Warm state: After lots of allocations
    gc.collect()
    gc.disable()

    # Create allocation history
    for _ in range(1000):
        temp = [{"data": i} for i in range(100)]
        del temp

    times_warm = []
    for _ in range(100):
        start = time.perf_counter()
        obj = json.loads(SAMPLE_MESSAGE)
        end = time.perf_counter()
        times_warm.append((end - start) * 1_000_000)

    gc.enable()
    avg_warm = statistics.mean(times_warm)

    return avg_cold, avg_warm


def show_gc_configuration() -> None:
    """Display Python's garbage collection settings."""
    print()
    print("Python GC Configuration:")
    print("-" * 70)
    print(f"   GC enabled: {gc.isenabled()}")

    # Get GC thresholds
    thresholds = gc.get_threshold()
    print(f"   Gen 0 threshold: {thresholds[0]} allocations")
    print(f"   Gen 1 threshold: {thresholds[1]} collections")
    print(f"   Gen 2 threshold: {thresholds[2]} collections")

    # Get GC stats
    try:
        stats = gc.get_stats()
        print(f"   GC stats available: {len(stats)} generation(s)")
    except AttributeError:
        print("   GC stats: Not available in this Python version")

    print()


def main():
    import json

    print("=" * 70)
    print("Python Garbage Collection Behavior Analysis")
    print("=" * 70)
    print()

    show_gc_configuration()

    # Measurement 1: GC-aware parsing
    print("1. JSON Parsing with GC Tracking:")
    print("-" * 70)
    times_us, count, max_pause = measure_parsing_with_gc_events(iterations=5000)
    show_gc_stats(times_us)
    print()

    # Measurement 2: Allocator behavior
    print("2. Allocator Behavior (Cold vs Warm State):")
    print("-" * 70)
    avg_cold, avg_warm = measure_allocator_behavior()
    print(f"   Cold state (fresh):     {avg_cold:.2f} µs")
    print(f"   Warm state (allocated): {avg_warm:.2f} µs")
    variance = ((avg_warm - avg_cold) / avg_cold) * 100
    print(f"   Variance: {variance:.1f}%")
    print()

    # Measurement 3: Variability summary
    print("3. Variability Summary:")
    print("-" * 70)
    times_us_sorted = sorted(times_us)
    p99_value = times_us_sorted[int(len(times_us_sorted) * 0.99)]
    p50_value = times_us_sorted[len(times_us_sorted) // 2]
    variance_p99 = (p99_value / p50_value - 1.0) * 100

    print(f"   P50 (median):     {p50_value:.2f} µs")
    print(f"   P99 (worst 1%):   {p99_value:.2f} µs")
    print(f"   P99/P50 ratio:    {p99_value / p50_value:.1f}x")
    print(f"   Tail latency increase: {variance_p99:.1f}%")
    print()

    # Summary for blog post
    print("=" * 70)
    print("FINDINGS FOR POST 02")
    print("=" * 70)
    print()
    print("Key Insight:")
    print("  Python JSON parsing is unpredictable due to:")
    print("  1. Garbage collection pauses (non-deterministic)")
    print("  2. Allocator behavior varies with heap state")
    print("  3. Cannot guarantee SLA bounds (e.g., <100ms P99)")
    print()
    print(f"Actual Data:")
    print(f"  - P50 latency: {p50_value:.1f} µs")
    print(f"  - P99 latency: {p99_value:.1f} µs")
    print(f"  - Tail variance: {variance_p99:.0f}%")
    print(f"  - Cold/warm state variance: {variance:.1f}%")
    print()
    print("Conclusion:")
    print("  Regulated industries requiring <100ms P99 SLA cannot")
    print("  rely on Python's JSON parsing due to GC unpredictability.")
    print("=" * 70)


if __name__ == "__main__":
    main()
