#pragma once
#pragma once
#include <unordered_map>
#include <memory>
#include <string>

#include <entt/entt.hpp>

#include <core/assert.h>

#include "scene_system.h"
#include "entity_id.h"

namespace ruya
{
	class Scene
	{
	public:
		Scene(const std::string& name);
		~Scene() = default;

		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;

		void SetName(const std::string& name);
		const std::string& GetName() const;

		EntityID CreateEntity();
		void DestroyEntity(EntityID entityId);

		bool IsEmpty() const;

		entt::basic_registry<EntityID>& GetENTTRegistry();

		template <typename T>
		void AddComponent(EntityID entityID)
		{
			registry.emplace<T>(entityID);
		}

		template <typename T>
		void RemoveComponent(EntityID entityID)
		{
			registry.erase<T>(entityID);
		}

		template <typename T>
		T* TryGetComponent(EntityID entityID)
		{
			return registry.try_get<T>(entityID);
		}

		template <typename T>
		bool HasComponent(EntityID entityID) const
		{
			return registry.all_of<T>(entityID);
		}

		template<typename... Components>
		auto GetSceneViewByComponentsType()
		{
			return registry.view<Components...>();
		}

		template<typename... Components>
		auto GetEntityGroupByComponentsType()
		{
			return registry.group<Components...>();
		}

		template<typename T>
		void AddSceneSystem(const std::string& systemName)
		{
			ENGINE_STATIC_ASSERT_MSG(std::is_base_of_v<SceneSystem, T>, "T must derive from SceneSystem class");

			if (sceneSystems.contains(systemName))
				return;

			std::unique_ptr<T> sceneSystem = std::make_unique<T>();

			sceneSystem->SetScene(*this);

			sceneSystems.insert({ systemName , std::move(sceneSystem) });
		}

		void RemoveSceneSystem(const std::string& systemName);

		template<typename T>
		T* TryGetSceneSystem(const std::string& sceneSystem)
		{
			if (!sceneSystems.contains(sceneSystem))
				return nullptr;

			return static_cast<T*>(sceneSystems[sceneSystem].get());
		}

		void OnStart();
		void OnUpdate();
		void OnShutdown();

		void OnEngineUpdate();

		void OnSceneLoad();
		void OnSceneUnload();

	private:
		std::string name;
		entt::basic_registry<EntityID> registry;
		std::unordered_map<std::string, std::unique_ptr<SceneSystem>> sceneSystems;
	};
}