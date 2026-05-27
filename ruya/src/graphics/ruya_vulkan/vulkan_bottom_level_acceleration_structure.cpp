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

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
    rangeInfo.firstVertex = 0;
    rangeInfo.primitiveCount = maxPrimitiveCount;
    rangeInfo.primitiveOffset = 0;
    rangeInfo.transformOffset = 0;
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {};
    buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
        | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
    buildGeometryInfo.geometryCount = 1;
    buildGeometryInfo.pGeometries = &accelerationStructureGeometry;

    VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo = {};
    buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildGeometryInfo,
        &maxPrimitiveCount,
        &buildSizesInfo
    );

    auto initialAccelerationStructureBuffer = std::make_unique<VulkanBuffer>(
        pVulkanContext,
        buildSizesInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    VkAccelerationStructureKHR initialAccelerationStructure;
    VkAccelerationStructureCreateInfoKHR initialAsCreateInfo = {};
    initialAsCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    initialAsCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    initialAsCreateInfo.size = buildSizesInfo.accelerationStructureSize;
    initialAsCreateInfo.buffer = initialAccelerationStructureBuffer->GetDeviceHandle();

    CHECK_VKRESULT(vkCreateAccelerationStructureKHR(device, &initialAsCreateInfo, nullptr, &initialAccelerationStructure));

    buildGeometryInfo.dstAccelerationStructure = initialAccelerationStructure;

    auto scratchBuffer = std::make_unique<VulkanBuffer>(
        pVulkanContext,
        buildSizesInfo.buildScratchSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    buildGeometryInfo.scratchData.deviceAddress = scratchBuffer->GetDeviceAddress();

    VkQueryPool queryPool;
    VkQueryPoolCreateInfo queryPoolCreateInfo = {};
    queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolCreateInfo.queryCount = 1;
    queryPoolCreateInfo.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
    CHECK_VKRESULT(vkCreateQueryPool(device, &queryPoolCreateInfo, nullptr, &queryPool));

    pVulkanContext->ImmediateSubmitCommand([&](VulkanCommandBuffer* cb)
        {
            VkMemoryBarrier transferToBuildBarrier = {};
            transferToBuildBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            transferToBuildBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            transferToBuildBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

            vkCmdPipelineBarrier(
                cb->GetDeviceHandle(),
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                0,
                1, &transferToBuildBarrier,
                0, nullptr,
                0, nullptr
            );

            vkCmdBuildAccelerationStructuresKHR(
                cb->GetDeviceHandle(), 1, &buildGeometryInfo, &pRangeInfo);

            VkMemoryBarrier buildToQueryBarrier = {};
            buildToQueryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            buildToQueryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            buildToQueryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

            vkCmdPipelineBarrier(
                cb->GetDeviceHandle(),
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                0,
                1, &buildToQueryBarrier,
                0, nullptr,
                0, nullptr
            );

            vkCmdResetQueryPool(cb->GetDeviceHandle(), queryPool, 0, 1);
            vkCmdWriteAccelerationStructuresPropertiesKHR(
                cb->GetDeviceHandle(), 1,
                &buildGeometryInfo.dstAccelerationStructure,
                VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                queryPool, 0);
        });

    VkDeviceSize compactSize = 0;
    vkGetQueryPoolResults(
        device, queryPool, 0, 1,
        sizeof(VkDeviceSize), &compactSize,
        sizeof(VkDeviceSize),
        VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT
    );

    vkDestroyQueryPool(device, queryPool, nullptr);

    auto compactAccelerationStructureBuffer = std::make_unique<VulkanBuffer>(
        pVulkanContext,
        compactSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    VkAccelerationStructureKHR compactAccelerationStructure;
    VkAccelerationStructureCreateInfoKHR compactAsCreateInfo = {};
    compactAsCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    compactAsCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    compactAsCreateInfo.size = compactSize;
    compactAsCreateInfo.buffer = compactAccelerationStructureBuffer->GetDeviceHandle();

    CHECK_VKRESULT(vkCreateAccelerationStructureKHR(device, &compactAsCreateInfo, nullptr, &compactAccelerationStructure));

    VkCopyAccelerationStructureInfoKHR copyInfo = {};
    copyInfo.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
    copyInfo.src = initialAccelerationStructure;
    copyInfo.dst = compactAccelerationStructure;
    copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;

    ImmediateSubmitTransferList finalTl;

    finalTl.buffers.push_back({
        &vertexBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
        });

    finalTl.buffers.push_back({
        &indexBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_ACCESS_INDEX_READ_BIT
        });

    finalTl.buffers.push_back({
        compactAccelerationStructureBuffer.get(),
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
        });

    pVulkanContext->ImmediateSubmitCommand([&](VulkanCommandBuffer* cb)
        {
            vkCmdCopyAccelerationStructureKHR(cb->GetDeviceHandle(), &copyInfo);
        }, finalTl);

    vkDestroyAccelerationStructureKHR(device, initialAccelerationStructure, nullptr);
    initialAccelerationStructureBuffer.reset();
    scratchBuffer.reset();

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
