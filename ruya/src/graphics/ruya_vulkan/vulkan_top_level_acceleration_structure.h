#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>

#include <glm/glm.hpp>

#include <core/ry_id.h>

#include "volk/volk.h"
#include "vulkan_buffer.h"
#include "vulkan_bottom_level_acceleration_structure.h"

namespace ruya
{
    class VulkanContext;
    class VulkanCommandBuffer;

	struct VulkanBottomLevelAccelerationStructureInstance
	{
		VulkanBottomLevelAccelerationStructure* blas;
		uint32_t instanceCustomIndex;
		glm::mat4 transform;
	};

    class VulkanTopLevelAccelerationStructure
    {
    public:
        VulkanTopLevelAccelerationStructure(VulkanContext* pVulkanContext, const std::vector<std::pair<uint32_t, VulkanBottomLevelAccelerationStructureInstance*>>& blasInstances, VulkanCommandBuffer* pCommandBuffer);
        ~VulkanTopLevelAccelerationStructure();

        VkAccelerationStructureKHR GetDeviceHandle() const;
        VulkanBuffer* GetBLASInstancesBuffer() const;
        VulkanBuffer* GetScratchBuffer() const;
        uint32_t GetInstanceCount() const;

    private:
        VkAccelerationStructureKHR deviceHandle;
        std::unique_ptr<VulkanBuffer> tlasBuffer;
        std::unique_ptr<VulkanBuffer> blasInstancesBuffer;
        std::unique_ptr<VulkanBuffer> scratchBuffer;

        uint32_t instanceCount;

        VkDevice device;

        std::unique_ptr<VulkanBuffer> stagingBuffer;
    };
}