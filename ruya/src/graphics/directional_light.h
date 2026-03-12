#pragma once

#include <glm/glm.hpp>

namespace ruya
{
	struct DirectionalLight
	{
		glm::vec4 ambientColor = glm::vec4(0.68f, 0.83f, 0.94f, 0.2f); // ambientColor.a is ambient light intensity
		glm::vec4 sunlightDirection = glm::normalize(glm::vec4(0.5f, -1.0f, -0.5f, 0.0f));
		glm::vec4 sunlightColor = glm::vec4(1.0f, 1.0f, 1.0f, 3.0f); // sunlightColor.a is directional light intensity
		glm::vec4 pad;
	};
}