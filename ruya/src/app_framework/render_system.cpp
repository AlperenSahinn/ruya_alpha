#include "render_system.h"
#include <iostream>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp> 
#include <glm/gtx/transform.hpp>

#include <engine.h>

#include <graphics/camera.h>
#include <graphics/render_data.h>

#include "transform_component.h"
#include "render_component.h"
#include "relationship_component.h"
#include "model_3d_component.h"

void ruya::RenderSystem::OnEngineUpdate()
{
    auto modelLoadingEntities = scene->GetSceneViewByComponentsType<Model3DComponent>();

    for (auto [entity, model3DComponent] : modelLoadingEntities.each())
    {
        if (model3DComponent.isLoaded == false)
        {
            engine->GetGraphics()->LoadModel3D(model3DComponent.modelAssetID);
            model3DComponent.isLoaded = true;
        }
    }

    auto renderGeometryCreationEntities = scene->GetSceneViewByComponentsType<RenderComponent, TransformComponent>();

    for (auto [entity, renderComp, transformComp] : renderGeometryCreationEntities.each())
    {
        if (!renderComp.renderGeometryID.IsValid())
        {
            glm::vec3 rotationRadians = glm::radians(transformComp.rotation);
            glm::quat rotationQuat = glm::quat(rotationRadians);
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), transformComp.position)
                * glm::toMat4(rotationQuat)
                * glm::scale(glm::mat4(1.0f), transformComp.scale);

            transformComp.transform = transform;

            renderComp.renderGeometryID = engine->GetGraphics()->CreateRenderGeometry(transformComp.transform, renderComp.meshID, renderComp.materialID);
        }
    }

	RenderData* renderData = engine->GetRenderDataWriteBuffer();
	renderData->camera.frameNumber = frameNumber;
	frameNumber++;

	auto entities = scene->GetSceneViewByComponentsType<TransformComponent, RelationshipComponent>();

    for (auto [entity, transformComp, relationshipComp] : entities.each())
    {
        if (relationshipComp.parentEntity == entt::null)
        {
            UpdateTransformRecursive(entity);
        }
    }
}

void ruya::RenderSystem::OnSceneLoad()
{
    auto modelLoadingEntities = scene->GetSceneViewByComponentsType<Model3DComponent>();

    for (auto [entity, model3DComponent] : modelLoadingEntities.each())
    {
        if (model3DComponent.isLoaded == false)
        {
            engine->GetGraphics()->LoadModel3D(model3DComponent.modelAssetID);
            model3DComponent.isLoaded = true;
        }
    }

    auto entities = scene->GetSceneViewByComponentsType<RenderComponent, TransformComponent>();

	for (auto [entity, renderComp, transformComp] : entities.each())
	{
		glm::vec3 rotationRadians = glm::radians(transformComp.rotation);
		glm::quat rotationQuat = glm::quat(rotationRadians);
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), transformComp.position)
			* glm::toMat4(rotationQuat)
			* glm::scale(glm::mat4(1.0f), transformComp.scale);

        transformComp.transform = transform;

		renderComp.renderGeometryID = engine->GetGraphics()->CreateRenderGeometry(transformComp.transform, renderComp.meshID, renderComp.materialID);;
	}
}

void ruya::RenderSystem::OnSceneUnload()
{
    auto modelLoadingEntities = scene->GetSceneViewByComponentsType<Model3DComponent>();

    for (auto [entity, model3DComponent] : modelLoadingEntities.each())
    {
        if (model3DComponent.isLoaded == true)
        {
            engine->GetGraphics()->UnloadModel3D(model3DComponent.modelAssetID);
            model3DComponent.isLoaded = false;
        }
    }

    for(auto& pair : engine->GetGraphics()->GetRenderGeometries())
    {
        engine->GetGraphics()->DestroyRenderGeometry(pair.first);
    }
}

void ruya::RenderSystem::UpdateTransformRecursive(EntityID entity)
{
    if (entity == entt::null) return;

    TransformComponent* transformComp = scene->TryGetComponent<TransformComponent>(entity);
    RelationshipComponent* relationshipComp = scene->TryGetComponent<RelationshipComponent>(entity);

    glm::vec3 rotationRadians = glm::radians(transformComp->rotation);
    glm::quat rotationQuat = glm::quat(rotationRadians);

    glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), transformComp->position)
        * glm::toMat4(rotationQuat)
        * glm::scale(glm::mat4(1.0f), transformComp->scale);

    if (relationshipComp->parentEntity != entt::null)
    {
        TransformComponent* parentTransformComp = scene->TryGetComponent<TransformComponent>(
            static_cast<uint32_t>(relationshipComp->parentEntity)
        );
        transformComp->transform = parentTransformComp->transform * localTransform;
    }
    else
    {
        transformComp->transform = localTransform;
    }

    if (RenderComponent* renderComponent = scene->TryGetComponent<RenderComponent>(entity))
    {
        if (renderComponent->meshID.IsValid() && renderComponent->renderGeometryID.IsValid() && renderComponent->materialID.IsValid())
        {
            engine->GetGraphics()->UpdateRenderGeometry(renderComponent->renderGeometryID, transformComp->transform, renderComponent->meshID, renderComponent->materialID, renderComponent->draw);
        }
    }

    if (relationshipComp->firstChildEntity != entt::null)
    {
        UpdateTransformRecursive(relationshipComp->firstChildEntity);
    }

    if (relationshipComp->nextEntity != entt::null)
    {
        UpdateTransformRecursive(relationshipComp->nextEntity);
    }
}
