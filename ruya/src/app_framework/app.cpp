#include "app.h"
#include <core/log.h>
#include <core/hash_code_generator.h>
#include <core/assert.h>

#include <tracy/tracy/Tracy.hpp>

void ruya::App::OnStart()
{
    ZoneScoped;

    for (auto& pair : loadedScenes)
    {
        Scene* scene = pair.second;
        scene->OnStart();
    }
}

void ruya::App::OnUpdate()
{
    ZoneScoped;

    for (auto& pair : loadedScenes)
    {
        Scene* scene = pair.second;
        scene->OnUpdate();
    }
}

void ruya::App::OnShutdown()
{
    ZoneScoped;

    for (auto& pair : loadedScenes)
    {
        Scene* scene = pair.second;
        scene->OnShutdown();
    }
}

void ruya::App::OnEngineUpdate()
{
    ZoneScoped;

    for (auto& pair : loadedScenes)
    {
        Scene* scene = pair.second;
        scene->OnEngineUpdate();
    }
}

ruya::RyID ruya::App::CreateScene(const std::string& name)
{
    RyID id(hash_code_generator::XXHash64(name));

    if (scenes.contains(id))
    {
        RUYA_LOG_WARN("This name is already in use by a scene. {}", name);
        return id;
    }

    scenes.insert({ id, std::make_unique<Scene>(name) });

    return id;
}

ruya::RyID ruya::App::RegisterScene(std::unique_ptr<Scene> scene)
{
    RyID id(hash_code_generator::XXHash64(scene->GetName()));

    if (scenes.contains(id))
    {
        RUYA_LOG_WARN("This id is already in use by a scene. {}", id);
        return id;
    }

    scenes.insert({ id, std::move(scene) });

    return id;
}

ruya::RyID ruya::App::GetSceneId(const std::string& name) const
{
    RyID id(hash_code_generator::XXHash64(name));
    if (scenes.contains(id))
        return id;

    ENGINE_ASSERT_MSG(false, "There is no scene named: {}", name);
    return RyID();
}

ruya::Scene* ruya::App::GetScene(RyID sceneId)
{
    if (!scenes.contains(sceneId))
    {
        ENGINE_ASSERT_MSG(false, "There is no scene with this id: {}", sceneId);
        return nullptr;
    }

    return scenes[sceneId].get();
}

void ruya::App::DestroyScene(RyID sceneId)
{
    if (!scenes.contains(sceneId)) 
        RUYA_LOG_WARN("There is no scene with this id: {}", sceneId);

    if (loadedScenes.contains(sceneId))
    {
        UnloadScene(sceneId);
    }

    scenes.erase(sceneId);
}

void ruya::App::LoadScene(RyID sceneId)
{
    if (!scenes.contains(sceneId))
    {
        RUYA_LOG_ERROR("There is no scene with this id: {}", sceneId);
        return;
    }

	if (loadedScenes.contains(sceneId))
	{
		RUYA_LOG_WARN("Scene is already loaded. Scene id: {}", sceneId);
		return;
	}

    loadedScenes.insert({ sceneId, scenes[sceneId].get()});
    scenes[sceneId]->OnSceneLoad();
    RUYA_LOG_INFO("Scene loaded: {}", scenes[sceneId]->GetName());
}

const std::vector<ruya::RyID> ruya::App::GetLoadedScenes() const
{
    std::vector<RyID> scenes;

    for (auto& pair : loadedScenes)
        scenes.push_back(pair.first);

    return scenes;
}

void ruya::App::UnloadScene(RyID sceneId)
{
    if (!scenes.contains(sceneId))
    {
        RUYA_LOG_ERROR("There is no scene with this id: {}", sceneId);
        return;
    }

    if (!loadedScenes.contains(sceneId))
    {
        RUYA_LOG_WARN("Scene is already unloaded. Scene id: {}", sceneId);
        return;
    }

    loadedScenes.erase(sceneId);

    scenes[sceneId]->OnSceneUnload();

    RUYA_LOG_INFO("Scene unloaded. {}", scenes[sceneId]->GetName());
}

bool ruya::App::HasScene(RyID sceneId) const
{
    return scenes.contains(sceneId);
}

void ruya::App::SetSceneAssetUUID(RyID sceneId, UUID uuid)
{
    sceneAssetMap[sceneId] = uuid;
}

ruya::UUID ruya::App::GetSceneAssetUUID(RyID sceneId) const
{
    if (sceneAssetMap.contains(sceneId))
        return sceneAssetMap.at(sceneId);
    return UUID::Invalid();
}
