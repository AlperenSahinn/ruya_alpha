project "ruya"
     kind "StaticLib"
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
          
          -- Third party
          "../third_party/include/"
     }

     libdirs
     {
          "../third_party/lib/mimalloc/%{cfg.buildcfg}",
          "../third_party/lib/sdl3/%{cfg.buildcfg}"
     }

     links
     {
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