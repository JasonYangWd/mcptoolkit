cmake_minimum_required(VERSION 3.16)
project(mcptoolkit CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(BUILD_SHARED_LIBS "Build mcptoolkit as a shared library" OFF)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)
option(ENABLE_CPPCHECK "Enable cppcheck static analysis" OFF)
option(ENABLE_CODE_COVERAGE "Enable code coverage analysis" OFF)

# ---------------------------------------------------------------------------
# Sanitizers (dynamic analysis)
# ---------------------------------------------------------------------------
if(ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
    message(STATUS "Sanitizers enabled: AddressSanitizer + UndefinedBehaviorSanitizer")
endif()

# ---------------------------------------------------------------------------
# Code Coverage
# ---------------------------------------------------------------------------
if(ENABLE_CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
    add_link_options(--coverage)
    message(STATUS "Code coverage enabled")
endif()

# ---------------------------------------------------------------------------
# Clang-Tidy (static analysis)
# ---------------------------------------------------------------------------
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_PROGRAM clang-tidy)
    if(CLANG_TIDY_PROGRAM)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_PROGRAM}")
        message(STATUS "clang-tidy enabled for static analysis")
    else()
        message(WARNING "clang-tidy requested but not found")
    endif()
endif()

# ---------------------------------------------------------------------------
# Cppcheck (static analysis)
# ---------------------------------------------------------------------------
if(ENABLE_CPPCHECK)
    find_program(CPPCHECK_PROGRAM cppcheck)
    if(CPPCHECK_PROGRAM)
        set(CMAKE_CXX_CPPCHECK "${CPPCHECK_PROGRAM}" "--enable=all" "--suppress=missingIncludeSystem")
        message(STATUS "cppcheck enabled for static analysis")
    else()
        message(WARNING "cppcheck requested but not found")
    endif()
endif()

# ---------------------------------------------------------------------------
# mcptoolkit library
# ---------------------------------------------------------------------------
set(TOOLKIT_SOURCES
    mcptoolkit/src/json/json_msg.cpp
    mcptoolkit/src/json/json_parser.cpp
    mcptoolkit/src/json/json_builder.cpp
    mcptoolkit/src/mcp_adapter.cpp
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
# Testing
# ---------------------------------------------------------------------------
enable_testing()
add_test(NAME unit_tests COMMAND testmcp)

# Add custom test targets
add_custom_target(test-run
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS testmcp
    COMMENT "Running unit tests..."
)

add_custom_target(test-dynamic
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config Debug
    DEPENDS testmcp
    COMMENT "Running dynamic tests with sanitizers (rebuild with -DENABLE_SANITIZERS=ON)..."
)

add_custom_target(test-coverage
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config Debug
    COMMENT "Running coverage analysis (rebuild with -DENABLE_CODE_COVERAGE=ON)..."
    DEPENDS testmcp
)
