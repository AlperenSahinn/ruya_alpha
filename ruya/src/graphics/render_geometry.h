#pragma once
#include <volk/volk.h>
#include <glm/glm.hpp>

#include <core/ry_id.h>

namespace ruya
{
	struct RenderGeometry
	{
		glm::mat4 transform;
		VkDeviceAddress vertexBufferAddress;
		VkDeviceAddress indexBufferAddress;
		uint32_t materialDescriptorIndex;
		RyID meshId;
		RyID materialId;
		bool draw;
	};

	struct RenderGeometryGPU
	{
		glm::mat4 transform;
		VkDeviceAddress vertexBufferAddress;
		VkDeviceAddress indexBufferAddress;
		uint32_t materialDescriptorIndex;
		uint32_t pad[3];
	};
}