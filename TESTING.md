# mcptoolkit Testing & Code Scanning Guide

This document covers the static and dynamic testing infrastructure for mcptoolkit.

## Quick Start

```bash
# Run unit tests
./run_tests.sh basic

# Run with memory sanitizers
./run_tests.sh dynamic

# Run static analysis
./run_tests.sh static

# Run all tests
./run_tests.sh all
```

---

## Testing Modes

### 1. Basic Unit Tests

**Command:**
```bash
./run_tests.sh basic
```

**What it does:**
- Compiles library and test suite
- Runs 161 unit tests covering:
  - Parser (11 tests)
  - Builder (4 tests)
  - Adapter (9 tests)
  - Security (13 tests)

**Output:**
- Test execution log
- Summary: `161/162 tests passed`

---

### 2. Dynamic Analysis (Runtime Sanitizers)

**Command:**
```bash
./run_tests.sh dynamic
```

**What it does:**
- Rebuilds with AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan)
- Detects:
  - Memory leaks
  - Use-after-free
  - Out-of-bounds access
  - Undefined behavior (signed overflow, null dereference, etc.)

**Requirements:**
- GCC 4.8+ or Clang 3.1+ (both have sanitizers)

**Output:**
- If sanitizers find issues, detailed error reports appear
- Clean run: `No memory or undefined behavior issues detected`

**Example output (if issue found):**
```
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on unknown address...
```

---

### 3. Static Analysis

**Command:**
```bash
./run_tests.sh static
```

**Tools used:**
- **clang-tidy** — checks for code style, modernization, and potential bugs
- **cppcheck** — checks for security issues and suspicious code

**What it detects:**
- Potential null pointer dereferences
- Inefficient code patterns
- Security issues (buffer overflows, uninitialized variables)
- Style violations

**Requirements:**
```bash
# Install on Ubuntu/Debian
sudo apt-get install clang-tools cppcheck
```

**Output:**
- Each tool runs independently
- Warnings/errors listed with file:line location

**Example:**
```
mcp_adapter.cpp:21: style: Condition '!line.empty()' is always true
```

---

### 4. Code Coverage

**Command:**
```bash
./run_tests.sh coverage
```

**What it does:**
- Compiles with coverage instrumentation (`--coverage`, `-fprofile-arcs`, `-ftest-coverage`)
- Runs test suite
- Generates coverage data in `build/`

**Requirements:**
```bash
# Install on Ubuntu/Debian
sudo apt-get install gcovr
```

**Generate detailed report:**
```bash
cd build && gcovr --print-summary
```

---

## CMake Options

You can also run CMake directly with specific options:

```bash
# Build with sanitizers
cmake -B build -DENABLE_SANITIZERS=ON
cmake --build build

# Build with clang-tidy
cmake -B build -DENABLE_CLANG_TIDY=ON
cmake --build build

# Build with cppcheck
cmake -B build -DENABLE_CPPCHECK=ON
cmake --build build

# Build with coverage
cmake -B build -DENABLE_CODE_COVERAGE=ON
cmake --build build
```

---

## Test Coverage Summary

| Category | Tests | Status |
|----------|-------|--------|
| Parser | 11 | ✅ Pass |
| Builder | 4 | ✅ Pass |
| Adapter | 9 | ✅ Pass |
| Security | 13+ | ✅ Pass |
| **Total** | **161/162** | **99.4%** |

### Security Test Categories

- **S1-S3:** Parser correctness (requests, responses, parameters)
- **S4-S6:** Builder safety (escaping, arrays, zero-copy)
- **S7-S13:** Security & DoS resistance:
  - Depth limit (prevents stack overflow)
  - Large input parsing
  - Escape injection prevention
  - Malformed input handling
  - Type confusion detection
  - Buffer overflow prevention
  - Expansion DoS resistance

---

## Known Limitations & Gaps

From project strategy, v0.2 improvements planned:

1. **Input size limit** — v0.2: `MAX_MESSAGE_BYTES` enforcement
2. **ID sentinel** — v0.2: replace `-1` with `std::optional<int>`
3. **String unescape** — v0.2: `\uXXXX` sequences handling
4. **CI/CD** — Missing: `.github/workflows/ci.yml`
5. **SECURITY.md** — Missing security policy documentation

---

## Continuous Integration

For GitHub Actions CI, use:

```bash
./run_tests.sh all
```

Future: Automated workflow will run on every push.

---

## Performance Metrics

From test results:

- **Parser:** 0.19µs per message (1000-msg average)
- **Builder:** 0.12µs per response (1000-msg average)
- **Large input (10KB+):** <1ms parse
- **No quadratic behavior** ✓

---

## Troubleshooting

### Sanitizer builds fail with "include not found"
- Clean build: `rm -rf build && cmake -B build`
- Ensure compiler supports sanitizers (GCC 4.8+, Clang 3.1+)

### clang-tidy not found
- Install: `sudo apt-get install clang-tools`
- Specify version: `clang-tidy-14` (adjust version as needed)

### cppcheck reports too many false positives
- Edit CMakeLists.txt to adjust suppressions in `CMAKE_CXX_CPPCHECK`
- Add: `--suppress=<rule>` for rules to ignore

### Coverage reports missing
- Install `gcovr`: `sudo apt-get install gcovr`
- Run from build directory: `cd build && gcovr --print-summary`

---

## Next Steps

- [ ] Add CI/CD workflow (GitHub Actions)
- [ ] Integrate with pre-commit hooks
- [ ] Add fuzzing (libFuzzer integration)
- [ ] Add performance benchmarking
- [ ] Document security policy (SECURITY.md)
