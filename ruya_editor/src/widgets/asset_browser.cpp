#include "asset_browser.h"

#include <filesystem>
#include <algorithm>

#define NOMINMAX
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

#include <imgui/imgui_impl_vulkan.h>

#include <engine.h>
#include <core/log.h>
#include <core/hash_code_generator.h>
#include <app_framework/render_component.h>
#include <app_framework/transform_component.h>
#include <app_framework/id_component.h>
#include <app_framework/app.h>
#include <assets_system/ry_asset_manager.h>

#include "editor_widgets.h"
#include "../editor_style.h"
#include "scene_outliner.h"
#include "material_editor.h"

extern ImFont* g_FontLarge;

#ifdef _WIN32
std::string OpenNativeFileDialog()
{
    OPENFILENAMEA ofn;
    CHAR szFile[260] = { 0 };

    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Supported Files\0*.png;*.glb\0GLTF Models (*.glb)\0*.glb\0Images (*.png)\0*.png\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE)
    {
        return std::string(ofn.lpstrFile);
    }
    return "";
}
#endif

namespace editor
{
    static bool s_OpenRenameAssetPopup = false;
    static std::string s_RenameAssetBuffer = "";
    static ruya::UUID s_AssetToRenameUUID = ruya::UUID(0);
}

std::string editor::AssetBrowser::GetRelativeDirectory() const
{
    std::filesystem::path rel = std::filesystem::relative(currentPath, ASSETS_DIR);
    if (rel == ".")
        return "";
    return rel.string();
}

bool editor::AssetBrowser::AssetNameExistsInCurrentDir(const std::string& stemName) const
{
    for (const auto& av : assetView)
    {
        if (av.type == RyAssetViewType::Folder)
            continue;
        if (av.name == stemName)
            return true;
    }
    return false;
}

editor::AssetBrowser::AssetBrowser()
{
    currentPath = ASSETS_DIR;
    RefreshAssets();
}

editor::AssetBrowser::~AssetBrowser()
{
}

ImVec4 editor::AssetBrowser::GetAssetTypeColor(RyAssetViewType type) const
{
    switch (type)
    {
    case RyAssetViewType::Folder:   return kColorFolder;
    case RyAssetViewType::GLTF:   return kColorMesh;
    case RyAssetViewType::Texture2D:      return kColorTexture;
    case RyAssetViewType::Material: return kColorMaterial;
    case RyAssetViewType::Scene:    return kColorScene;
    default:                        return kColorInvalid;
    }
}

const char* editor::AssetBrowser::GetAssetTypeIcon(RyAssetViewType type) const
{
    switch (type)
    {
    case RyAssetViewType::Folder: return IconStr(u8"\uE24A");
    case RyAssetViewType::GLTF: return IconStr(u8"\uEC7C");
    case RyAssetViewType::Texture2D: return IconStr(u8"\uE2CA");
    case RyAssetViewType::Material: return IconStr(u8"\uE18E");
    case RyAssetViewType::Scene: return IconStr(u8"\uE7AE");
    default: return IconStr(u8"\uE650");
    }
}

const char* editor::AssetBrowser::GetAssetTypeName(RyAssetViewType type) const
{
    switch (type)
    {
    case RyAssetViewType::Folder: return "Folder";
    case RyAssetViewType::GLTF: return "Mesh";
    case RyAssetViewType::Texture2D: return "Texture";
    case RyAssetViewType::Material: return "Material";
    case RyAssetViewType::Scene: return "Scene";
    default: return "Unknown";
    }
}

const char* editor::AssetBrowser::GetTextureFormatName(RyTextureFormatView format) const
{
    switch (format)
    {
    case RyTextureFormatView::SRGB:      return "sRGB";
    case RyTextureFormatView::Linear:    return "Linear";
    case RyTextureFormatView::NormalMap: return "Normal Map";
    default:                             return "Unknown";
    }
}

editor::RyTextureFormatView editor::AssetBrowser::DetectTextureFormat(ruya::UUID assetUUID) const
{
    ktxTexture2* tex = engine->GetAssetManager()->LoadKTX2Header(assetUUID);
    if (!tex)
        return RyTextureFormatView::Unknown;

    RyTextureFormatView result = RyTextureFormatView::Unknown;

    switch (tex->vkFormat)
    {
    case VK_FORMAT_BC7_SRGB_BLOCK:
        result = RyTextureFormatView::SRGB;
        break;
    case VK_FORMAT_BC7_UNORM_BLOCK:
        result = RyTextureFormatView::Linear;
        break;
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
        result = RyTextureFormatView::NormalMap;
        break;
    default:
    {
        const VkFormat fmt = static_cast<VkFormat>(tex->vkFormat);
        const bool isSRGB =
            fmt == VK_FORMAT_R8G8B8A8_SRGB ||
            fmt == VK_FORMAT_R8G8B8_SRGB ||
            fmt == VK_FORMAT_B8G8R8A8_SRGB ||
            fmt == VK_FORMAT_BC1_RGB_SRGB_BLOCK ||
            fmt == VK_FORMAT_BC1_RGBA_SRGB_BLOCK ||
            fmt == VK_FORMAT_BC2_SRGB_BLOCK ||
            fmt == VK_FORMAT_BC3_SRGB_BLOCK;
        result = isSRGB ? RyTextureFormatView::SRGB : RyTextureFormatView::Linear;
    }
    break;
    }

    ktxTexture_Destroy(ktxTexture(tex));
    return result;
}

void editor::AssetBrowser::Draw()
{
    const char* title = IconLabel(u8"\uE0F4", "Asset Browser");

    ImGui::Begin(title, nullptr, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::Button(IconStr(u8"\uE2C2")))
        {
            currentPath = std::filesystem::path(ASSETS_DIR);
            RefreshAssets();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Home");

        if (ImGui::Button(IconStr(u8"\uE05A")))
        {
            std::filesystem::path rootAssets = std::filesystem::path(ASSETS_DIR);
            if (currentPath != rootAssets)
            {
                currentPath = currentPath.parent_path();
                RefreshAssets();
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Back");

        if (ImGui::Button(IconStr(u8"\uE036")))
        {
            RefreshAssets();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh");

        if (ImGui::Button(IconStr(u8"\uE248")))
        {
            engine->GetAssetManager()->SaveAllAssets();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save All Assets");

        float searchBarWidth = 180.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - searchBarWidth - 16.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::SetNextItemWidth(searchBarWidth);
        ImGui::InputTextWithHint("##AssetSearch", IconLabel(u8"\uE238", "Search..."),
            assetSearchFilter, sizeof(assetSearchFilter));
        ImGui::PopStyleVar();

        ImGui::EndMenuBar();
    }

    {
        std::filesystem::path rootAssets = std::filesystem::path(ASSETS_DIR);
        std::filesystem::path relPath = std::filesystem::relative(currentPath, rootAssets);

        ImGui::PushStyleColor(ImGuiCol_Text, kColorFolder);
        ImGui::TextUnformatted(IconStr(u8"\uE2C2"));
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 2.0f);

        if (ImGui::SmallButton("Assets"))
        {
            currentPath = rootAssets;
            RefreshAssets();
        }

        if (relPath != "." && !relPath.empty())
        {
            std::filesystem::path buildPath = rootAssets;
            for (auto& part : relPath)
            {
                buildPath /= part;
                ImGui::SameLine(0, 2.0f);
                ImGui::TextDisabled("/");
                ImGui::SameLine(0, 2.0f);
                if (ImGui::SmallButton(part.string().c_str()))
                {
                    currentPath = buildPath;
                    RefreshAssets();
                }
            }
        }
        ImGui::Spacing();
    }

    if (openCreateScenePopup)
    {
        ImGui::OpenPopup("Create Scene");
        openCreateScenePopup = false;
    }

    if (ImGui::BeginPopupModal("Create Scene", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Scene Name:");
        char buffer[128] = {};
        strncpy(buffer, sceneNameBuffer.c_str(), sizeof(buffer) - 1);
        if (ImGui::InputText("##SceneNameInput", buffer, IM_ARRAYSIZE(buffer)))
            sceneNameBuffer = buffer;

        ImGui::Spacing();
        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            if (!sceneNameBuffer.empty())
            {
                if (AssetNameExistsInCurrentDir(sceneNameBuffer))
                {
                    duplicateWarningMessage = "An asset named \"" + sceneNameBuffer + "\" already exists in this directory.";
                    openDuplicateWarningPopup = true;
                }
                else
                {
                    createdSceneName = sceneNameBuffer;

                    ruya::RyID newSceneId = engine->GetApp()->CreateScene(createdSceneName);

                    std::string relDir = GetRelativeDirectory();
                    ruya::UUID newAssetUUID = engine->GetAssetManager()->CreateAsset(createdSceneName, ruya::RyAssetType::RyScene, relDir);

                    engine->GetApp()->SetSceneAssetUUID(newSceneId, newAssetUUID);
                    ruya::Scene* newlyCreatedScene = engine->GetApp()->GetScene(newSceneId);
                    engine->GetAssetManager()->SaveScene(newAssetUUID, newlyCreatedScene);

                    RefreshAssets();
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    if (ImGui::BeginTable("AssetBrowserTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::BeginChild("FolderPanel", ImVec2(0, 0), false);
        DrawFolderTree(std::filesystem::path(ASSETS_DIR), 0);
        ImGui::EndChild();

        ImGui::TableNextColumn();
        ImGui::BeginChild("FilePanel", ImVec2(0, 0), false);

        DrawAssetGrid();

        if (ImGui::BeginPopupContextWindow("Asset Browser Context Menu",
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverExistingPopup))
        {
            if (ImGui::MenuItem(IconLabel(u8"\uE20C", "Import Asset")))
            {
#ifdef _WIN32
                std::string selectedFile = OpenNativeFileDialog();
                if (!selectedFile.empty())
                {
                    std::filesystem::path filePath(selectedFile);
                    std::string relDir = GetRelativeDirectory();
                    std::string stemName = filePath.stem().string();

                    if (AssetNameExistsInCurrentDir(stemName))
                    {
                        duplicateWarningMessage = "An asset named \"" + stemName + "\" already exists in this directory. Import cancelled.";
                        openDuplicateWarningPopup = true;
                    }
                    else if (filePath.extension() == ".glb")
                    {
                        engine->GetAssetManager()->ImportGLTF(filePath.string(), relDir);
                        RefreshAssets();
                    }
                    else if (filePath.extension() == ".png")
                    {
                        engine->GetAssetManager()->ImportPNG(filePath.string(), relDir, ruya::TextureFormat::SRGB);
                        RefreshAssets();
                    }
                }
#else

#endif
            }

            if (ImGui::MenuItem(IconLabel(u8"\uE258", "Create Folder")))
            {
                newFolderName.clear();
                openCreateFolderPopup = true;
            }

            if (ImGui::MenuItem(IconLabel(u8"\uE7AE", "Create Scene")))
            {
                openCreateScenePopup = true;
                sceneNameBuffer = "";
            }

            if (ImGui::MenuItem(IconLabel(u8"\uE18E", "Create Material")))
            {
                std::string baseName = "new_material";
                std::string candidateName = baseName;

                int counter = 1;
                while (AssetNameExistsInCurrentDir(candidateName))
                {
                    candidateName = baseName + "_" + std::to_string(counter);
                    counter++;
                }

                std::string relDir = GetRelativeDirectory();
                ruya::UUID materialId = engine->GetAssetManager()->CreateAsset(candidateName, ruya::RyAssetType::RyMaterial, relDir);

                ruya::RyMaterial newMaterial{};

                ruya::RyAsset* defaultMaterialAsset = engine->GetAssetManager()->GetAssetByPath("ruya_files/materials/default_material.ryasset");
                if (defaultMaterialAsset)
                {
                    if (auto defaultMaterial = engine->GetAssetManager()->LoadRyMaterial(defaultMaterialAsset->uuid))
                    {
                        newMaterial.albedoUUID = defaultMaterial->albedoUUID;
                        newMaterial.metallicRoughnessUUID = defaultMaterial->metallicRoughnessUUID;
                        newMaterial.normalUUID = defaultMaterial->normalUUID;
                    }
                }

                engine->GetAssetManager()->SaveMaterial(materialId, newMaterial);
                RefreshAssets();
            }

            ImGui::EndPopup();
        }

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (!ImGui::IsAnyItemHovered())
            {
                selectedAssetIndex = -1;
                selectedAssetIndices.clear();
                lastClickedAssetIndex = -1;
            }
        }

        if (s_OpenRenameAssetPopup)
        {
            ImGui::OpenPopup("Rename Asset");
            s_OpenRenameAssetPopup = false;
        }

        if (ImGui::BeginPopupModal("Rename Asset", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("New asset name:");
            char buffer[128] = {};
            strncpy(buffer, s_RenameAssetBuffer.c_str(), sizeof(buffer) - 1);
            if (ImGui::InputText("##RenameAssetInput", buffer, IM_ARRAYSIZE(buffer)))
                s_RenameAssetBuffer = buffer;

            ImGui::Spacing();
            if (ImGui::Button("Rename", ImVec2(120, 0)))
            {
                if (!s_RenameAssetBuffer.empty())
                {
                    engine->GetAssetManager()->RenameAsset(s_AssetToRenameUUID, s_RenameAssetBuffer);
                    RefreshAssets();
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (openDeleteAssetPopup)
        {
            ImGui::OpenPopup("Delete Asset?");
            openDeleteAssetPopup = false;
        }

        if (ImGui::BeginPopupModal("Delete Asset?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Are you sure you want to delete this asset?");
            ImGui::Spacing();
            ImGui::TextDisabled("%s", assetToDeleteName.c_str());
            ImGui::Spacing();

            if (ImGui::Button("Yes", ImVec2(120, 0)))
            {
                ruya::RyAsset* asset = engine->GetAssetManager()->TryGetAsset(assetToDeleteUUID);
                if (asset)
                {
                    std::string cleanName = std::filesystem::path(asset->assetName).stem().string();
                    std::filesystem::path ryassetPath = std::filesystem::path(ASSETS_DIR) / asset->directory / (cleanName + ".ryasset");
                    if (std::filesystem::exists(ryassetPath))
                    {
                        try { std::filesystem::remove(ryassetPath); }
                        catch (...) {}
                    }

                    bool hasExternalResource = (asset->ryAssetType != ruya::RyAssetType::RyMaterial && asset->ryAssetType != ruya::RyAssetType::RyScene);
                    if (hasExternalResource)
                    {
                        std::filesystem::path resourcePath = std::filesystem::path(ASSETS_DIR) / asset->directory / asset->assetName;
                        if (std::filesystem::exists(resourcePath))
                        {
                            try { std::filesystem::remove(resourcePath); }
                            catch (...) {}
                        }
                    }

                    engine->GetAssetManager()->DestroyAsset(assetToDeleteUUID);
                }
                RefreshAssets();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (openDuplicateWarningPopup)
        {
            ImGui::OpenPopup("Duplicate Asset");
            openDuplicateWarningPopup = false;
        }

        if (ImGui::BeginPopupModal("Duplicate Asset", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("%s", duplicateWarningMessage.c_str());
            ImGui::Spacing();
            if (ImGui::Button("OK", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::EndChild();

        ImGui::EndTable();
    }

    if (openCreateFolderPopup)
    {
        ImGui::OpenPopup("Create Folder");
        openCreateFolderPopup = false;
    }

    if (ImGui::BeginPopupModal("Create Folder", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Folder Name:");
        char buffer[128] = {};
        strncpy(buffer, newFolderName.c_str(), sizeof(buffer) - 1);
        if (ImGui::InputText("##NewFolderInput", buffer, IM_ARRAYSIZE(buffer)))
            newFolderName = buffer;

        ImGui::Spacing();
        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            if (!newFolderName.empty())
            {
                std::filesystem::path newPath = currentPath / newFolderName;
                if (std::filesystem::exists(newPath))
                {
                    duplicateWarningMessage = "A folder named \"" + newFolderName + "\" already exists.";
                    openDuplicateWarningPopup = true;
                }
                else
                {
                    std::filesystem::create_directory(newPath);
                    RefreshAssets();
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (openDeleteFolderPopup)
    {
        ImGui::OpenPopup("Delete Folder?");
        openDeleteFolderPopup = false;
    }

    if (ImGui::BeginPopupModal("Delete Folder?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Are you sure you want to delete this folder?");
        ImGui::Spacing();
        ImGui::TextDisabled("%s", folderToDelete.string().c_str());
        ImGui::Spacing();

        if (ImGui::Button("Yes", ImVec2(120, 0)))
        {
            try { std::filesystem::remove_all(folderToDelete); RefreshAssets(); }
            catch (...) {}
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (openRenamePopup)
    {
        ImGui::OpenPopup("Rename Folder");
        openRenamePopup = false;
    }

    if (ImGui::BeginPopupModal("Rename Folder", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("New name:");
        char buffer[128] = {};
        strncpy(buffer, renameBuffer.c_str(), sizeof(buffer) - 1);
        if (ImGui::InputText("##RenameFolderInput", buffer, IM_ARRAYSIZE(buffer)))
            renameBuffer = buffer;

        ImGui::Spacing();
        if (ImGui::Button("Rename", ImVec2(120, 0)))
        {
            if (!renameBuffer.empty())
            {
                try {
                    std::filesystem::rename(folderToRename, folderToRename.parent_path() / renameBuffer);
                    RefreshAssets();
                }
                catch (...) {}
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    {
        int assetCount = 0;
        for (const auto& av : assetView)
        {
            if (av.type != RyAssetViewType::Folder)
                assetCount++;
        }

        int folderCount = 0;
        for (const auto& av : assetView)
        {
            if (av.type == RyAssetViewType::Folder)
                folderCount++;
        }

        ImGui::Separator();
        ImGui::TextDisabled("%d asset(s), %d folder(s)", assetCount, folderCount);
    }

    ImGui::End();
}

void editor::AssetBrowser::DrawAssetGrid()
{
    const float iconAreaSize = thumbnailSize;
    const float textRowH = ImGui::GetTextLineHeight();
    const float cardPad = 6.0f;
    const float cardW = iconAreaSize + cardPad * 2.0f;
    const float cardH = cardPad + iconAreaSize + 4.0f + textRowH + cardPad;
    const float cellStride = cardW + ImGui::GetStyle().ItemSpacing.x;

    float panelWidth = ImGui::GetContentRegionAvail().x;
    int   columns = std::max(1, (int)(panelWidth / cellStride));

    int fileIndex = 0;
    int columnIndex = 0;

    for (int i = 0; i < (int)assetView.size(); i++)
    {
        const auto& asset = assetView[i];

        if (assetSearchFilter[0] != '\0')
        {
            std::string lowerName = asset.name;
            std::string lowerFilter = assetSearchFilter;
            for (auto& c : lowerName)   c = (char)tolower(c);
            for (auto& c : lowerFilter) c = (char)tolower(c);
            if (lowerName.find(lowerFilter) == std::string::npos)
                continue;
        }

        ImGui::PushID(fileIndex);

        ImVec4      typeColor = GetAssetTypeColor(asset.type);
        const char* typeIcon = GetAssetTypeIcon(asset.type);

        ImVec2 cardPos = ImGui::GetCursorScreenPos();
        bool   isSelected = (selectedAssetIndices.count(i) > 0) || (selectedAssetIndex == i);

        ImGui::BeginGroup();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2      cardMin = cardPos;
        ImVec2      cardMax = ImVec2(cardPos.x + cardW, cardPos.y + cardH);

        ImU32 bgCol = isSelected
            ? ImGui::ColorConvertFloat4ToU32(ImVec4(typeColor.x * 0.3f, typeColor.y * 0.3f, typeColor.z * 0.3f, 0.50f))
            : ImGui::ColorConvertFloat4ToU32(ImVec4(0.12f, 0.13f, 0.15f, 0.80f));
        dl->AddRectFilled(cardMin, cardMax, bgCol, 4.0f);

        if (isSelected)
            dl->AddRect(cardMin, cardMax, ImGui::ColorConvertFloat4ToU32(typeColor), 4.0f, 0, 1.5f);

        if (g_FontLarge) ImGui::PushFont(g_FontLarge);

        ImVec2 iconSize = ImGui::CalcTextSize(typeIcon);
        float iconAreaX = cardPos.x + cardPad;
        float iconAreaY = cardPos.y + cardPad;
        float iconX = iconAreaX + (iconAreaSize - iconSize.x) * 0.5f;
        float iconY = iconAreaY + (iconAreaSize - iconSize.y) * 0.5f;
        dl->AddText(ImVec2(iconX, iconY), ImGui::ColorConvertFloat4ToU32(typeColor), typeIcon);

        if (g_FontLarge) ImGui::PopFont();

        float maxTextWidth = cardW - (cardPad * 2.0f);
        std::string displayName = asset.name;

        if (!isSelected)
        {
            if (ImGui::CalcTextSize(displayName.c_str()).x > maxTextWidth)
            {
                float dotWidth = ImGui::CalcTextSize("...").x;

                while (displayName.size() > 0 &&
                    (ImGui::CalcTextSize(displayName.c_str()).x + dotWidth) > maxTextWidth)
                {
                    displayName.pop_back();
                }

                displayName += "...";
            }
        }

        float  nameTextW = ImGui::CalcTextSize(displayName.c_str()).x;
        ImVec2 namePos = ImVec2(
            cardPos.x + (cardW - nameTextW) * 0.5f,
            cardPos.y + cardPad + iconAreaSize + 4.0f);

        dl->AddText(namePos,
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.85f, 0.85f, 0.88f, 1.0f)),
            displayName.c_str());

        ImGui::InvisibleButton("##card", ImVec2(cardW, cardH));

        if (ImGui::IsItemClicked())
        {
            ImGuiIO& io = ImGui::GetIO();

            if (asset.type == RyAssetViewType::Folder)
            {
                if (!io.KeyShift && !io.KeyCtrl)
                {
                    selectedAssetIndices.clear();
                    selectedAssetIndex = -1;
                }
            }
            else if (io.KeyShift && lastClickedAssetIndex >= 0)
            {
                int a = std::min(lastClickedAssetIndex, i);
                int b = std::max(lastClickedAssetIndex, i);
                if (!io.KeyCtrl) selectedAssetIndices.clear();

                for (int k = a; k <= b; ++k)
                {
                    if (k < 0 || k >= (int)assetView.size()) continue;
                    if (assetView[k].type == RyAssetViewType::Folder) continue;

                    if (assetSearchFilter[0] != '\0')
                    {
                        std::string ln = assetView[k].name;
                        std::string lf = assetSearchFilter;
                        for (auto& c : ln) c = (char)tolower(c);
                        for (auto& c : lf) c = (char)tolower(c);
                        if (ln.find(lf) == std::string::npos) continue;
                    }

                    selectedAssetIndices.insert(k);
                }
                selectedAssetIndex = i;
            }
            else if (io.KeyCtrl)
            {
                auto it = selectedAssetIndices.find(i);
                if (it != selectedAssetIndices.end())
                {
                    selectedAssetIndices.erase(it);
                    selectedAssetIndex = selectedAssetIndices.empty() ? -1 : *selectedAssetIndices.begin();
                }
                else
                {
                    selectedAssetIndices.insert(i);
                    selectedAssetIndex = i;
                }
                lastClickedAssetIndex = i;
            }
            else
            {
                if (selectedAssetIndices.count(i) > 0 && selectedAssetIndices.size() > 1)
                {
                    pressedAssetIndex = i;
                }
                else
                {
                    selectedAssetIndices.clear();
                    selectedAssetIndices.insert(i);
                    selectedAssetIndex = i;
                    lastClickedAssetIndex = i;
                }
            }
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && pressedAssetIndex == i)
        {
            ImGuiIO& io = ImGui::GetIO();
            const bool dragWasActive = (ImGui::GetDragDropPayload() != nullptr);

            if (!io.KeyShift && !io.KeyCtrl && !dragWasActive)
            {
                selectedAssetIndices.clear();
                selectedAssetIndices.insert(i);
                selectedAssetIndex = i;
                lastClickedAssetIndex = i;
            }
            pressedAssetIndex = -1;
        }

        if (ImGui::IsItemHovered())
        {
            if (asset.type == RyAssetViewType::Texture2D)
            {
                ImGui::SetTooltip("%s  [%s - %s]",
                    asset.name.c_str(),
                    GetAssetTypeName(asset.type),
                    GetTextureFormatName(asset.textureFormat));
            }
            else
            {
                ImGui::SetTooltip("%s  [%s]", asset.name.c_str(), GetAssetTypeName(asset.type));
            }

            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                ProcessDoubleClick(asset);
        }

        if (ImGui::IsItemHovered() && !isSelected)
        {
            dl->AddRect(cardMin, cardMax,
                ImGui::ColorConvertFloat4ToU32(ImVec4(typeColor.x, typeColor.y, typeColor.z, 0.4f)),
                4.0f, 0, 1.0f);
        }

        if (asset.type == RyAssetViewType::GLTF && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            struct MeshDragPayload { ruya::UUID uuid; char name[128]; };
            struct MeshDragPayloadMulti { uint32_t count; ruya::UUID uuids[64]; char names[64][128]; };

            bool thisIsInSelection = selectedAssetIndices.count(i) > 0;

            std::vector<int> selectedMeshIndices;
            if (thisIsInSelection)
            {
                selectedMeshIndices.reserve(selectedAssetIndices.size());
                for (int idx : selectedAssetIndices)
                {
                    if (idx >= 0 && idx < static_cast<int>(assetView.size()) &&
                        assetView[idx].type == RyAssetViewType::GLTF)
                    {
                        selectedMeshIndices.push_back(idx);
                    }
                }
            }

            if (thisIsInSelection && selectedMeshIndices.size() > 1)
            {
                MeshDragPayloadMulti multi{};
                uint32_t cap = static_cast<uint32_t>(sizeof(multi.uuids) / sizeof(multi.uuids[0]));
                multi.count = std::min<uint32_t>(static_cast<uint32_t>(selectedMeshIndices.size()), cap);
                for (uint32_t k = 0; k < multi.count; ++k)
                {
                    const RyAssetView& av = assetView[selectedMeshIndices[k]];
                    multi.uuids[k] = av.assetUUID;
                    strncpy(multi.names[k], av.name.c_str(), sizeof(multi.names[k]) - 1);
                    multi.names[k][sizeof(multi.names[k]) - 1] = '\0';
                }
                ImGui::SetDragDropPayload("ASSET_MESH_MULTI", &multi, sizeof(MeshDragPayloadMulti));
                ImGui::Text("%s %u meshes", typeIcon, multi.count);
            }
            else
            {
                MeshDragPayload meshPayload{};
                meshPayload.uuid = asset.assetUUID;
                strncpy(meshPayload.name, asset.name.c_str(), sizeof(meshPayload.name) - 1);
                ImGui::SetDragDropPayload("ASSET_MESH", &meshPayload, sizeof(MeshDragPayload));
                ImGui::Text("%s %s", typeIcon, asset.name.c_str());
            }
            ImGui::EndDragDropSource();
        }

        if (asset.type == RyAssetViewType::Texture2D && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImGui::SetDragDropPayload("ASSET_TEXTURE", &asset.assetUUID, sizeof(ruya::UUID));
            ImGui::Text("%s %s", typeIcon, asset.name.c_str());
            ImGui::EndDragDropSource();
        }

        if (asset.type == RyAssetViewType::Material && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImGui::SetDragDropPayload("ASSET_MATERIAL", &asset.assetUUID, sizeof(ruya::UUID));
            ImGui::Text("%s %s", typeIcon, asset.name.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginPopupContextItem("AssetContextGrid"))
        {
            if (ImGui::MenuItem(IconLabel(u8"\uEBC6", "Rename Asset")))
            {
                s_AssetToRenameUUID = asset.assetUUID;
                s_RenameAssetBuffer = asset.name;
                s_OpenRenameAssetPopup = true;
            }

            if (asset.type == RyAssetViewType::Texture2D)
            {
                if (ImGui::BeginMenu(IconLabel(u8"\uE3AE", "Reimport As")))
                {
                    if (ImGui::MenuItem("SRGB"))
                    {
                        engine->GetAssetManager()->ReimportTexture(asset.assetUUID, ruya::TextureFormat::SRGB);
                        RefreshAssets();
                    }
                    if (ImGui::MenuItem("Linear"))
                    {
                        engine->GetAssetManager()->ReimportTexture(asset.assetUUID, ruya::TextureFormat::Linear);
                        RefreshAssets();
                    }
                    if (ImGui::MenuItem("Normal Map"))
                    {
                        engine->GetAssetManager()->ReimportTexture(asset.assetUUID, ruya::TextureFormat::NormalMap);
                        RefreshAssets();
                    }
                    ImGui::EndMenu();
                }
            }

            if (asset.type != RyAssetViewType::Folder)
            {
                ImGui::Separator();
                if (ImGui::MenuItem(IconLabel(u8"\uE4A6", "Delete Asset")))
                {
                    assetToDeleteUUID = asset.assetUUID;
                    assetToDeleteName = asset.name;
                    openDeleteAssetPopup = true;
                }
            }

            ImGui::EndPopup();
        }

        ImGui::EndGroup();

        columnIndex++;
        if (columnIndex < columns)
            ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);
        else
            columnIndex = 0;

        ImGui::PopID();
        fileIndex++;
    }
}

void editor::AssetBrowser::DrawFolderTree(const std::filesystem::path& path, int depth)
{
    float indentSize = 14.0f;

    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            if (entry.is_directory())
            {
                if (depth > 0)
                    ImGui::Indent(indentSize);

                ImGui::PushID(entry.path().string().c_str());

                bool isSelected = (currentPath == entry.path());
                bool isParentOfCurrent = std::filesystem::path(currentPath).string()
                    .find(entry.path().string()) == 0;

                ImGui::PushStyleColor(ImGuiCol_Text, kColorFolder);
                const char* iconToUse = (isSelected || isParentOfCurrent)
                    ? IconStr(u8"\uE256")
                    : IconStr(u8"\uE24A");
                ImGui::TextUnformatted(iconToUse);
                ImGui::PopStyleColor();
                ImGui::SameLine(0, 4.0f);

                bool clicked = ImGui::Selectable(
                    entry.path().filename().string().c_str(),
                    isSelected,
                    ImGuiSelectableFlags_AllowDoubleClick
                );

                if (ImGui::BeginPopupContextItem("FolderContextMenu"))
                {
                    if (ImGui::MenuItem(IconLabel(u8"\uE264", "Create Folder")))
                    {
                        newFolderName.clear();
                        openCreateFolderPopup = true;
                    }
                    if (ImGui::MenuItem(IconLabel(u8"\uE4F6", "Rename")))
                    {
                        folderToRename = entry.path();
                        renameBuffer = entry.path().filename().string();
                        openRenamePopup = true;
                    }
                    if (ImGui::MenuItem(IconLabel(u8"\uE4A6", "Delete")))
                    {
                        folderToDelete = entry.path();
                        openDeleteFolderPopup = true;
                    }
                    ImGui::EndPopup();
                }

                if (clicked)
                {
                    currentPath = entry.path();
                    selectedAssetIndex = -1;
                    RefreshAssets();
                }

                ImGui::PopID();

                if (isSelected || isParentOfCurrent)
                    DrawFolderTree(entry.path(), depth + 1);

                if (depth > 0)
                    ImGui::Unindent(indentSize);
            }
        }
    }
    catch (const std::filesystem::filesystem_error&)
    {
    }
}

void editor::AssetBrowser::RefreshAssets()
{
    assetView.clear();
    selectedAssetIndex = -1;
    selectedAssetIndices.clear();
    lastClickedAssetIndex = -1;

    if (!std::filesystem::exists(currentPath))
        currentPath = std::filesystem::path(ASSETS_DIR);

    for (const auto& entry : std::filesystem::directory_iterator(currentPath))
    {
        std::filesystem::path relativePath = std::filesystem::relative(entry.path(), ASSETS_DIR);

        RyAssetView assetIcon;
        assetIcon.name = entry.path().stem().string();

        if (entry.is_directory())
        {
            assetIcon.type = RyAssetViewType::Folder;
        }
        else if (entry.is_regular_file() && entry.path().extension() == ".ryasset")
        {
            ruya::RyAsset* asset = engine->GetAssetManager()->GetAssetByPath(relativePath.string());
            if (!asset) continue;

            if (asset->ryAssetType == ruya::RyAssetType::GLTF) assetIcon.type = RyAssetViewType::GLTF;
            else if (asset->ryAssetType == ruya::RyAssetType::RyMaterial) assetIcon.type = RyAssetViewType::Material;
            else if (asset->ryAssetType == ruya::RyAssetType::KTX2) assetIcon.type = RyAssetViewType::Texture2D;
            else if (asset->ryAssetType == ruya::RyAssetType::RyScene) assetIcon.type = RyAssetViewType::Scene;
            else continue;

            assetIcon.assetUUID = asset->uuid;

            if (assetIcon.type == RyAssetViewType::Texture2D)
                assetIcon.textureFormat = DetectTextureFormat(asset->uuid);
        }
        else
        {
            continue;
        }

        assetView.push_back(assetIcon);
    }

    std::sort(assetView.begin(), assetView.end(), [](const RyAssetView& a, const RyAssetView& b)
        {
            if (a.type == RyAssetViewType::Folder && b.type != RyAssetViewType::Folder) return true;
            if (a.type != RyAssetViewType::Folder && b.type == RyAssetViewType::Folder) return false;
            return a.name < b.name;
        });
}

void editor::AssetBrowser::ProcessDoubleClick(const RyAssetView& assetView)
{
    if (assetView.type == RyAssetViewType::Folder)
    {
        currentPath /= assetView.name;
        RefreshAssets();
    }

    if (assetView.type == RyAssetViewType::Scene)
    {
        ruya::RyID sceneId(ruya::hash_code_generator::XXHash64(assetView.name));

        if (engine->GetApp()->HasScene(sceneId))
        {
            engine->GetApp()->DestroyScene(sceneId);
        }

        auto loadedScene = engine->GetAssetManager()->LoadScene(assetView.assetUUID);
        if (loadedScene)
        {
            ruya::RyID newId = engine->GetApp()->RegisterScene(std::move(loadedScene));
            engine->GetApp()->SetSceneAssetUUID(newId, assetView.assetUUID);
            engine->GetApp()->LoadScene(newId);
        }
        else
        {
            RUYA_LOG_ERROR("Failed to load scene from asset.");
        }
    }

    if (assetView.type == RyAssetViewType::Material)
        MaterialEditor::OpenWindow(assetView.assetUUID);
}