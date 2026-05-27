#pragma once
#include <string>
#include <cstdint>

#include <nlohmann_json/json.hpp>
#include <magic_enum/magic_enum.hpp>
#include <glm/glm.hpp>

#include <core/uuid.h>
#include <core/nlohmann_glm.h>

namespace ruya
{
    enum class RyAssetType
    {
        RyScene,
        GLTF,
        KTX2,
        RyMaterial
    };

    struct RyAsset
    {
        UUID uuid = UUID::Invalid();
        RyAssetType ryAssetType;
        std::string assetName = ""; // example: "player_texture"
        std::string directory = ""; // example: "textures/characters"
        std::string sourcePath = ""; // source path of orginal file for reimporting
    };

    inline void to_json(nlohmann::json& j, const RyAsset& asset)
    {
        j = nlohmann::json{
            {"uuid", asset.uuid},
            {"ryAssetType", std::string(magic_enum::enum_name(asset.ryAssetType))},
            {"assetName", asset.assetName},
            {"directory", asset.directory},
            {"sourcePath", asset.sourcePath}
        };
    }

    inline void from_json(const nlohmann::json& j, RyAsset& asset)
    {
        j.at("uuid").get_to(asset.uuid);

        std::string typeName;
        j.at("ryAssetType").get_to(typeName);

        if (auto typeValue = magic_enum::enum_cast<RyAssetType>(typeName))
        {
            asset.ryAssetType = *typeValue;
        }

        j.at("assetName").get_to(asset.assetName);
        j.at("directory").get_to(asset.directory);
        j.at("sourcePath").get_to(asset.sourcePath);
    }
}