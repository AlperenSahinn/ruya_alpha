#pragma once
#include <vector>
#include <cstdint>
#include <unordered_map>

#include <volk/volk.h>

#include <core/ry_id.h>

#include "vulkan_image.h"
#include "vulkan_buffer.h"
#include "vulkan_rasterization_pipeline.h"
#include "vulkan_compute_pipeline.h"
#include "vulkan_ray_tracing_pipeline.h"
#include "vulkan_top_level_acceleration_structure.h"

namespace ruya
{
	class VulkanContext;

	class VulkanCommandBuffer
	{
	public:
		VulkanCommandBuffer(VulkanContext* pVulkanContext, VkCommandPool comandPool, VkCommandBufferLevel level);
		~VulkanCommandBuffer() = default;

		VulkanCommandBuffer(const VulkanCommandBuffer&) = delete;
		VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) = delete;

		VkCommandBuffer GetDeviceHandle() const;

		void Begin(VkCommandBufferUsageFlags usageFlags);
		void End();
		void Reset();

		void SetViewPort(uint32_t width, uint32_t height);

		void BeginRenderPass(VkExtent2D renderAreaSize, std::vector<VkRenderingAttachmentInfo>& colorAttachmentInfos, VkRenderingAttachmentInfo* pDepthAttachment);
		void EndRenderPass();

		void CopyBufferToImage(const VulkanBuffer& sourceBuffer, VulkanImage* pDestinationImage);
		void CopyBufferToBuffer(const VulkanBuffer& sourceBuffer, VulkanBuffer* pDestinationBuffer, VkBufferCopy& bufferCopy);

		void BufferMemoryBarrier(VulkanBuffer* pBuffer, VkAccessFlags dstAccessMask, VkPipelineStageFlags dstStageMask, VkDeviceSize offset, VkDeviceSize size);

		void GeneralMemoryBarrier(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);

		void ImageMemoryBarrier(VkImage image,
			VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask,
			VkImageLayout oldLayout,
			VkImageLayout newLayout,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask,
			VkImageSubresourceRange subresourceRange);

		void ImageMemoryBarrier(
			VulkanImage* pImage,
			VkAccessFlags dstAccessMask, 
			VkImageLayout newLayout, 
			VkPipelineStageFlags dstStageMask, 
			VkImageSubresourceRange subresourceRange);

		void BlitImage(VulkanImage* pSoruceImage, VulkanImage* pDestinationImage, VkFilter filter);

		void BindRasterizationPipeline(const VulkanRasterizationPipeline& pipeline);

		void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstIndex);
		void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance);
		void DrawIndirect(const VulkanBuffer& indirectBuffer, uint32_t drawCount, uint32_t stride);

		void PushConstant(const void* data, uint32_t size, VkPipelineLayout pipelineLayout, VkShaderStageFlags shaderStageFlags);

		void BindDescriptorSets(const std::vector<VkDescriptorSet>& descriptorSets, VkPipelineLayout pipelineLayout, VkPipelineBindPoint pipelineBindPoint);

		void BindVertexBuffer(const VulkanBuffer& vertexBuffer);
		void BindIndexBuffer(const VulkanBuffer& indexBuffer);

		void DispatchComputePipeline(VulkanComputePipeline* pComputePipeline, const std::vector<VkDescriptorSet>& descriptorSets, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

		void UpdateTLAS(VulkanTopLevelAccelerationStructure* pTLAS, const std::vector<std::pair<uint32_t, VulkanBottomLevelAccelerationStructureInstance*>>& blasInstances);

		void TraceRays(VulkanRayTracingPipeline* pRayTracingPipeline, const std::vector<VkDescriptorSet>& descriptorSets, uint32_t width, uint32_t height);

		void ReleaseBufferOwnership(
			VulkanBuffer* pBuffer,
			uint32_t srcQueueFamily,
			uint32_t dstQueueFamily,
			VkPipelineStageFlags srcStageMask,
			VkAccessFlags srcAccessMask);

		void AcquireBufferOwnership(
			VulkanBuffer* pBuffer,
			uint32_t srcQueueFamily,
			uint32_t dstQueueFamily,
			VkPipelineStageFlags dstStageMask,
			VkAccessFlags dstAccessMask);

		void ReleaseImageOwnership(
			VulkanImage* pImage,
			uint32_t srcQueueFamily,
			uint32_t dstQueueFamily,
			VkImageLayout newLayout,
			VkPipelineStageFlags srcStageMask,
			VkAccessFlags srcAccessMask,
			VkImageSubresourceRange subresourceRange);

		void AcquireImageOwnership(
			VulkanImage* pImage,
			uint32_t srcQueueFamily,
			uint32_t dstQueueFamily,
			VkImageLayout oldLayout,
			VkImageLayout newLayout,
			VkPipelineStageFlags dstStageMask,
			VkAccessFlags dstAccessMask,
			VkImageSubresourceRange subresourceRange);

	private:
		VkCommandBuffer deviceHandle;
	};
}