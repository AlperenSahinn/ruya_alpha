#pragma once
#include <string>
#include <cstdint>

#include <cereal/archives/json.hpp>

#include <core/ry_id.h>

namespace ruya
{
	enum class RyAssetType
	{
		Folder,
		Scene,
		Prefab,
		Model,
		Image2D,
		Sound
	};

	enum class RyAssetSourceExtension
	{
		ryscene,
		ryprefab,
		gltf,
		png
	};

	struct RyAsset
	{
		std::string name;
		RyID id;
		RyAssetType type;
		RyAssetSourceExtension sourceExtension;
		std::string path; //relative to assets dir.
	};

	template<typename Archive>
	void serialize(Archive& archive, RyAsset& ryAsset)
	{
		archive(CEREAL_NVP(ryAsset.name), CEREAL_NVP(ryAsset.id), CEREAL_NVP(ryAsset.type), CEREAL_NVP(ryAsset.sourceExtension), CEREAL_NVP(ryAsset.path));
	}
}