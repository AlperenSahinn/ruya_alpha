#include "descriptor_set_layout.h"

#include <engine.h>

ruya::DescriptorSetLayout::DescriptorSetLayout()
{
	bindingIndex = 0;

	vulkanDescriptorSetLayout = std::make_unique<VulkanDescriptorSetLayout>(engine->GetGraphics()->GetVulkanContext());
}

ruya::DescriptorSetLayout::~DescriptorSetLayout()
{
}

void ruya::DescriptorSetLayout::AddBinding(DescriptorType descriptorType, uint32_t count)
{
	if (descriptorType == DescriptorType::STORAGE_IMAGE)
		vulkanDescriptorSetLayout->AddBinding(bindingIndex, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, count);

	else if (descriptorType == DescriptorType::STORAGE_BUFFER)
		vulkanDescriptorSetLayout->AddBinding(bindingIndex, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, count);

	else if (descriptorType == DescriptorType::COMBINED_IMAGE_SAMPLER)
		vulkanDescriptorSetLayout->AddBinding(bindingIndex, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, count);

	else if (descriptorType == DescriptorType::UNIFORM_BUFFER)
		vulkanDescriptorSetLayout->AddBinding(bindingIndex, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, count);

	else if (descriptorType == DescriptorType::ACCELERATION_STRUCTURE)
		vulkanDescriptorSetLayout->AddBinding(bindingIndex, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, count);

	bindingIndex++;
}

void ruya::DescriptorSetLayout::Build()
{
	std::vector<VkDescriptorBindingFlags> bindingFlags;
	
	for(int i = 0; i < bindingIndex; i++)
	{
		bindingFlags.push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
	}
	
	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();

	vulkanDescriptorSetLayout->Build(
		VK_SHADER_STAGE_ALL,
		&descriptorSetLayoutBindingFlagsCreateInfo,
		VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
}

ruya::VulkanDescriptorSetLayout* ruya::DescriptorSetLayout::GetVulkanDescriptorSetLayout() const
{
	return vulkanDescriptorSetLayout.get();
}
