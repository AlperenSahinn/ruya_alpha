#include "vulkan_frame_resource.h"

#include "vulkan_helpers.h"
#include "vulkan_context.h"

void ruya::VulkanFrameResource::Create(VulkanContext* pVulkanContext)
{
	device = pVulkanContext->GetDevice();

	VkCommandPoolCreateInfo commandPoolInfo = {};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.pNext = nullptr;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolInfo.queueFamilyIndex = pVulkanContext->GetDeviceQueueFamilyIndex();

	CHECK_VKRESULT(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool));

	commandBuffer = std::make_unique<VulkanCommandBuffer>(pVulkanContext, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	std::vector<VulkanDescriptorPoolSizeRatio> descriptorPoolSizeRatio =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 128 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 128 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128},
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 128 },
	};

	descriptorPool = std::make_unique<VulkanDescriptorPool>(pVulkanContext, descriptorPoolSizeRatio, 512);

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	CHECK_VKRESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &renderFence));
}

void ruya::VulkanFrameResource::Destroy(VulkanContext* pVulkanContext)
{
	vkDestroyFence(device, renderFence, nullptr);
	vkDestroyCommandPool(device, commandPool, nullptr);
}

VkCommandPool* ruya::VulkanFrameResource::GetCommandPool()
{
	return &commandPool;
}

ruya::VulkanCommandBuffer* ruya::VulkanFrameResource::GetCommandBuffer() const
{
	return commandBuffer.get();
}

ruya::VulkanDescriptorPool* ruya::VulkanFrameResource::GetDescriptorPool() const
{
	return descriptorPool.get();
}

VkFence* ruya::VulkanFrameResource::GetRenderFence()
{
	return &renderFence;
}
