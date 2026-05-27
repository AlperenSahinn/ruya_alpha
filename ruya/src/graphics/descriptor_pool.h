#pragma once
#include <memory>

#include "ruya_vulkan/vulkan_descriptor_pool.h"

#include "descriptor_type.h"
#include "descriptor_set.h"
#include "descriptor_set_layout.h"

namespace ruya
{
	struct DescriptorPoolSizeRatio
	{
		DescriptorType descriptorType;
		float ratio;
	};

	class DescriptorPool
	{
	public:
		DescriptorPool(std::vector<DescriptorPoolSizeRatio>& descriptorPoolSizeRatios, uint32_t setCount);
		~DescriptorPool();

		DescriptorPool(const DescriptorPool&) = delete;
		DescriptorPool& operator=(const DescriptorPool&) = delete;

		VulkanDescriptorPool* GetVulkanDescriptorDescriptorPool() const;

		std::unique_ptr<DescriptorSet> AllocateDescriptorSet(DescriptorSetLayout* descriptorSetLayout, uint32_t count = 1, bool isVariableDescriptorCount = false);
		void FreeDescriptorSet(DescriptorSet* descriptorSet);

	private:
		std::unique_ptr<VulkanDescriptorPool> vulkanDescriptorPool;
	};
}