workspace "RuyaEngine"
    architecture "x64"
    configurations { "debug", "release" }
    characterset "Unicode"
    startproject "ruya_editor"

    defines { "VK_NO_PROTOTYPES" }

    filter "system:windows"
      buildoptions { "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus", "/MP", "/utf-8"}


OutputDir = "%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}"

include "ruya/build_ruya.lua"
include "ruya_editor/build_ruya_editor.lua"
include "ruya_app/build_ruya_app.lua"
