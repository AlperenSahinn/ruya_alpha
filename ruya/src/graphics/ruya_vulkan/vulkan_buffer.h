#pragma once
#include <Volk/volk.h>
#include <vulkan_memory_allocator/vk_mem_alloc.h>

namespace ruya
{
	class VulkanContext;

	class VulkanBuffer
	{
	public:
		VulkanBuffer(VulkanContext* pVulkanContext, size_t allocationSize, VkBufferUsageFlags bufferUsageFlags, VmaMemoryUsage memoryUsage);
		~VulkanBuffer();

		VulkanBuffer(const VulkanBuffer&) = delete;
		VulkanBuffer& operator=(const VulkanBuffer&) = delete;

		VulkanBuffer(VulkanBuffer&&) = default;
		VulkanBuffer& operator=(VulkanBuffer&&) = default;

		void UploadData(void* pData, size_t dataSize);
		void* MapMemory();
		void UnmapMemory();

		VkBuffer GetDeviceHandle() const;
		VkDeviceAddress GetDeviceAddress() const;

	public:
		VkPipelineStageFlags stageMask;
		VkAccessFlags accessMask;

	private:
		VkBuffer deviceHandle;
		VmaAllocation allocation;
		VmaAllocationInfo allocationInfo;
		VkDeviceAddress deviceAddress;
		VmaAllocator allocator;
	};
}