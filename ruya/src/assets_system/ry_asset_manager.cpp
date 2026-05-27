#include "ry_asset_manager.h"

#include <fstream>
#include <cstring>
#include <vector>
#include <limits>
#include <algorithm>
#include <cmath>
#include <cctype>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#include <tiny_gltf/tiny_gltf.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image/stb_image_resize2.h>

#include <vulkan/vulkan_core.h>

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <core/hash_code_generator.h>
#include <engine.h>
#include <core/log.h>
#include <app_framework/id_component.h>
#include <app_framework/relationship_component.h>
#include <app_framework/transform_component.h>
#include <app_framework/render_component.h>
#include <app_framework/physics_component.h>

namespace
{
    struct JsonOutputArchive
    {
        nlohmann::json& j;
        JsonOutputArchive(nlohmann::json& j) : j(j) {}

        template<typename... Args>
        void operator()(Args&&... args)
        {
            (j.push_back(std::forward<Args>(args)), ...);
        }
    };

    struct JsonInputArchive
    {
        const nlohmann::json& j;
        size_t index = 0;
        JsonInputArchive(const nlohmann::json& j) : j(j) {}

        template<typename... Args>
        void operator()(Args&... args)
        {
            ((args = j.at(index++).get<std::decay_t<Args>>()), ...);
        }
    };
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

                assetRegistry[asset.uuid] = std::make_unique<RyAsset>(asset);

                std::filesystem::path rel = std::filesystem::relative(path, ASSETS_DIR);
                std::string dirOnly = rel.parent_path().string();

                assetRegistry[asset.uuid]->directory = dirOnly;

                SerializeAsset(asset.uuid);
            }
            catch (const std::exception& ex)
            {
                std::string errorStr = "[RyAssetManager] Failed to load asset: " + path.string() + "\nReason: " + ex.what();
                RUYA_LOG_ERROR(errorStr.c_str());
            }
        }
    }
}

void ruya::RyAssetManager::SaveAsset(UUID uuid)
{
    SerializeAsset(uuid);
}

void ruya::RyAssetManager::SaveAllAssets()
{
    int savedCount = 0;
    int failedCount = 0;

    for (const auto& [uuid, asset] : assetRegistry)
    {
        if (SerializeAsset(uuid))
        {
            savedCount++;
        }
        else
        {
            failedCount++;
            RUYA_LOG_ERROR("[RyAssetManager] Failed to save asset: {}", asset->assetName.c_str());
        }
    }

    RUYA_LOG_INFO("[RyAssetManager] SaveAllAssets completed. Successfully saved: {}, Failed: {}", savedCount, failedCount);
}

ruya::UUID ruya::RyAssetManager::CreateAsset(const std::string& name, RyAssetType type, const std::string& directory)
{
    UUID uuid = GenerateRandomUUID();

    std::unique_ptr<RyAsset> asset = std::make_unique<RyAsset>();
    asset->uuid = uuid;
    asset->assetName = name;
    asset->ryAssetType = type;
    asset->directory = directory;

    assetRegistry[uuid] = std::move(asset);

    SerializeAsset(uuid);

    return uuid;
}

ruya::RyAsset* ruya::RyAssetManager::TryGetAsset(UUID uuid)
{
    auto it = assetRegistry.find(uuid);
    if (it == assetRegistry.end())
        return nullptr;

    return it->second.get();
}

ruya::RyAsset* ruya::RyAssetManager::GetAssetByPath(const std::string& path)
{
    RyAsset asset = DeserializeAsset(path);

    if (asset.uuid == UUID(0))
        return nullptr;

    auto it = assetRegistry.find(asset.uuid);
    if (it == assetRegistry.end())
        return nullptr;

    return it->second.get();
}

void ruya::RyAssetManager::DestroyAsset(UUID uuid)
{
    auto it = assetRegistry.find(uuid);
    if (it == assetRegistry.end())
        return;

    RyAsset* asset = it->second.get();

    std::string cleanName = std::filesystem::path(asset->assetName).stem().string();
    std::filesystem::path baseDir = std::filesystem::path(ASSETS_DIR) / asset->directory;
    std::filesystem::path ryassetPath = baseDir / (cleanName + ".ryasset");
    std::filesystem::path resourcePath = ResolveResourcePath(*asset);

    std::error_code ec;
    if (std::filesystem::exists(ryassetPath))
        std::filesystem::remove(ryassetPath, ec);

    bool hasExternalResource = (asset->ryAssetType != RyAssetType::RyScene);

    bool resourceShared = false;
    if (hasExternalResource)
    {
        for (const auto& [otherUuid, otherAsset] : assetRegistry)
        {
            if (otherUuid == uuid || !otherAsset)
                continue;
            if (ResolveResourcePath(*otherAsset) == resourcePath)
            {
                resourceShared = true;
                break;
            }
        }
    }

    if (hasExternalResource && !resourceShared && std::filesystem::exists(resourcePath))
        std::filesystem::remove(resourcePath, ec);

    assetRegistry.erase(uuid);
}

bool ruya::RyAssetManager::RenameAsset(UUID uuid, const std::string& newName)
{
    auto it = assetRegistry.find(uuid);
    if (it == assetRegistry.end())
        return false;

    RyAsset* asset = it->second.get();

    std::string cleanOldName = std::filesystem::path(asset->assetName).stem().string();

    if (cleanOldName == newName)
        return true;

    std::filesystem::path currentResourcePath = std::filesystem::path(ASSETS_DIR) / asset->directory / asset->assetName;
    std::string extension = currentResourcePath.extension().string();

    std::string newAssetName = newName + extension;
    std::filesystem::path newResourcePath = std::filesystem::path(ASSETS_DIR) / asset->directory / newAssetName;

    std::filesystem::path oldAssetPath = std::filesystem::path(ASSETS_DIR) / asset->directory / (cleanOldName + ".ryasset");
    std::filesystem::path newAssetPath = std::filesystem::path(ASSETS_DIR) / asset->directory / (newName + ".ryasset");

    if (std::filesystem::exists(newAssetPath))
    {
        RUYA_LOG_ERROR("[RyAssetManager] Cannot rename asset: {} already exists.", newAssetPath.string().c_str());
        return false;
    }

    bool hasOwnResourceFile = (asset->ryAssetType != RyAssetType::RyScene);

    if (hasOwnResourceFile && std::filesystem::exists(newResourcePath))
    {
        RUYA_LOG_ERROR("[RyAssetManager] Cannot rename asset: {} already exists.", newResourcePath.string().c_str());
        return false;
    }

    if (hasOwnResourceFile && std::filesystem::exists(currentResourcePath))
    {
        try {
            std::filesystem::rename(currentResourcePath, newResourcePath);
        }
        catch (const std::exception& e) {
            RUYA_LOG_ERROR("[RyAssetManager] Failed to rename resource file: {}", e.what());
            return false;
        }
    }

    if (std::filesystem::exists(oldAssetPath))
    {
        try {
            std::filesystem::rename(oldAssetPath, newAssetPath);
        }
        catch (const std::exception& e) {
            if (hasOwnResourceFile && std::filesystem::exists(newResourcePath)) {
                std::filesystem::rename(newResourcePath, currentResourcePath);
            }
            RUYA_LOG_ERROR("[RyAssetManager] Failed to rename .ryasset file: {}", e.what());
            return false;
        }
    }

    asset->assetName = newAssetName;
    SerializeAsset(uuid);

    RUYA_LOG_INFO("[RyAssetManager] Successfully renamed asset to: {}", newAssetName.c_str());
    return true;
}

bool ruya::RyAssetManager::MoveAsset(UUID uuid, const std::string& newDirectory)
{
    auto it = assetRegistry.find(uuid);
    if (it == assetRegistry.end())
        return false;

    RyAsset* asset = it->second.get();

    std::string oldDir = asset->directory;
    std::string newDir = newDirectory;
    std::replace(oldDir.begin(), oldDir.end(), '\\', '/');
    std::replace(newDir.begin(), newDir.end(), '\\', '/');

    if (oldDir == newDir)
        return true;

    std::filesystem::path baseAssets = std::filesystem::path(ASSETS_DIR);

    std::string cleanName = std::filesystem::path(asset->assetName).stem().string();

    std::filesystem::path oldRyassetPath = baseAssets / oldDir / (cleanName + ".ryasset");
    std::filesystem::path newRyassetPath = baseAssets / newDir / (cleanName + ".ryasset");
    std::filesystem::path oldResourcePath = baseAssets / oldDir / asset->assetName;
    std::filesystem::path newResourcePath = baseAssets / newDir / asset->assetName;

    bool hasExternalResource = (asset->ryAssetType != RyAssetType::RyScene);

    if (std::filesystem::exists(newRyassetPath))
    {
        RUYA_LOG_ERROR("[RyAssetManager] Cannot move asset: {} already exists.", newRyassetPath.string().c_str());
        return false;
    }
    if (hasExternalResource && std::filesystem::exists(newResourcePath))
    {
        RUYA_LOG_ERROR("[RyAssetManager] Cannot move asset: {} already exists.", newResourcePath.string().c_str());
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(newRyassetPath.parent_path(), ec);
    if (ec)
    {
        RUYA_LOG_ERROR("[RyAssetManager] Failed to create destination directory: {}", ec.message().c_str());
        return false;
    }

    bool resourceMoved = false;
    if (hasExternalResource && std::filesystem::exists(oldResourcePath))
    {
        try
        {
            std::filesystem::rename(oldResourcePath, newResourcePath);
            resourceMoved = true;
        }
        catch (const std::exception& e)
        {
            RUYA_LOG_ERROR("[RyAssetManager] Failed to move resource file: {}", e.what());
            return false;
        }
    }

    if (std::filesystem::exists(oldRyassetPath))
    {
        try
        {
            std::filesystem::rename(oldRyassetPath, newRyassetPath);
        }
        catch (const std::exception& e)
        {
            if (resourceMoved)
            {
                std::error_code rb;
                std::filesystem::rename(newResourcePath, oldResourcePath, rb);
            }
            RUYA_LOG_ERROR("[RyAssetManager] Failed to move .ryasset file: {}", e.what());
            return false;
        }
    }

    asset->directory = newDir;
    SerializeAsset(uuid);

    RUYA_LOG_INFO("[RyAssetManager] Moved asset '{}' to '{}'", asset->assetName.c_str(), newDir.c_str());
    return true;
}

bool ruya::RyAssetManager::MoveFolder(const std::string& folderRelative, const std::string& newParentDirectory)
{
    std::string folderRel = folderRelative;
    std::string newParent = newParentDirectory;
    std::replace(folderRel.begin(), folderRel.end(), '\\', '/');
    std::replace(newParent.begin(), newParent.end(), '\\', '/');

    if (folderRel.empty())
    {
        RUYA_LOG_ERROR("[RyAssetManager] MoveFolder: source folder is empty.");
        return false;
    }

    std::filesystem::path baseAssets = std::filesystem::path(ASSETS_DIR);
    std::filesystem::path src = baseAssets / folderRel;
    std::string folderName = std::filesystem::path(folderRel).filename().string();
    std::filesystem::path dst = baseAssets / newParent / folderName;

    if (!std::filesystem::exists(src) || !std::filesystem::is_directory(src))
    {
        RUYA_LOG_ERROR("[RyAssetManager] MoveFolder: source does not exist: {}", src.string().c_str());
        return false;
    }

    {
        std::error_code ec;
        std::filesystem::path srcCanon = std::filesystem::weakly_canonical(src, ec);
        std::filesystem::path dstParent = baseAssets / newParent;
        std::filesystem::path dstParentCanon = std::filesystem::weakly_canonical(dstParent, ec);

        if (!ec)
        {
            std::string srcStr = srcCanon.string();
            std::string dstStr = dstParentCanon.string();
            std::replace(srcStr.begin(), srcStr.end(), '\\', '/');
            std::replace(dstStr.begin(), dstStr.end(), '\\', '/');
            if (dstStr == srcStr || dstStr.rfind(srcStr + "/", 0) == 0)
            {
                RUYA_LOG_ERROR("[RyAssetManager] MoveFolder: cannot move folder into itself or a descendant.");
                return false;
            }
        }
    }

    if (std::filesystem::exists(dst))
    {
        RUYA_LOG_ERROR("[RyAssetManager] MoveFolder: destination already exists: {}", dst.string().c_str());
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(dst.parent_path(), ec);

    try
    {
        std::filesystem::rename(src, dst);
    }
    catch (const std::exception& e)
    {
        RUYA_LOG_ERROR("[RyAssetManager] MoveFolder failed: {}", e.what());
        return false;
    }

    std::string newFolderRel = newParent.empty() ? folderName : (newParent + "/" + folderName);

    for (auto& [uuid, assetPtr] : assetRegistry)
    {
        if (!assetPtr) continue;

        std::string dir = assetPtr->directory;
        std::replace(dir.begin(), dir.end(), '\\', '/');

        bool exactMatch = (dir == folderRel);
        bool prefixMatch = (dir.rfind(folderRel + "/", 0) == 0);

        if (!exactMatch && !prefixMatch)
            continue;

        std::string suffix = exactMatch ? std::string() : dir.substr(folderRel.size());
        assetPtr->directory = newFolderRel + suffix;

        SerializeAsset(uuid);
    }

    RUYA_LOG_INFO("[RyAssetManager] Moved folder '{}' to '{}'", folderRel.c_str(), newFolderRel.c_str());
    return true;
}

bool ruya::RyAssetManager::EncodePNGToKTX2(const std::string& pngPath,
    const std::string& outKtxPath,
    TextureFormat format)
{
    int width = 0, height = 0, channels = 0;
    stbi_uc* pixels = stbi_load(pngPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
    {
        RUYA_LOG_ERROR("[RyAssetManager] stbi_load failed for: {}", pngPath.c_str());
        return false;
    }

    const uint32_t maxDim = std::max(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    uint32_t mipLevels = 1;
    {
        uint32_t d = maxDim;
        while (d > 1) { d >>= 1; ++mipLevels; }
    }

    ktxTextureCreateInfo createInfo{};
    createInfo.vkFormat = (format == TextureFormat::SRGB)
        ? VK_FORMAT_R8G8B8A8_SRGB
        : VK_FORMAT_R8G8B8A8_UNORM;
    createInfo.baseWidth = static_cast<uint32_t>(width);
    createInfo.baseHeight = static_cast<uint32_t>(height);
    createInfo.baseDepth = 1;
    createInfo.numDimensions = 2;
    createInfo.numLevels = mipLevels;
    createInfo.numLayers = 1;
    createInfo.numFaces = 1;
    createInfo.isArray = KTX_FALSE;
    createInfo.generateMipmaps = KTX_FALSE;

    ktxTexture2* texture = nullptr;
    KTX_error_code result = ktxTexture2_Create(&createInfo,
        KTX_TEXTURE_CREATE_ALLOC_STORAGE,
        &texture);
    if (result != KTX_SUCCESS)
    {
        RUYA_LOG_ERROR("[RyAssetManager] ktxTexture2_Create failed: {}", ktxErrorString(result));
        stbi_image_free(pixels);
        return false;
    }

    const ktx_size_t basePixelByteCount = static_cast<ktx_size_t>(width) * height * 4;
    result = ktxTexture_SetImageFromMemory(ktxTexture(texture),
        0, 0, 0,
        pixels,
        basePixelByteCount);
    if (result != KTX_SUCCESS)
    {
        RUYA_LOG_ERROR("[RyAssetManager] ktxTexture_SetImageFromMemory (mip 0) failed: {}", ktxErrorString(result));
        stbi_image_free(pixels);
        ktxTexture_Destroy(ktxTexture(texture));
        return false;
    }

    int prevW = width;
    int prevH = height;
    std::vector<stbi_uc> prevLevel(pixels, pixels + basePixelByteCount);
    stbi_image_free(pixels);

    if (format == TextureFormat::NormalMap)
    {
        const size_t pixelCount = static_cast<size_t>(prevW) * prevH;
        for (size_t i = 0; i < pixelCount; ++i)
        {
            float x = (prevLevel[i * 4 + 0] / 255.0f) * 2.0f - 1.0f;
            float y = (prevLevel[i * 4 + 1] / 255.0f) * 2.0f - 1.0f;
            float z = (prevLevel[i * 4 + 2] / 255.0f) * 2.0f - 1.0f;
            float len = std::sqrt(x * x + y * y + z * z);
            if (len > 1e-8f) { x /= len; y /= len; z /= len; }
            else { x = 0.0f; y = 0.0f; z = 1.0f; }
            prevLevel[i * 4 + 0] = static_cast<stbi_uc>(std::clamp((x * 0.5f + 0.5f) * 255.0f + 0.5f, 0.0f, 255.0f));
            prevLevel[i * 4 + 1] = static_cast<stbi_uc>(std::clamp((y * 0.5f + 0.5f) * 255.0f + 0.5f, 0.0f, 255.0f));
            prevLevel[i * 4 + 2] = static_cast<stbi_uc>(std::clamp((z * 0.5f + 0.5f) * 255.0f + 0.5f, 0.0f, 255.0f));
            prevLevel[i * 4 + 3] = 255;
        }

        result = ktxTexture_SetImageFromMemory(ktxTexture(texture),
            0, 0, 0,
            prevLevel.data(),
            basePixelByteCount);
        if (result != KTX_SUCCESS)
        {
            RUYA_LOG_ERROR("[RyAssetManager] ktxTexture_SetImageFromMemory (normalmap mip 0 rewrite) failed: {}", ktxErrorString(result));
            ktxTexture_Destroy(ktxTexture(texture));
            return false;
        }
    }

    bool mipFailed = false;
    for (uint32_t level = 1; level < mipLevels; ++level)
    {
        int curW = std::max(1, prevW >> 1);
        int curH = std::max(1, prevH >> 1);
        const size_t curPixels = static_cast<size_t>(curW) * curH;
        const size_t curBytes = curPixels * 4;

        std::vector<stbi_uc> curLevel(curBytes);

        bool ok = false;
        if (format == TextureFormat::SRGB)
        {
            ok = (stbir_resize_uint8_srgb(
                prevLevel.data(), prevW, prevH, 0,
                curLevel.data(), curW, curH, 0,
                STBIR_RGBA) != nullptr);
        }
        else if (format == TextureFormat::Linear)
        {
            ok = (stbir_resize_uint8_linear(
                prevLevel.data(), prevW, prevH, 0,
                curLevel.data(), curW, curH, 0,
                STBIR_RGBA) != nullptr);
        }
        else
        {
            const size_t prevPixels = static_cast<size_t>(prevW) * prevH;
            std::vector<float> prevFloat(prevPixels * 4);
            for (size_t i = 0; i < prevPixels; ++i)
            {
                prevFloat[i * 4 + 0] = (prevLevel[i * 4 + 0] / 255.0f) * 2.0f - 1.0f;
                prevFloat[i * 4 + 1] = (prevLevel[i * 4 + 1] / 255.0f) * 2.0f - 1.0f;
                prevFloat[i * 4 + 2] = (prevLevel[i * 4 + 2] / 255.0f) * 2.0f - 1.0f;
                prevFloat[i * 4 + 3] = 1.0f;
            }

            std::vector<float> curFloat(curPixels * 4);
            ok = (stbir_resize_float_linear(
                prevFloat.data(), prevW, prevH, 0,
                curFloat.data(), curW, curH, 0,
                STBIR_RGBA) != nullptr);

            if (ok)
            {
                for (size_t i = 0; i < curPixels; ++i)
                {
                    float x = curFloat[i * 4 + 0];
                    float y = curFloat[i * 4 + 1];
                    float z = curFloat[i * 4 + 2];
                    float len = std::sqrt(x * x + y * y + z * z);
                    if (len > 1e-8f) { x /= len; y /= len; z /= len; }
                    else { x = 0.0f; y = 0.0f; z = 1.0f; }
                    curLevel[i * 4 + 0] = static_cast<stbi_uc>(std::clamp((x * 0.5f + 0.5f) * 255.0f + 0.5f, 0.0f, 255.0f));
                    curLevel[i * 4 + 1] = static_cast<stbi_uc>(std::clamp((y * 0.5f + 0.5f) * 255.0f + 0.5f, 0.0f, 255.0f));
                    curLevel[i * 4 + 2] = static_cast<stbi_uc>(std::clamp((z * 0.5f + 0.5f) * 255.0f + 0.5f, 0.0f, 255.0f));
                    curLevel[i * 4 + 3] = 255;
                }
            }
        }

        if (!ok)
        {
            RUYA_LOG_ERROR("[RyAssetManager] mip resize failed at level {}", level);
            mipFailed = true;
            break;
        }

        result = ktxTexture_SetImageFromMemory(ktxTexture(texture),
            level, 0, 0,
            curLevel.data(),
            curBytes);
        if (result != KTX_SUCCESS)
        {
            RUYA_LOG_ERROR("[RyAssetManager] ktxTexture_SetImageFromMemory (mip {}) failed: {}",
                level, ktxErrorString(result));
            mipFailed = true;
            break;
        }

        prevW = curW;
        prevH = curH;
        prevLevel = std::move(curLevel);
    }

    if (mipFailed)
    {
        ktxTexture_Destroy(ktxTexture(texture));
        return false;
    }

    ktxBasisParams basisParams{};
    basisParams.structSize = sizeof(ktxBasisParams);
    basisParams.threadCount = std::thread::hardware_concurrency();
    basisParams.verbose = KTX_FALSE;
    basisParams.codec = KTX_BASIS_CODEC_UASTC_LDR_4x4;
    basisParams.uastcFlags = KTX_PACK_UASTC_LEVEL_DEFAULT;
    basisParams.uastcRDO = KTX_FALSE;
    basisParams.uastcRDOQualityScalar = 0.5f;
    basisParams.uastcRDODictSize = 8192;
    basisParams.uastcRDOMaxSmoothBlockErrorScale = 10.0f;
    basisParams.uastcRDOMaxSmoothBlockStdDev = 18.0f;
    basisParams.uastcRDODontFavorSimplerModes = KTX_FALSE;
    basisParams.uastcRDONoMultithreading = KTX_FALSE;
    basisParams.normalMap = (format == TextureFormat::NormalMap) ? KTX_TRUE : KTX_FALSE;

    result = ktxTexture2_CompressBasisEx(texture, &basisParams);
    if (result != KTX_SUCCESS)
    {
        RUYA_LOG_ERROR("[RyAssetManager] ktxTexture2_CompressBasisEx failed: {}", ktxErrorString(result));
        ktxTexture_Destroy(ktxTexture(texture));
        return false;
    }

    ktx_transcode_fmt_e transcodeTarget = KTX_TTF_BC7_RGBA;
    if (format == TextureFormat::NormalMap)
        transcodeTarget = KTX_TTF_BC5_RG;

    result = ktxTexture2_TranscodeBasis(texture, transcodeTarget, 0);
    if (result != KTX_SUCCESS)
    {
        RUYA_LOG_ERROR("[RyAssetManager] ktxTexture2_TranscodeBasis failed: {}", ktxErrorString(result));
        ktxTexture_Destroy(ktxTexture(texture));
        return false;
    }

    std::filesystem::create_directories(std::filesystem::path(outKtxPath).parent_path());
    result = ktxTexture_WriteToNamedFile(ktxTexture(texture), outKtxPath.c_str());
    ktxTexture_Destroy(ktxTexture(texture));

    if (result != KTX_SUCCESS)
    {
        RUYA_LOG_ERROR("[RyAssetManager] ktxTexture_WriteToNamedFile failed: {}", ktxErrorString(result));
        return false;
    }

    return true;
}

ruya::UUID ruya::RyAssetManager::ImportPNG(const std::string& sourcePath,
    const std::string& importDirectory,
    TextureFormat format)
{
    std::filesystem::path src(sourcePath);

    if (!std::filesystem::exists(src) || src.extension() != ".png")
    {
        RUYA_LOG_ERROR("[RyAssetManager] Invalid PNG source path or file does not exist: {}", sourcePath.c_str());
        return UUID::Invalid();
    }

    std::string stemName = src.stem().string();
    std::string ktxFileName = stemName + ".ktx2";

    std::filesystem::path destDir = std::filesystem::path(ASSETS_DIR) / importDirectory;
    std::filesystem::path destKtxPath = destDir / ktxFileName;
    std::filesystem::path ryassetPath = destDir / (stemName + ".ryasset");

    if (std::filesystem::exists(destKtxPath))
    {
        RUYA_LOG_ERROR("[RyAssetManager] Cannot import PNG: '{}' already exists at destination.", ktxFileName.c_str());
        return UUID::Invalid();
    }

    if (std::filesystem::exists(ryassetPath))
    {
        RUYA_LOG_ERROR("[RyAssetManager] Cannot import PNG: asset '{}' already exists.", stemName.c_str());
        return UUID::Invalid();
    }

    try
    {
        std::filesystem::create_directories(destDir);
    }
    catch (const std::exception& e)
    {
        RUYA_LOG_ERROR("[RyAssetManager] Failed to create destination directory: {}", e.what());
        return UUID::Invalid();
    }

    if (!EncodePNGToKTX2(sourcePath, destKtxPath.string(), format))
    {
        RUYA_LOG_ERROR("[RyAssetManager] Failed to encode PNG to KTX2: {}", sourcePath.c_str());
        return UUID::Invalid();
    }

    UUID uuid = GenerateRandomUUID();
    auto asset = std::make_unique<RyAsset>();
    asset->uuid = uuid;
    asset->assetName = ktxFileName;
    asset->ryAssetType = RyAssetType::KTX2;
    asset->directory = importDirectory;
    asset->sourcePath = sourcePath;

    assetRegistry[uuid] = std::move(asset);
    SerializeAsset(uuid);

    RUYA_LOG_INFO("[RyAssetManager] Imported texture: {}", ktxFileName.c_str());
    return uuid;
}

bool ruya::RyAssetManager::ReimportTexture(UUID assetUUID, TextureFormat newFormat)
{
    auto it = assetRegistry.find(assetUUID);
    if (it == assetRegistry.end())
    {
        RUYA_LOG_ERROR("[RyAssetManager] ReimportTexture: asset UUID not found.");
        return false;
    }

    RyAsset* asset = it->second.get();
    if (asset->ryAssetType != RyAssetType::KTX2)
    {
        RUYA_LOG_ERROR("[RyAssetManager] ReimportTexture: asset is not a KTX2 texture.");
        return false;
    }

    if (asset->sourcePath.empty() || !std::filesystem::exists(asset->sourcePath))
    {
        RUYA_LOG_ERROR("[RyAssetManager] ReimportTexture: original PNG source not found: {}", asset->sourcePath.c_str());
        return false;
    }

    std::filesystem::path destKtxPath = std::filesystem::path(ASSETS_DIR) / asset->directory / asset->assetName;

    std::filesystem::path tempPath = destKtxPath;
    tempPath += ".tmp";

    if (!EncodePNGToKTX2(asset->sourcePath, tempPath.string(), newFormat))
    {
        std::error_code ec;
        std::filesystem::remove(tempPath, ec);
        return false;
    }

    std::error_code ec;
    std::filesystem::remove(destKtxPath, ec);
    std::filesystem::rename(tempPath, destKtxPath, ec);
    if (ec)
    {
        RUYA_LOG_ERROR("[RyAssetManager] ReimportTexture: failed to replace KTX2 file: {}", ec.message().c_str());
        return false;
    }

    RUYA_LOG_INFO("[RyAssetManager] Reimported texture: {}", asset->assetName.c_str());
    return true;
}

ktxTexture2* ruya::RyAssetManager::LoadKTX2Header(UUID assetUUID)
{
    auto it = assetRegistry.find(assetUUID);
    if (it == assetRegistry.end())
    {
        RUYA_LOG_ERROR("[RyAssetManager] LoadKTX2Header: asset UUID not found.");
        return nullptr;
    }

    RyAsset* asset = it->second.get();
    if (asset->ryAssetType != RyAssetType::KTX2)
    {
        RUYA_LOG_ERROR("[RyAssetManager] LoadKTX2Header: asset is not a KTX2 texture.");
        return nullptr;
    }

    std::filesystem::path ktxPath = std::filesystem::path(ASSETS_DIR) / asset->directory / asset->assetName;

    ktxTexture2* texture = nullptr;
    KTX_error_code result = ktxTexture2_CreateFromNamedFile(
        ktxPath.string().c_str(),
        KTX_TEXTURE_CREATE_NO_FLAGS,
        &texture);

    if (result != KTX_SUCCESS || !texture)
    {
        RUYA_LOG_ERROR("[RyAssetManager] Failed to read KTX2 header: {} ({})",
            ktxPath.string().c_str(),
            ktxErrorString(result));
        return nullptr;
    }

    return texture;
}

ktxTexture2* ruya::RyAssetManager::LoadKTX2(UUID assetUUID)
{
    auto it = assetRegistry.find(assetUUID);
    if (it == assetRegistry.end())
    {
        RUYA_LOG_ERROR("[RyAssetManager] LoadKTX2: asset UUID not found in registry.");
        return nullptr;
    }

    RyAsset* asset = it->second.get();
    if (asset->ryAssetType != RyAssetType::KTX2)
    {
        RUYA_LOG_ERROR("[RyAssetManager] LoadKTX2: asset is not a KTX2 texture.");
        return nullptr;
    }

    std::filesystem::path ktxPath = std::filesystem::path(ASSETS_DIR) / asset->directory / asset->assetName;

    ktxTexture2* texture = nullptr;
    KTX_error_code result = ktxTexture2_CreateFromNamedFile(
        ktxPath.string().c_str(),
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &texture);

    if (result != KTX_SUCCESS || !texture)
    {
        RUYA_LOG_ERROR("[RyAssetManager] Failed to load KTX2 texture: {} ({})",
            ktxPath.string().c_str(),
            ktxErrorString(result));
        return nullptr;
    }

    return texture;
}

bool ruya::RyAssetManager::LoadGLBModel(const std::string& path, tinygltf::Model& outModel)
{
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ret = loader.LoadBinaryFromFile(&outModel, &err, &warn, path);

    if (!warn.empty()) RUYA_LOG_WARN("[RyAssetManager] GLB warn: {}", warn.c_str());
    if (!err.empty())  RUYA_LOG_ERROR("[RyAssetManager] GLB error: {}", err.c_str());
    if (!ret)
    {
        RUYA_LOG_ERROR("[RyAssetManager] Failed to load GLB: {}", path.c_str());
        return false;
    }

    return true;
}

std::vector<ruya::RyAssetManager::GLTFPrimitiveRef>
ruya::RyAssetManager::EnumerateGLTFPrimitives(const tinygltf::Model& model)
{
    std::vector<GLTFPrimitiveRef> refs;

    for (int meshIdx = 0; meshIdx < static_cast<int>(model.meshes.size()); ++meshIdx)
    {
        const tinygltf::Mesh& gltfMesh = model.meshes[meshIdx];
        for (int primIdx = 0; primIdx < static_cast<int>(gltfMesh.primitives.size()); ++primIdx)
        {
            const tinygltf::Primitive& primitive = gltfMesh.primitives[primIdx];
            if (primitive.attributes.find("POSITION") == primitive.attributes.end())
                continue;

            refs.push_back(GLTFPrimitiveRef{ meshIdx, primIdx });
        }
    }

    return refs;
}

bool ruya::RyAssetManager::DecodeGLTFPrimitive(const tinygltf::Model& model,
    int meshIndex, int primIndex,
    RyMeshData* outData,
    RyMeshInfo* outInfo)
{
    if (meshIndex < 0 || meshIndex >= static_cast<int>(model.meshes.size()))
        return false;

    const tinygltf::Mesh& gltfMesh = model.meshes[meshIndex];
    if (primIndex < 0 || primIndex >= static_cast<int>(gltfMesh.primitives.size()))
        return false;

    const tinygltf::Primitive& primitive = gltfMesh.primitives[primIndex];

    if (primitive.attributes.find("POSITION") == primitive.attributes.end())
        return false;

    glm::vec3 initialPosition(0.0f);
    for (const tinygltf::Node& node : model.nodes)
    {
        if (node.mesh != meshIndex) continue;

        if (node.matrix.size() == 16)
        {
            glm::mat4 mat = glm::make_mat4(node.matrix.data());
            glm::vec3 scale, translation, skew;
            glm::quat rotation;
            glm::vec4 persp;
            glm::decompose(mat, scale, rotation, translation, skew, persp);
            initialPosition = translation;
        }
        else if (node.translation.size() == 3)
        {
            initialPosition = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
        }
        break;
    }

    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;

    int posIdx = primitive.attributes.at("POSITION");
    size_t vertexCount = model.accessors[posIdx].count;
    vertices.resize(vertexCount);

    auto loadAttribute = [&](const std::string& attrName, auto lambda)
        {
            auto attrIt = primitive.attributes.find(attrName);
            if (attrIt == primitive.attributes.end()) return;

            const tinygltf::Accessor& acc = model.accessors[attrIt->second];
            const tinygltf::BufferView& bv = model.bufferViews[acc.bufferView];
            const tinygltf::Buffer& buf = model.buffers[bv.buffer];

            const uint8_t* dataStart = &buf.data[bv.byteOffset + acc.byteOffset];
            int stride = (bv.byteStride == 0)
                ? tinygltf::GetComponentSizeInBytes(acc.componentType) *
                tinygltf::GetNumComponentsInType(acc.type)
                : static_cast<int>(bv.byteStride);

            for (size_t i = 0; i < acc.count; ++i)
                lambda(i, dataStart + i * stride);
        };

    loadAttribute("POSITION", [&](size_t i, const uint8_t* data) {
        const float* p = reinterpret_cast<const float*>(data);
        vertices[i].position = glm::vec3(p[0], p[1], p[2]);
        });

    loadAttribute("NORMAL", [&](size_t i, const uint8_t* data) {
        const float* n = reinterpret_cast<const float*>(data);
        vertices[i].normal = glm::normalize(glm::vec3(n[0], n[1], n[2]));
        });

    loadAttribute("TEXCOORD_0", [&](size_t i, const uint8_t* data) {
        const float* uv = reinterpret_cast<const float*>(data);
        vertices[i].texCoord = glm::vec2(uv[0], uv[1]);
        });

    if (primitive.indices >= 0)
    {
        const tinygltf::Accessor& indexAcc = model.accessors[primitive.indices];
        const tinygltf::BufferView& indexBv = model.bufferViews[indexAcc.bufferView];
        const tinygltf::Buffer& indexBuf = model.buffers[indexBv.buffer];
        const uint8_t* indexData = &indexBuf.data[indexBv.byteOffset + indexAcc.byteOffset];
        int stride = (indexBv.byteStride == 0)
            ? tinygltf::GetComponentSizeInBytes(indexAcc.componentType)
            : static_cast<int>(indexBv.byteStride);

        indices.reserve(indexAcc.count);
        for (size_t i = 0; i < indexAcc.count; ++i)
        {
            if (indexAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                indices.push_back(*reinterpret_cast<const uint16_t*>(indexData + i * stride));
            else if (indexAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                indices.push_back(*reinterpret_cast<const uint32_t*>(indexData + i * stride));
            else if (indexAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                indices.push_back(*reinterpret_cast<const uint8_t*>(indexData + i * stride));
        }
    }

    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        Vertex& v0 = vertices[indices[i]];
        Vertex& v1 = vertices[indices[i + 1]];
        Vertex& v2 = vertices[indices[i + 2]];

        glm::vec3 edge1 = v1.position - v0.position;
        glm::vec3 edge2 = v2.position - v0.position;
        glm::vec2 dUV1 = v1.texCoord - v0.texCoord;
        glm::vec2 dUV2 = v2.texCoord - v0.texCoord;

        float denom = dUV1.x * dUV2.y - dUV2.x * dUV1.y;
        if (glm::abs(denom) < 1e-6f)
            continue;

        float r = 1.0f / denom;

        glm::vec3 tangent = (edge1 * dUV2.y - edge2 * dUV1.y) * r;
        glm::vec3 bitangent = (edge2 * dUV1.x - edge1 * dUV2.x) * r;

        v0.tangent += tangent;  v0.bitangent += bitangent;
        v1.tangent += tangent;  v1.bitangent += bitangent;
        v2.tangent += tangent;  v2.bitangent += bitangent;
    }

    for (auto& v : vertices)
    {
        if (glm::length(v.tangent) > 1e-6f)
        {
            v.tangent = glm::normalize(v.tangent - v.normal * glm::dot(v.normal, v.tangent));

            float handedness = (glm::dot(glm::cross(v.normal, v.tangent), v.bitangent) < 0.0f) ? -1.0f : 1.0f;
            v.bitangent = glm::cross(v.normal, v.tangent) * handedness;
        }
        else
        {
            glm::vec3 up = (glm::abs(v.normal.y) < 0.999f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
            v.tangent = glm::normalize(glm::cross(up, v.normal));
            v.bitangent = glm::cross(v.normal, v.tangent);
        }
    }

    if (outInfo)
    {
        glm::vec3 aabbMin(std::numeric_limits<float>::max());
        glm::vec3 aabbMax(-std::numeric_limits<float>::max());
        for (const auto& v : vertices)
        {
            aabbMin = glm::min(aabbMin, v.position);
            aabbMax = glm::max(aabbMax, v.position);
        }

        if (vertices.empty())
        {
            aabbMin = glm::vec3(0.0f);
            aabbMax = glm::vec3(0.0f);
        }

        outInfo->aabbMin = aabbMin;
        outInfo->aabbMax = aabbMax;
        outInfo->initialPosition = initialPosition;
    }

    if (outData)
    {
        outData->vertices = std::move(vertices);
        outData->indices = std::move(indices);
    }

    return true;
}

bool ruya::RyAssetManager::WritePrimitiveToGLB(const tinygltf::Model& srcModel,
    int meshIndex, int primIndex,
    const std::string& outGlbPath)
{
    if (meshIndex < 0 || meshIndex >= static_cast<int>(srcModel.meshes.size()))
        return false;

    const tinygltf::Mesh& srcMesh = srcModel.meshes[meshIndex];
    if (primIndex < 0 || primIndex >= static_cast<int>(srcMesh.primitives.size()))
        return false;

    const tinygltf::Primitive& srcPrim = srcMesh.primitives[primIndex];

    auto getLocalMatrix = [](const tinygltf::Node& node) -> glm::mat4
        {
            if (node.matrix.size() == 16)
                return glm::make_mat4(node.matrix.data());

            glm::mat4 t(1.0f), r(1.0f), s(1.0f);
            if (node.translation.size() == 3)
                t = glm::translate(glm::mat4(1.0f),
                    glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
            if (node.rotation.size() == 4)
            {
                glm::quat q(static_cast<float>(node.rotation[3]),   // w
                    static_cast<float>(node.rotation[0]),   // x
                    static_cast<float>(node.rotation[1]),   // y
                    static_cast<float>(node.rotation[2]));  // z
                r = glm::mat4_cast(q);
            }
            if (node.scale.size() == 3)
                s = glm::scale(glm::mat4(1.0f),
                    glm::vec3(node.scale[0], node.scale[1], node.scale[2]));

            return t * r * s;
        };

    std::unordered_map<int, int> parentOf;
    for (int n = 0; n < static_cast<int>(srcModel.nodes.size()); ++n)
        for (int child : srcModel.nodes[n].children)
            parentOf[child] = n;

    int targetNode = -1;
    for (int n = 0; n < static_cast<int>(srcModel.nodes.size()); ++n)
    {
        if (srcModel.nodes[n].mesh == meshIndex) { targetNode = n; break; }
    }

    glm::mat4 worldMatrix(1.0f);
    if (targetNode >= 0)
    {
        std::vector<int> chain;
        int cur = targetNode;
        while (cur >= 0)
        {
            chain.push_back(cur);
            auto it = parentOf.find(cur);
            cur = (it != parentOf.end()) ? it->second : -1;
        }

        for (auto it = chain.rbegin(); it != chain.rend(); ++it)
            worldMatrix = worldMatrix * getLocalMatrix(srcModel.nodes[*it]);
    }

    tinygltf::Model out;
    out.asset.version = "2.0";
    out.asset.generator = "RyAssetManager";

    tinygltf::Buffer outBuffer;
    auto appendToBuffer = [&](const tinygltf::Model& model, int bufferViewIndex) -> int
        {
            const tinygltf::BufferView& srcView = model.bufferViews[bufferViewIndex];
            const tinygltf::Buffer& srcBuf = model.buffers[srcView.buffer];

            const size_t alignedOffset = (outBuffer.data.size() + 3) & ~size_t(3);
            outBuffer.data.resize(alignedOffset);

            const uint8_t* begin = srcBuf.data.data() + srcView.byteOffset;
            outBuffer.data.insert(outBuffer.data.end(), begin, begin + srcView.byteLength);

            tinygltf::BufferView newView;
            newView.buffer = 0;
            newView.byteOffset = alignedOffset;
            newView.byteLength = srcView.byteLength;
            newView.byteStride = srcView.byteStride;
            newView.target = srcView.target;

            out.bufferViews.push_back(newView);
            return static_cast<int>(out.bufferViews.size()) - 1;
        };

    auto remapAccessor = [&](int srcAccessorIndex) -> int
        {
            const tinygltf::Accessor& srcAcc = srcModel.accessors[srcAccessorIndex];

            tinygltf::Accessor newAcc = srcAcc;
            if (srcAcc.bufferView >= 0)
                newAcc.bufferView = appendToBuffer(srcModel, srcAcc.bufferView);
            else
                newAcc.bufferView = -1;

            out.accessors.push_back(newAcc);
            return static_cast<int>(out.accessors.size()) - 1;
        };

    tinygltf::Primitive newPrim;
    newPrim.mode = srcPrim.mode;

    for (const auto& attr : srcPrim.attributes)
        newPrim.attributes[attr.first] = remapAccessor(attr.second);

    if (srcPrim.indices >= 0)
        newPrim.indices = remapAccessor(srcPrim.indices);

    if (srcPrim.material >= 0 && srcPrim.material < static_cast<int>(srcModel.materials.size()))
    {
        const tinygltf::Material& srcMat = srcModel.materials[srcPrim.material];
        tinygltf::Material newMat = srcMat;

        std::unordered_map<int, int> textureRemap;
        auto remapTexture = [&](int srcTexIndex) -> int
            {
                if (srcTexIndex < 0) return -1;
                auto it = textureRemap.find(srcTexIndex);
                if (it != textureRemap.end()) return it->second;

                const tinygltf::Texture& srcTex = srcModel.textures[srcTexIndex];
                tinygltf::Texture newTex = srcTex;

                if (srcTex.source >= 0 && srcTex.source < static_cast<int>(srcModel.images.size()))
                {
                    const tinygltf::Image& srcImg = srcModel.images[srcTex.source];
                    tinygltf::Image newImg = srcImg;
                    if (srcImg.bufferView >= 0)
                        newImg.bufferView = appendToBuffer(srcModel, srcImg.bufferView);
                    out.images.push_back(newImg);
                    newTex.source = static_cast<int>(out.images.size()) - 1;
                }

                if (srcTex.sampler >= 0 && srcTex.sampler < static_cast<int>(srcModel.samplers.size()))
                {
                    out.samplers.push_back(srcModel.samplers[srcTex.sampler]);
                    newTex.sampler = static_cast<int>(out.samplers.size()) - 1;
                }

                out.textures.push_back(newTex);
                int newIndex = static_cast<int>(out.textures.size()) - 1;
                textureRemap[srcTexIndex] = newIndex;
                return newIndex;
            };

        newMat.pbrMetallicRoughness.baseColorTexture.index =
            remapTexture(srcMat.pbrMetallicRoughness.baseColorTexture.index);
        newMat.pbrMetallicRoughness.metallicRoughnessTexture.index =
            remapTexture(srcMat.pbrMetallicRoughness.metallicRoughnessTexture.index);
        newMat.normalTexture.index = remapTexture(srcMat.normalTexture.index);
        newMat.occlusionTexture.index = remapTexture(srcMat.occlusionTexture.index);
        newMat.emissiveTexture.index = remapTexture(srcMat.emissiveTexture.index);

        out.materials.push_back(newMat);
        newPrim.material = 0;
    }
    else
    {
        newPrim.material = -1;
    }

    tinygltf::Mesh newMesh;
    newMesh.name = srcMesh.name;
    newMesh.primitives.push_back(newPrim);
    out.meshes.push_back(newMesh);

    tinygltf::Node newNode;
    newNode.mesh = 0;
    newNode.name = srcMesh.name;

    {
        const glm::mat4 identity(1.0f);
        if (worldMatrix != identity)
        {
            newNode.matrix.resize(16);
            const float* m = glm::value_ptr(worldMatrix);
            for (int i = 0; i < 16; ++i)
                newNode.matrix[i] = static_cast<double>(m[i]);
        }
    }

    out.nodes.push_back(newNode);

    tinygltf::Scene newScene;
    newScene.nodes.push_back(0);
    out.scenes.push_back(newScene);
    out.defaultScene = 0;

    out.buffers.push_back(std::move(outBuffer));

    std::filesystem::path outDir = std::filesystem::path(outGlbPath).parent_path();
    if (!outDir.empty())
        std::filesystem::create_directories(outDir);

    tinygltf::TinyGLTF writer;
    bool ok = writer.WriteGltfSceneToFile(&out, outGlbPath, true, true, false, true);

    if (!ok)
    {
        RUYA_LOG_ERROR("[RyAssetManager] Failed to write primitive GLB: {}", outGlbPath.c_str());
        return false;
    }

    return true;
}

void ruya::RyAssetManager::ImportGLTF(const std::string& path, const std::string& importDirectory)
{
    size_t slashPos = path.find_last_of("/\\");
    std::string filename = (slashPos != std::string::npos) ? path.substr(slashPos + 1) : path;
    size_t dotPos = filename.rfind('.');
    std::string modelName = (dotPos != std::string::npos) ? filename.substr(0, dotPos) : filename;

    tinygltf::Model model;
    if (!LoadGLBModel(path, model))
        return;

    std::vector<GLTFPrimitiveRef> primitives = EnumerateGLTFPrimitives(model);
    if (primitives.empty())
    {
        RUYA_LOG_WARN("[RyAssetManager] GLB has no importable primitives: {}", path.c_str());
        return;
    }

    std::filesystem::path destDir = std::filesystem::path(ASSETS_DIR) / importDirectory;
    std::error_code ec;
    std::filesystem::create_directories(destDir, ec);
    if (ec)
    {
        RUYA_LOG_ERROR("[RyAssetManager] Failed to create destination directory: {}", ec.message().c_str());
        return;
    }

    std::unordered_map<std::string, int> nameUsage;

    auto makeUniqueName = [&](std::string base) -> std::string
        {
            int& count = nameUsage[base];
            std::string result = (count == 0) ? base : base + "_" + std::to_string(count);
            ++count;
            return result;
        };

    for (const GLTFPrimitiveRef& ref : primitives)
    {
        const tinygltf::Mesh& gltfMesh = model.meshes[ref.meshIndex];

        std::string nodeName;
        for (const tinygltf::Node& node : model.nodes)
        {
            if (node.mesh == ref.meshIndex) { nodeName = node.name; break; }
        }

        std::string meshLabel = !nodeName.empty() ? nodeName
            : !gltfMesh.name.empty() ? gltfMesh.name
            : "mesh" + std::to_string(ref.meshIndex);

        if (gltfMesh.primitives.size() > 1)
            meshLabel += "_part" + std::to_string(ref.primIndex);

        std::string assetName = makeUniqueName(modelName + "_" + meshLabel);
        std::string glbFileName = assetName + ".glb";

        std::filesystem::path destGlbPath = destDir / glbFileName;
        if (std::filesystem::exists(destGlbPath))
        {
            RUYA_LOG_ERROR("[RyAssetManager] Cannot import mesh: '{}' already exists at destination.",
                glbFileName.c_str());
            continue;
        }

        if (!WritePrimitiveToGLB(model, ref.meshIndex, ref.primIndex, destGlbPath.string()))
        {
            RUYA_LOG_ERROR("[RyAssetManager] Failed to extract mesh '{}' from {}",
                assetName.c_str(), filename.c_str());
            continue;
        }

        UUID uuid = GenerateRandomUUID();
        auto asset = std::make_unique<RyAsset>();
        asset->uuid = uuid;
        asset->assetName = glbFileName;
        asset->ryAssetType = RyAssetType::GLTF;
        asset->directory = importDirectory;
        asset->sourcePath = path;

        assetRegistry[uuid] = std::move(asset);
        SerializeAsset(uuid);

        RUYA_LOG_INFO("[RyAssetManager] Imported mesh '{}' from {}",
            assetName.c_str(), filename.c_str());
    }
}

std::filesystem::path ruya::RyAssetManager::ResolveResourcePath(const RyAsset& asset) const
{
    return std::filesystem::path(ASSETS_DIR) / asset.directory / asset.assetName;
}

std::optional<ruya::RyMeshData> ruya::RyAssetManager::LoadGLTF(UUID assetUUID)
{
    auto registryIt = assetRegistry.find(assetUUID);
    if (registryIt == assetRegistry.end())
    {
        RUYA_LOG_ERROR("[RyAssetManager] LoadGLTF: asset UUID not found in registry.");
        return std::nullopt;
    }

    if (registryIt->second->ryAssetType != RyAssetType::GLTF)
    {
        RUYA_LOG_ERROR("[RyAssetManager] LoadGLTF: asset is not a GLTF.");
        return std::nullopt;
    }

    const RyAsset* asset = registryIt->second.get();
    std::filesystem::path gltfPath = ResolveResourcePath(*asset);

    if (!std::filesystem::exists(gltfPath))
    {
        RUYA_LOG_ERROR("[RyAssetManager] LoadGLTF: GLTF resource missing: {}", gltfPath.string().c_str());
        return std::nullopt;
    }

    tinygltf::Model model;
    if (!LoadGLBModel(gltfPath.string(), model))
        return std::nullopt;

    std::vector<GLTFPrimitiveRef> primitives = EnumerateGLTFPrimitives(model);
    if (primitives.empty())
    {
        RUYA_LOG_ERROR("[RyAssetManager] LoadGLTF: no primitives in {}", gltfPath.string().c_str());
        return std::nullopt;
    }

    const GLTFPrimitiveRef& ref = primitives.front();

    RyMeshData data;
    if (!DecodeGLTFPrimitive(model, ref.meshIndex, ref.primIndex, &data, nullptr))
    {
        RUYA_LOG_ERROR("[RyAssetManager] LoadGLTF: failed to decode primitive for {}", gltfPath.string().c_str());
        return std::nullopt;
    }

    return data;
}

std::optional<ruya::RyMeshInfo> ruya::RyAssetManager::GetGLTFInfo(UUID assetUUID)
{
    auto registryIt = assetRegistry.find(assetUUID);
    if (registryIt == assetRegistry.end())
        return std::nullopt;

    if (registryIt->second->ryAssetType != RyAssetType::GLTF)
        return std::nullopt;

    const RyAsset* asset = registryIt->second.get();
    std::filesystem::path gltfPath = ResolveResourcePath(*asset);

    if (!std::filesystem::exists(gltfPath))
        return std::nullopt;

    tinygltf::Model model;
    if (!LoadGLBModel(gltfPath.string(), model))
        return std::nullopt;

    std::vector<GLTFPrimitiveRef> primitives = EnumerateGLTFPrimitives(model);
    if (primitives.empty())
        return std::nullopt;

    const GLTFPrimitiveRef& ref = primitives.front();

    RyMeshInfo info;
    if (!DecodeGLTFPrimitive(model, ref.meshIndex, ref.primIndex, nullptr, &info))
        return std::nullopt;

    return info;
}


std::optional<ruya::RyMaterial> ruya::RyAssetManager::LoadRyMaterial(UUID assetUUID)
{
    auto registryIt = assetRegistry.find(assetUUID);
    if (registryIt == assetRegistry.end())
        return std::nullopt;

    if (registryIt->second->ryAssetType != RyAssetType::RyMaterial)
        return std::nullopt;

    std::filesystem::path matPath = std::filesystem::path(ASSETS_DIR) / registryIt->second->directory / registryIt->second->assetName;

    if (!std::filesystem::exists(matPath))
        return RyMaterial{};

    std::ifstream in(matPath);
    if (!in)
    {
        RUYA_LOG_ERROR("[RyAssetManager] Failed to open material file: {}", matPath.string().c_str());
        return std::nullopt;
    }

    try
    {
        nlohmann::json j;
        in >> j;
        return j.get<RyMaterial>();
    }
    catch (const std::exception& e)
    {
        RUYA_LOG_ERROR("[RyAssetManager] Failed to parse material: {}", e.what());
        return std::nullopt;
    }
}

bool ruya::RyAssetManager::SaveMaterial(UUID assetUUID, const RyMaterial& material)
{
    auto registryIt = assetRegistry.find(assetUUID);
    if (registryIt == assetRegistry.end())
        return false;

    if (registryIt->second->ryAssetType != RyAssetType::RyMaterial)
        return false;

    std::filesystem::path matPath = std::filesystem::path(ASSETS_DIR) / registryIt->second->directory / registryIt->second->assetName;

    std::filesystem::create_directories(matPath.parent_path());

    std::ofstream out(matPath);
    if (!out) return false;

    try
    {
        nlohmann::json j = material;
        out << j.dump(4);
    }
    catch (const std::exception& e)
    {
        RUYA_LOG_ERROR("[RyAssetManager] Failed to save material: {}", e.what());
        return false;
    }

    return true;
}

bool ruya::RyAssetManager::SaveScene(UUID assetUUID, Scene* scene)
{
    if (!scene) return false;

    auto registryIt = assetRegistry.find(assetUUID);
    if (registryIt == assetRegistry.end() || registryIt->second->ryAssetType != RyAssetType::RyScene)
    {
        RUYA_LOG_ERROR("[RyAssetManager] SaveScene: Invalid Asset UUID or not a RyScene.");
        return false;
    }

    std::filesystem::path scenePath = std::filesystem::path(ASSETS_DIR) / registryIt->second->directory / registryIt->second->assetName;
    std::filesystem::create_directories(scenePath.parent_path());

    nlohmann::json root;
    root["scene_name"] = scene->GetName();
    root["entities"] = nlohmann::json::array();
    root["components"] = nlohmann::json::object();

    auto& registry = scene->GetENTTRegistry();

    using RegistryType = entt::basic_registry<ruya::EntityID>;

    JsonOutputArchive entArchive(root["entities"]);
    entt::basic_snapshot<RegistryType>{ registry }.get<ruya::EntityID>(entArchive);

    auto save_component = [&]<typename T>(const char* name) {
        root["components"][name] = nlohmann::json::array();
        JsonOutputArchive compArchive(root["components"][name]);
        entt::basic_snapshot<RegistryType>{ registry }.get<T>(compArchive);
    };

    save_component.template operator() < IdComponent > ("IdComponent");
    save_component.template operator() < TransformComponent > ("TransformComponent");
    save_component.template operator() < RelationshipComponent > ("RelationshipComponent");
    save_component.template operator() < PhysicsComponent > ("PhysicsComponent");
    save_component.template operator() < RenderComponent > ("RenderComponent");

    std::ofstream outStream(scenePath);
    if (!outStream.is_open()) return false;

    outStream << root.dump(4);
    RUYA_LOG_INFO("[RyAssetManager] Saved Scene: {}", scene->GetName().c_str());

    return true;
}

std::unique_ptr<ruya::Scene> ruya::RyAssetManager::LoadScene(UUID assetUUID)
{
    auto registryIt = assetRegistry.find(assetUUID);
    if (registryIt == assetRegistry.end() || registryIt->second->ryAssetType != RyAssetType::RyScene)
        return nullptr;

    std::filesystem::path scenePath = std::filesystem::path(ASSETS_DIR) / registryIt->second->directory / registryIt->second->assetName;
    std::ifstream inStream(scenePath);
    if (!inStream.is_open()) return nullptr;

    nlohmann::json root;
    inStream >> root;

    std::string sceneName = root.value("scene_name", "Untitled Scene");
    auto newScene = std::make_unique<Scene>(sceneName);
    auto& registry = newScene->GetENTTRegistry();

    using RegistryType = entt::basic_registry<ruya::EntityID>;
    entt::basic_continuous_loader<RegistryType> loader{ registry };

    if (root.contains("entities"))
    {
        JsonInputArchive entArchive(root["entities"]);
        loader.get<ruya::EntityID>(entArchive);
    }

    auto load_component = [&]<typename T>(const char* name) {
        if (root.contains("components") && root["components"].contains(name))
        {
            JsonInputArchive compArchive(root["components"][name]);

            auto archive_wrapper = [&compArchive, &loader](auto& value) {
                compArchive(value);

                using DataType = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<DataType, RelationshipComponent>) {
                    if (value.parentEntity != static_cast<ruya::EntityID>(entt::null))
                        value.parentEntity = loader.map(value.parentEntity);

                    if (value.firstChildEntity != static_cast<ruya::EntityID>(entt::null))
                        value.firstChildEntity = loader.map(value.firstChildEntity);

                    if (value.nextEntity != static_cast<ruya::EntityID>(entt::null))
                        value.nextEntity = loader.map(value.nextEntity);
                }
                else if constexpr (std::is_same_v<DataType, IdComponent>) {
                    if (value.enttID != static_cast<ruya::EntityID>(entt::null))
                        value.enttID = loader.map(value.enttID);
                }
                };

            loader.get<T>(archive_wrapper);
        }
    };

    load_component.template operator() < IdComponent > ("IdComponent");
    load_component.template operator() < TransformComponent > ("TransformComponent");
    load_component.template operator() < RelationshipComponent > ("RelationshipComponent");
    load_component.template operator() < PhysicsComponent > ("PhysicsComponent");
    load_component.template operator() < RenderComponent > ("RenderComponent");

    RUYA_LOG_INFO("[RyAssetManager] Loaded Scene: {}", sceneName.c_str());
    return newScene;
}

bool ruya::RyAssetManager::SerializeAsset(UUID uuid)
{
    auto it = assetRegistry.find(uuid);
    if (it == assetRegistry.end())
        return false;

    RyAsset* asset = it->second.get();

    std::string dir = asset->directory;
    std::replace(dir.begin(), dir.end(), '\\', '/');

    std::string cleanName = std::filesystem::path(asset->assetName).stem().string();

    std::filesystem::path fullPath = std::filesystem::path(ASSETS_DIR) / dir / (cleanName + ".ryasset");

    std::filesystem::create_directories(fullPath.parent_path());

    std::ofstream out(fullPath);
    if (!out) return false;

    try
    {
        nlohmann::json j = *asset;
        out << j.dump(4);
    }
    catch (const std::exception& e) {
        RUYA_LOG_ERROR("[RyAssetManager] Serialization failed: {}", e.what());
        return false;
    }
    return true;
}

ruya::RyAsset ruya::RyAssetManager::DeserializeAsset(const std::string& path)
{
    std::filesystem::path fullPath = std::filesystem::path(ASSETS_DIR) / path;
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
        nlohmann::json j;
        in >> j;
        asset = j.get<RyAsset>();
    }
    catch (const std::exception& e)
    {
        RUYA_LOG_ERROR("[RyAssetManager] Deserialization failed: {}", e.what());
        return RyAsset{};
    }

    return asset;
}