#include "vulkan_command_buffer.h"
#include "vulkan_helpers.h"
#include "vulkan_context.h"

#include <glm/gtc/type_ptr.hpp>

ruya::VulkanCommandBuffer::VulkanCommandBuffer(VulkanContext* pVulkanContext, VkCommandPool pComandPool, VkCommandBufferLevel level)
{
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = pComandPool;
	commandBufferAllocateInfo.commandBufferCount = 1;
	commandBufferAllocateInfo.level = level;

	CHECK_VKRESULT(vkAllocateCommandBuffers(pVulkanContext->GetDevice(), &commandBufferAllocateInfo, &deviceHandle));
}

VkCommandBuffer ruya::VulkanCommandBuffer::GetDeviceHandle() const
{
	return deviceHandle;
}

void ruya::VulkanCommandBuffer::Begin(VkCommandBufferUsageFlags usageFlags)
{
	VkCommandBufferBeginInfo commandBufferBegininfo = {};
	commandBufferBegininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBegininfo.pNext = nullptr;
	commandBufferBegininfo.pInheritanceInfo = nullptr;
	commandBufferBegininfo.flags = usageFlags;

	CHECK_VKRESULT_DEBUG(vkBeginCommandBuffer(deviceHandle, &commandBufferBegininfo));
}

void ruya::VulkanCommandBuffer::End()
{
	CHECK_VKRESULT_DEBUG(vkEndCommandBuffer(deviceHandle));
}

void ruya::VulkanCommandBuffer::Reset()
{
	CHECK_VKRESULT_DEBUG(vkResetCommandBuffer(deviceHandle, 0));
}

void ruya::VulkanCommandBuffer::SetViewPort(uint32_t width, uint32_t height)
{
	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = static_cast<float>(height);
	viewport.width = static_cast<float>(width);
	viewport.height = -static_cast<float>(height);
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	vkCmdSetViewport(deviceHandle, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = width;
	scissor.extent.height = height;

	vkCmdSetScissor(deviceHandle, 0, 1, &scissor);
}

void ruya::VulkanCommandBuffer::BeginRenderPass(
	VkExtent2D renderAreaSize, 
	std::vector<VkRenderingAttachmentInfo>& colorAttachmentInfos, 
	VkRenderingAttachmentInfo* pDepthAttachment)
{
	VkRenderingInfo renderingInfo = {};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea = VkRect2D{ VkOffset2D { 0, 0 }, renderAreaSize };
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = colorAttachmentInfos.size();
	renderingInfo.pColorAttachments = colorAttachmentInfos.data();
	renderingInfo.pDepthAttachment = pDepthAttachment;
	renderingInfo.pStencilAttachment = nullptr;

	vkCmdBeginRendering(deviceHandle, &renderingInfo);
}

void ruya::VulkanCommandBuffer::EndRenderPass()
{
	vkCmdEndRendering(deviceHandle);
}

void ruya::VulkanCommandBuffer::CopyBufferToImage(const VulkanBuffer& sourceBuffer, VulkanImage* pDestinationImage)
{
	VkBufferImageCopy2 copyRegion = {};
	copyRegion.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
	copyRegion.pNext = nullptr;
	copyRegion.bufferOffset = 0;
	copyRegion.bufferRowLength = 0;
	copyRegion.bufferImageHeight = 0;
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.baseArrayLayer = 0;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageSubresource.mipLevel = 0;
	copyRegion.imageOffset = { 0, 0, 0 };
	copyRegion.imageExtent = pDestinationImage->GetExtent();

	VkCopyBufferToImageInfo2 copyInfo = {};
	copyInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
	copyInfo.pNext = nullptr;
	copyInfo.srcBuffer = sourceBuffer.GetDeviceHandle();
	copyInfo.dstImage = pDestinationImage->GetDeviceHandle();;
	copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	copyInfo.regionCount = 1;
	copyInfo.pRegions = &copyRegion;

	vkCmdCopyBufferToImage2(deviceHandle, &copyInfo);
}

void ruya::VulkanCommandBuffer::CopyBufferToBuffer(const VulkanBuffer& sourceBuffer, VulkanBuffer* pDestinationBuffer, VkBufferCopy& bufferCopy)
{
	vkCmdCopyBuffer(deviceHandle, sourceBuffer.GetDeviceHandle(), pDestinationBuffer->GetDeviceHandle(), 1, &bufferCopy);
}

void ruya::VulkanCommandBuffer::BufferMemoryBarrier(
	VulkanBuffer* pBuffer,
	VkAccessFlags dstAccessMask,
	VkPipelineStageFlags dstStageMask,
	VkDeviceSize offset,
	VkDeviceSize size)
{
	VkBufferMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.pNext = nullptr;
	barrier.srcAccessMask = pBuffer->accessMask;
	barrier.dstAccessMask = dstAccessMask;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = pBuffer->GetDeviceHandle();
	barrier.offset = offset;
	barrier.size = size;

	vkCmdPipelineBarrier(
		deviceHandle,
		pBuffer->stageMask,
		dstStageMask,
		0,
		0, nullptr,
		1, &barrier,
		0, nullptr
	);

	pBuffer->stageMask = dstStageMask;
	pBuffer->accessMask = dstAccessMask;
}

void ruya::VulkanCommandBuffer::GeneralMemoryBarrier(
	VkAccessFlags srcAccessMask,
	VkAccessFlags dstAccessMask,
	VkPipelineStageFlags srcStageMask,
	VkPipelineStageFlags dstStageMask)
{
	VkMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.pNext = nullptr;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;

	vkCmdPipelineBarrier(
		deviceHandle,
		srcStageMask,
		dstStageMask,
		0,
		1, &barrier,
		0, nullptr,
		0, nullptr
	);
}

void ruya::VulkanCommandBuffer::ImageMemoryBarrier(
	VkImage image,
	VkAccessFlags srcAccessMask,
	VkAccessFlags dstAccessMask,
	VkImageLayout oldLayout,
	VkImageLayout newLayout,
	VkPipelineStageFlags srcStageMask,
	VkPipelineStageFlags dstStageMask,
	VkImageSubresourceRange subresourceRange)
{
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = nullptr;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = subresourceRange;

	vkCmdPipelineBarrier(
		deviceHandle,
		srcStageMask,
		dstStageMask,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
}

void ruya::VulkanCommandBuffer::ImageMemoryBarrier(
	VulkanImage* pImage, 
	VkAccessFlags dstAccessMask,  
	VkImageLayout newLayout, 
	VkPipelineStageFlags dstStageMask, 
	VkImageSubresourceRange subresourceRange)
{

	ImageMemoryBarrier(pImage->GetDeviceHandle(), pImage->accessMask, dstAccessMask, pImage->imageLayout, newLayout, pImage->stageMask, dstStageMask, subresourceRange);

	pImage->stageMask = dstStageMask;
	pImage->accessMask = dstAccessMask;
	pImage->imageLayout = newLayout;
}

void ruya::VulkanCommandBuffer::BlitImage(VulkanImage* pSoruceImage, VulkanImage* pDestinationImage, VkFilter filter)
{
	VkImageBlit imageBlit = {};
	imageBlit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	imageBlit.srcOffsets[0] = { 0, 0, 0 };
	imageBlit.srcOffsets[1] = { int(pSoruceImage->GetExtent().width), int(pSoruceImage->GetExtent().height), 1};
	imageBlit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	imageBlit.dstOffsets[0] = { 0, 0, 0 };
	imageBlit.dstOffsets[1] = { int(pDestinationImage->GetExtent().width), int(pDestinationImage->GetExtent().height), 1 };

	VkImageSubresourceRange colorRange{};
	colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorRange.baseMipLevel = 0;
	colorRange.levelCount = 1;
	colorRange.baseArrayLayer = 0;
	colorRange.layerCount = 1;

	ImageMemoryBarrier(
		pSoruceImage,
		VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		colorRange
	);

	ImageMemoryBarrier(
		pDestinationImage,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		colorRange
	);

	vkCmdBlitImage(
		deviceHandle, 
		pSoruceImage->GetDeviceHandle(), 
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		pDestinationImage->GetDeviceHandle(), 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &imageBlit, filter);
}

void ruya::VulkanCommandBuffer::BindRasterizationPipeline(const VulkanRasterizationPipeline& pipeline)
{
	vkCmdBindPipeline(deviceHandle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetDeviceHandle());
}

void ruya::VulkanCommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstIndex)
{
	vkCmdDraw(deviceHandle, vertexCount, instanceCount, firstVertex, firstIndex);
}

void ruya::VulkanCommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance)
{
	vkCmdDrawIndexed(deviceHandle, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void ruya::VulkanCommandBuffer::DrawIndirect(const VulkanBuffer& indirectBuffer, uint32_t drawCount, uint32_t stride)
{
	vkCmdDrawIndirect(deviceHandle, indirectBuffer.GetDeviceHandle(), 0, drawCount, stride);
}

void ruya::VulkanCommandBuffer::PushConstant(const void* data, uint32_t size, VkPipelineLayout pipelineLayout, VkShaderStageFlags shaderStageFlags)
{
	vkCmdPushConstants(deviceHandle, pipelineLayout, shaderStageFlags, 0, size, data);
}

void ruya::VulkanCommandBuffer::BindDescriptorSets(
	const std::vector<VkDescriptorSet>& descriptorSets, 
	VkPipelineLayout pipelineLayout, 
	VkPipelineBindPoint pipelineBindPoint)
{
	vkCmdBindDescriptorSets(deviceHandle, pipelineBindPoint, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
}

void ruya::VulkanCommandBuffer::BindVertexBuffer(const VulkanBuffer& vertexBuffer)
{
	std::vector<VkBuffer> buffers = { vertexBuffer.GetDeviceHandle() };
	VkDeviceSize off = 0;
	vkCmdBindVertexBuffers(deviceHandle, 0, 1, buffers.data(), &off);
}

void ruya::VulkanCommandBuffer::BindIndexBuffer(const VulkanBuffer& indexBuffer)
{
	vkCmdBindIndexBuffer(deviceHandle, indexBuffer.GetDeviceHandle(), 0, VK_INDEX_TYPE_UINT32);
}

void ruya::VulkanCommandBuffer::DispatchComputePipeline(
	VulkanComputePipeline* pComputePipeline, 
	const std::vector<VkDescriptorSet>& descriptorSets, 
	uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	vkCmdBindPipeline(deviceHandle, VK_PIPELINE_BIND_POINT_COMPUTE, pComputePipeline->GetDeviceHandle());
	vkCmdDispatch(deviceHandle, groupCountX, groupCountY, groupCountZ);
}

void ruya::VulkanCommandBuffer::UpdateTLAS(VulkanTopLevelAccelerationStructure* pTLAS, const std::vector<std::pair<uint32_t, VulkanBottomLevelAccelerationStructureInstance*>>& blasInstances)
{
	uint32_t instanceCount = pTLAS->GetInstanceCount();
	if (blasInstances.size() != instanceCount)
	{
		return;
	}

	VkAccelerationStructureInstanceKHR* mappedData = static_cast<VkAccelerationStructureInstanceKHR*>(pTLAS->GetBLASInstancesBuffer()->MapMemory());

	for (uint32_t i = 0; i < instanceCount; ++i)
	{
		const auto& pair = blasInstances[i];
		const VulkanBottomLevelAccelerationStructureInstance* blasInstance = pair.second;

		VkAccelerationStructureInstanceKHR& asInstance = mappedData[i];

		const float* m = glm::value_ptr(blasInstance->transform);
		asInstance.transform.matrix[0][0] = m[0]; asInstance.transform.matrix[0][1] = m[4]; asInstance.transform.matrix[0][2] = m[8];  asInstance.transform.matrix[0][3] = m[12];
		asInstance.transform.matrix[1][0] = m[1]; asInstance.transform.matrix[1][1] = m[5]; asInstance.transform.matrix[1][2] = m[9];  asInstance.transform.matrix[1][3] = m[13];
		asInstance.transform.matrix[2][0] = m[2]; asInstance.transform.matrix[2][1] = m[6]; asInstance.transform.matrix[2][2] = m[10]; asInstance.transform.matrix[2][3] = m[14];

		asInstance.instanceCustomIndex = pair.first;
		asInstance.mask = 0xFF;
		asInstance.instanceShaderBindingTableRecordOffset = 0;
		asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

		if (blasInstance->blas != nullptr)
		{
			asInstance.accelerationStructureReference = blasInstance->blas->GetDeviceAddress();
		}
		else
		{
			asInstance.accelerationStructureReference = 0;
		}
	}

	pTLAS->GetBLASInstancesBuffer()->UnmapMemory();

	VkMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

	vkCmdPipelineBarrier(
		deviceHandle,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		0,
		1, &barrier,
		0, nullptr,
		0, nullptr
	);

	VkAccelerationStructureGeometryInstancesDataKHR instancesData = {};
	instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	instancesData.data.deviceAddress = pTLAS->GetBLASInstancesBuffer()->GetDeviceAddress();

	VkAccelerationStructureGeometryKHR asGeometry = {};
	asGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	asGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	asGeometry.geometry.instances = instancesData;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &asGeometry;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.srcAccelerationStructure = pTLAS->GetDeviceHandle();
	buildInfo.dstAccelerationStructure = pTLAS->GetDeviceHandle();
	buildInfo.scratchData.deviceAddress = pTLAS->GetScratchBuffer()->GetDeviceAddress();

	VkAccelerationStructureBuildRangeInfoKHR rangeInfo{ instanceCount, 0, 0, 0 };
	const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

	vkCmdBuildAccelerationStructuresKHR(deviceHandle, 1, &buildInfo, &pRangeInfo);
}

void ruya::VulkanCommandBuffer::TraceRays(VulkanRayTracingPipeline* pRayTracingPipeline, const std::vector<VkDescriptorSet>& descriptorSets, uint32_t width, uint32_t height)
{
	vkCmdBindPipeline(deviceHandle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pRayTracingPipeline->GetDeviceHandle());
	vkCmdBindDescriptorSets(deviceHandle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pRayTracingPipeline->GetPipelineLayout(), 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	vkCmdTraceRaysKHR(deviceHandle,
		&pRayTracingPipeline->GetRayGenerationRegion(),
		&pRayTracingPipeline->GetMissRegion(),
		&pRayTracingPipeline->GetHitRegion(),
		&pRayTracingPipeline->GetCallableRegion(),
		width,
		height,
		1
	);
}

void ruya::VulkanCommandBuffer::ReleaseBufferOwnership(
	VulkanBuffer* pBuffer,
	uint32_t srcQueueFamily,
	uint32_t dstQueueFamily,
	VkPipelineStageFlags srcStageMask,
	VkAccessFlags srcAccessMask)
{
	VkBufferMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = 0;
	barrier.srcQueueFamilyIndex = srcQueueFamily;
	barrier.dstQueueFamilyIndex = dstQueueFamily;
	barrier.buffer = pBuffer->GetDeviceHandle();
	barrier.offset = 0;
	barrier.size = VK_WHOLE_SIZE;

	vkCmdPipelineBarrier(
		deviceHandle,
		srcStageMask,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0,
		0, nullptr,
		1, &barrier,
		0, nullptr
	);
}

void ruya::VulkanCommandBuffer::AcquireBufferOwnership(
	VulkanBuffer* pBuffer,
	uint32_t srcQueueFamily,
	uint32_t dstQueueFamily,
	VkPipelineStageFlags dstStageMask,
	VkAccessFlags dstAccessMask)
{
	VkBufferMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = dstAccessMask;
	barrier.srcQueueFamilyIndex = srcQueueFamily;
	barrier.dstQueueFamilyIndex = dstQueueFamily;
	barrier.buffer = pBuffer->GetDeviceHandle();
	barrier.offset = 0;
	barrier.size = VK_WHOLE_SIZE;

	vkCmdPipelineBarrier(
		deviceHandle,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		dstStageMask,
		0,
		0, nullptr,
		1, &barrier,
		0, nullptr
	);

	pBuffer->stageMask = dstStageMask;
	pBuffer->accessMask = dstAccessMask;
}

void ruya::VulkanCommandBuffer::ReleaseImageOwnership(
	VulkanImage* pImage,
	uint32_t srcQueueFamily,
	uint32_t dstQueueFamily,
	VkImageLayout newLayout,
	VkPipelineStageFlags srcStageMask,
	VkAccessFlags srcAccessMask,
	VkImageSubresourceRange subresourceRange)
{
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = 0;
	barrier.oldLayout = pImage->imageLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = srcQueueFamily;
	barrier.dstQueueFamilyIndex = dstQueueFamily;
	barrier.image = pImage->GetDeviceHandle();
	barrier.subresourceRange = subresourceRange;

	vkCmdPipelineBarrier(
		deviceHandle,
		srcStageMask,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	pImage->imageLayout = newLayout;
}

void ruya::VulkanCommandBuffer::AcquireImageOwnership(
	VulkanImage* pImage,
	uint32_t srcQueueFamily,
	uint32_t dstQueueFamily,
	VkImageLayout oldLayout,
	VkImageLayout newLayout,
	VkPipelineStageFlags dstStageMask,
	VkAccessFlags dstAccessMask,
	VkImageSubresourceRange subresourceRange)
{
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = dstAccessMask;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = srcQueueFamily;
	barrier.dstQueueFamilyIndex = dstQueueFamily;
	barrier.image = pImage->GetDeviceHandle();
	barrier.subresourceRange = subresourceRange;

	vkCmdPipelineBarrier(
		deviceHandle,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		dstStageMask,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	pImage->imageLayout = newLayout;
	pImage->stageMask = dstStageMask;
	pImage->accessMask = dstAccessMask;
}
