#pragma once
#include <memory>
#include <stdint.h>

#include <volk/volk.h>

#include "vulkan_command_buffer.h"
#include "vulkan_descriptor_pool.h"

namespace ruya
{
	class VulkanContext;

	class VulkanFrameResource
	{
	public:
		void Create(VulkanContext* pVulkanContext);
		void Destroy(VulkanContext* pVulkanContext);

		VkCommandPool* GetCommandPool();
		VulkanCommandBuffer* GetCommandBuffer() const;
		VulkanDescriptorPool* GetDescriptorPool() const;
		VkFence* GetRenderFence();

	private:
		VkCommandPool commandPool;
		std::unique_ptr<VulkanCommandBuffer> commandBuffer;
		std::unique_ptr<VulkanDescriptorPool> descriptorPool;
		VkFence renderFence;
		VkDevice device;
	};
}