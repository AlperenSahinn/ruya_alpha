#pragma once
#include <Volk/volk.h>
#include <vulkan_memory_allocator/vk_mem_alloc.h>
#include <ktx/ktx.h>

namespace ruya
{
	class VulkanContext;

	class VulkanImage
	{
	public:
		VulkanImage(VulkanContext* pVulkanContext, VkImageType imageType, VkExtent3D imageExtent, VkFormat format, VkImageAspectFlags imageAspectFlags);
		VulkanImage(VulkanContext* pVulkanContext, ktxTexture2* ktxTex, VkImageAspectFlags imageAspectFlags);
		~VulkanImage();

		VulkanImage(const VulkanImage&) = delete;
		VulkanImage& operator=(const VulkanImage&) = delete;

		VkImage GetDeviceHandle() const;
		VkImageView GetImageView() const;
		VkExtent3D GetExtent() const;

	public:
		VkPipelineStageFlags2 stageMask;
		VkAccessFlags2 accessMask;
		VkImageLayout imageLayout;

	private:
		VkImage deviceHandle;
		VkImageView imageView;
		VmaAllocation allocation;
		VmaAllocationInfo allocationInfo;
		VkExtent3D extent;
		VkDevice device;
		VmaAllocator allocator;
	};
}