workspace "remotescan"
    architecture "x64"
    location "build" -- Generated files will go into a 'build' directory
    configurations { "debug", "release" }
    startproject "remotescan" -- Optional: Sets a default startup project in Visual Studio

    IncludeDir = {}
    IncludeDir["external"] = "external"
    IncludeDir["efsw"] = "external/efsw/include"

    project "remotescan"
        kind "ConsoleApp" -- Or "StaticLib", "SharedLib"
        language "C++"
        cppdialect "C++20" -- Specifies C++20

        -- Specify Clang as the toolset for Visual Studio projects
        toolset "clang"

        files { "src/**.cpp", "src/**.h", "external/pugixml/pugixml.cpp" } -- Includes all .cpp and .h files in the src directory
        includedirs { 
            "include",
            "./",
            "%{IncludeDir.external}",
            "%{IncludeDir.efsw}",
        } -- Adds the 'include' directory to the project's include paths

        filter "configurations:debug"
            defines { "DEBUG" }
            symbols "On"
            libdirs {
                "external/efsw/lib"
            }
            links {
            "efsw-static-debug"
            }

        filter ("configurations:debug", "system:windows")
            defines { "_CRT_SECURE_NO_WARNINGS" }

        filter "configurations:release"
            defines { "NDEBUG" }
            optimize "On"
            libdirs {
                "external/efsw/lib"
            }
            links {
                "efsw-static-release"
            }
