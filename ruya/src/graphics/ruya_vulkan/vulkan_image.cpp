#include "vulkan_image.h"
#include "vulkan_helpers.h"
#include "vulkan_context.h"
#include "vulkan_buffer.h"
#include "vulkan_command_buffer.h"

ruya::VulkanImage::VulkanImage(VulkanContext* pVulkanContext, void* data, VkImageType imageType, VkExtent3D imageExtent, VkFormat format, VkImageAspectFlags imageAspectFlags, bool isMipmapped)
{
	device = pVulkanContext->GetDevice();
	allocator = pVulkanContext->GetVulkanMemoryAllocator();

	extent = imageExtent;

	size_t data_size = extent.depth * extent.width * extent.height * 4;
	std::unique_ptr<VulkanBuffer> uploadbuffer = std::make_unique<VulkanBuffer>(pVulkanContext, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	uploadbuffer->UploadData(data, data_size);

	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.pNext = nullptr;
	imageCreateInfo.imageType = imageType;
	imageCreateInfo.format = format;
	imageCreateInfo.extent = extent;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	if(format  != VK_FORMAT_R8G8B8A8_SRGB)
	{
		imageCreateInfo.usage = imageCreateInfo.usage | VK_IMAGE_USAGE_STORAGE_BIT;
	}

	if (isMipmapped)
	{
		imageCreateInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
	}

	VmaAllocationCreateInfo allocCreateInfo = {};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocCreateInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	CHECK_VKRESULT(vmaCreateImage(allocator, &imageCreateInfo, &allocCreateInfo, &deviceHandle, &allocation, &allocationInfo));

	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.pNext = nullptr;

	if (imageType == VK_IMAGE_TYPE_1D)
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
	else if (imageType == VK_IMAGE_TYPE_2D)
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	else if (imageType == VK_IMAGE_TYPE_3D)
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;

	imageViewCreateInfo.image = deviceHandle;
	imageViewCreateInfo.format = format;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = imageCreateInfo.mipLevels;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	imageViewCreateInfo.subresourceRange.aspectMask = imageAspectFlags;

	CHECK_VKRESULT(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageView));

	stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	accessMask = VK_ACCESS_2_NONE;
	imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	pVulkanContext->ImmediateSubmitCommand([&](VulkanCommandBuffer* commandBuffer)
		{
			VkBufferImageCopy copyRegion = {};
			copyRegion.bufferOffset = 0;
			copyRegion.bufferRowLength = 0;
			copyRegion.bufferImageHeight = 0;

			copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.imageSubresource.mipLevel = 0;
			copyRegion.imageSubresource.baseArrayLayer = 0;
			copyRegion.imageSubresource.layerCount = 1;
			copyRegion.imageExtent = extent;

			commandBuffer->ImageMemoryBarrier(
				this,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

			commandBuffer->CopyBufferToImage(*uploadbuffer, this);
		}
	);

	if (isMipmapped)
	{
		uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;

		pVulkanContext->ImmediateSubmitCommand([&](VulkanCommandBuffer* commandBuffer)
			{

				commandBuffer->ImageMemoryBarrier(deviceHandle,
					VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_ACCESS_TRANSFER_READ_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

				for (uint32_t i = 1; i < mipLevels; i++)
				{
					VkImageBlit image_blit{};

					image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					image_blit.srcSubresource.layerCount = 1;
					image_blit.srcSubresource.mipLevel = i - 1;
					image_blit.srcOffsets[1].x = int32_t(extent.width >> (i - 1));
					image_blit.srcOffsets[1].y = int32_t(extent.height >> (i - 1));
					image_blit.srcOffsets[1].z = 1;

					image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					image_blit.dstSubresource.layerCount = 1;
					image_blit.dstSubresource.mipLevel = i;
					image_blit.dstOffsets[1].x = int32_t(extent.width >> i);
					image_blit.dstOffsets[1].y = int32_t(extent.height >> i);
					image_blit.dstOffsets[1].z = 1;

					commandBuffer->ImageMemoryBarrier(
						deviceHandle,
						0,
						VK_ACCESS_TRANSFER_WRITE_BIT,
						VK_IMAGE_LAYOUT_UNDEFINED,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						VK_PIPELINE_STAGE_TRANSFER_BIT,
						VK_PIPELINE_STAGE_TRANSFER_BIT,
						{ VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1 });

					vkCmdBlitImage(
						commandBuffer->GetDeviceHandle(),
						deviceHandle,
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						deviceHandle,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1,
						&image_blit,
						VK_FILTER_LINEAR);

					commandBuffer->ImageMemoryBarrier(
						deviceHandle,
						VK_ACCESS_TRANSFER_WRITE_BIT,
						VK_ACCESS_TRANSFER_READ_BIT,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						VK_PIPELINE_STAGE_TRANSFER_BIT,
						VK_PIPELINE_STAGE_TRANSFER_BIT,
						{ VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1 });
				}

				commandBuffer->ImageMemoryBarrier(
					deviceHandle,
					VK_ACCESS_TRANSFER_READ_BIT,
					VK_ACCESS_SHADER_READ_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					{ VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 });
			}
		);
	}

	stageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	accessMask = VK_ACCESS_SHADER_READ_BIT;
	imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

ruya::VulkanImage::VulkanImage(VulkanContext* pVulkanContext, VkImageType imageType, VkExtent3D imageExtent, VkFormat format, VkImageAspectFlags imageAspectFlags)
{
	device = pVulkanContext->GetDevice();
	allocator = pVulkanContext->GetVulkanMemoryAllocator();

	extent = imageExtent;

	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.pNext = nullptr;
	imageCreateInfo.imageType = imageType;
	imageCreateInfo.format = format;
	imageCreateInfo.extent = imageExtent;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	if (imageAspectFlags & VK_IMAGE_ASPECT_COLOR_BIT)
		imageCreateInfo.usage = imageCreateInfo.usage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	if (imageAspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT)
		imageCreateInfo.usage = imageCreateInfo.usage | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	if (format != VK_FORMAT_R8G8B8A8_SRGB)
	{
		imageCreateInfo.usage = imageCreateInfo.usage | VK_IMAGE_USAGE_STORAGE_BIT;
	}

	VmaAllocationCreateInfo allocCreateInfo = {};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocCreateInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	CHECK_VKRESULT(vmaCreateImage(allocator, &imageCreateInfo, &allocCreateInfo, &deviceHandle, &allocation, &allocationInfo));

	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.pNext = nullptr;

	if (imageType == VK_IMAGE_TYPE_1D)
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
	else if (imageType == VK_IMAGE_TYPE_2D)
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	else if (imageType == VK_IMAGE_TYPE_3D)
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;

	imageViewCreateInfo.image = deviceHandle;
	imageViewCreateInfo.format = format;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = imageCreateInfo.mipLevels;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	imageViewCreateInfo.subresourceRange.aspectMask = imageAspectFlags;

	CHECK_VKRESULT(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageView));

	stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	accessMask = VK_ACCESS_2_NONE;
	imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

ruya::VulkanImage::~VulkanImage()
{
	vkDestroyImageView(device, imageView, nullptr);
	vmaDestroyImage(allocator, deviceHandle, allocation);
}

VkImage ruya::VulkanImage::GetDeviceHandle() const
{
	return deviceHandle;
}

VkImageView ruya::VulkanImage::GetImageView() const
{
	return imageView;
}

VkExtent3D ruya::VulkanImage::GetExtent() const
{
	return extent;
}
