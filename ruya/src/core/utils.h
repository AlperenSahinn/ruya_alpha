#pragma once
#include <memory>
#include <windows.h>
#include <filesystem>
#include <string>

namespace ruya::utils
{
	inline std::filesystem::path GetAssetsDir()
	{
		char buffer[MAX_PATH];
		GetModuleFileNameA(NULL, buffer, MAX_PATH);
		std::filesystem::path exePath(buffer);
		std::filesystem::path exeDir = exePath.parent_path();
		std::filesystem::path assetsDir = exeDir / "../../../../";
		assetsDir = std::filesystem::canonical(assetsDir) / "assets/";
		return assetsDir;
	}

	inline uint32_t AlignUp(uint32_t value, uint32_t alignment)
	{
		return (value + alignment - 1) & ~(alignment - 1);
	}
}