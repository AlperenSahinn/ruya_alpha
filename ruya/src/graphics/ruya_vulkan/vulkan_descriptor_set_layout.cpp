#include "vulkan_descriptor_set_layout.h"

#include "vulkan_context.h"
#include "vulkan_helpers.h"

ruya::VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VulkanContext* pVulkanContext)
{
	device = pVulkanContext->GetDevice();
}

ruya::VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
{
	vkDestroyDescriptorSetLayout(device, deviceHandle, nullptr);
}

void ruya::VulkanDescriptorSetLayout::AddBinding(uint32_t binding, VkDescriptorType descriptorType, uint32_t descriptorCount)
{
	VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {};
	descriptorSetLayoutBinding.binding = binding;
	descriptorSetLayoutBinding.descriptorType = descriptorType;
	descriptorSetLayoutBinding.descriptorCount = descriptorCount;
	bindings.push_back(descriptorSetLayoutBinding);
}

void ruya::VulkanDescriptorSetLayout::Build(VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags descriptorSetLayoutCreateFlags)
{
	for (auto& binding : bindings)
	{
		binding.stageFlags |= shaderStages;
	}

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	descriptorSetLayoutCreateInfo.pNext = pNext;
	descriptorSetLayoutCreateInfo.pBindings = bindings.data();
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	descriptorSetLayoutCreateInfo.flags = descriptorSetLayoutCreateFlags;

	CHECK_VKRESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &deviceHandle));
}

VkDescriptorSetLayout ruya::VulkanDescriptorSetLayout::GetDeviceHandle() const
{
	return deviceHandle;
}
