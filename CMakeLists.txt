cmake_minimum_required(VERSION 3.16)
project(mcptoolkit CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

