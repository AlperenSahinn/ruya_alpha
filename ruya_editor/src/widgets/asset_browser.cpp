#include "asset_browser.h"

#include <filesystem>

#include <imgui/imgui_impl_vulkan.h>

#include <engine.h>
#include <core/log.h>
#include <core/hash_code_generator.h>

#include "scene_outliner.h"

editor::AssetBrowser::AssetBrowser()
{
    pFileDialog = std::make_unique<ImGui::FileBrowser>();
    pFileDialog->SetTitle("File Browser");
    pFileDialog->SetTypeFilters({ ".png", ".gltf" });

    currentPath = ASSETS_DIR;

    openCreateScenePopup = false;

    openCreateFolderPopup = false;
    openDeleteFolderPopup = false;
    openRenamePopup = false;

    RefreshAssets();
}

editor::AssetBrowser::~AssetBrowser()
{
}

void editor::AssetBrowser::Draw()
{
    const char8_t* utf8_assetBrowserIcon = u8"\uE0F4 Asset Browser";
    const char* assetBrowserIcon = reinterpret_cast<const char*>(utf8_assetBrowserIcon);

    const char8_t* utf8_homeButtonIcon = u8"\uE2C2";
    const char* homeButtonIcon = reinterpret_cast<const char*>(utf8_homeButtonIcon);

    const char8_t* utf8_backButtonIcon = u8"\uE05A";
    const char* backButtonIcon = reinterpret_cast<const char*>(utf8_backButtonIcon);

    const char8_t* utf8_refreshButtonIcon = u8"\uE036";
    const char* refreshButtonIcon = reinterpret_cast<const char*>(utf8_refreshButtonIcon);

    const char8_t* utf8_saveButtonIcon = u8"\uE248";
    const char* saveButtonIcon = reinterpret_cast<const char*>(utf8_saveButtonIcon);

    const char8_t* utf8_imageIcon = u8"\uEB18 ";
    const char* imageIcon = reinterpret_cast<const char*>(utf8_imageIcon);

    const char8_t* utf8_modelIcon = u8"\uE1DA ";
    const char* modelIcon = reinterpret_cast<const char*>(utf8_modelIcon);

    const char8_t* utf8_sceneIcon = u8"\uE7AE ";
    const char* sceneIcon = reinterpret_cast<const char*>(utf8_sceneIcon);

    const char8_t* utf8_invalidIcon = u8"\uE650 ";
    const char* invalidIcon = reinterpret_cast<const char*>(utf8_invalidIcon);

    ImGui::Begin(assetBrowserIcon, nullptr, ImGuiWindowFlags_MenuBar);
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::Button(homeButtonIcon))
        {
            std::filesystem::path rootAssets = std::filesystem::path(ASSETS_DIR);
            currentPath = rootAssets;
            RefreshAssets();
        }
        if (ImGui::Button(backButtonIcon))
        {
            std::filesystem::path rootAssets = std::filesystem::path(ASSETS_DIR);
            if (currentPath != rootAssets)
            {
                currentPath = currentPath.parent_path();
                RefreshAssets();
            }
        }
        if (ImGui::Button(refreshButtonIcon))
        {
            RefreshAssets();
        }
        if (ImGui::Button(saveButtonIcon))
        {
            SaveScene();
        }

        ImGui::EndMenuBar();
    }

    if (openCreateScenePopup)
    {
        ImGui::OpenPopup("Create Scene");
        openCreateScenePopup = false;
    }

    if (ImGui::BeginPopupModal("Create Scene", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Scene Name:");

        char buffer[128];
        memset(buffer, 0, sizeof(buffer));

        strncpy(buffer, sceneNameBuffer.c_str(), sizeof(buffer) - 1);

        if (ImGui::InputText("##SceneNameInput", buffer, IM_ARRAYSIZE(buffer)))
        {
            sceneNameBuffer = buffer;
        }

        ImGui::Spacing();

        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            createdSceneName = sceneNameBuffer;

            ruya::RyID sceneId = engine->GetApp()->CreateScene(createdSceneName);

            engine->GetAssetManager()->SerializeScene(*engine->GetApp()->GetScene(sceneId), currentPath.string());

            ImGui::CloseCurrentPopup();

            RefreshAssets();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    pFileDialog->Display();
    if (pFileDialog->HasSelected())
    {
        std::filesystem::path filePath = pFileDialog->GetSelected();

        if (filePath.extension() == ".gltf")
        {
            ImportGLTF(filePath.string());
        }

        pFileDialog->ClearSelected();
        RefreshAssets();
    }

    if (ImGui::BeginTable("AssetBrowserTable", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 250.0f);
        ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthFixed, 300.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGui::BeginChild("FolderPanel", ImVec2(0, 0), false);
        DrawFolderTree(std::filesystem::path(ASSETS_DIR), 0);
        ImGui::EndChild();

        ImGui::TableNextColumn();

        ImGui::BeginChild("FilePanel", ImVec2(0, 0), false);
        int fileIndex = 0;
        for (const auto& asset : assetView)
        {
            if (asset.type == ruya::RyAssetType::Folder)
                continue;

            const char* prefix = "";

            switch (asset.type)
            {
            case ruya::RyAssetType::Model:
                prefix = modelIcon;
                break;

            case ruya::RyAssetType::Scene:
                prefix = sceneIcon;
                break;

            default:
                prefix = invalidIcon;
                break;
            }

            std::string selectableLabel = std::string(prefix) + asset.name;

            ImGui::PushID(fileIndex);

            bool clicked = ImGui::Selectable(selectableLabel.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                ProcessDoubleClick(asset);
            }

            if (asset.type == ruya::RyAssetType::Model)
            {
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                {
                    ImGui::OpenPopup("ModelContextMenu");
                }

                if (ImGui::BeginPopupContextItem("ModelContextMenu"))
                {
                    if (ImGui::MenuItem("Create Entity From Model"))
                    {
                        if (SceneOutliner::selectedScene.IsValid())
                        {
                            engine->GetGraphics()->LoadModel3D(asset.assetID);
                            engine->GetGraphics()->GetModel3D(asset.assetID)->CreateEntityFromModel(*engine->GetApp()->GetScene(SceneOutliner::selectedScene));
                        }
                    }
                    ImGui::EndPopup();
                }
            }

            ImGui::PopID();
            fileIndex++;
        }

        if (ImGui::BeginPopupContextWindow("Asset Browser Contex Menu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverExistingPopup))
        {
            if (ImGui::MenuItem("Import Asset"))
                pFileDialog->Open();

            if (ImGui::MenuItem("Create Folder"))
            {
                newFolderName.clear();
                openCreateFolderPopup = true;
            }

            if (ImGui::MenuItem("Create Scene"))
            {
                openCreateScenePopup = true;
                sceneNameBuffer = "";
            }

            ImGui::EndPopup();
        }

        ImGui::EndChild();

        ImGui::TableNextColumn();

        ImGui::BeginChild("DetailPanel", ImVec2(0, 0), false);

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
                std::filesystem::path newFolder = currentPath / newFolderName;
                std::filesystem::create_directory(newFolder);
                RefreshAssets();
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
        ImGui::Text("%s", folderToDelete.string().c_str());

        if (ImGui::Button("Yes", ImVec2(120, 0)))
        {
            try
            {
                std::filesystem::remove_all(folderToDelete);
                RefreshAssets();
            }
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
                std::filesystem::path newPath = folderToRename.parent_path() / renameBuffer;

                try
                {
                    std::filesystem::rename(folderToRename, newPath);
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

    ImGui::End();
}

void editor::AssetBrowser::DrawFolderTree(const std::filesystem::path& path, int depth)
{
    float indentSize = 16.0f;

    const char8_t* utf8_folderClosedIcon = u8"\uE24A";
    const char* folderClosedIcon = reinterpret_cast<const char*>(utf8_folderClosedIcon);

    const char8_t* utf8_folderOpenIcon = u8"\uE256";
    const char* folderOpenIcon = reinterpret_cast<const char*>(utf8_folderOpenIcon);

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

                const char* iconToUse = (isSelected || isParentOfCurrent)
                    ? folderOpenIcon
                    : folderClosedIcon;

                ImGui::Text(iconToUse);
                ImGui::SameLine();

                bool clicked = ImGui::Selectable(
                    entry.path().filename().string().c_str(),
                    isSelected,
                    ImGuiSelectableFlags_AllowDoubleClick
                );

                if (ImGui::BeginPopupContextItem("FolderContextMenu"))
                {
                    if (ImGui::MenuItem("Create Folder"))
                    {
                        newFolderName.clear();
                        openCreateFolderPopup = true;
                    }

                    if (ImGui::MenuItem("Rename"))
                    {
                        folderToRename = entry.path();
                        renameBuffer = entry.path().filename().string();
                        openRenamePopup = true;
                    }

                    if (ImGui::MenuItem("Delete"))
                    {
                        folderToDelete = entry.path();
                        openDeleteFolderPopup = true;
                    }

                    ImGui::EndPopup();
                }

                if (clicked)
                {
                    currentPath = entry.path();
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
    catch (const std::filesystem::filesystem_error& e)
    {
    }
}

void editor::AssetBrowser::RefreshAssets()
{
    assetView.clear();

    for (const auto& entry : std::filesystem::directory_iterator(currentPath))
    {
        std::filesystem::path relativePath = std::filesystem::relative(entry.path(), ASSETS_DIR);

        RyAssetView assetIcon;
        assetIcon.name = entry.path().stem().string();

        if (entry.is_directory())
        {
            assetIcon.type = ruya::RyAssetType::Folder;
        }
        else if (entry.is_regular_file() && entry.path().extension() == ".ryasset")
        {
            ruya::RyAsset asset = engine->GetAssetManager()->DeserializeAsset(relativePath.string());

            assetIcon.type = ruya::RyAssetType::Model;
            assetIcon.assetID = asset.id;
        }
        else if (entry.is_regular_file() && entry.path().extension() == ".ryscene")
        {
            assetIcon.type = ruya::RyAssetType::Scene;
        }
        else
        {
            continue;
        }

        assetView.push_back(assetIcon);
    }
}

void editor::AssetBrowser::ProcessDoubleClick(const RyAssetView& assetView)
{
    if (assetView.type == ruya::RyAssetType::Folder)
    {
        currentPath /= assetView.name;
        RefreshAssets();
    }

    if (assetView.type == ruya::RyAssetType::Scene)
    {
        engine->GetApp()->LoadScene(engine->GetApp()->GetSceneId(assetView.name));
    }
}

void editor::AssetBrowser::ImportGLTF(std::string path)
{
    std::filesystem::path sourcePath = path;
    std::filesystem::path targetPath = currentPath / sourcePath.filename();

    try
    {
        std::filesystem::copy_file(sourcePath, targetPath, std::filesystem::copy_options::overwrite_existing);

        std::filesystem::path sourceBinPath = sourcePath;
        sourceBinPath.replace_extension(".bin");

        std::filesystem::path targetBinPath = targetPath;
        targetBinPath.replace_extension(".bin");

        if (std::filesystem::exists(sourceBinPath))
        {
            std::filesystem::copy_file(sourceBinPath, targetBinPath, std::filesystem::copy_options::overwrite_existing);
        }
        else
        {
            RUYA_LOG_ERROR((".bin file does not exist for GLTF file: " + sourceBinPath.string()).c_str());
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        RUYA_LOG_ERROR(("Failed to copy files: " + std::string(e.what())).c_str());
        return;
    }

    ruya::RyID newAssetID = engine->GetAssetManager()->CreateAsset(
        sourcePath.stem().string(),
        ruya::RyAssetType::Model,
        ruya::RyAssetSourceExtension::gltf,
        std::filesystem::relative(targetPath, ASSETS_DIR).parent_path().string() + "/"
    );

    ruya::RyAsset* newAsset = engine->GetAssetManager()->GetAsset(newAssetID);

    if (!engine->GetAssetManager()->SerializeAsset(newAssetID))
    {
        RUYA_LOG_ERROR(("Failed to serialize asset: " + newAsset->name).c_str());
        return;
    }
}

void editor::AssetBrowser::SaveScene()
{
    engine->GetAssetManager()->SerializeScene(*engine->GetApp()->GetScene(SceneOutliner::selectedScene), currentPath.string());
}
