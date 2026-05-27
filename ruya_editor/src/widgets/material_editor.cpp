#include "material_editor.h"

#include <engine.h>

#include "editor_widgets.h"

void editor::MaterialEditor::Draw()
{
    for (auto it = openMaterialWindows.begin(); it != openMaterialWindows.end(); )
    {
        ruya::UUID materialID = it->first;
        bool& isOpen = it->second;

        if (!isOpen)
        {
            it = openMaterialWindows.erase(it);
            continue;
        }

        ruya::RyAsset* materialAsset = engine->GetAssetManager()->TryGetAsset(materialID);
        std::optional<ruya::RyMaterial> loadedMaterial = engine->GetAssetManager()->LoadRyMaterial(materialID);

        if (loadedMaterial.has_value() && materialAsset)
        {
            ruya::RyMaterial material = *loadedMaterial;

            std::string windowTitle = IconLabel(u8"\uE18E", materialAsset->assetName.c_str());
            windowTitle += "###MaterialEditor_" + materialID.ToString();

            ImGuiViewport* mainViewport = ImGui::GetMainViewport();
            ImVec2 desiredSize = ImVec2(480.0f, 360.0f);
            ImVec2 centerPos = ImVec2(
                mainViewport->WorkPos.x + (mainViewport->WorkSize.x - desiredSize.x) * 0.5f,
                mainViewport->WorkPos.y + (mainViewport->WorkSize.y - desiredSize.y) * 0.5f);

            ImGui::SetNextWindowSize(desiredSize, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(centerPos, ImGuiCond_FirstUseEver);

            if (ImGui::Begin(windowTitle.c_str(), &isOpen))
            {
                bool materialDirty = false;

                auto DrawTextureSlot = [&](const char* label, ruya::UUID& textureID)
                    {
                        ImGui::PushID(label);
                        ImGui::Text("%s", label);

                        std::string buttonLabel = "None (Drop Texture Here)##drop_btn";

                        if (textureID != ruya::UUID::Invalid())
                        {
                            ruya::RyAsset* texAsset = engine->GetAssetManager()->TryGetAsset(textureID);
                            if (texAsset)
                                buttonLabel = texAsset->assetName + "##drop_btn";
                        }

                        ImGui::Button(buttonLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 30.0f));

                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_TEXTURE"))
                            {
                                IM_ASSERT(payload->DataSize == sizeof(ruya::UUID));
                                ruya::UUID droppedUUID = *(const ruya::UUID*)payload->Data;

                                if (textureID != droppedUUID)
                                {
                                    textureID = droppedUUID;
                                    materialDirty = true;
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                        ImGui::Spacing();
                        ImGui::PopID();
                    };

                DrawTextureSlot("Albedo Texture", material.albedoUUID);
                DrawTextureSlot("Normal Texture", material.normalUUID);
                DrawTextureSlot("Metallic / Roughness Texture", material.metallicRoughnessUUID);

                if (materialDirty)
                    engine->GetAssetManager()->SaveMaterial(materialID, material);
            }
            ImGui::End();

            ++it;
        }
        else
        {
            it = openMaterialWindows.erase(it);
        }
    }
}

void editor::MaterialEditor::OpenWindow(ruya::UUID materialID)
{
    openMaterialWindows[materialID] = true;
}