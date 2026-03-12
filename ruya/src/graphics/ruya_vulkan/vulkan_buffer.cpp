#include "vulkan_buffer.h"
#include "vulkan_helpers.h"
#include "vulkan_context.h"

ruya::VulkanBuffer::VulkanBuffer(VulkanContext* pVulkanContext, size_t allocationSize, VkBufferUsageFlags bufferUsageFlags, VmaMemoryUsage memoryUsage)
{
	allocator = pVulkanContext->GetVulkanMemoryAllocator();

	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = allocationSize;
	bufferCreateInfo.usage = bufferUsageFlags | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	VmaAllocationCreateInfo allocationCreateInfo = {};
	allocationCreateInfo.usage = memoryUsage;
	allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	CHECK_VKRESULT(vmaCreateBufferWithAlignment(
		allocator,
		&bufferCreateInfo,
		&allocationCreateInfo,
		128,
		&deviceHandle,
		&allocation,
		&allocationInfo
	));

	VkBufferDeviceAddressInfo vertexBufferDeviceAdressInfo = {};
	vertexBufferDeviceAdressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	vertexBufferDeviceAdressInfo.buffer = deviceHandle;

	deviceAddress = vkGetBufferDeviceAddress(pVulkanContext->GetDevice(), &vertexBufferDeviceAdressInfo);

	stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	accessMask = VK_ACCESS_NONE;
}

ruya::VulkanBuffer::~VulkanBuffer()
{
	vmaDestroyBuffer(allocator, deviceHandle, allocation);
}

void ruya::VulkanBuffer::UploadData(void* pData, size_t dataSize)
{
	void* mappedData;
	vmaMapMemory(allocator, allocation, &mappedData);

	if (mappedData && pData && dataSize > 0)
	{
		std::memcpy(mappedData, pData, dataSize);
	}

	vmaUnmapMemory(allocator, allocation);
}

void* ruya::VulkanBuffer::MapMemory()
{
	void* mappedData;
	vmaMapMemory(allocator, allocation, &mappedData);
	return mappedData;
}

void ruya::VulkanBuffer::UnmapMemory()
{
	vmaUnmapMemory(allocator, allocation);
}

VkBuffer ruya::VulkanBuffer::GetDeviceHandle() const
{
	return deviceHandle;
}

VkDeviceAddress ruya::VulkanBuffer::GetDeviceAddress() const
{
	return deviceAddress;
}
