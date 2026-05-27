#include "texture_2d.h"

#include <engine.h>

ruya::Texture2D::Texture2D(uint32_t width, uint32_t height, ColorFormat colorFormat)
{
	VkExtent3D imageExtend = {};
	imageExtend.width = width;
	imageExtend.height = height;
	imageExtend.depth = 1;

	if (colorFormat == ColorFormat::R32G32B32A32_SFLOAT)
		vulkanImage = std::make_unique<VulkanImage>(engine->GetGraphics()->GetVulkanContext(), VK_IMAGE_TYPE_2D, imageExtend, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	else if (colorFormat == ColorFormat::R16G16B16A16_SFLOAT)
		vulkanImage = std::make_unique<VulkanImage>(engine->GetGraphics()->GetVulkanContext(), VK_IMAGE_TYPE_2D, imageExtend, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	else if (colorFormat == ColorFormat::R8_UNORM)
		vulkanImage = std::make_unique<VulkanImage>(engine->GetGraphics()->GetVulkanContext(), VK_IMAGE_TYPE_2D, imageExtend, VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	else if (colorFormat == ColorFormat::R16_SFLOAT)
		vulkanImage = std::make_unique<VulkanImage>(engine->GetGraphics()->GetVulkanContext(), VK_IMAGE_TYPE_2D, imageExtend, VK_FORMAT_R16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	else if(colorFormat == ColorFormat::D32_SFLOAT)
		vulkanImage = std::make_unique<VulkanImage>(engine->GetGraphics()->GetVulkanContext(), VK_IMAGE_TYPE_2D, imageExtend, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void ruya::Texture2D::Load(ktxTexture2* ktxtexturePtr, Texture2DSampler sampler)
{
	this->sampler = sampler;

	vulkanImage = std::make_unique<VulkanImage>(engine->GetGraphics()->GetVulkanContext(), ktxtexturePtr, VK_IMAGE_ASPECT_COLOR_BIT);

	isLoaded = true;
}

void ruya::Texture2D::Unload()
{
	isLoaded = false;

	vulkanImage.reset();
}

void ruya::Texture2D::SetValid()
{
	valid.store(true, std::memory_order_release);
}

void ruya::Texture2D::SetInvalid()
{
	valid.store(false, std::memory_order_release);
}

bool ruya::Texture2D::IsValid()
{
	return valid.load(std::memory_order_acquire);
}
