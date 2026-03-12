#pragma once
#include <stdint.h>

#include <glm/glm.hpp>

#include <core/ry_id.h>

#include "material.h"

namespace ruya
{
	struct RenderCommand
	{
		RyID renderGeometryId;
		RyID meshId;
		RyID materialId;
		glm::mat4 transform;
		bool updateBuffer;
	};
}