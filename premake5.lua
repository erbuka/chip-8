workspace "Chip8"
    architecture "x86_64"
    configurations { "Debug", "Release" }
    startproject "Chip8"

    filter "configurations:Debug"
        symbols "On"
        optimize "Off"
        defines { "DEBUG" }
    
    filter "configurations:Release"
        symbols "Off"
        optimize "On"

    filter "system:windows"
        systemversion "latest"

project "GLFW"
    kind "StaticLib"
    language "C++"
    location "vendor/glfw"

    objdir "bin-int/%{cfg.buildcfg}/%{prj.name}"
    targetdir "bin/%{cfg.buildcfg}/%{prj.name}"    

    includedirs { 
        "vendor/glfw/include" 
    }

    defines {
        "_CRT_SECURE_NO_WARNINGS"
    }

    files {
        "vendor/glfw/src/internal.h",
        "vendor/glfw/src/mappings.h",
        
        "vendor/glfw/src/context.c",        
        "vendor/glfw/src/init.c",        
        "vendor/glfw/src/input.c",        
        "vendor/glfw/src/monitor.c",        
        "vendor/glfw/src/vulkan.c",     
        "vendor/glfw/src/window.c",        
    }

    filter "system:windows"

        defines { "_GLFW_WIN32" }

        files {
            "vendor/glfw/src/win32_platform.h",        
            "vendor/glfw/src/win32_joystick.h",        
            "vendor/glfw/src/wgl_context.h",
            "vendor/glfw/src/egl_context.h",
            "vendor/glfw/src/osmesa_context.h",

            "vendor/glfw/src/win32_init.c",
            "vendor/glfw/src/win32_joystick.c",
            "vendor/glfw/src/win32_monitor.c",
            "vendor/glfw/src/win32_time.c",
            "vendor/glfw/src/win32_thread.c",
            "vendor/glfw/src/win32_window.c",
            "vendor/glfw/src/wgl_context.c",
            "vendor/glfw/src/egl_context.c",
            "vendor/glfw/src/osmesa_context.c",
        }

project "Glad"
    kind "StaticLib"
    language "C++"
    location "vendor/glad"

    objdir "bin-int/%{cfg.buildcfg}/%{prj.name}"
    targetdir "bin/%{cfg.buildcfg}/%{prj.name}"

    includedirs { "vendor/glad/include" }

    files { "vendor/glad/src/**.c" }

project "ImGui"
    kind "StaticLib"
    language "C++"
    location "vendor/imgui"

    objdir "bin-int/%{cfg.buildcfg}/%{prj.name}"
    targetdir "bin/%{cfg.buildcfg}/%{prj.name}"

    includedirs { 
        "vendor/imgui",
        "vendor/glad/include",
        "vendor/glfw/include"
    }

    defines { "IMGUI_IMPL_OPENGL_LOADER_GLAD" }

    links { "Glad", "GLFW" }

    files { "vendor/imgui/**.cpp" }

project "Chip8"
    language "C++"
    cppdialect "C++17"
    location "Chip8"
    characterset "MBCS"

    objdir "bin-int/%{cfg.buildcfg}/%{prj.name}"
    targetdir "bin/%{cfg.buildcfg}/%{prj.name}"
    debugdir "bin/%{cfg.buildcfg}/%{prj.name}"

    includedirs {
        "vendor/glfw/include",
        "vendor/glad/include",
        "vendor/spdlog/include",
        "vendor/imgui",
        "vendor/glm",
        "vendor/bass/include"
    }

    links { "Glad", "opengl32", "GLFW", "ImGui", "bass" }

    files { "Chip8/**.cpp", "Chip8/**.h" }

    postbuildcommands {
        "{COPY} roms/ ../bin/%{cfg.buildcfg}/%{prj.name}/roms" 
    }

    filter "configurations:Debug"
        kind "ConsoleApp"
        
    filter "configurations:Release"
        kind "WindowedApp"

    filter "system:windows"
        libdirs { "vendor/bass/lib/windows" }
        postbuildcommands "{COPY} ../vendor/bass/lib/windows/bass.dll ../bin/%{cfg.buildcfg}/%{prj.name}/"
