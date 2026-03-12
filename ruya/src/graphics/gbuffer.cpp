#include "gbuffer.h"

ruya::GBuffer::GBuffer(VulkanContext* pVulkanContext, uint32_t width, uint32_t height)
{
	albedoDepth = std::make_unique<VulkanImage>(pVulkanContext, VK_IMAGE_TYPE_2D, VkExtent3D{ width, height, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	positionMetallic = std::make_unique<VulkanImage>(pVulkanContext, VK_IMAGE_TYPE_2D, VkExtent3D{ width, height, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	normalRoughness = std::make_unique<VulkanImage>(pVulkanContext, VK_IMAGE_TYPE_2D, VkExtent3D{ width, height, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	vertexNormals = std::make_unique<VulkanImage>(pVulkanContext, VK_IMAGE_TYPE_2D, VkExtent3D{ width, height, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	depth = std::make_unique<VulkanImage>(pVulkanContext, VK_IMAGE_TYPE_2D, VkExtent3D{ width, height, 1 }, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);

	positionMetallicDownsampled = std::make_unique<VulkanImage>(pVulkanContext, VK_IMAGE_TYPE_2D, VkExtent3D{ width / 4, height / 4, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	vertexNormalsDownsampled = std::make_unique<VulkanImage>(pVulkanContext, VK_IMAGE_TYPE_2D, VkExtent3D{ width / 4, height / 4, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
}

ruya::GBuffer::~GBuffer()
{
}

ruya::VulkanImage* ruya::GBuffer::GetAlbedoDepth() const
{
	return albedoDepth.get();
}

ruya::VulkanImage* ruya::GBuffer::GetPositionMetallic() const
{
	return positionMetallic.get();
}

ruya::VulkanImage* ruya::GBuffer::GetNormalRoughness() const
{
	return normalRoughness.get();
}

ruya::VulkanImage* ruya::GBuffer::GetVertexNormals() const
{
	return vertexNormals.get();
}

ruya::VulkanImage* ruya::GBuffer::GetDepth() const
{
	return depth.get();
}

ruya::VulkanImage* ruya::GBuffer::GetPositionMetallicDownsample() const
{
	return positionMetallicDownsampled.get();
}

ruya::VulkanImage* ruya::GBuffer::GetVertexNormalsDownsample() const
{
	return vertexNormalsDownsampled.get();
}
