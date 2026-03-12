#pragma once
#include <memory>

#include "ruya_vulkan/vulkan_context.h"
#include "ruya_vulkan/vulkan_image.h"

namespace ruya
{
	class GBuffer
	{
	public:
		GBuffer(VulkanContext* pVulkanContext, uint32_t width, uint32_t height);
		~GBuffer();

		GBuffer(const GBuffer&) = delete;
		GBuffer& operator=(const GBuffer&) = delete;

		VulkanImage* GetAlbedoDepth() const;
		VulkanImage* GetPositionMetallic() const;
		VulkanImage* GetNormalRoughness() const;
		VulkanImage* GetVertexNormals() const;
		VulkanImage* GetDepth() const;

		VulkanImage* GetPositionMetallicDownsample() const;
		VulkanImage* GetVertexNormalsDownsample() const;

	private:
		std::unique_ptr<VulkanImage> albedoDepth;
		std::unique_ptr<VulkanImage> positionMetallic;
		std::unique_ptr<VulkanImage> normalRoughness;
		std::unique_ptr<VulkanImage> vertexNormals;
		std::unique_ptr<VulkanImage> depth;

		std::unique_ptr<VulkanImage> positionMetallicDownsampled;
		std::unique_ptr<VulkanImage> vertexNormalsDownsampled;
	};
}