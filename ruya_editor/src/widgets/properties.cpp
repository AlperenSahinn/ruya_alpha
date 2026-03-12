#include "properties.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp> 
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <engine.h>
#include <app_framework/id_component.h>
#include <app_framework/relationship_component.h>
#include <app_framework/transform_component.h>
#include <app_framework/render_component.h>

#include "scene_outliner.h"

void editor::Properties::Draw()
{
    const char8_t* utf8_propertiesIcon = u8"\uE34C Properties";
    const char* propertiesIcon = reinterpret_cast<const char*>(utf8_propertiesIcon);

    ruya::RyID scene = SceneOutliner::selectedScene;

    ImGui::Begin(propertiesIcon);

    if (!scene.IsValid() || engine->GetApp()->GetScene(scene)->IsEmpty())
    {
        ImGui::End();
        return;
    }

    uint32_t entity = SceneOutliner::selectedEntityFromSceneOutliner;

    ruya::IdComponent* idComponent = engine->GetApp()->GetScene(scene)->TryGetComponent<ruya::IdComponent>(static_cast<uint32_t>(entity));

    ImGui::Text("Id Component");
	ImGui::BeginChild("IdComponentBox", ImVec2(0, 70), true);
	ImGui::Text("Name: %s", idComponent->name.c_str());
	ImGui::Separator();
	ImGui::Text("Id: %s", std::to_string(idComponent->enttID).c_str());

	ImGui::EndChild();

    if (ruya::TransformComponent* transformComponent =
        engine->GetApp()->GetScene(scene)->TryGetComponent<ruya::TransformComponent>((uint32_t)entity))
    {
        if (ImGui::CollapsingHeader("Transform Component", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::BeginChild("TransformComponentBox", ImVec2(0, 190), true);
            UpdateTransformRecursiveUI(entity);
            ImGui::EndChild();
        }
    }

    if (ruya::RenderComponent* renderComponent =
        engine->GetApp()->GetScene(scene)->TryGetComponent<ruya::RenderComponent>((uint32_t)entity))
    {
        if (ImGui::CollapsingHeader("Render Component", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::BeginChild("RenderComponentBox", ImVec2(0, 190), true);
            
            ImGui::Text("Render Geometry Id: %s", std::to_string(renderComponent->renderGeometryID).c_str());
            ImGui::Separator();
            ImGui::Text("Mesh Id: %s", std::to_string(renderComponent->meshID).c_str());
            ImGui::Separator();
            ImGui::Text("Material Id: %s", std::to_string(renderComponent->materialID).c_str());
            ImGui::Separator();
            ImGui::Checkbox("Visible: ", &renderComponent->draw);


            ImGui::EndChild();
        }
    }

    ImGui::End();
}

void editor::Properties::UpdateTransformRecursiveUI(uint32_t entity)
{
    ruya::RyID scene = SceneOutliner::selectedScene;

    if (ruya::TransformComponent* transformComponent =
        engine->GetApp()->GetScene(scene)->TryGetComponent<ruya::TransformComponent>((uint32_t)entity))
    {
        const char8_t* utf8_positionIcon = u8"\uE0A4";
        const char* positionIcon = reinterpret_cast<const char*>(utf8_positionIcon);

        std::string positionLabel = std::string(positionIcon) + " Position";

        ImGui::Text(positionLabel.c_str());
        if (ImGui::DragFloat3(("##Position" + std::to_string((uint32_t)entity)).c_str(), glm::value_ptr(transformComponent->position), 0.1f))

        ImGui::Separator();

        const char8_t* utf8_rotationIcon = u8"\uE096";
        const char* rotationIcon = reinterpret_cast<const char*>(utf8_rotationIcon);

        std::string rotationLabel = std::string(rotationIcon) + " Rotation";

        ImGui::Text(rotationLabel.c_str());
        if (ImGui::DragFloat3(("##Rotation" + std::to_string((uint32_t)entity)).c_str(), glm::value_ptr(transformComponent->rotation), 0.1f))

        ImGui::Separator();

        const char8_t* utf8_scaleIcon = u8"\uE0A6";
        const char* scaleIcon = reinterpret_cast<const char*>(utf8_scaleIcon);

        std::string scaleLabel = std::string(scaleIcon) + " Scale";

        ImGui::Text(scaleLabel.c_str());
        if (ImGui::DragFloat3(("##Scale" + std::to_string((uint32_t)entity)).c_str(), glm::value_ptr(transformComponent->scale), 0.1f));
    }
}
