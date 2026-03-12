#include "vulkan_compute_pipeline.h"

#include <memory>

#include "vulkan_context.h"
#include "vulkan_helpers.h"
#include "vulkan_shader.h"

ruya::VulkanComputePipeline::VulkanComputePipeline(
	VulkanContext* pVulkanContext,
	std::string computeShaderPath, 
	std::vector<VulkanDescriptorSetLayout*>& descriptorSetLayouts, 
	std::vector<VkPushConstantRange>& pushConstantRanges)
{
	device = pVulkanContext->GetDevice();

	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pushConstantRangeCount = pushConstantRanges.size();
	computeLayout.pPushConstantRanges = pushConstantRanges.data();
	computeLayout.setLayoutCount = descriptorSetLayouts.size();

	std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts;

	for (VulkanDescriptorSetLayout* layout : descriptorSetLayouts)
	{
		vkDescriptorSetLayouts.push_back(layout->GetDeviceHandle());
	}

	computeLayout.pSetLayouts = vkDescriptorSetLayouts.data();

	CHECK_VKRESULT(vkCreatePipelineLayout(device, &computeLayout, nullptr, &pipelineLayout));

	std::unique_ptr<VulkanShader> computeShader = std::make_unique<VulkanShader>(pVulkanContext, computeShaderPath.c_str());

	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = computeShader->GetDeviceHandle();
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = pipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;

	CHECK_VKRESULT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &deviceHandle));
}

const VkPipeline& ruya::VulkanComputePipeline::GetDeviceHandle()
{
	return deviceHandle;
}

VkPipelineLayout& ruya::VulkanComputePipeline::GetPipelineLayout()
{
	return pipelineLayout;
}

ruya::VulkanComputePipeline::~VulkanComputePipeline()
{
	vkDestroyPipeline(device, deviceHandle, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
}

