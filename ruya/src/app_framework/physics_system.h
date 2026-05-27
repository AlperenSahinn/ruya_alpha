#pragma once
#include <vector>
#include "scene.h"
#include "scene_system.h"

#include <graphics/render_data.h>

namespace ruya
{
	class PhysicsSystem : public SceneSystem
	{
	public:
		PhysicsSystem() = default;
		~PhysicsSystem() = default;

		PhysicsSystem(const PhysicsSystem&) = delete;
		PhysicsSystem& operator=(const PhysicsSystem&) = delete;
		
		void OnStart() override;
		void OnUpdate() override;
		void OnShutdown() override;

		void OnEngineUpdate() override;

		void OnSceneLoad() override;
		void OnSceneUnload() override;

	private:
		void UpdateTransformRecursive(EntityID entity, std::vector<UpdateRenderItemTransformCommand>& updateRenderItemTransformCommands);
	};
}

