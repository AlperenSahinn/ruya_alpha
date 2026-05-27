#include "rasterization_pipeline.h"

#include <engine.h>

ruya::RasterizationPipeline::RasterizationPipeline(
	const std::string& vertexShaderPath, 
	const std::string& fragmentShaderPath, 
	const std::vector<DescriptorSetLayout*>& pDescriptorSetLayouts,
	const std::vector<ColorFormat>& colorAttachmentFormats,
	ColorFormat depthFormat)
{
	std::vector<VkFormat> vulkanColorAttachmentFormats;

	for (const ColorFormat& colorAttachmentFormat : colorAttachmentFormats)
	{
		if (colorAttachmentFormat == ColorFormat::R32G32B32A32_SFLOAT) vulkanColorAttachmentFormats.push_back(VK_FORMAT_R32G32B32A32_SFLOAT);
		else if (colorAttachmentFormat == ColorFormat::R16G16B16A16_SFLOAT) vulkanColorAttachmentFormats.push_back(VK_FORMAT_R16G16B16A16_SFLOAT);
		else if (colorAttachmentFormat == ColorFormat::R8G8B8A8_UNORM) vulkanColorAttachmentFormats.push_back(VK_FORMAT_R8G8B8A8_UNORM);
	}

	VkFormat depthAttachment;
	if (depthFormat == ColorFormat::D32_SFLOAT) depthAttachment = VK_FORMAT_D32_SFLOAT;

	std::vector<VulkanDescriptorSetLayout*> vulkanDescriptorSetLayouts;

	for(DescriptorSetLayout* descriptorSetLayout : pDescriptorSetLayouts)
	{
		vulkanDescriptorSetLayouts.push_back(descriptorSetLayout->GetVulkanDescriptorSetLayout());
	}

	std::vector<VkPushConstantRange> pushConstantRanges;

	vulkanRasterizationPipeline = std::make_unique<VulkanRasterizationPipeline>(
		engine->GetGraphics()->GetVulkanContext(),
		vertexShaderPath,
		fragmentShaderPath,
		vulkanDescriptorSetLayouts,
		vulkanColorAttachmentFormats,
		depthAttachment,
		true,
		VK_COMPARE_OP_LESS_OR_EQUAL,
		pushConstantRanges,
		true,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	vulkanIndirectDrawBuffer = std::make_unique<VulkanBuffer>(
		engine->GetGraphics()->GetVulkanContext(),
		kMaxRenderItemCount * sizeof(VkDrawIndirectCommand),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU);
}

void ruya::RasterizationPipeline::Dispatch(
	uint32_t frameBufferWidth, 
	uint32_t frameBufferHeight,
	const std::vector<Texture2D*>& pColorAttachments,
	Texture2D* pDepthAttachment,
	const std::vector<DescriptorSet*>& pDescriptorSets,
	const std::vector<RyID>& renderGeometryIDs)
{
	VkExtent2D extent{ frameBufferWidth, frameBufferHeight };

	std::vector<VkRenderingAttachmentInfo> renderingAttachmentInfos;

	for(Texture2D* texture2D : pColorAttachments)
	{
		VkRenderingAttachmentInfo vkRenderingAttachmentInfo = {};
		vkRenderingAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		vkRenderingAttachmentInfo.imageView = texture2D->GetVulkanImage()->GetImageView();
		vkRenderingAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		vkRenderingAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		vkRenderingAttachmentInfo.clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		vkRenderingAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		renderingAttachmentInfos.push_back(vkRenderingAttachmentInfo);
	}

	VkRenderingAttachmentInfo depthAttachmentInfo = {};
	depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachmentInfo.imageView = pDepthAttachment->GetVulkanImage()->GetImageView();
	depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachmentInfo.clearValue.depthStencil = { 1.0f, 0 };
	depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkImageSubresourceRange colorRange{};
	colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorRange.baseMipLevel = 0;
	colorRange.levelCount = 1;
	colorRange.baseArrayLayer = 0;
	colorRange.layerCount = 1;

	VulkanCommandBuffer* commandBuffer = engine->GetGraphics()->GetVulkanContext()->GetCurrentFrameResource()->GetCommandBuffer();

	for (Texture2D* texture2D : pColorAttachments)
	{
		commandBuffer->ImageMemoryBarrier(
			texture2D->GetVulkanImage(),
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			colorRange);
	}

	VkImageSubresourceRange depthRange{};
	depthRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthRange.baseMipLevel = 0;
	depthRange.levelCount = 1;
	depthRange.baseArrayLayer = 0;
	depthRange.layerCount = 1;

	commandBuffer->ImageMemoryBarrier(
		pDepthAttachment->GetVulkanImage(),
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		depthRange);

	commandBuffer->SetViewPort(extent.width, extent.height);

	commandBuffer->BeginRenderPass(extent, renderingAttachmentInfos, &depthAttachmentInfo);

	commandBuffer->BindRasterizationPipeline(*vulkanRasterizationPipeline);

	std::vector<VkDescriptorSet> descriptorSets;
	for(DescriptorSet* descriptorSet : pDescriptorSets)
	{
		descriptorSets.push_back(descriptorSet->vkDescriptorSet);
	}

	commandBuffer->BindDescriptorSets(descriptorSets, vulkanRasterizationPipeline->GetPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS);

	std::vector<VkDrawIndirectCommand> drawCmds;
	drawCmds.reserve(renderGeometryIDs.size());

	for(RyID ryID : renderGeometryIDs)
	{
			const RenderItem* renderItem = engine->GetGraphics()->GetRenderItem(ryID);

			VkDrawIndirectCommand cmd{};
			cmd.vertexCount = engine->GetGraphics()->GetMeshBuffer(renderItem->GetMeshBufferRyID())->GetIndexCount();
			cmd.instanceCount = 1;
			cmd.firstVertex = 0;
			cmd.firstInstance = static_cast<uint32_t>(ryID.GetRawID());
			drawCmds.push_back(cmd);
	}

	uint32_t drawCount = static_cast<uint32_t>(drawCmds.size());

	if (drawCount > 0)
	{
		vulkanIndirectDrawBuffer->UploadData(drawCmds.data(), drawCount * sizeof(VkDrawIndirectCommand));
		commandBuffer->DrawIndirect(*vulkanIndirectDrawBuffer, drawCount, sizeof(VkDrawIndirectCommand));
	}

	commandBuffer->EndRenderPass();
}
