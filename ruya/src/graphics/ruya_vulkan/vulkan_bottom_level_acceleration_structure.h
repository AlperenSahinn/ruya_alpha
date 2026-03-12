#pragma once
#include <memory>

#include "volk/volk.h"

#include "vulkan_buffer.h"

namespace ruya
{
	class VulkanContext;

	class VulkanBottomLevelAccelerationStructure
	{
	public:
		VulkanBottomLevelAccelerationStructure(
			VulkanContext* pVulkanContext,
			VulkanBuffer& vertexBuffer, 
			uint32_t vertexCount, 
			VkDeviceSize vertexSize, 
			VulkanBuffer& indexBuffer, 
			uint32_t indexCount);
		~VulkanBottomLevelAccelerationStructure();

		VulkanBottomLevelAccelerationStructure(const VulkanBottomLevelAccelerationStructure&) = delete;
		VulkanBottomLevelAccelerationStructure& operator=(const VulkanBottomLevelAccelerationStructure&) = delete;

		VkAccelerationStructureKHR GetDeviceHandle() const;
		VkDeviceAddress GetDeviceAddress() const;

	private:
		VkAccelerationStructureKHR deviceHandle;
		std::unique_ptr<VulkanBuffer> buffer;
		VkDeviceAddress deviceAddress;

		VkDevice device;
	};
}