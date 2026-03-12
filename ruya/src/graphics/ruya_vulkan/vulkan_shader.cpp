#include "vulkan_shader.h"

#include <fstream>
#include <vector>
#include <stdint.h>

#include <core/log.h>

#include "vulkan_helpers.h"
#include "vulkan_context.h"

ruya::VulkanShader::VulkanShader(VulkanContext* pVulkanContext, const char* filePath)
{
	device = pVulkanContext->GetDevice();

	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		RUYA_LOG_ERROR("[File System] Failed to open shader file: %s", filePath);
	}

	size_t fileSize = (size_t)file.tellg();

	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	file.seekg(0);

	file.read((char*)buffer.data(), fileSize);

	file.close();

	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.pNext = nullptr;

	createInfo.codeSize = buffer.size() * sizeof(uint32_t);
	createInfo.pCode = buffer.data();

	CHECK_VKRESULT(vkCreateShaderModule(device, &createInfo, nullptr, &deviceHandle));
}

ruya::VulkanShader::~VulkanShader()
{
	vkDestroyShaderModule(device, deviceHandle, nullptr);
}

VkShaderModule ruya::VulkanShader::GetDeviceHandle() const
{
	return deviceHandle;
}