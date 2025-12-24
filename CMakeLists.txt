cmake_minimum_required(VERSION 3.27 FATAL_ERROR)

project(remotescan)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 20)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

add_subdirectory(external)

set(REMOTESCAN_CPP_SOURCE
	src/api/api-base.cpp
	src/api/api-base.h
	src/api/api-emby.cpp
	src/api/api-emby.h
	src/api/api-manager.cpp
	src/api/api-manager.h
	src/api/api-plex.cpp
	src/api/api-plex.h
	src/config-reader/config-reader.cpp
	src/config-reader/config-reader.h
	src/config-reader/config-reader-types.h
	src/logger/ansii-remove-formatter.cpp
	src/logger/ansii-remove-formatter.h
	src/logger/log-apprise.cpp
	src/logger/log-apprise.h
	src/logger/logger.cpp
	src/logger/logger.h
	src/logger/logger-types.h
	src/logger/log-utils.h
	src/main.cpp
	src/remote-scan.cpp
	src/remote-scan.h
	src/types.h
	src/update-listener.cpp
	src/update-listener.h
	src/version.h
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
