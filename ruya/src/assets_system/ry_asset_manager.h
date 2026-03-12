#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>
#include <fstream>
#include <queue>
#include <memory>

#include <cereal/cereal.hpp>
#include <cereal/types/queue.hpp>
#include <entt/entt.hpp>

#include <core/ry_id.h>
#include <app_framework/scene.h>

#include <cereal/archives/json.hpp>

#include "ry_asset.h"

namespace ruya 
{
	class RyAssetManager 
	{
	public:
		RyAssetManager();
		~RyAssetManager();

		RyAssetManager(const RyAssetManager&) = delete;
		RyAssetManager& operator=(const RyAssetManager&) = delete;

		void Init();
		void ScanAssets();

		bool SerializeAsset(RyID assetID);
		RyAsset DeserializeAsset(const std::string& path);

		RyID CreateAsset(const std::string& name, RyAssetType type, RyAssetSourceExtension sourceExtension, const std::string& path);
		RyAsset* GetAsset(RyID assetID);

        bool SerializeScene(Scene& scene, const std::string& path);
        std::unique_ptr<Scene> DeserializeScene(const std::string& path);

		void SerializeCurrentState();
		void LoadState();

		template<typename Archive>
		friend void serialize(Archive& archive, RyAssetManager& ryAssetManager);

	private:
		std::unordered_map<RyID, std::unique_ptr<RyAsset>> assetRegistry;
		RyID nextAssetID;
		std::queue<RyID> availableAssetIDs;
	};

	template<typename Archive>
	void serialize(Archive& archive, RyAssetManager& ryAssetManager)
	{
		archive(CEREAL_NVP(ryAssetManager.nextAssetID), ryAssetManager.availableAssetIDs);
	}
}
