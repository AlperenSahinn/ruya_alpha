#include "vulkan_bottom_level_acceleration_structure.h"

#include "vulkan_context.h"
#include "vulkan_helpers.h"

ruya::VulkanBottomLevelAccelerationStructure::VulkanBottomLevelAccelerationStructure(
	VulkanContext* pVulkanContext,
	VulkanBuffer& vertexBuffer, 
	uint32_t vertexCount, 
	VkDeviceSize vertexSize, 
	VulkanBuffer& indexBuffer, 
	uint32_t indexCount)
{
	device = pVulkanContext->GetDevice();

	VkDeviceAddress vertexBufferDeviceAddress = vertexBuffer.GetDeviceAddress();
	VkDeviceAddress indexBufferDeviceAddress = indexBuffer.GetDeviceAddress();

	uint32_t maxPrimitiveCount = indexCount / 3;

	VkAccelerationStructureGeometryTrianglesDataKHR trianglesData = {};
	trianglesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	trianglesData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	trianglesData.vertexData.deviceAddress = vertexBufferDeviceAddress;
	trianglesData.vertexStride = vertexSize;
	trianglesData.indexType = VK_INDEX_TYPE_UINT32;
	trianglesData.indexData.deviceAddress = indexBufferDeviceAddress;
	trianglesData.maxVertex = vertexCount - 1;

	VkAccelerationStructureGeometryKHR accelerationStructureGeometry = {};
	accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	accelerationStructureGeometry.geometry.triangles = trianglesData;

	VkAccelerationStructureBuildRangeInfoKHR* accelerationStructureBuildRangeInfo = new VkAccelerationStructureBuildRangeInfoKHR();
	accelerationStructureBuildRangeInfo->firstVertex = 0;
	accelerationStructureBuildRangeInfo->primitiveCount = maxPrimitiveCount;
	accelerationStructureBuildRangeInfo->primitiveOffset = 0;
	accelerationStructureBuildRangeInfo->transformOffset = 0;

	VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = {};
	accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
	accelerationStructureBuildGeometryInfo.geometryCount = 1;
	accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo = {};
	accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	vkGetAccelerationStructureBuildSizesKHR(
		device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&accelerationStructureBuildGeometryInfo,
		&maxPrimitiveCount,
		&accelerationStructureBuildSizesInfo
	);

	VkAccelerationStructureKHR accelerationStructure;

	std::unique_ptr<VulkanBuffer> accelerationStructureBuffer = std::make_unique<VulkanBuffer>(
		pVulkanContext,
		accelerationStructureBuildSizesInfo.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

	VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo = {};
	accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
	accelerationStructureCreateInfo.buffer = accelerationStructureBuffer->GetDeviceHandle();

	CHECK_VKRESULT(vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &accelerationStructure));

	accelerationStructureBuildGeometryInfo.dstAccelerationStructure = accelerationStructure;

	std::unique_ptr<VulkanBuffer> scratchBuffer = std::make_unique<VulkanBuffer>(
		pVulkanContext,
		accelerationStructureBuildSizesInfo.buildScratchSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY
	);

	accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer->GetDeviceAddress();

	pVulkanContext->ImmediateSubmitCommand([&](VulkanCommandBuffer* commandBuffer)
		{
			vkCmdBuildAccelerationStructuresKHR(commandBuffer->GetDeviceHandle(), 1, &accelerationStructureBuildGeometryInfo, &accelerationStructureBuildRangeInfo);
		});

	VkQueryPool queryPool;
	VkQueryPoolCreateInfo queryPoolCreateInfo = {};
	queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	queryPoolCreateInfo.queryCount = 1;
	queryPoolCreateInfo.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
	vkCreateQueryPool(device, &queryPoolCreateInfo, nullptr, &queryPool);

	pVulkanContext->ImmediateSubmitCommand([&](VulkanCommandBuffer* commandBuffer)
		{
			vkCmdResetQueryPool(commandBuffer->GetDeviceHandle(), queryPool, 0, 1);
			vkCmdWriteAccelerationStructuresPropertiesKHR(commandBuffer->GetDeviceHandle(), 1, &accelerationStructureBuildGeometryInfo.dstAccelerationStructure,
				VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0);
		});

	std::vector<VkDeviceSize> compactSize(1);
	vkGetQueryPoolResults(device, queryPool, 0, 1, sizeof(VkDeviceSize), compactSize.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT);

	vkDestroyQueryPool(device, queryPool, nullptr);

	accelerationStructureBuildSizesInfo.accelerationStructureSize = compactSize[0];

	VkAccelerationStructureKHR compactAccelerationStructure;

	std::unique_ptr<VulkanBuffer> compactAccelerationStructureBuffer = std::make_unique<VulkanBuffer>(
		pVulkanContext,
		accelerationStructureBuildSizesInfo.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY
	);

	VkAccelerationStructureCreateInfoKHR compactAccelerationStructureCreateInfo{ };
	compactAccelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	compactAccelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
	compactAccelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	compactAccelerationStructureCreateInfo.buffer = compactAccelerationStructureBuffer->GetDeviceHandle();

	vkCreateAccelerationStructureKHR(device, &compactAccelerationStructureCreateInfo, nullptr, &compactAccelerationStructure);

	VkCopyAccelerationStructureInfoKHR copyAccelerationStructureInfo = {};
	copyAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
	copyAccelerationStructureInfo.src = accelerationStructureBuildGeometryInfo.dstAccelerationStructure;
	copyAccelerationStructureInfo.dst = compactAccelerationStructure;
	copyAccelerationStructureInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;

	pVulkanContext->ImmediateSubmitCommand([&](VulkanCommandBuffer* commandBufferd)
		{
			vkCmdCopyAccelerationStructureKHR(commandBufferd->GetDeviceHandle(), &copyAccelerationStructureInfo);
		});

	vkDestroyAccelerationStructureKHR(device, accelerationStructure, nullptr);


	deviceHandle = compactAccelerationStructure;
	buffer = std::move(compactAccelerationStructureBuffer);

	VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
	addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addressInfo.accelerationStructure = deviceHandle;

	deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
}

ruya::VulkanBottomLevelAccelerationStructure::~VulkanBottomLevelAccelerationStructure()
{
	vkDestroyAccelerationStructureKHR(device, deviceHandle, nullptr);
}

VkAccelerationStructureKHR ruya::VulkanBottomLevelAccelerationStructure::GetDeviceHandle() const
{
	return deviceHandle;
}

VkDeviceAddress ruya::VulkanBottomLevelAccelerationStructure::GetDeviceAddress() const
{
	return deviceAddress;
}
