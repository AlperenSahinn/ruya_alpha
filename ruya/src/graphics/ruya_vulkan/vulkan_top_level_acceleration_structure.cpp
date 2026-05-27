#include "vulkan_top_level_acceleration_structure.h"
#include "vulkan_context.h"

ruya::VulkanTopLevelAccelerationStructure::VulkanTopLevelAccelerationStructure(VulkanContext* pVulkanContext, const std::vector<std::pair<uint32_t, VulkanBottomLevelAccelerationStructureInstance*>>& blasInstances, VulkanCommandBuffer* pCommandBuffer)
{
    device = pVulkanContext->GetDevice();

    instanceCount = static_cast<uint32_t>(blasInstances.size());

    blasInstancesBuffer = std::make_unique<VulkanBuffer>(
        pVulkanContext,
        instanceCount * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
    tlasInstances.reserve(instanceCount);

    for (auto& blasInstancePair : blasInstances)
    {
        const VulkanBottomLevelAccelerationStructureInstance* blasInstance = blasInstancePair.second;

        const glm::mat4& transform = blasInstance->transform;

        VkAccelerationStructureInstanceKHR asInstance = {};
        glm::mat4 transposed = glm::transpose(transform);
        memcpy(&asInstance.transform.matrix[0][0], &transposed[0][0], sizeof(float) * 4);
        memcpy(&asInstance.transform.matrix[1][0], &transposed[1][0], sizeof(float) * 4);
        memcpy(&asInstance.transform.matrix[2][0], &transposed[2][0], sizeof(float) * 4);

        asInstance.instanceCustomIndex = blasInstance->instanceCustomIndex;

        VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
        addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;

        if (blasInstance->blas == nullptr)
        {
            continue;
        }

        addressInfo.accelerationStructure = blasInstance->blas->GetDeviceHandle();

        asInstance.accelerationStructureReference =
            static_cast<uint64_t>(vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo));

        asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        asInstance.mask = 0xFF;
        asInstance.instanceShaderBindingTableRecordOffset = 0;

        tlasInstances.emplace_back(asInstance);
    }

    if (tlasInstances.empty())
    {
        return;
    }

    uint32_t builtInstanceCount = static_cast<uint32_t>(tlasInstances.size());

    stagingBuffer = std::make_unique<VulkanBuffer>(
        pVulkanContext,
        builtInstanceCount * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );

    void* data = stagingBuffer->MapMemory();
    memcpy(data, tlasInstances.data(), builtInstanceCount * sizeof(VkAccelerationStructureInstanceKHR));
    stagingBuffer->UnmapMemory();

    VkBufferCopy copyRegion = {};
    copyRegion.size = builtInstanceCount * sizeof(VkAccelerationStructureInstanceKHR);

    pCommandBuffer->CopyBufferToBuffer(*stagingBuffer, blasInstancesBuffer.get(), copyRegion);

    VkBufferMemoryBarrier copyBarrier = {};
    copyBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    copyBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    copyBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    copyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    copyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    copyBarrier.buffer = blasInstancesBuffer->GetDeviceHandle();
    copyBarrier.offset = 0;
    copyBarrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        pCommandBuffer->GetDeviceHandle(),
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        0,
        0, nullptr,
        1, &copyBarrier,
        0, nullptr
    );

    VkAccelerationStructureGeometryInstancesDataKHR instancesData = {};
    instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesData.data.deviceAddress = blasInstancesBuffer->GetDeviceAddress();

    VkAccelerationStructureGeometryKHR asGeometry = {};
    asGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    asGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    asGeometry.geometry.instances = instancesData;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
        VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &asGeometry;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &builtInstanceCount,
        &sizeInfo
    );

    tlasBuffer = std::make_unique<VulkanBuffer>(
        pVulkanContext,
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    VkAccelerationStructureCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.buffer = tlasBuffer->GetDeviceHandle();

    vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &deviceHandle);

    scratchBuffer = std::make_unique<VulkanBuffer>(
        pVulkanContext,
        sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    buildInfo.dstAccelerationStructure = deviceHandle;
    buildInfo.scratchData.deviceAddress = scratchBuffer->GetDeviceAddress();

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{ builtInstanceCount, 0, 0, 0 };
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    vkCmdBuildAccelerationStructuresKHR(
        pCommandBuffer->GetDeviceHandle(),
        1,
        &buildInfo,
        &pRangeInfo
    );

    VkMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(
        pCommandBuffer->GetDeviceHandle(),
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        1, &barrier,
        0, nullptr,
        0, nullptr
    );
}

ruya::VulkanTopLevelAccelerationStructure::~VulkanTopLevelAccelerationStructure()
{
    vkDestroyAccelerationStructureKHR(device, deviceHandle, nullptr);
}

VkAccelerationStructureKHR ruya::VulkanTopLevelAccelerationStructure::GetDeviceHandle() const
{
    return deviceHandle;
}

ruya::VulkanBuffer* ruya::VulkanTopLevelAccelerationStructure::GetBLASInstancesBuffer() const
{
    return blasInstancesBuffer.get();
}

ruya::VulkanBuffer* ruya::VulkanTopLevelAccelerationStructure::GetScratchBuffer() const
{
    return scratchBuffer.get();
}

uint32_t ruya::VulkanTopLevelAccelerationStructure::GetInstanceCount() const
{
    return instanceCount;
}
