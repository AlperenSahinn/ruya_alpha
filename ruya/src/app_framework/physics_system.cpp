#include "physics_system.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp> 
#include <glm/gtx/transform.hpp>

#include <engine.h>

#include "physics_component.h"
#include "transform_component.h"
#include "render_component.h"
#include "relationship_component.h"

void ruya::PhysicsSystem::OnStart()
{
    auto transformEntities = scene->GetSceneViewByComponentsType<TransformComponent>();

    for (auto [entity, transformComp] : transformEntities.each())
    {
        transformComp.updatePosition = transformComp.position;
        transformComp.updateRotation = transformComp.rotation;
        transformComp.updateScale = transformComp.scale;
        transformComp.updateTransform = transformComp.transform;
    }

    auto physicsEntities = scene->GetSceneViewByComponentsType<TransformComponent, PhysicsComponent>();

    for (auto [entity, transformComp, physicsComp] : physicsEntities.each())
    {
        if (physicsComp.allocated && physicsComp.motionType == PhysicsMotionType::Dynamic && physicsComp.riggidBodyRyID.IsValid())
        {
            glm::vec3 rotatedOffset = transformComp.rotation * physicsComp.centerPosition;

            engine->GetPhysics()->SetRiggedBodyPositionAndRotation(
                physicsComp.riggidBodyRyID,
                transformComp.position + rotatedOffset,
                transformComp.rotation
            );

            engine->GetPhysics()->ResetRigidBodyVelocity(physicsComp.riggidBodyRyID);
        }
    }

    engine->GetPhysics()->StartSimulation();
}

void ruya::PhysicsSystem::OnUpdate()
{
    ZoneScoped;

    engine->GetPhysics()->UpdateSimulation();

    auto dynamicEntities = scene->GetSceneViewByComponentsType<PhysicsComponent, TransformComponent, RelationshipComponent>();

    std::vector<EntityID> parentEntities;

    for (auto [entity, physicsComp, transformComp, relationshipComp] : dynamicEntities.each())
    {
        if (!physicsComp.allocated) continue;
        if (physicsComp.motionType != PhysicsMotionType::Dynamic) continue;
        if (!physicsComp.riggidBodyRyID.IsValid()) continue;

        glm::vec3 physicsPos;
        glm::quat physicsRotQuat;

        engine->GetPhysics()->GetRiggedBodyPositionAndRotation(physicsComp.riggidBodyRyID, physicsPos, physicsRotQuat);

        glm::vec3 rotatedOffset = physicsRotQuat * physicsComp.centerPosition;

        transformComp.updatePosition = physicsPos - rotatedOffset;
        transformComp.updateRotation = physicsRotQuat;

        if (relationshipComp.parentEntity == entt::null)
        {
            parentEntities.push_back(entity);
        }
    }

    if (parentEntities.empty()) return;

    uint32_t threadCount = engine->GetJobScheduler()->ThreadCount();
    uint32_t totalParents = static_cast<uint32_t>(parentEntities.size());

    uint32_t jobCount = std::min(threadCount, totalParents);
    uint32_t chunkSize = (totalParents + jobCount - 1) / jobCount;

    std::vector<std::vector<UpdateRenderItemTransformCommand>> perTaskOutputs(jobCount);

    struct BatchUpdateTask
    {
        PhysicsSystem* sys;
        EntityID* parentsData;
        uint32_t startIdx;
        uint32_t endIdx;
        std::vector<UpdateRenderItemTransformCommand>* output;

        void operator()()
        {
            for (uint32_t i = startIdx; i < endIdx; ++i)
            {
                sys->UpdateTransformRecursive(parentsData[i], *output);
            }
        }
    };

    std::vector<BatchUpdateTask> tasks;
    tasks.reserve(jobCount);

    for (uint32_t i = 0; i < jobCount; ++i)
    {
        uint32_t start = i * chunkSize;
        uint32_t end = std::min(start + chunkSize, totalParents);

        if (start < end)
        {
            tasks.push_back({
                this,
                parentEntities.data(),
                start,
                end,
                &perTaskOutputs[i]
                });
        }
    }

    if (!tasks.empty())
    {
        ruya::job_system::Counter syncCounter;
        engine->GetJobScheduler()->SubmitBatch(syncCounter, tasks.begin(), tasks.end());
        engine->GetJobScheduler()->Wait(syncCounter);

        size_t totalCommands = 0;
        for (const auto& out : perTaskOutputs)
            totalCommands += out.size();

        if (totalCommands > 0)
        {
            RenderData* renderData = engine->GetRenderDataWriteBuffer();
            auto& targetQueue = renderData->updateRenderItemTransformCommands;
            size_t writeOffset = targetQueue.size();
            targetQueue.resize(writeOffset + totalCommands);

            for (const auto& out : perTaskOutputs)
            {
                size_t count = out.size();
                if (count > 0)
                {
                    std::memcpy(
                        targetQueue.data() + writeOffset,
                        out.data(),
                        count * sizeof(UpdateRenderItemTransformCommand)
                    );
                    writeOffset += count;
                }
            }
        }
    }
}

void ruya::PhysicsSystem::OnShutdown()
{
    auto transformEntities = scene->GetSceneViewByComponentsType<TransformComponent>();

    for (auto [entity, transformComp] : transformEntities.each())
    {
        transformComp.updatePosition = transformComp.position;
        transformComp.updateRotation = transformComp.rotation;
        transformComp.updateRotation = transformComp.rotation;
        transformComp.updateScale = transformComp.scale;
        transformComp.updateTransform = transformComp.transform;
    }

    auto physicsEntities = scene->GetSceneViewByComponentsType<TransformComponent, PhysicsComponent>();

    for (auto [entity, transformComp, physicsComp] : physicsEntities.each())
    {
        if (physicsComp.allocated && physicsComp.motionType == PhysicsMotionType::Dynamic && physicsComp.riggidBodyRyID.IsValid())
        {
            glm::vec3 rotatedOffset = transformComp.rotation * physicsComp.centerPosition;

            engine->GetPhysics()->SetRiggedBodyPositionAndRotation(
                physicsComp.riggidBodyRyID,
                transformComp.position + rotatedOffset,
                transformComp.rotation
            );

            engine->GetPhysics()->ResetRigidBodyVelocity(physicsComp.riggidBodyRyID);
        }
    }
}

void ruya::PhysicsSystem::OnEngineUpdate()
{
    ZoneScoped;

    auto renderGeometryCreationEntities = scene->GetSceneViewByComponentsType<PhysicsComponent, TransformComponent>();

    for (auto [entity, physicsComponent, transformComponent] : renderGeometryCreationEntities.each())
    {
        if (!physicsComponent.allocated)
        {
            if (RenderComponent* renderComponent = scene->TryGetComponent<RenderComponent>(entity))
            {
                const AABB& aabb = engine->GetGraphics()->GetMeshBuffer(engine->GetGraphics()->GetRenderItem(renderComponent->renderItemID)->GetMeshBufferRyID())->GetAABB();
                physicsComponent.halfWidhtHeightDepth = glm::vec3(abs(aabb.max.x - aabb.min.x), abs(aabb.max.y - aabb.min.y), abs(aabb.max.z - aabb.min.z)) / 2.0f;
                physicsComponent.centerPosition = (aabb.max + aabb.min) / 2.0f;
            }

            physicsComponent.riggidBodyRyID = engine->GetPhysics()->CreateRiggedBody(
                physicsComponent.halfWidhtHeightDepth,
                physicsComponent.centerPosition + transformComponent.position,
                transformComponent.rotation,
                physicsComponent.motionType);
            physicsComponent.allocated = true;
        }
    }

    engine->GetPhysics()->DebugDrawRiggedBodies();
}

void ruya::PhysicsSystem::OnSceneLoad()
{
    auto renderGeometryCreationEntities = scene->GetSceneViewByComponentsType<PhysicsComponent, TransformComponent>();

    for (auto [entity, physicsComponent, transformComponent] : renderGeometryCreationEntities.each())
    {
        if (!physicsComponent.allocated)
        {
            if (RenderComponent* renderComponent = scene->TryGetComponent<RenderComponent>(entity))
            {
                const AABB& aabb = engine->GetGraphics()->GetMeshBuffer(engine->GetGraphics()->GetRenderItem(renderComponent->renderItemID)->GetMeshBufferRyID())->GetAABB();
                physicsComponent.halfWidhtHeightDepth = glm::vec3(abs(aabb.max.x - aabb.min.x), abs(aabb.max.y - aabb.min.y), abs(aabb.max.z - aabb.min.z)) / 2.0f;
                physicsComponent.centerPosition = (aabb.max + aabb.min) / 2.0f;
            }

            physicsComponent.riggidBodyRyID = engine->GetPhysics()->CreateRiggedBody(
                physicsComponent.halfWidhtHeightDepth,
                physicsComponent.centerPosition + transformComponent.position,
                transformComponent.rotation,
                physicsComponent.motionType);
            physicsComponent.allocated = true;
        }
    }
}

void ruya::PhysicsSystem::OnSceneUnload()
{
    auto renderGeometryCreationEntities = scene->GetSceneViewByComponentsType<PhysicsComponent, TransformComponent>();

    for (auto [entity, physicsComponent, transformComponent] : renderGeometryCreationEntities.each())
    {
        if (physicsComponent.allocated)
        {
            engine->GetPhysics()->DestroyRiggedBody(physicsComponent.riggidBodyRyID);
            physicsComponent.allocated = false;
        }
    }
}

void ruya::PhysicsSystem::UpdateTransformRecursive(EntityID entity, std::vector<UpdateRenderItemTransformCommand>& updateRenderItemTransformCommands)
{
    ZoneScoped;

    if (entity == entt::null) return;

    TransformComponent* transformComp = scene->TryGetComponent<TransformComponent>(entity);
    RelationshipComponent* relationshipComp = scene->TryGetComponent<RelationshipComponent>(entity);

    glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), transformComp->updatePosition)
        * glm::toMat4(transformComp->updateRotation)
        * glm::scale(glm::mat4(1.0f), transformComp->updateScale);

    if (relationshipComp->parentEntity != entt::null)
    {
        TransformComponent* parentTransformComp = scene->TryGetComponent<TransformComponent>(
            static_cast<uint32_t>(relationshipComp->parentEntity)
        );
        transformComp->updateTransform = parentTransformComp->updateTransform * localTransform;
    }
    else
    {
        transformComp->updateTransform = localTransform;
    }

    if (RenderComponent* renderComponent = scene->TryGetComponent<RenderComponent>(entity))
    {
        if (renderComponent->renderItemID.IsValid() && renderComponent->allocated)
        {
            updateRenderItemTransformCommands.push_back({ renderComponent->renderItemID, transformComp->updateTransform });
        }
    }

    if (relationshipComp->firstChildEntity != entt::null)
    {
        UpdateTransformRecursive(relationshipComp->firstChildEntity, updateRenderItemTransformCommands);
    }

    if (relationshipComp->nextEntity != entt::null)
    {
        UpdateTransformRecursive(relationshipComp->nextEntity, updateRenderItemTransformCommands);
    }
}