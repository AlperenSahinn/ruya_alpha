#pragma once

namespace ruya
{
	class Scene;

	class SceneSystem
	{
	public:
		SceneSystem() = default;
		~SceneSystem() = default;

		SceneSystem(const SceneSystem&) = delete;
		SceneSystem& operator=(const SceneSystem&) = delete;

		virtual void OnStart() {}
		virtual void OnUpdate() {}
		virtual void OnShutdown() {}

		virtual void OnEngineUpdate() {}

		virtual void OnSceneLoad() {}
		virtual void OnSceneUnload() {}

		inline void SetScene(Scene& scene) { this->scene = &scene; }

	protected:
		Scene* scene;
	};
}