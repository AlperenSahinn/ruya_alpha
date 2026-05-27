#pragma once
#include <vector>
#include <stdint.h>

#include <volk/volk.h>

namespace ruya
{
	class VulkanContext;

	struct VulkanDescriptorPoolSizeRatio
	{
		VkDescriptorType descriptorType;
		float ratio;
	};

	class VulkanDescriptorPool
	{
	public:
		VulkanDescriptorPool(VulkanContext* pVulkanContext, std::vector<VulkanDescriptorPoolSizeRatio> poolSizeRatios, uint32_t setCount);
		~VulkanDescriptorPool();

		VulkanDescriptorPool(const VulkanDescriptorPool&) = delete;
		VulkanDescriptorPool& operator=(const VulkanDescriptorPool&) = delete;

		VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout, uint32_t descriptorCount = 1, bool variableDescriptorCount = false);
		void FreeDescriptorSet(VkDescriptorSet descriptorSet);
		void Reset();

	private:
		VkDescriptorPool deviceHandle;
		std::vector<VulkanDescriptorPoolSizeRatio> descriptorPoolSizeRatios;
		VkDevice device;
	};
}