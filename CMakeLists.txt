cmake_minimum_required(VERSION 3.25)

project(remotescan)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(external)

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/wd4129>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/MP>")

add_compile_definitions("$<$<CONFIG:Debug>:DEBUG>")

add_executable(remotescan)

target_sources(remotescan PRIVATE
	src/api/api-base.cpp
	src/api/api-emby.cpp
	src/api/api-manager.cpp
	src/api/api-plex.cpp
	src/config-reader/config-reader.cpp
	src/logger/ansii-remove-formatter.cpp
	src/logger/log-apprise.cpp
	src/logger/logger.cpp
	src/main.cpp
	src/remote-scan.cpp
	src/update-listener.cpp
)

target_include_directories(remotescan PRIVATE include
	external
	src
	external/efsw/include
)

target_link_libraries(remotescan PRIVATE
	efsw-static
	pugixml-static
)
