#include "lighting_system.h"

#include <engine.h>
#include <graphics/render_data.h>

ruya::LightingSystem::LightingSystem()
{
	directionalLight = std::make_unique<DirectionalLight>();
}

void ruya::LightingSystem::OnEngineUpdate()
{
	RenderData* renderData = engine->GetRenderDataWriteBuffer();
	renderData->directionalLight.sunlightDirection = directionalLight->sunlightDirection;
	renderData->directionalLight.sunlightColor = directionalLight->sunlightColor;
	renderData->directionalLight.ambientColor = directionalLight->ambientColor;
	renderData->directionalLight.pad = directionalLight->pad;
}

ruya::DirectionalLight* ruya::LightingSystem::GetDirectionalLight() const
{
	return directionalLight.get();
}
