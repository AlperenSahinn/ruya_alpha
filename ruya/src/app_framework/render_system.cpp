#include "render_system.h"
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp> 
#include <glm/gtx/transform.hpp>

#include <engine.h>

#include <graphics/camera.h>
#include <graphics/render_data.h>

#include "transform_component.h"
#include "render_component.h"

void ruya::RenderSystem::OnStart()
{
    auto transformEntities = scene->GetSceneViewByComponentsType<TransformComponent>();

    for (auto [entity, transformComp] : transformEntities.each())
    {
        transformComp.updatePosition = transformComp.position;
        transformComp.updateRotation = transformComp.rotation;
        transformComp.updateScale = transformComp.scale;
        transformComp.updateTransform = transformComp.transform;
    }

    auto renderEntities = scene->GetSceneViewByComponentsType<RenderComponent, TransformComponent>();

    for (auto [entity, renderComponent, transformComp] : renderEntities.each())
    {
        if (renderComponent.renderItemID.IsValid() && renderComponent.allocated)
        {
            engine->GetRenderDataWriteBuffer()->updateRenderItemTransformCommands.push_back({ renderComponent.renderItemID, transformComp.transform });
        }
    }
}

void ruya::RenderSystem::OnShutdown()
{
    auto transformEntities = scene->GetSceneViewByComponentsType<TransformComponent>();

    for (auto [entity, transformComp] : transformEntities.each())
    {
        transformComp.updatePosition = transformComp.position;
        transformComp.updateRotation = transformComp.rotation;
        transformComp.updateScale = transformComp.scale;
        transformComp.updateTransform = transformComp.transform;
    }

    auto renderEntities = scene->GetSceneViewByComponentsType<RenderComponent, TransformComponent>();

    for (auto [entity, renderComponent, transformComp] : renderEntities.each())
    {
        if (renderComponent.renderItemID.IsValid() && renderComponent.allocated)
        {
            engine->GetRenderDataWriteBuffer()->updateRenderItemTransformCommands.push_back({ renderComponent.renderItemID, transformComp.transform });
        }
    }
}

void ruya::RenderSystem::OnEngineUpdate()
{
    auto renderGeometryCreationEntities = scene->GetSceneViewByComponentsType<RenderComponent, TransformComponent>();

    for (auto [entity, renderComp, transformComp] : renderGeometryCreationEntities.each())
    {
        if (!renderComp.allocated)
        {
            glm::quat rotationQuat = transformComp.rotation;
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), transformComp.position)
                * glm::toMat4(rotationQuat)
                * glm::scale(glm::mat4(1.0f), transformComp.scale);

            transformComp.transform = transform;

            renderComp.renderItemID = engine->GetGraphics()->CreateRenderItem(transformComp.transform, renderComp.meshUUID, renderComp.materialUUID);
            renderComp.allocated = true;
        }
    }

	RenderData* renderData = engine->GetRenderDataWriteBuffer();
    renderData->camera.prevViewproj = engine->GetRenderDataReadBuffer()->camera.viewproj;
    renderData->camera.prevViewPos = engine->GetRenderDataReadBuffer()->camera.viewPos;
    renderData->camera.frameNumber = frameNumber;

	frameNumber++;
}

void ruya::RenderSystem::OnSceneLoad()
{
    auto entities = scene->GetSceneViewByComponentsType<RenderComponent, TransformComponent>();

	for (auto [entity, renderComp, transformComp] : entities.each())
	{
        if (!renderComp.allocated)
        {
            glm::quat rotationQuat = glm::quat(transformComp.rotation);
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), transformComp.position)
                * glm::toMat4(rotationQuat)
                * glm::scale(glm::mat4(1.0f), transformComp.scale);

            transformComp.transform = transform;

            renderComp.renderItemID = engine->GetGraphics()->CreateRenderItem(transformComp.transform, renderComp.meshUUID, renderComp.materialUUID);
            renderComp.allocated = true;
        }
	}
}

void ruya::RenderSystem::OnSceneUnload()
{
    auto entities = scene->GetSceneViewByComponentsType<RenderComponent>();

    for (auto [entity, renderComp] : entities.each())
    {
        if (renderComp.allocated)
        {
            engine->GetGraphics()->DestroyRenderItem(renderComp.renderItemID);
        }
    }
}
