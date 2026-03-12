project "ruya_app"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    targetdir "bin/%{cfg.buildcfg}"
    staticruntime "off"
    characterset "Unicode"

    files { "src/**.h", "src/**.cpp" }

    includedirs
    {
        "src",

	    "../ruya/src",

        -- Third party
        "../third_party/include/"
    }

    libdirs
    {
        -- Third party
        "../third_party/lib/mimalloc/%{cfg.buildcfg}",
        "../third_party/lib/sdl3/%{cfg.buildcfg}"
    }

    links
    {
        "ruya",

        -- Third party
        "mimalloc.dll.lib",
        "SDL3"
    }

    targetdir ("../bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("../intermediates/" .. OutputDir .. "/%{prj.name}")

    filter "system:windows"
        systemversion "latest"
        defines { "WINDOWS" }

    filter "configurations:debug"
        defines { "DEBUG" }
        runtime "Debug"
        optimize "Off"
        symbols "On"

    filter "configurations:release"
        defines { "RELEASE" }
        runtime "Release"
        optimize "On"
        symbols "Off"