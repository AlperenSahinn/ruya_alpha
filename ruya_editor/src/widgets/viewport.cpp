#include "viewport.h"

#include <imgui/imgui_impl_vulkan.h>

#include <engine.h>
#include <app_framework/render_component.h>
#include <app_framework/transform_component.h>
#include <app_framework/id_component.h>

#include "editor_widgets.h"
#include "../editor_style.h"
#include "scene_outliner.h"

bool editor::Viewport::isActive = false;

editor::Viewport::Viewport()
{
    selectedRenderTarget = "toneMapPassResultImage";
    RecreateDrawTextures();
}

editor::Viewport::~Viewport()
{
    DestroyDrawTextures();
}

void editor::Viewport::DestroyDrawTextures()
{
    for (auto& [name, descriptorSets] : renderTargetDescriptorSets)
    {
        for (VkDescriptorSet ds : descriptorSets)
        {
            if (ds != VK_NULL_HANDLE)
                ImGui_ImplVulkan_RemoveTexture(ds);
        }
    }
    renderTargetDescriptorSets.clear();
}

void editor::Viewport::RecreateDrawTextures()
{
    DestroyDrawTextures();

    uint32_t frameBufferCount = engine->GetGraphics()->GetFrameBufferCount();
    ruya::GraphicsPipeline* pipeline = engine->GetGraphics()->GetStandartPipeline();
    if (pipeline == nullptr)
        return;

    std::unordered_map<std::string, uint32_t>& renderTargetsMap = pipeline->GetRenderTargetsMap();

    for (const auto& [name, index] : renderTargetsMap)
    {
        std::vector<VkDescriptorSet> descriptorSets;
        descriptorSets.reserve(frameBufferCount);

        bool valid = true;
        for (uint32_t i = 0; i < frameBufferCount; i++)
        {
            ruya::Texture2D* renderTarget = engine->GetGraphics()->GetRenderTarget(i, name);
            if (renderTarget == nullptr)
            {
                valid = false;
                break;
            }

            descriptorSets.push_back(ImGui_ImplVulkan_AddTexture(
                engine->GetGraphics()->GetNearestSampler(),
                renderTarget->GetVulkanImage()->GetImageView(),
                VK_IMAGE_LAYOUT_GENERAL));
        }

        if (valid)
        {
            renderTargetDescriptorSets[name] = std::move(descriptorSets);
        }
        else
        {
            for (VkDescriptorSet ds : descriptorSets)
            {
                if (ds != VK_NULL_HANDLE)
                    ImGui_ImplVulkan_RemoveTexture(ds);
            }
        }
    }

    if (renderTargetDescriptorSets.find(selectedRenderTarget) == renderTargetDescriptorSets.end())
    {
        selectedRenderTarget = "toneMapPassResultImage";
    }
}

void editor::Viewport::Draw()
{
    const char* viewportTitle = IconLabel(u8"\uEAA2", "Viewport");

    ImGui::Begin(viewportTitle, nullptr);

    isActive = ImGui::IsWindowHovered();

    ImVec2 availSize = ImGui::GetContentRegionAvail();
    ImVec2 windowPos = ImGui::GetCursorScreenPos();

    if (availSize.x <= 0 || availSize.y <= 0)
    {
        ImGui::End();
        return;
    }

    uint32_t targetWidth = static_cast<uint32_t>(availSize.x);
    uint32_t targetHeight = static_cast<uint32_t>(availSize.y);

    ruya::Texture2D* selectedTexture = engine->GetGraphics()->GetRenderTarget(engine->GetGraphics()->GetCurrentFrameIndex(), selectedRenderTarget);
    if (selectedTexture == nullptr)
    {
        ImGui::End();
        return;
    }

    ruya::VulkanImage* resultImage = selectedTexture->GetVulkanImage();
    VkExtent3D currentExtent = resultImage->GetExtent();

    if (currentExtent.width != targetWidth || currentExtent.height != targetHeight)
    {
        engine->GetGraphics()->SetFrameBufferExtent(targetWidth, targetHeight);
        RecreateDrawTextures();
    }

    auto it = renderTargetDescriptorSets.find(selectedRenderTarget);
    if (it != renderTargetDescriptorSets.end())
    {
        uint32_t frameIndex = engine->GetGraphics()->GetCurrentFrameIndex();
        if (frameIndex < it->second.size())
        {
            ImGui::SetCursorScreenPos(windowPos);
            ImGui::Image(it->second[frameIndex], availSize);
        }
    }

    if (ImGui::BeginDragDropTarget())
    {
        struct MeshDragPayload { ruya::UUID uuid; char name[128]; };
        struct MeshDragPayloadMulti { uint32_t count; ruya::UUID uuids[64]; char names[64][128]; };

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MESH"))
        {
            if (SceneOutliner::selectedScene.IsValid())
            {
                const MeshDragPayload* meshPayload = static_cast<const MeshDragPayload*>(payload->Data);
                ruya::RyAsset* default_material = engine->GetAssetManager()->GetAssetByPath("ruya_files/materials/default_material.ryasset");
                ruya::Scene* scene = engine->GetApp()->GetScene(SceneOutliner::selectedScene);
                ruya::EntityID entityID = scene->CreateEntity();
                scene->AddComponent<ruya::RenderComponent>(entityID);
                ruya::RenderComponent* renderComponent = scene->TryGetComponent<ruya::RenderComponent>(entityID);
                renderComponent->meshUUID = meshPayload->uuid;
                renderComponent->materialUUID = default_material->uuid;
                scene->TryGetComponent<ruya::TransformComponent>(entityID)->position = engine->GetAssetManager()->GetGLTFInfo(renderComponent->meshUUID)->initialPosition;
                if (ruya::IdComponent* idComp = scene->TryGetComponent<ruya::IdComponent>(static_cast<uint32_t>(entityID)))
                    idComp->name = meshPayload->name;
                SceneOutliner::SelectEntity(static_cast<uint32_t>(entityID), false);
            }
        }
        else if (const ImGuiPayload* payloadMulti = ImGui::AcceptDragDropPayload("ASSET_MESH_MULTI"))
        {
            if (SceneOutliner::selectedScene.IsValid())
            {
                const MeshDragPayloadMulti* multi = static_cast<const MeshDragPayloadMulti*>(payloadMulti->Data);
                ruya::RyAsset* default_material = engine->GetAssetManager()->GetAssetByPath("ruya_files/materials/default_material.ryasset");
                ruya::Scene* scene = engine->GetApp()->GetScene(SceneOutliner::selectedScene);

                SceneOutliner::ClearEntitySelection();

                for (uint32_t k = 0; k < multi->count; ++k)
                {
                    ruya::EntityID entityID = scene->CreateEntity();
                    scene->AddComponent<ruya::RenderComponent>(entityID);
                    ruya::RenderComponent* renderComponent = scene->TryGetComponent<ruya::RenderComponent>(entityID);
                    renderComponent->meshUUID = multi->uuids[k];
                    renderComponent->materialUUID = default_material->uuid;
                    scene->TryGetComponent<ruya::TransformComponent>(entityID)->position = engine->GetAssetManager()->GetGLTFInfo(renderComponent->meshUUID)->initialPosition;
                    if (ruya::IdComponent* idComp = scene->TryGetComponent<ruya::IdComponent>(static_cast<uint32_t>(entityID)))
                        idComp->name = multi->names[k];

                    SceneOutliner::selectedEntities.insert(static_cast<uint32_t>(entityID));
                    SceneOutliner::selectedEntityFromSceneOutliner = static_cast<uint32_t>(entityID);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    const char* visibleIcon = IconStr(u8"\uE220");
    const char* renderTargetIcon = IconStr(u8"\uEAA2");
    const char* cameraIcon = IconStr(u8"\uE3AF");
    ImVec2 iconTextSize = ImGui::CalcTextSize(visibleIcon);
    ImVec2 buttonSize = ImVec2(iconTextSize.x + ImGui::GetStyle().FramePadding.x * 2.0f, iconTextSize.y + ImGui::GetStyle().FramePadding.y * 2.0f);

    ImVec2 rtDropdownPos = ImVec2(windowPos.x + availSize.x - (buttonSize.x * 3.0f) - 16.0f, windowPos.y + 8.0f);

    ImGui::SetCursorScreenPos(rtDropdownPos);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 0.7f));
    if (ImGui::Button(renderTargetIcon))
    {
        ImGui::OpenPopup("RenderTargetMenu");
    }
    ImGui::PopStyleColor();

    if (ImGui::BeginPopup("RenderTargetMenu"))
    {
        ImGui::TextDisabled("Render Targets");
        ImGui::Separator();

        for (const auto& [name, descriptorSets] : renderTargetDescriptorSets)
        {
            bool isSelected = (selectedRenderTarget == name);
            if (ImGui::MenuItem(name.c_str(), nullptr, isSelected))
            {
                selectedRenderTarget = name;
            }
        }

        ImGui::EndPopup();
    }

    ImVec2 cameraDropdownPos = ImVec2(windowPos.x + availSize.x - (buttonSize.x * 2.0f) - 12.0f, windowPos.y + 8.0f);

    ImGui::SetCursorScreenPos(cameraDropdownPos);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 0.7f));
    if (ImGui::Button(cameraIcon))
    {
        ImGui::OpenPopup("CameraSettingsMenu");
    }
    ImGui::PopStyleColor();

    if (ImGui::BeginPopup("CameraSettingsMenu"))
    {
        ImGui::TextDisabled("Camera Settings");
        ImGui::Separator();

        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("##CameraIntValue", &(engine->GetRenderDataWriteBuffer()->camera.pathTracerSampleCount));

        ImGui::EndPopup();
    }

    ImVec2 dropdownPos = ImVec2(windowPos.x + availSize.x - buttonSize.x - 8.0f, windowPos.y + 8.0f);

    ImGui::SetCursorScreenPos(dropdownPos);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 0.7f));
    if (ImGui::Button(visibleIcon))
    {
        ImGui::OpenPopup("VisibilityMenu");
    }
    ImGui::PopStyleColor();

    if (ImGui::BeginPopup("VisibilityMenu"))
    {
        ImGui::MenuItem("Physics Debug Lines", nullptr, engine->GetPhysics()->DrawDebugLines());
        ImGui::Separator();
        ImGui::EndPopup();
    }

    ImGui::End();
}

bool editor::Viewport::IsActive()
{
    return isActive;
}