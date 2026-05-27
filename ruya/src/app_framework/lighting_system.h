#pragma once
#include <vector>
#include <memory>

#include <graphics/atmospheric_light.h>

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

		AtmosphericLight* GetDirectionalLight() const;

	private:
		std::unique_ptr<AtmosphericLight> directionalLight;
	};
}