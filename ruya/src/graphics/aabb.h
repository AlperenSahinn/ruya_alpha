#pragma once
#include <glm/glm.hpp>
#include <nlohmann_json/json.hpp>

#include <core/nlohmann_glm.h>

namespace ruya
{
	struct AABB
	{
		glm::vec3 min;
		glm::vec3 max;
		float pad[2];

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(AABB, min, max)
	};
}