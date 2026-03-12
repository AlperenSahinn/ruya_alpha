#pragma once
#include <string>
#include <memory>

#include "ruya_vulkan/vulkan_image.h"

#include "graphics_resource.h"

namespace ruya
{
	enum class ImageType
	{
		SRGB,
		NonColor,
		EXR
	};

	enum class ImageSampler
	{
		Nearest,
		Linear
	};

	class Image2D : public IGraphicsResource
	{
	public:
		Image2D(std::string filePath, ImageType type, ImageSampler sampler);
		~Image2D();

		Image2D(const Image2D&) = delete;
		Image2D& operator=(const Image2D&) = delete;

		void Load() override;
		void Unload() override;

		VulkanImage* GetVulkanImage() const;

		uint32_t GetDescriptorIndex() const;
		void SetDescriptorIndex(uint32_t index);

	private:
		std::string filePath;
		uint32_t width;
		uint32_t height;
		uint32_t depth;
		ImageType type;
		ImageSampler sampler;
		std::unique_ptr<VulkanImage> vulkanImage;
		uint32_t descriptorIndex;
	};
}