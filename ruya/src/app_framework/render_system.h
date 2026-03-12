#pragma once
#include <vector>

#include "scene.h"
#include "scene_system.h"

namespace ruya
{
	class RenderSystem : public SceneSystem
	{
	public:
		RenderSystem() = default;
		~RenderSystem() = default;

		RenderSystem(const SceneSystem&) = delete;
		RenderSystem& operator=(const SceneSystem&) = delete;

		void OnEngineUpdate() override;

		void OnSceneLoad() override;
		void OnSceneUnload() override;

	private:
		void UpdateTransformRecursive(EntityID entity);

	private:
		uint32_t frameNumber = 0;
	};
}
