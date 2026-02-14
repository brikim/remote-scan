cmake_minimum_required(VERSION 3.25...3.30)

# 1. SILENCE DEPRECATION WARNINGS
set(CMAKE_POLICY_DEFAULT_CMP0000 NEW)

project(remote-scan LANGUAGES CXX)

# 2. GLOBAL SETTINGS
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 3. IPO/LTO SUPPORT CHECK
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

# 4. DEPENDENCIES (FetchContent)
include(FetchContent)
include(Dependencies.cmake)

# Find Threads globally (Required for watcher on both Windows and Linux)
find_package(Threads REQUIRED)

# Disable watcher's extra targets to prevent Ninja build collisions
set(BUILD_LIB OFF CACHE BOOL "" FORCE)
set(BUILD_BIN OFF CACHE BOOL "" FORCE)
set(BUILD_PKG_CONFIG OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_Declare(warp    GIT_REPOSITORY ${WARP_REPO}    GIT_TAG ${WARP_VERSION})
FetchContent_Declare(glaze   GIT_REPOSITORY ${GLAZE_REPO}   GIT_TAG ${GLAZE_VERSION})
FetchContent_Declare(watcher GIT_REPOSITORY ${WATCHER_REPO} GIT_TAG ${WATCHER_VERSION})

FetchContent_MakeAvailable(warp glaze watcher)

# 5. SOURCE DEFINITIONS
set(REMOTESCAN_SOURCES
    src/config-reader/config-reader.cpp
    src/file-monitor.cpp
    src/main.cpp
    src/remote-scan.cpp
    src/scan.cpp
)

# 6. CREATE THE TARGET
add_executable(remote-scan ${REMOTESCAN_SOURCES})

# 7. APPLY TARGET PROPERTIES
if(ipo_supported)
    set_property(TARGET remote-scan PROPERTY 
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE
    )
endif()

target_compile_options(remote-scan PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /MP>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# 8. PLATFORM SPECIFIC (UNIX/SSL)
if(UNIX)
    target_link_options(remote-scan PRIVATE 
        "$<$<CONFIG:Release>:-s>"
        -static-libstdc++ 
        -static-libgcc
    )
    find_package(OpenSSL REQUIRED)
    target_link_libraries(remote-scan PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(remote-scan PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
else()
    # Apply MSVC Link-Time Code Generation
    target_link_options(remote-scan PRIVATE $<$<CONFIG:Release>:/LTCG>)
endif()

# 9. PRECOMPILED HEADERS
target_precompile_headers(remote-scan PRIVATE
    <format>
    <string>
    <vector>
    <memory>
    <chrono>
    <iostream>
    <string_view>

    # Third-Party
    <glaze/glaze.hpp>
    <wtr/watcher.hpp>
)

# 10. INCLUDES & LINKING
target_include_directories(remote-scan PRIVATE include 
    src
    "${watcher_SOURCE_DIR}/include"
)

target_link_libraries(remote-scan PRIVATE
    warp::warp
    glaze::glaze
    wtr.hdr_watcher
    Threads::Threads
)