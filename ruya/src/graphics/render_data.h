#pragma once
#include <cstdint>
#include <queue>
#include <memory>

#include "render_command.h"
#include "camera.h"
#include "directional_light.h"

namespace ruya
{	
	struct RenderData
	{
		Camera camera;
		DirectionalLight directionalLight;
		std::vector<RenderCommand> renderCommands;
	};
}