#pragma once
#include <unordered_map>
#include <string>
#include <cstdint>
#include <memory>
#include <vector>

#include <core/ry_id.h>
#include <core/uuid.h>

#include "scene.h"

namespace ruya
{
	enum class AppState
	{
		AppStart,
		AppUpdate,
		AppPause,
		AppShutdown,
		AppReady
	};

	class App
	{
	public:
		App() = default;
		virtual ~App() = default;

		App(const App&) = delete;
		App& operator=(const App&) = delete;

		virtual void Init() = 0;

		void OnStart();
		void OnUpdate();
		void OnShutdown();

		void OnEngineUpdate();

		RyID CreateScene(const std::string& name);
		RyID RegisterScene(std::unique_ptr<Scene> scene);
		RyID GetSceneId(const std::string& name) const;
		Scene* GetScene(RyID sceneId);
		void DestroyScene(RyID sceneId);

		void LoadScene(RyID sceneId);
		const std::vector<RyID> GetLoadedScenes() const;
		void UnloadScene(RyID sceneId);

		bool HasScene(RyID sceneId) const;
		void SetSceneAssetUUID(RyID sceneId, UUID uuid);
		UUID GetSceneAssetUUID(RyID sceneId) const;

	private:
		std::unordered_map<RyID, std::unique_ptr<Scene>> scenes;
		std::unordered_map<RyID, Scene*> loadedScenes;

		std::unordered_map<RyID, UUID> sceneAssetMap;
	};
}