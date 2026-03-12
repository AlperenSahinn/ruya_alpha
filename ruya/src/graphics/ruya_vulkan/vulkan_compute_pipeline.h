#pragma once
#include <string>
#include <vector>

#include <volk/volk.h>

#include "vulkan_descriptor_set_layout.h"

namespace ruya
{
	class VulkanContext;

	class VulkanComputePipeline
	{
	public:
		VulkanComputePipeline(
			VulkanContext* pVulkanContext,
			std::string computeShaderPath,
			std::vector<VulkanDescriptorSetLayout*>& descriptorSetLayouts,
			std::vector<VkPushConstantRange>& pushConstantRanges);
		~VulkanComputePipeline();

		VulkanComputePipeline(const VulkanComputePipeline&) = delete;
		VulkanComputePipeline& operator=(const VulkanComputePipeline&) = delete;

		const VkPipeline& GetDeviceHandle();
		VkPipelineLayout& GetPipelineLayout();

	private:
		VkPipelineLayout pipelineLayout;
		VkPipeline deviceHandle;

		VkDevice device;
	};
}