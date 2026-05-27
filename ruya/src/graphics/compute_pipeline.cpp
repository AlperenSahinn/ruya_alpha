#include "compute_pipeline.h"

#include <engine.h>

ruya::ComputePipeline::ComputePipeline(const std::string& computeShaderPathconst, std::vector<DescriptorSetLayout*>& pDescriptorSetLayouts, std::vector<VkPushConstantRange> pushConstantRanges)
{
	std::vector<VulkanDescriptorSetLayout*> vulkanDescriptorSetLayouts;

	for (DescriptorSetLayout* descriptorSetLayout : pDescriptorSetLayouts)
	{
		vulkanDescriptorSetLayouts.push_back(descriptorSetLayout->GetVulkanDescriptorSetLayout());
	}

	vulkanComputePipeline = std::make_unique<VulkanComputePipeline>(
		engine->GetGraphics()->GetVulkanContext(),
		computeShaderPathconst,
		vulkanDescriptorSetLayouts,
		pushConstantRanges);
}

void ruya::ComputePipeline::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ, std::vector<Texture2D*> imageReads, std::vector<Texture2D*> imageWrites, const std::vector<DescriptorSet*>& pDescriptorSets, uint32_t pushConstantSize, void* pushConstantData)
{
	VkImageSubresourceRange colorRange{};
	colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorRange.baseMipLevel = 0;
	colorRange.levelCount = 1;
	colorRange.baseArrayLayer = 0;
	colorRange.layerCount = 1;

	VulkanCommandBuffer* commandBuffer = engine->GetGraphics()->GetVulkanContext()->GetCurrentFrameResource()->GetCommandBuffer();

	for(Texture2D* texture2D : imageReads)
	{
		commandBuffer->ImageMemoryBarrier(
			texture2D->GetVulkanImage(),
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			colorRange
		);
	}

	for (Texture2D* texture2D : imageWrites)
	{
		commandBuffer->ImageMemoryBarrier(
			texture2D->GetVulkanImage(),
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			colorRange
		);
	}

	std::vector<VkDescriptorSet> descriptorSets;
	for (DescriptorSet* descriptorSet : pDescriptorSets)
	{
		descriptorSets.push_back(descriptorSet->vkDescriptorSet);
	}

	commandBuffer->BindDescriptorSets(descriptorSets, vulkanComputePipeline->GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
	
	if(pushConstantSize != 0)
		commandBuffer->PushConstant(pushConstantData, pushConstantSize, vulkanComputePipeline->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT);

	commandBuffer->DispatchComputePipeline(vulkanComputePipeline.get(), descriptorSets, groupCountX, groupCountY, groupCountZ);
}
