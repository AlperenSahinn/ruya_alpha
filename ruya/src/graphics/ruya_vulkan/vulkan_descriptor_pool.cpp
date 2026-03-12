#include "vulkan_descriptor_pool.h"
#include "vulkan_helpers.h"
#include "vulkan_context.h"

ruya::VulkanDescriptorPool::VulkanDescriptorPool(VulkanContext* pVulkanContext, std::vector<VulkanDescriptorPoolSizeRatio> poolSizeRatios, uint32_t setCount)
{
	device = pVulkanContext->GetDevice();

	descriptorPoolSizeRatios = poolSizeRatios;

	std::vector<VkDescriptorPoolSize> poolSizes;
	for (VulkanDescriptorPoolSizeRatio descriptorPoolSizeRatio : descriptorPoolSizeRatios)
	{
		poolSizes.push_back(VkDescriptorPoolSize
			{
			.type = descriptorPoolSizeRatio.descriptorType,
			.descriptorCount = uint32_t(descriptorPoolSizeRatio.ratio)
			});
	}

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	descriptorPoolCreateInfo.maxSets = setCount;
	descriptorPoolCreateInfo.poolSizeCount = (uint32_t)poolSizes.size();
	descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();

	vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &deviceHandle);
}

ruya::VulkanDescriptorPool::VulkanDescriptorPool::~VulkanDescriptorPool()
{
	Reset();

	vkDestroyDescriptorPool(device, deviceHandle, nullptr);
}

VkDescriptorSet ruya::VulkanDescriptorPool::AllocateDescriptorSet(VkDescriptorSetLayout layout, bool variableDescriptorCount, uint32_t descriptorCount)
{
	VkDescriptorSet descriptorSet;

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.descriptorPool = deviceHandle;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &layout;

	if (variableDescriptorCount)
	{
		VkDescriptorSetVariableDescriptorCountAllocateInfo descriptorSetVariableDescriptorCountAllocateInfo = {};
		descriptorSetVariableDescriptorCountAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
		descriptorSetVariableDescriptorCountAllocateInfo.descriptorSetCount = 1;
		descriptorSetVariableDescriptorCountAllocateInfo.pDescriptorCounts = &descriptorCount;

		descriptorSetAllocateInfo.pNext = &descriptorSetVariableDescriptorCountAllocateInfo;
	}

	CHECK_VKRESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

	return descriptorSet;
}

void ruya::VulkanDescriptorPool::FreeDescriptorSet(VkDescriptorSet descriptorSet)
{
	CHECK_VKRESULT(vkFreeDescriptorSets(device, deviceHandle, 1, &descriptorSet));
}

void ruya::VulkanDescriptorPool::Reset()
{
	vkResetDescriptorPool(device, deviceHandle, 0);
}