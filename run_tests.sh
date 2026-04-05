#!/bin/bash
# mcptoolkit Testing and Code Scanning Script
# Provides static and dynamic analysis options

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_header() {
    echo -e "${BLUE}=== $1 ===${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

# Default to basic test if no args
if [ $# -eq 0 ]; then
    set -- "basic"
fi

case "$1" in
    basic)
        print_header "Running Basic Unit Tests"
        rm -rf build
        cmake -B build -DCMAKE_BUILD_TYPE=Debug
        cmake --build build
        ./build/testmcp
        print_success "All unit tests passed"
        ;;

    dynamic)
        print_header "Running Dynamic Analysis (Sanitizers)"
        if ! command -v gcc &> /dev/null && ! command -v clang &> /dev/null; then
            print_error "GCC or Clang required for sanitizers"
            exit 1
        fi
        rm -rf build
        cmake -B build \
            -DCMAKE_BUILD_TYPE=Debug \
            -DENABLE_SANITIZERS=ON
        cmake --build build
        print_success "Build complete with AddressSanitizer and UndefinedBehaviorSanitizer"
        echo "Running tests..."
        ./build/testmcp
        print_success "No memory or undefined behavior issues detected"
        ;;

    static)
        print_header "Running Static Analysis"

        # Check for clang-tidy
        if command -v clang-tidy &> /dev/null; then
            print_success "clang-tidy found"
            rm -rf build
            cmake -B build \
                -DCMAKE_BUILD_TYPE=Debug \
                -DENABLE_CLANG_TIDY=ON
            cmake --build build 2>&1 | grep -E "warning:|error:" || print_success "No clang-tidy issues found"
        else
            print_warning "clang-tidy not found (install: sudo apt-get install clang-tools)"
        fi

        # Check for cppcheck
        if command -v cppcheck &> /dev/null; then
            print_success "cppcheck found"
            cppcheck --enable=all --suppress=missingIncludeSystem \
                mcptoolkit/src mcptoolkit/include TestMcp 2>&1 | head -30 || true
        else
            print_warning "cppcheck not found (install: sudo apt-get install cppcheck)"
        fi
        ;;

    coverage)
        print_header "Running Code Coverage Analysis"
        if ! command -v gcov &> /dev/null; then
            print_error "gcov required for coverage (install: sudo apt-get install gcovr)"
            exit 1
        fi
        rm -rf build
        cmake -B build \
            -DCMAKE_BUILD_TYPE=Debug \
            -DENABLE_CODE_COVERAGE=ON
        cmake --build build
        ./build/testmcp

        # Generate coverage report
        cd build
        gcov CMakeFiles/testmcp.dir/TestMcp/TestMcp.cpp.o 2>/dev/null || true
        print_success "Coverage data generated in ./build"
        echo "To view detailed report, install gcovr: sudo apt-get install gcovr"
        ;;

    all)
        print_header "Running Full Test Suite"
        echo "1. Basic unit tests..."
        $0 basic
        echo ""
        echo "2. Dynamic analysis..."
        $0 dynamic
        echo ""
        echo "3. Static analysis..."
        $0 static
        echo ""
        print_success "All tests completed"
        ;;

    *)
        echo "Usage: $0 [basic|dynamic|static|coverage|all]"
        echo ""
        echo "Options:"
        echo "  basic      Run basic unit tests (default)"
        echo "  dynamic    Run with AddressSanitizer/UndefinedBehaviorSanitizer"
        echo "  static     Run clang-tidy and cppcheck static analysis"
        echo "  coverage   Generate code coverage report"
        echo "  all        Run all tests"
        exit 1
        ;;
esac
