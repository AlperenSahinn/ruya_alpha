#include "Scene.h"

#include <engine.h>
#include "id_component.h"
#include "relationship_component.h"
#include "transform_component.h"

#include "render_system.h"
#include "lighting_system.h"
#include "physics_system.h"

ruya::Scene::Scene(const std::string& name)
    : name(name)
{
    AddSceneSystem<PhysicsSystem>("PhysicsSystem");
    AddSceneSystem<LightingSystem>("LightingSystem");
    AddSceneSystem<RenderSystem>("RenderSystem");
}

const std::string& ruya::Scene::GetName() const
{
    return name;
}

void ruya::Scene::SetName(const std::string& name)
{
    this->name = name;
}

ruya::EntityID ruya::Scene::CreateEntity()
{
    EntityID entity = registry.create();

    AddComponent<IdComponent>(static_cast<uint32_t>(entity));
    IdComponent* idComponent = TryGetComponent<IdComponent>(static_cast<uint32_t>(entity));
    idComponent->name = "New Entity";
    idComponent->enttID = entity;

    AddComponent<RelationshipComponent>(static_cast<uint32_t>(entity));
    AddComponent<TransformComponent>(static_cast<uint32_t>(entity));

    return static_cast<std::uint32_t>(entity);
}

void ruya::Scene::DestroyEntity(EntityID entity)
{
    registry.destroy(entity);
}

bool ruya::Scene::IsEmpty() const
{
    return registry.view<IdComponent>().empty();
}

entt::basic_registry<ruya::EntityID>& ruya::Scene::GetENTTRegistry()
{
    return registry;
}

void ruya::Scene::RemoveSceneSystem(const std::string& systemName)
{
    if (!sceneSystems.contains(systemName))
        return;

    sceneSystems.erase(systemName);
}

void ruya::Scene::OnStart()
{
    ZoneScoped;
    const std::string fullName = "Scene: " + name;
    ZoneName(fullName.c_str(), fullName.size());

    for (auto& [type, systemPtr] : sceneSystems)
    {
        systemPtr->OnStart();
    }
}

void ruya::Scene::OnUpdate()
{
    ZoneScoped;
    const std::string fullName = "Scene: " + name;
    ZoneName(fullName.c_str(), fullName.size());

    for (auto& [type, systemPtr] : sceneSystems)
    {
        systemPtr->OnUpdate();
    }
}

void ruya::Scene::OnShutdown()
{
    ZoneScoped;
    const std::string fullName = "Scene: " + name;
    ZoneName(fullName.c_str(), fullName.size());

    for (auto& [type, systemPtr] : sceneSystems)
    {
        systemPtr->OnShutdown();
    }
}

void ruya::Scene::OnEngineUpdate()
{
    ZoneScoped;
    const std::string fullName = "Scene: " + name;
    ZoneName(fullName.c_str(), fullName.size());

    for (auto& [type, systemPtr] : sceneSystems)
    {
        systemPtr->OnEngineUpdate();
    }
}

void ruya::Scene::OnSceneLoad()
{
    ZoneScoped;
    const std::string fullName = "Scene: " + name;
    ZoneName(fullName.c_str(), fullName.size());

    for (auto& [type, systemPtr] : sceneSystems)
    {
        systemPtr->OnSceneLoad();
    }
}

void ruya::Scene::OnSceneUnload()
{
    ZoneScoped;
    const std::string fullName = "Scene: " + name;
    ZoneName(fullName.c_str(), fullName.size());

    for (auto& [type, systemPtr] : sceneSystems)
    {
        systemPtr->OnSceneUnload();
    }
}
