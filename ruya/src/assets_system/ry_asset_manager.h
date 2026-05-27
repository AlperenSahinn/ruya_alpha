#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <optional>

#include <vector>
#include <cstdint>

#include <ktx/ktx.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <core/uuid.h>
#include <core/ry_id.h>

#include <app_framework/scene.h>
#include <graphics/vertex.h>

#include "ry_asset.h"
#include "ry_material.h"

namespace tinygltf { struct Model; }

namespace ruya
{
    enum class TextureFormat
    {
        SRGB,
        Linear,
        NormalMap
    };

    struct RyMeshData
    {
        std::vector<Vertex>   vertices;
        std::vector<uint32_t> indices;
    };

    struct RyMeshInfo
    {
        glm::vec3 aabbMin = glm::vec3(0.0f);
        glm::vec3 aabbMax = glm::vec3(0.0f);
        glm::vec3 initialPosition = glm::vec3(0.0f);
    };

    class RyAssetManager
    {
    public:
        RyAssetManager() = default;
        ~RyAssetManager() = default;

        RyAssetManager(const RyAssetManager&) = delete;
        RyAssetManager& operator=(const RyAssetManager&) = delete;

        void ScanAssets();
        void SaveAsset(UUID uuid);
        void SaveAllAssets();

        UUID CreateAsset(const std::string& name, RyAssetType type, const std::string& directory);
        RyAsset* TryGetAsset(UUID uuid);
        RyAsset* GetAssetByPath(const std::string& assetName);
        void DestroyAsset(UUID uuid);
        bool RenameAsset(UUID uuid, const std::string& newName);
        bool MoveAsset(UUID uuid, const std::string& newDirectory);
        bool MoveFolder(const std::string& folderRelative, const std::string& newParentDirectory);

        UUID ImportPNG(const std::string& sourcePath,
            const std::string& importDirectory,
            TextureFormat format);

        bool ReimportTexture(UUID assetUUID, TextureFormat newFormat);

        ktxTexture2* LoadKTX2Header(UUID assetUUID);

        ktxTexture2* LoadKTX2(UUID assetUUID);

        void ImportGLTF(const std::string& path, const std::string& importDirectory);

        std::optional<RyMeshData> LoadGLTF(UUID assetUUID);
        std::optional<RyMeshInfo> GetGLTFInfo(UUID assetUUID);

        std::optional<RyMaterial> LoadRyMaterial(UUID assetUUID);
        bool SaveMaterial(UUID assetUUID, const RyMaterial& material);

        bool SaveScene(UUID assetUUID, Scene* scene);
        std::unique_ptr<Scene> LoadScene(UUID assetUUID);

    private:
        bool SerializeAsset(UUID uuid);
        RyAsset DeserializeAsset(const std::string& path);

        bool LoadGLBModel(const std::string& path, tinygltf::Model& outModel);

        std::filesystem::path ResolveResourcePath(const RyAsset& asset) const;

        struct GLTFPrimitiveRef { int meshIndex; int primIndex; };
        std::vector<GLTFPrimitiveRef> EnumerateGLTFPrimitives(const tinygltf::Model& model);

        bool DecodeGLTFPrimitive(const tinygltf::Model& model,
            int meshIndex, int primIndex,
            RyMeshData* outData,
            RyMeshInfo* outInfo);

        bool WritePrimitiveToGLB(const tinygltf::Model& srcModel,
            int meshIndex, int primIndex,
            const std::string& outGlbPath);

        bool EncodePNGToKTX2(const std::string& pngPath,
            const std::string& outKtxPath,
            TextureFormat format);

    private:
        std::unordered_map<UUID, std::unique_ptr<RyAsset>> assetRegistry;
    };
}