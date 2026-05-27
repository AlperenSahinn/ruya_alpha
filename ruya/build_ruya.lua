project "ruya"
    kind "StaticLib"

    files 
    { 
        "src/**.h", 
        "src/**.cpp",
        
        -- Third party
        "../%{ThirdPartyDir}/include/volk/volk.c",
        "../%{ThirdPartyDir}/include/vk_bootstrap/VkBootstrap.cpp",
        "../%{ThirdPartyDir}/include/ImGui/imgui*.cpp",
        "../%{ThirdPartyDir}/include/tracy/TracyClient.cpp"
    }

    includedirs
    {
        "src",

        -- Third party
        "../%{ThirdPartyDir}/include/"
    }

    libdirs
    {
        -- Third party
        "../%{ThirdPartyDir}/lib/mimalloc/%{cfg.buildcfg}",
        "../%{ThirdPartyDir}/lib/sdl3/%{cfg.buildcfg}",
        "../%{ThirdPartyDir}/lib/jolt/%{cfg.buildcfg}",
        "../%{ThirdPartyDir}/lib/ktx/%{cfg.buildcfg}"
    }

    links
    {
        -- Third party
        "mimalloc.dll.lib",
        "SDL3",
        "Jolt",
        "ktx"
    }