#include "ray_tracing_pipeline.h"

#include <engine.h>

#include "ruya_vulkan/vulkan_descriptor_set_layout.h"

ruya::RayTracingPipeline::RayTracingPipeline(
	const std::string& rayGenShaderPath,
	const std::string& anyHitShaderPath,
	const std::string& closestHitShaderPath,
	const std::vector<std::string>& rayMissShaderPaths,
	const std::vector<DescriptorSetLayout*>& pDescriptorSetLayouts)
{
	std::vector<VulkanDescriptorSetLayout*> vulkanDescriptorSetLayouts;

	for (DescriptorSetLayout* descriptorSetLayout : pDescriptorSetLayouts)
	{
		vulkanDescriptorSetLayouts.push_back(descriptorSetLayout->GetVulkanDescriptorSetLayout());
	}

	vulkanRayTracingPipeline = std::make_unique<VulkanRayTracingPipeline>(
		engine->GetGraphics()->GetVulkanContext(),
		rayGenShaderPath,
		rayMissShaderPaths,
		anyHitShaderPath,
		closestHitShaderPath,
		vulkanDescriptorSetLayouts);
}

void ruya::RayTracingPipeline::Dispatch(
	uint32_t width,
	uint32_t height,
	std::vector<Texture2D*> imageReads,
	std::vector<Texture2D*> imageWrites,
	const std::vector<DescriptorSet*>& pDescriptorSets)
{
	VkImageSubresourceRange colorRange{};
	colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorRange.baseMipLevel = 0;
	colorRange.levelCount = 1;
	colorRange.baseArrayLayer = 0;
	colorRange.layerCount = 1;

	VulkanCommandBuffer* commandBuffer = engine->GetGraphics()->GetVulkanContext()->GetCurrentFrameResource()->GetCommandBuffer();

	for (Texture2D* texture2D : imageReads)
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

	commandBuffer->TraceRays(vulkanRayTracingPipeline.get(), descriptorSets, width, height);
}
