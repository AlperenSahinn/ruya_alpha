#include "descriptor_pool.h"

#include <engine.h>

ruya::DescriptorPool::DescriptorPool(std::vector<DescriptorPoolSizeRatio>& descriptorPoolSizeRatios, uint32_t setCount)
{
	std::vector<VulkanDescriptorPoolSizeRatio> vulkanDescriptorPoolSizeRatios;

	for(DescriptorPoolSizeRatio& descriptorPoolSizeRatio : descriptorPoolSizeRatios)
	{
		if (descriptorPoolSizeRatio.descriptorType == DescriptorType::STORAGE_BUFFER)
			vulkanDescriptorPoolSizeRatios.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorPoolSizeRatio.ratio });

		else if (descriptorPoolSizeRatio.descriptorType == DescriptorType::COMBINED_IMAGE_SAMPLER)
			vulkanDescriptorPoolSizeRatios.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorPoolSizeRatio.ratio });

		else if (descriptorPoolSizeRatio.descriptorType == DescriptorType::ACCELERATION_STRUCTURE)
			vulkanDescriptorPoolSizeRatios.push_back({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, descriptorPoolSizeRatio.ratio });
	}

	vulkanDescriptorPool = std::make_unique<VulkanDescriptorPool>(engine->GetGraphics()->GetVulkanContext(), vulkanDescriptorPoolSizeRatios, setCount);
}

ruya::DescriptorPool::~DescriptorPool()
{
}

ruya::VulkanDescriptorPool* ruya::DescriptorPool::GetVulkanDescriptorDescriptorPool() const
{
	return vulkanDescriptorPool.get();
}

std::unique_ptr<ruya::DescriptorSet> ruya::DescriptorPool::AllocateDescriptorSet(DescriptorSetLayout* descriptorSetLayout, uint32_t descriptorCount, bool isVariableDescriptorCount)
{
	std::unique_ptr<DescriptorSet> descriptorSet = std::make_unique<DescriptorSet>();

	descriptorSet->vkDescriptorSet = vulkanDescriptorPool->AllocateDescriptorSet(
			descriptorSetLayout->GetVulkanDescriptorSetLayout()->GetDeviceHandle(), 
			isVariableDescriptorCount,
			descriptorCount);

	return std::move(descriptorSet);
}

void ruya::DescriptorPool::FreeDescriptorSet(DescriptorSet* descriptorSet)
{
	vulkanDescriptorPool->FreeDescriptorSet(descriptorSet->vkDescriptorSet);
}
