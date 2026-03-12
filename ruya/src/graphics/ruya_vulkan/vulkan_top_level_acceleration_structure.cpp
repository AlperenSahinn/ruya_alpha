#include "vulkan_top_level_acceleration_structure.h"
#include "vulkan_context.h"

ruya::VulkanTopLevelAccelerationStructure::VulkanTopLevelAccelerationStructure(VulkanContext* pVulkanContext, std::unordered_map<RyID, std::unique_ptr<VulkanBottomLevelAccelerationStructureInstance>>& blasInstances)
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

    for (auto& pair : blasInstances)
    {
        VulkanBottomLevelAccelerationStructureInstance* blasInstance = pair.second.get();

        const glm::mat4& transform = blasInstance->transform;

        VkAccelerationStructureInstanceKHR asInstance = {};
        glm::mat4 transposed = glm::transpose(transform);
        memcpy(&asInstance.transform.matrix[0][0], &transposed[0][0], sizeof(float) * 4);
        memcpy(&asInstance.transform.matrix[1][0], &transposed[1][0], sizeof(float) * 4);
        memcpy(&asInstance.transform.matrix[2][0], &transposed[2][0], sizeof(float) * 4);

        asInstance.instanceCustomIndex = blasInstance->instanceCustomIndex;

        VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
        addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addressInfo.accelerationStructure = blasInstance->blas->GetDeviceHandle();

        asInstance.accelerationStructureReference =
            static_cast<uint64_t>(vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo));

        asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        asInstance.mask = 0xFF;
        asInstance.instanceShaderBindingTableRecordOffset = 0;

        tlasInstances.emplace_back(asInstance);
    }

    std::unique_ptr<VulkanBuffer> stagingBuffer = std::make_unique<VulkanBuffer>(
        pVulkanContext,
        instanceCount * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );

    void* data = stagingBuffer->MapMemory();
    memcpy(data, tlasInstances.data(), instanceCount * sizeof(VkAccelerationStructureInstanceKHR));
    stagingBuffer->UnmapMemory();

    VkBufferCopy copyRegion = {};
    copyRegion.size = instanceCount * sizeof(VkAccelerationStructureInstanceKHR);

    pVulkanContext->ImmediateSubmitCommand([&](VulkanCommandBuffer* commandBuffer)
        {
            commandBuffer->CopyBufferToBuffer(*stagingBuffer, blasInstancesBuffer.get(), copyRegion);
        });

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
        &instanceCount,
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

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{ instanceCount, 0, 0, 0 };
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    pVulkanContext->ImmediateSubmitCommand([&](VulkanCommandBuffer* commandBuffer)
        {
            vkCmdBuildAccelerationStructuresKHR(
                commandBuffer->GetDeviceHandle(),
                1,
                &buildInfo,
                &pRangeInfo
            );
        });
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
