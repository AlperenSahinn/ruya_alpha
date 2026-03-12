#pragma once
#include <vector>

#include <glm/glm.hpp>

namespace ruya
{
	struct Camera
	{
		glm::mat4 view;
		glm::vec4 viewPos;
		glm::mat4 proj;
		glm::mat4 viewproj;

		float fov = 60.0f;
		float width = 1600.0f;
		float height = 900.0f;
		float cameraNear = 0.1f;
		float cameraFar = 100.0f;
		float frameNumber;
		float sampleCount;
		float pad2;
	};
}