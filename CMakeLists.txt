cmake_minimum_required(VERSION 3.25...3.30)

# 1. SILENCE DEPRECATION WARNINGS (Must be before project())
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

FetchContent_Declare(warp     GIT_REPOSITORY ${WARP_REPO}    GIT_TAG ${WARP_VERSION})
FetchContent_Declare(glaze    GIT_REPOSITORY ${GLAZE_REPO}   GIT_TAG ${GLAZE_VERSION})
FetchContent_Declare(httplib  GIT_REPOSITORY ${HTTPLIB_REPO} GIT_TAG ${HTTPLIB_VERSION})

set(HTTPLIB_COMPILE OFF CACHE BOOL "" FORCE)
set(SPDLOG_MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}" CACHE STRING "" FORCE)

FetchContent_MakeAvailable(warp glaze httplib)

if(NOT TARGET watcher_lib)
    add_library(watcher_lib INTERFACE)
    set_target_properties(watcher_lib PROPERTIES 
       VERSION 0.14.3
       GITHUB_URL "https://github.com/e-dant/watcher"
    )
    target_include_directories(watcher_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/external)
    target_compile_features(watcher_lib INTERFACE cxx_std_20)

    # On Linux, the library needs threading support
    if(UNIX)
        find_package(Threads REQUIRED)
        target_link_libraries(watcher_lib INTERFACE Threads::Threads)
    endif()
endif()

set(REMOTESCAN_SOURCES
	src/config-reader/config-reader.cpp
	src/main.cpp
	src/remote-scan.cpp
)

# 6. CREATE THE TARGET (Only call this ONCE)
add_executable(remote-scan ${REMOTESCAN_SOURCES})

# 7. APPLY TARGET PROPERTIES (Must be AFTER add_executable)
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
    # Apply MSVC Link-Time Code Generation (The Windows version of LTO)
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
    <httplib.h>
    <glaze/glaze.hpp>
    <watcher/watcher.hpp>
)

# 10. INCLUDES & LINKING
target_include_directories(remote-scan PRIVATE include src)

target_link_libraries(remote-scan PRIVATE
    warp::warp
    glaze::glaze
    httplib::httplib
    watcher_lib
)
