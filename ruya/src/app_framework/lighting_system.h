#pragma once
#include <vector>
#include <memory>

#include <graphics/directional_light.h>

#include "scene_system.h"

namespace ruya
{
	class LightingSystem : public SceneSystem
	{
	public:
		LightingSystem();
		~LightingSystem() = default;

		LightingSystem(const SceneSystem&) = delete;
		LightingSystem& operator=(const SceneSystem&) = delete;

		void OnEngineUpdate() override;

		DirectionalLight* GetDirectionalLight() const;

	private:
		std::unique_ptr<DirectionalLight> directionalLight;
	};
}