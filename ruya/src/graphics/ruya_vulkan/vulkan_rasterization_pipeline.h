#pragma once
#include <vector>
#include <string>
#include <stdint.h>

#include <volk/volk.h>

#include "vulkan_descriptor_set_layout.h"

namespace ruya
{
	class VulkanRasterizationPipeline
	{
	public:
		VulkanRasterizationPipeline(
			VulkanContext* pVulkanContext,
			std::string vertexShaderPath,
			std::string fragmentShaderPath,
			std::vector<VulkanDescriptorSetLayout*>& descriptorSetLayouts,
			std::vector<VkFormat>& colorAttachments,
			VkFormat& depthAttachments,
			bool enableDepthTest,
			VkCompareOp depthCompareOp,
			std::vector<VkPushConstantRange>& pushConstantRanges,
			bool depthClipEnable = true);
		~VulkanRasterizationPipeline();

		VulkanRasterizationPipeline(const VulkanRasterizationPipeline&) = delete;
		VulkanRasterizationPipeline& operator=(const VulkanRasterizationPipeline&) = delete;

		VkPipeline GetDeviceHandle() const;
		VkPipelineLayout GetPipelineLayout() const;

	private:
		VkPipelineLayout pipelineLayout;
		VkPipeline deviceHandle;
		VkDevice device;
	};
}