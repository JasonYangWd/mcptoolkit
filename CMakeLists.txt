cmake_minimum_required(VERSION 3.16)
project(mcptoolkit CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Security: Compiler hardening & sanitizers
# ---------------------------------------------------------------------------
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" ON)
option(ENABLE_HARDENING "Enable compiler hardening flags" ON)

if(ENABLE_HARDENING)
    if(MSVC)
        # MSVC hardening flags
        add_compile_options(/W4 /WX)  # All warnings as errors
    else()
        # GCC/Clang hardening flags
        add_compile_options(
            -Wall -Wextra -Wpedantic
            -Wconversion -Wsign-conversion
            -Wformat=2 -Wformat-security
            -Wstrict-overflow=5
            -Wtrampolines
            -Wl,-z,now -Wl,-z,relro  # Full RELRO
        )
        add_link_options(-Wl,-z,now -Wl,-z,relro)
    endif()
endif()

if(ENABLE_SANITIZERS AND NOT MSVC)
    # ASan + UBSan for runtime bug detection
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------------
# Option: build mcptoolkit as a shared library (DLL on Windows, .so on Linux)
# Default is static. Enable with: cmake -DBUILD_SHARED_LIBS=ON
# ---------------------------------------------------------------------------
option(BUILD_SHARED_LIBS "Build mcptoolkit as a shared library" OFF)

# ---------------------------------------------------------------------------
# mcptoolkit library
# ---------------------------------------------------------------------------
set(TOOLKIT_SOURCES
    mcptoolkit/src/json/json_msg.cpp
    mcptoolkit/src/json/json_parser.cpp
    mcptoolkit/src/json/json_builder.cpp
    mcptoolkit/src/mcp_adapter.cpp
    mcptoolkit/src/input_validation.cpp
    mcptoolkit/src/rate_limiter.cpp
    mcptoolkit/src/timeout_manager.cpp
    mcptoolkit/src/request_queue.cpp
    mcptoolkit/src/resource_monitor.cpp
    mcptoolkit/src/path_validator.cpp
    mcptoolkit/src/authentication_handler.cpp
    mcptoolkit/dllmain.cpp      # guarded with #ifdef _WIN32 inside
)

add_library(mcptoolkit ${TOOLKIT_SOURCES})

target_include_directories(mcptoolkit
    PUBLIC
        mcptoolkit/include
        mcptoolkit/include/json
    PRIVATE
        mcptoolkit           # for pch.h
)

# When building as a shared library, expose the dllexport/dllimport macro
if(BUILD_SHARED_LIBS)
    target_compile_definitions(mcptoolkit
        PUBLIC  MCPTOOLKIT_SHARED
        PRIVATE MCPTOOLKIT_EXPORTS
    )
endif()

# Compiler warnings and platform-specific options
if(MSVC)
    target_compile_options(mcptoolkit PRIVATE /W3 /wd4251)
    target_compile_definitions(mcptoolkit PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mcptoolkit PRIVATE -Wall -Wextra)
    if(BUILD_SHARED_LIBS)
        # Hide symbols by default; only those tagged MCPTOOLKIT are exported
        target_compile_options(mcptoolkit PRIVATE -fvisibility=hidden)
    endif()
endif()

# ---------------------------------------------------------------------------
# TestMcp executable
# ---------------------------------------------------------------------------
add_executable(testmcp TestMcp/TestMcp.cpp)

target_link_libraries(testmcp PRIVATE mcptoolkit)

# TestMcp/TestMcp.cpp uses #include "json_parser.h" (no subdirectory prefix)
target_include_directories(testmcp PRIVATE
    TestMcp
)

if(MSVC)
    target_compile_options(testmcp PRIVATE /W3)
    target_compile_definitions(testmcp PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(testmcp PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Google Test for rate limiting & DOS defense tests
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.13.0.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(test_rate_limiting mcptoolkit/test/test_rate_limiting.cpp)
target_link_libraries(test_rate_limiting PRIVATE mcptoolkit gtest gtest_main)
target_include_directories(test_rate_limiting PRIVATE mcptoolkit/include)

add_executable(test_path_validator mcptoolkit/test/test_path_validator.cpp)
target_link_libraries(test_path_validator PRIVATE mcptoolkit gtest gtest_main)
target_include_directories(test_path_validator PRIVATE mcptoolkit/include)

add_executable(test_input_size_limits mcptoolkit/test/test_input_size_limits.cpp)
target_link_libraries(test_input_size_limits PRIVATE mcptoolkit gtest gtest_main)
target_include_directories(test_input_size_limits PRIVATE mcptoolkit/include)

add_executable(test_authentication mcptoolkit/test/test_authentication.cpp)
target_link_libraries(test_authentication PRIVATE mcptoolkit gtest gtest_main)
target_include_directories(test_authentication PRIVATE mcptoolkit/include)

enable_testing()
add_test(NAME RateLimitingTests COMMAND test_rate_limiting)
add_test(NAME PathValidatorTests COMMAND test_path_validator)
add_test(NAME InputSizeLimitsTests COMMAND test_input_size_limits)
add_test(NAME AuthenticationTests COMMAND test_authentication)

# ---------------------------------------------------------------------------
# libfuzzer for input validation testing (Clang only)
# ---------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT MSVC)
    add_executable(fuzz_json_parser mcptoolkit/test/fuzz_json_parser.cpp)
    target_link_libraries(fuzz_json_parser PRIVATE mcptoolkit)
    target_include_directories(fuzz_json_parser PRIVATE mcptoolkit/include)
    target_compile_options(fuzz_json_parser PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_options(fuzz_json_parser PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
