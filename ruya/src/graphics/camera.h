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
		glm::mat4 invViewproj;
		glm::mat4 prevViewproj;
		glm::vec4 prevViewPos;

		float fov = 60.0f;
		uint32_t width = 1600;
		uint32_t height = 900;
		float cameraNear = 0.1f;
		float cameraFar = 100.0f;
		uint32_t frameNumber;
		float sampleCount;
		int pathTracerSampleCount = 1;
	};
}