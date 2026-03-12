#include "scene_outliner.h"

#include <engine.h>
#include <app_framework/relationship_component.h>

ruya::EntityID editor::SceneOutliner::selectedEntityFromSceneOutliner = 0;
ruya::RyID editor::SceneOutliner::selectedScene = ruya::RyID();

void editor::SceneOutliner::Draw()
{
    const char8_t* utf8_sceneOutlinerIcon = u8"\uEE48 Scene Outliner";
    const char* sceneOutlinerIcon = reinterpret_cast<const char*>(utf8_sceneOutlinerIcon);
    ImGui::Begin(sceneOutlinerIcon);
    std::vector<ruya::RyID> sceneIds = engine->GetApp()->GetLoadedScenes();
    for (ruya::RyID id : sceneIds)
    {
        std::string sceneLabel = engine->GetApp()->GetScene(id)->GetName();
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selectedScene == id)
            flags |= ImGuiTreeNodeFlags_Selected;
        bool opened = ImGui::TreeNodeEx(
            (void*)(uint64_t)(id.GetRawID() + 100000),
            flags,
            "%s", sceneLabel.c_str()
        );

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            selectedScene = id;
            selectedEntityFromSceneOutliner = 0;
        }

        if (!id.IsValid())
        {
            if (opened)
                ImGui::TreePop();
            continue;
        }

        if (opened)
        {
            auto entities = engine->GetApp()->GetScene(id)->GetSceneViewByComponentsType<ruya::RelationshipComponent>();
            for (auto [entity, relationshipComponent] : entities.each())
            {
                if (relationshipComponent.parentEntity == entt::null)
                {
                    DrawEntityNode(entity, id);
                }
            }
            ImGui::TreePop();
        }
    }
    if (ImGui::BeginPopupContextWindow("SceneOutlinerContextMenu", ImGuiPopupFlags_MouseButtonRight))
    {
        if (ImGui::MenuItem("Unload Selected Scene"))
        {
            engine->GetApp()->UnloadScene(selectedScene);
            selectedScene = ruya::RyID();
        }

        if (ImGui::MenuItem("Create Entity In Selected Scene"))
        {
            if (selectedScene.IsValid())
            {
                auto scene = engine->GetApp()->GetScene(selectedScene);
                auto newEntity = scene->CreateEntity();
                selectedEntityFromSceneOutliner = static_cast<uint32_t>(newEntity);
            }
        }
        ImGui::EndPopup();
    }
    ImGui::End();
}

void editor::SceneOutliner::DrawEntityNode(ruya::EntityID entity, ruya::RyID sceneId)
{
    const char8_t* utf8_entityIcon = u8"\uE2AE";
    const char* entityIcon = reinterpret_cast<const char*>(utf8_entityIcon);

    std::string label = std::string(entityIcon) + " Entity " + std::to_string(static_cast<uint32_t>(entity));

    ruya::RelationshipComponent* rel = engine->GetApp()->GetScene(sceneId)->TryGetComponent<ruya::RelationshipComponent>(static_cast<uint32_t>(entity));

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    if (rel->firstChildEntity == entt::null)
        flags |= ImGuiTreeNodeFlags_Leaf;

    if (sceneId == selectedScene && selectedEntityFromSceneOutliner == static_cast<uint32_t>(entity))
        flags |= ImGuiTreeNodeFlags_Selected;

    bool opened = ImGui::TreeNodeEx(
        (void*)(uint64_t)entity,
        flags,
        "%s", label.c_str()
    );

    if (sceneId == selectedScene && ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        selectedEntityFromSceneOutliner = static_cast<uint32_t>(entity);
    }

    if (opened)
    {
        ruya::EntityID child = rel->firstChildEntity;
        while (child != entt::null)
        {
            DrawEntityNode(child, sceneId);
            child = engine->GetApp()->GetScene(sceneId)->TryGetComponent<ruya::RelationshipComponent>(
                static_cast<uint32_t>(child)
            )->nextEntity;
        }

        ImGui::TreePop();
    }
}


