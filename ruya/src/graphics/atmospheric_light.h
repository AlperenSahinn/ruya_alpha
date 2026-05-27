#pragma once

#include <glm/glm.hpp>

namespace ruya
{
	struct AtmosphericLight
	{
		glm::vec4 sunDirection = glm::vec4(0.5f, -0.5f, -0.5f, 0.0f);
		glm::vec4 sunColor = glm::vec4(1.0f, 0.98f, 0.95f, 10.0f);            // .a = sun intensity
		glm::vec4 skyParams = glm::vec4(0.9998f, 0.9999f, 5.0f, 0.002f);           // .x = sun disk size, .y = sun disk sharpness, .z = sky exposure, .w = fog density
		glm::vec4 groundAlbedo = glm::vec4(0.18f, 0.18f, 0.18f, 0.1f);        // .rgb = ground color, .a = ground reflectance
		glm::vec4 scatterParams = glm::vec4(21.0e-6f, 0.76f, 8000.0f, 1200.0f);       // .x = mie beta, .y = mie g, .z = rayleigh height, .w = mie height
	};
}