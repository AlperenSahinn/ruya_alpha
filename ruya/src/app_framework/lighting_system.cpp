#include "lighting_system.h"

#include <engine.h>
#include <graphics/render_data.h>

ruya::LightingSystem::LightingSystem()
{
	directionalLight = std::make_unique<AtmosphericLight>();
}

void ruya::LightingSystem::OnEngineUpdate()
{
	RenderData* renderData = engine->GetRenderDataWriteBuffer();
	renderData->directionalLight = *directionalLight.get();
}

ruya::AtmosphericLight* ruya::LightingSystem::GetDirectionalLight() const
{
	return directionalLight.get();
}
