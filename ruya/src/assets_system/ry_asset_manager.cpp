#include "ry_asset_manager.h"
#include <fstream>

#include <cereal/archives/json.hpp>

#include <core/hash_code_generator.h>
#include <engine.h>
#include <core/log.h>
#include <app_framework/id_component.h>
#include <app_framework/relationship_component.h>
#include <app_framework/transform_component.h>
#include <app_framework/render_component.h>
#include <app_framework/model_3d_component.h>

ruya::RyAssetManager::RyAssetManager()
    :nextAssetID(0)
{
}

ruya::RyAssetManager::~RyAssetManager()
{
    SerializeCurrentState();
}

void ruya::RyAssetManager::Init()
{
    LoadState();
}

void ruya::RyAssetManager::ScanAssets()
{
    using namespace std::filesystem;

    if (!exists(ASSETS_DIR))
        return;

    for (auto& entry : recursive_directory_iterator(ASSETS_DIR))
    {
        if (!entry.is_regular_file())
            continue;

        const auto& path = entry.path();

        if (path.extension() == ".ryasset")
        {
            try
            {
                std::string relativePath = std::filesystem::relative(path, ASSETS_DIR).string();

                RyAsset asset = DeserializeAsset(relativePath);

                assetRegistry[asset.id] = std::make_unique<RyAsset>(asset);

                std::filesystem::path rel = std::filesystem::relative(path, ASSETS_DIR);
                std::string dirOnly = rel.parent_path().string();

                assetRegistry[asset.id]->path = dirOnly + "/";

                SerializeAsset(asset.id);
            }
            catch (const std::exception& ex)
            {
                std::string errorStr = "[RyAssetManager] Failed to load asset: " + path.string() + "\nReason: " + ex.what();
                RUYA_LOG_ERROR(errorStr.c_str());
            }
        }

        if (path.extension() == ".ryscene")
        {
            std::string relativePath = std::filesystem::relative(path, ASSETS_DIR).string();
            std::unique_ptr<Scene> scene = DeserializeScene(relativePath);
            engine->GetApp()->RegisterScene(std::move(scene));
        }
    }
}

bool ruya::RyAssetManager::SerializeAsset(RyID assetID)
{
    if (!assetRegistry.contains(assetID) && !assetID.IsValid())
    {
        RUYA_LOG_ERROR("[RyAssetManager] Serialization failed: asset id not exist");
        return false;
    }

    RyAsset asset = *assetRegistry[assetID].get();

    std::string normalizedPath = asset.path;
    std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
    asset.path = normalizedPath;

    std::filesystem::path fullPath = std::filesystem::path(ASSETS_DIR) / asset.path / (asset.name + ".ryasset");

    std::ofstream out(fullPath);
    if (!out)
        return false;

    try
    {
        cereal::JSONOutputArchive archive(out);
        archive(CEREAL_NVP(asset));
    }
    catch (const std::exception& e)
    {
        RUYA_LOG_ERROR("[RyAssetManager] Serialization failed: %s", e.what());
        return false;
    }

    return true;
}

ruya::RyAsset ruya::RyAssetManager::DeserializeAsset(const std::string& path)
{
    std::filesystem::path fullPath = ASSETS_DIR + path;
    std::ifstream in(fullPath);

    if (!in)
    {
        std::string errorStr = "[RyAssetManager] Failed to load asset : " + fullPath.string();
        RUYA_LOG_ERROR(errorStr.c_str());
        return RyAsset{};
    }

    RyAsset asset;

    try
    {
        cereal::JSONInputArchive archive(in);
        archive(asset);
    }
    catch (const std::exception& e)
    {
        RUYA_LOG_ERROR("[RyAssetManager] Deserialization failed: %s", e.what());
        return RyAsset{};
    }

    return asset;
}

ruya::RyID ruya::RyAssetManager::CreateAsset(const std::string& name, RyAssetType type, RyAssetSourceExtension sourceExtension, const std::string& path)
{
    RyID id;

    if (availableAssetIDs.empty())
    {
        id = nextAssetID;
        nextAssetID = RyID(id.GetRawID() + 1);
    }
    else
    {
        id = availableAssetIDs.front();
        availableAssetIDs.pop();
    }

    if (assetRegistry.contains(id))
        return id;

    std::unique_ptr<RyAsset> asset = std::make_unique<RyAsset>();
    asset->id = id;
    asset->name = name;
    asset->type = type;
    asset->sourceExtension = sourceExtension;
    asset->path = path + "/";

    assetRegistry.insert({ id, std::move(asset)});
}

ruya::RyAsset* ruya::RyAssetManager::GetAsset(RyID assetID)
{
    return assetRegistry[assetID].get();
}

bool ruya::RyAssetManager::SerializeScene(Scene& scene, const std::string& path) {
    entt::basic_registry<EntityID>& reg = scene.GetENTTRegistry();

    std::filesystem::path fullPath = std::filesystem::path(ASSETS_DIR) / path / (scene.GetName() + ".ryscene");
    std::filesystem::path directory = fullPath.parent_path();

    if (!std::filesystem::exists(directory)) {
        std::filesystem::create_directories(directory);
    }

    std::ofstream out(fullPath);
    if (!out) {
        std::cerr << "Failed to open file for writing: " << fullPath << std::endl;
        return false;
    }

    try {
        cereal::JSONOutputArchive output{ out };

        entt::basic_snapshot snapshot{ reg };
        snapshot.get<entt::entity>(output);
        snapshot.get<IdComponent>(output);
        snapshot.get<RelationshipComponent>(output);
        snapshot.get<TransformComponent>(output);
        snapshot.get<RenderComponent>(output);
        snapshot.get<Model3DComponent>(output);

    }
    catch (const std::exception& e) {
        std::cerr << "Serialization error: " << e.what() << std::endl;
        return false;
    }

    return true;
}

std::unique_ptr<ruya::Scene> ruya::RyAssetManager::DeserializeScene(const std::string& path) {
    std::filesystem::path fullPath = std::filesystem::path(ASSETS_DIR) / path;

    if (!std::filesystem::exists(fullPath)) {
        std::cerr << "Scene file does not exist: " << fullPath << std::endl;
        return nullptr;
    }

    std::ifstream in(fullPath);
    if (!in) {
        std::cerr << "Failed to open file for reading: " << fullPath << std::endl;
        return nullptr;
    }

    std::string sceneName = fullPath.stem().string();
    std::unique_ptr<Scene> scene = std::make_unique<Scene>(sceneName);
    entt::basic_registry<EntityID>& reg = scene->GetENTTRegistry();

    try {
        cereal::JSONInputArchive input{ in };

        entt::basic_snapshot_loader loader{ reg };
        loader.get<entt::entity>(input);
        loader.get<IdComponent>(input);
        loader.get<RelationshipComponent>(input);
        loader.get<TransformComponent>(input);
        loader.get<RenderComponent>(input);
        loader.get<Model3DComponent>(input);
    }
    catch (const std::exception& e) {
        std::cerr << "Deserialization error: " << e.what() << std::endl;
        return nullptr;
    }

    return scene;
}

void ruya::RyAssetManager::SerializeCurrentState()
{
    std::filesystem::path fullPath = std::filesystem::path(ASSETS_DIR) / "ruya_files/engine_registry/asset_manager_state.rymeta";

    std::ofstream out(fullPath);
    if (!out)
        return;

    try
    {
        cereal::JSONOutputArchive archive(out);
        archive(*this);
    }
    catch (const std::exception& e)
    {
        RUYA_LOG_ERROR("[RyAssetManager] Asset manager state serialization failed: %s", e.what());
        return;
    }
}

void ruya::RyAssetManager::LoadState()
{
    std::filesystem::path fullPath = std::filesystem::path(ASSETS_DIR) / "ruya_files/engine_registry/asset_manager_state.rymeta";

    if (!std::filesystem::exists(fullPath))
        return;

    std::ifstream in(fullPath);
    if (!in)
    {
        RUYA_LOG_ERROR("[RyAssetManager] Failed to open asset manager state file for reading");
        return;
    }

    try
    {
        cereal::JSONInputArchive archive(in);
        archive(*this);
    }
    catch (const std::exception& e)
    {
        RUYA_LOG_ERROR("[RyAssetManager] Asset manager state deserialization failed: %s", e.what());
        return;
    }
}


