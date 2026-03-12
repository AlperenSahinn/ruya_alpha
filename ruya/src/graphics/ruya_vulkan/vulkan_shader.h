#pragma once

#include <volk/volk.h>

namespace ruya
{
	class VulkanContext;

	class VulkanShader
	{
	public:
		VulkanShader(VulkanContext* pVulkanContext, const char* filePath);
		~VulkanShader();

		VulkanShader(const VulkanShader&) = delete;
		VulkanShader& operator=(const VulkanShader&) = delete;

		VkShaderModule GetDeviceHandle() const;

	private:
		VkShaderModule deviceHandle;
		VkDevice device;
	};
}