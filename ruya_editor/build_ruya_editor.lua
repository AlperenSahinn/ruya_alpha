project "ruya_editor"
    kind "ConsoleApp"

    files 
    { 
        "src/**.h", 
        "src/**.cpp",
        "../assets/ruya_files/scripts/ruya.rc"
    }

    includedirs
    {
        "src",
        
        "../ruya/src",
        "../ruya_app/src",

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
        "ruya",
        "ruya_app",

        -- Third party
        "mimalloc.dll.lib",
        "SDL3",
        "Jolt",
        "ktx"
    }

    filter "system:windows"
        postbuildcommands
        {
            "{COPY} %{wks.location}%{ThirdPartyDir}/lib/mimalloc/%{cfg.buildcfg}/mimalloc.dll %{cfg.buildtarget.directory}",
            "{COPY} %{wks.location}%{ThirdPartyDir}/lib/mimalloc/%{cfg.buildcfg}/mimalloc-redirect.dll %{cfg.buildtarget.directory}",
            "{COPY} %{wks.location}%{ThirdPartyDir}/lib/sdl3/%{cfg.buildcfg}/SDL3.dll %{cfg.buildtarget.directory}",
            "{COPY} %{wks.location}%{ThirdPartyDir}/lib/ktx/%{cfg.buildcfg}/ktx.dll %{cfg.buildtarget.directory}"
        }