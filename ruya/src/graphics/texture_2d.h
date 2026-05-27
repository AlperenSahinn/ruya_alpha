#pragma once
#include <memory>
#include <cstdint>
#include <atomic>

#include <ktx/ktx.h>

#include <core/ry_id.h>

#include "ruya_vulkan/vulkan_image.h"

#include "color_format.h"

namespace ruya
{
	enum class Texture2DType
	{
		SRGB,
		NonColor,
		EXR
	};

	enum class Texture2DSampler
	{
		Nearest,
		Linear
	};

	class Texture2D
	{
	public:
		Texture2D() = default;
		Texture2D(uint32_t width, uint32_t height, ColorFormat colorFormat);
		~Texture2D() = default;

		Texture2D(const Texture2D&) = delete;
		Texture2D& operator=(const Texture2D&) = delete;

		void Load(ktxTexture2* ktxtexturePtr, Texture2DSampler sampler);
		void Unload();
		bool IsLoaded() { return isLoaded; }

		VulkanImage* GetVulkanImage() { return vulkanImage.get(); }

		void SetValid();
		void SetInvalid();
		bool IsValid();

	private:
		std::unique_ptr<VulkanImage> vulkanImage;
		Texture2DSampler sampler;
		bool isLoaded = false;

		std::atomic<bool> valid{ false };
	};
}