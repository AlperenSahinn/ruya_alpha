#pragma once
#include <memory>

#include "ruya_vulkan/vulkan_context.h"
#include "ruya_vulkan/vulkan_descriptor_set_layout.h"
#include "ruya_vulkan/vulkan_buffer.h"
#include "ruya_vulkan/vulkan_descriptor_pool.h"

#include "gbuffer.h"

namespace ruya
{
	class FrameBuffer
	{
	public:
		FrameBuffer(VulkanContext* pVulkanContext, uint32_t width, uint32_t height);
		~FrameBuffer();

		FrameBuffer(const FrameBuffer&) = delete;
		FrameBuffer& operator=(const FrameBuffer&) = delete;

		FrameBuffer(FrameBuffer&&) = default;
		FrameBuffer& operator=(FrameBuffer&&) = default;

		void UpdateDescriptors(VulkanContext* pVulkanContext);

		GBuffer* GetGBuffer() const;
		VulkanImage* GetDirectionalLightShadowMapImage() const;
		VulkanImage* GetLightPassImage() const;

		VkDescriptorSet GetCameraDataBufferDescriptorSet() const;
		VkDescriptorSet GetDirectionalLightDataBufferDescriptorSet() const;
		VkDescriptorSet GetGBufferDescriptorSet() const;
		VkDescriptorSet GetDownsampledGBufferDescriptorSet() const;
		VkDescriptorSet GetDirectionalLightShadowMapImageDescriptorSet() const;
		VkDescriptorSet GetLightPassImageDescriptorSet() const;

		uint32_t GetWidth() const;
		uint32_t GetHeight() const;

	private:
		uint32_t frameWidth;
		uint32_t frameHeight;

		std::unique_ptr<VulkanBuffer> cameraDataBuffer;
		std::unique_ptr<VulkanBuffer> directionalLightDataBuffer;

		VkDescriptorSet cameraDataBufferDescriptorSet;
		VkDescriptorSet directionalLightDataBufferDescriptorSet;
		VkDescriptorSet gBufferDescriptorSet;
		VkDescriptorSet downsampledGBufferDescriptorSet;
		VkDescriptorSet directionalLightShadowMapImageDescriptorSet;
		VkDescriptorSet lightPassImageDescriptorSet;

		std::unique_ptr<GBuffer> gBuffer;
		std::unique_ptr<VulkanImage> directionalLightShadowMapImage;
		std::unique_ptr<VulkanImage> lightPassImage;
	};
}