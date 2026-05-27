project "ruya_app"
    kind "StaticLib"

    files 
    { 
        "src/**.h", 
        "src/**.cpp" 
    }

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
        "../third_party/lib/sdl3/%{cfg.buildcfg}",
        "../third_party/lib/jolt/%{cfg.buildcfg}",
        "../third_party/lib/ktx/%{cfg.buildcfg}"
    }

    links
    {
        "ruya",

        -- Third party
        "mimalloc.dll.lib",
        "SDL3",
        "Jolt",
        "ktx"
    }