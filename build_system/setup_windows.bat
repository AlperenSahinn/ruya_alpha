@echo off

pushd ..
build_system\premake\premake5.exe --file=build.lua vs2026
popd
pause