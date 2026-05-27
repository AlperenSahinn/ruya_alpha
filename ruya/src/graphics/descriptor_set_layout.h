#pragma once
#include <memory>

#include "ruya_vulkan/vulkan_descriptor_set_layout.h"

#include "descriptor_type.h"

namespace ruya
{
	class DescriptorSetLayout
	{
	public:
		DescriptorSetLayout();
		~DescriptorSetLayout();

		DescriptorSetLayout(const DescriptorSetLayout&) = delete;
		DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

		void AddBinding(DescriptorType descriptorType, uint32_t count);
		void Build();

		VulkanDescriptorSetLayout* GetVulkanDescriptorSetLayout() const;

	private:
		uint32_t bindingIndex;
		std::unique_ptr<VulkanDescriptorSetLayout> vulkanDescriptorSetLayout;
	};
}