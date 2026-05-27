workspace "RuyaEngine"
    architecture "x64"
    configurations { "Debug", "Release", "Distribution", "Profiler_Tracy" }
    startproject "ruya_editor"

    language "C++"
    cppdialect "C++20"
    staticruntime "off"
    characterset "Unicode"
    exceptionhandling "Off"

    OutputDir = "%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}"
    
    targetdir ("bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("intermediates/" .. OutputDir .. "/%{prj.name}")

    ThirdPartyDir = "third_party"

    defines {
        "GLM_FORCE_DEPTH_ZERO_TO_ONE",
        "GLM_ENABLE_EXPERIMENTAL", 
        "VK_NO_PROTOTYPES",
        "JPH_OBJECT_STREAM", 
        "JPH_SUPPRESS_WARNINGS",
        "JPH_DEBUG_RENDERER",
        "JPH_PROFILE_ENABLED",
        "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
    }

    filter "system:windows"
        systemversion "latest"
        defines { "WINDOWS", "_HAS_EXCEPTIONS=0" }
        buildoptions { "/Zc:preprocessor", "/Zc:__cplusplus", "/MP", "/utf-8" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        runtime "Debug"
        optimize "Off"
        symbols "On"

    filter "configurations:Release"
        defines { "RELEASE", "NDEBUG" }
        runtime "Release"
        optimize "On"
        symbols "On"

    filter "configurations:Profiler_Tracy"
        defines { "RELEASE", "NDEBUG", "TRACY_ENABLE" }
        runtime "Release"
        optimize "On"
        symbols "On"

    filter "configurations:Distribution"
        defines { "DISTRIBUTION", "NDEBUG" }
        runtime "Release"
        optimize "On"
        symbols "Off"

    filter {}

include "ruya/build_ruya.lua"
include "ruya_app/build_ruya_app.lua"
include "ruya_editor/build_ruya_editor.lua"