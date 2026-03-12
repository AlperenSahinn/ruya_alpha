project "ruya_editor"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    targetdir "bin/%{cfg.buildcfg}"
    staticruntime "off"
    characterset "Unicode"

    files 
    { 
        "src/**.h", 
        "src/**.cpp",

        -- Third party
        "../third_party/include/volk/volk.c",
        "../third_party/include/vk_bootstrap/VkBootstrap.cpp",
        "../third_party/include/ImGui/imgui.cpp",
        "../third_party/include/ImGui/imgui_draw.cpp",
        "../third_party/include/ImGui/imgui_tables.cpp",
        "../third_party/include/ImGui/imgui_widgets.cpp",
        "../third_party/include/ImGui/imgui_impl_sdl3.cpp",
        "../third_party/include/ImGui/imgui_impl_vulkan.cpp"
    }

    includedirs
    {
        "src",

	    "../ruya/src",
        "../ruya_app/src",

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
        "ruya_app",

        -- Third party
        "mimalloc.dll.lib",
        "SDL3"
    }

    targetdir ("../bin/" .. OutputDir .. "/%{prj.name}")
     objdir ("../intermediates/" .. OutputDir .. "/%{prj.name}")

    filter "system:windows"
        systemversion "latest"
        defines { "WINDOWS" }
        files { "../assets/ruya_files/scripts/ruya.rc" }

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
        
    filter "system:windows"
        systemversion "latest"
        defines { "WINDOWS" }

        postbuildcommands
        {
            "xcopy /Y /I \"$(SolutionDir)third_party\\lib\\mimalloc\\%{cfg.buildcfg}\\mimalloc.dll\" \"$(TargetDir)\"",
            "xcopy /Y /I \"$(SolutionDir)third_party\\lib\\mimalloc\\%{cfg.buildcfg}\\mimalloc-redirect.dll\" \"$(TargetDir)\"",
            "xcopy /Y /I \"$(SolutionDir)third_party\\lib\\sdl3\\%{cfg.buildcfg}\\SDL3.dll\" \"$(TargetDir)\""
        }