#include "image_2d.h"

#include <volk/volk.h>
#include <stb_image/stb_image.h>

#include <engine.h>
#include <core/log.h>

#include "material.h"

ruya::Image2D::Image2D(std::string filePath, ImageType type, ImageSampler sampler)
{
	this->filePath = filePath;
	this->type = type;
	this->sampler = sampler;
}

ruya::Image2D::~Image2D()
{
}

void ruya::Image2D::Load()
{
	int width, height, channels;

	stbi_uc* data = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
	if (!data)
	{
		RUYA_LOG_ERROR("[Texture2D] Failed to load image: %s", filePath.c_str());
	}

	width = static_cast<uint32_t>(width);
	height = static_cast<uint32_t>(height);
	depth = 1;

	VkExtent3D imageExtend = {};
	imageExtend.width = width;
	imageExtend.height = height;
	imageExtend.depth = depth;

	if (type == ImageType::SRGB)
		vulkanImage = std::make_unique<VulkanImage>(engine->GetGraphics()->GetVulkanContext(), data, VK_IMAGE_TYPE_2D, imageExtend, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, true);

	else if (type == ImageType::NonColor)
		vulkanImage = std::make_unique<VulkanImage>(engine->GetGraphics()->GetVulkanContext(), data, VK_IMAGE_TYPE_2D, imageExtend, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, true);

	stbi_image_free(data);
}

void ruya::Image2D::Unload()
{
	vulkanImage.reset();
}

ruya::VulkanImage* ruya::Image2D::GetVulkanImage() const
{
	return vulkanImage.get();
}

uint32_t ruya::Image2D::GetDescriptorIndex() const
{
	return descriptorIndex;
}

void ruya::Image2D::SetDescriptorIndex(uint32_t index)
{
	descriptorIndex = index;
}
