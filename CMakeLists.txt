cmake_minimum_required(VERSION 3.25)

project(remotescan)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 20)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

add_subdirectory(external)

set(REMOTESCAN_CPP_SOURCE
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

add_executable(remotescan ${REMOTESCAN_CPP_SOURCE})

target_include_directories(remotescan PRIVATE include
	external
	src
	external/efsw/include
)

target_link_libraries(remotescan PRIVATE
	efsw-static
	pugixml-static
)

add_compile_definitions("$<$<CONFIG:Debug>:DEBUG>")
