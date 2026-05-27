#pragma once
#include <cstdint>
#include <queue>
#include <memory>

#include <core/uuid.h>

#include "camera.h"
#include "atmospheric_light.h"
#include "debug_line_pipeline.h"

namespace ruya
{	
	struct UpdateRenderItemTransformCommand
	{
		RyID renderItemRyID = RyID::Invalid();
		glm::mat4 transform = glm::mat4(1.0f);
	};

	struct UpdateRenderItemMaterialCommand
	{
		RyID renderItemRyID = RyID::Invalid();
		UUID materialUUID = UUID::Invalid();
	};

	struct RenderData
	{
		Camera camera;
		AtmosphericLight directionalLight;
		std::vector<DebugVertex> debugVertexLines;
		std::vector<UpdateRenderItemTransformCommand> updateRenderItemTransformCommands;
		std::vector<UpdateRenderItemMaterialCommand> updateRenderItemMaterialCommand;
	};
}