workspace "Chip8"
    location(_ACTION)
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
    location(_ACTION)
    kind "StaticLib"
    language "C"

    objdir "bin-int/%{cfg.buildcfg}/%{prj.name}"
    targetdir "bin/%{cfg.buildcfg}/%{prj.name}"
    debugdir "bin/%{cfg.buildcfg}/%{prj.name}"

    defines { "GLFW_STATIC" }

    includedirs {
        "vendor/glfw/src"
    }
    files {
        "vendor/glfw/src/egl_context.c",
        "vendor/glfw/src/context.c",
        "vendor/glfw/src/init.c",
        "vendor/glfw/src/input.c",
        "vendor/glfw/src/monitor.c",
        "vendor/glfw/src/osmesa_context.c",
        "vendor/glfw/src/vulkan.c",
        "vendor/glfw/src/window.c",
        "vendor/glfw/src/xkb_unicode.c",
    }

    filter "system:windows"
        defines { "_GLFW_WIN32" }
        files {
            "vendor/glfw/src/wgl_context.c",
            "vendor/glfw/src/win32_init.c",
            "vendor/glfw/src/win32_joystick.c",
            "vendor/glfw/src/win32_monitor.c",
            "vendor/glfw/src/win32_thread.c",
            "vendor/glfw/src/win32_time.c",
            "vendor/glfw/src/win32_window.c",
        }

    filter "system:linux"
        defines { "_GLFW_X11" }
        files {
            "vendor/glfw/src/glx_context.c",
            "vendor/glfw/src/x11_init.c",
            "vendor/glfw/src/x11_monitor.c",
            "vendor/glfw/src/x11_window.c",
            "vendor/glfw/src/posix_thread.c",
            "vendor/glfw/src/posix_time.c",
            "vendor/glfw/src/linux_joystick.c",
        }

project "Glad"
    location(_ACTION)
    kind "StaticLib"
    language "C"

    objdir "bin-int/%{cfg.buildcfg}/%{prj.name}"
    targetdir "bin/%{cfg.buildcfg}/%{prj.name}"

    includedirs { "vendor/glad/include" }

    files { "vendor/glad/src/**.c" }


project "ImGui"
    location(_ACTION)
    kind "StaticLib"
    language "C++"

    objdir "bin-int/%{cfg.buildcfg}/%{prj.name}"
    targetdir "bin/%{cfg.buildcfg}/%{prj.name}"
    debugdir "bin/%{cfg.buildcfg}/%{prj.name}"

    includedirs {
        "vendor/imgui",
        "vendor/imgui/backends",
        "vendor/glfw/include",
        "vendor/glad/include"
    }

    defines { "IMGUI_IMPL_OPENGL_LOADER_GLAD" }

    files {
        "vendor/imgui/*.cpp",
        "vendor/imgui/backends/imgui_impl_glfw.cpp",
        "vendor/imgui/backends/imgui_impl_opengl3.cpp",
    }

project "Chip8"
    location(_ACTION)
    language "C++"
    cppdialect "C++17"
    characterset "MBCS"

    objdir "bin-int/%{cfg.buildcfg}/%{prj.name}"
    targetdir "bin/%{cfg.buildcfg}/%{prj.name}"
    debugdir "bin/%{cfg.buildcfg}/%{prj.name}"

    includedirs {
        "vendor/glfw/include",
        "vendor/glad/include",
        "vendor/spdlog/include",
        "vendor/imgui",
        "vendor/imgui/backends",
        "vendor/glm",
        "vendor/miniaudio"
    }


    files { "src/**.cpp", "src/**.h" }

    postbuildcommands {
        "{COPY} ../roms/ ../bin/%{cfg.buildcfg}/%{prj.name}/roms"
    }

    filter "system:windows"
        links { "Glad", "opengl32", "GLFW", "ImGui" }

    filter "system:linux"
        links { "Glad", "GL", "GLFW", "ImGui", "X11", "pthread", "dl" }

    filter "configurations:Debug"
        kind "ConsoleApp"

    filter "configurations:Release"
        kind "WindowedApp"
