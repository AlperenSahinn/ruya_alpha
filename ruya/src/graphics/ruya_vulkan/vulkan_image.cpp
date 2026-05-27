#include "vulkan_image.h"
#include "vulkan_helpers.h"
#include "vulkan_context.h"
#include "vulkan_buffer.h"
#include "vulkan_command_buffer.h"

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

ruya::VulkanImage::VulkanImage(VulkanContext* pVulkanContext, ktxTexture2* ktxTex, VkImageAspectFlags imageAspectFlags)
{
    device = pVulkanContext->GetDevice();
    allocator = pVulkanContext->GetVulkanMemoryAllocator();

    extent.width = ktxTex->baseWidth;
    extent.height = ktxTex->baseHeight;
    extent.depth = 1;

    const uint32_t mipLevels = ktxTex->numLevels;
    const VkFormat format = static_cast<VkFormat>(ktxTex->vkFormat);
    const size_t totalSize = static_cast<size_t>(ktxTex->dataSize);

    std::unique_ptr<VulkanBuffer> uploadBuffer = std::make_unique<VulkanBuffer>(
        pVulkanContext, totalSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);
    uploadBuffer->UploadData(ktxTex->pData, totalSize);

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent = extent;
    imageCreateInfo.mipLevels = mipLevels;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    CHECK_VKRESULT(vmaCreateImage(allocator, &imageCreateInfo, &allocCreateInfo,
        &deviceHandle, &allocation, &allocationInfo));

    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image = deviceHandle;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.subresourceRange.aspectMask = imageAspectFlags;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = mipLevels;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;
    CHECK_VKRESULT(vkCreateImageView(device, &viewCreateInfo, nullptr, &imageView));

    stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    accessMask = 0;
    imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    std::vector<VkBufferImageCopy> copyRegions;
    copyRegions.reserve(mipLevels);

    for (uint32_t mip = 0; mip < mipLevels; ++mip)
    {
        ktx_size_t mipOffset = 0;
        if (ktxTexture_GetImageOffset(ktxTexture(ktxTex), mip, 0, 0, &mipOffset) != KTX_SUCCESS)
            continue;

        VkBufferImageCopy region = {};
        region.bufferOffset = mipOffset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = imageAspectFlags;
        region.imageSubresource.mipLevel = mip;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = {
            std::max(1u, extent.width >> mip),
            std::max(1u, extent.height >> mip),
            1u
        };
        copyRegions.push_back(region);
    }

    VkImageSubresourceRange fullRange{ imageAspectFlags, 0, mipLevels, 0, 1 };

    ImmediateSubmitTransferList tl;
    tl.images.push_back({
        this,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        fullRange
        });

    pVulkanContext->ImmediateSubmitCommand([&](VulkanCommandBuffer* commandBuffer)
        {
            commandBuffer->ImageMemoryBarrier(
                this,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                fullRange);

            vkCmdCopyBufferToImage(
                commandBuffer->GetDeviceHandle(),
                uploadBuffer->GetDeviceHandle(),
                deviceHandle,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                static_cast<uint32_t>(copyRegions.size()),
                copyRegions.data());
        }, tl);

    stageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    accessMask = VK_ACCESS_SHADER_READ_BIT;
    imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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
