#pragma once
#include <vector>
#include <string>
#include <stdint.h>

#include <volk/volk.h>
#include "vulkan_descriptor_set_layout.h"

namespace ruya
{
    struct VertexInputDescription
    {
        std::vector<VkVertexInputBindingDescription>   bindings;
        std::vector<VkVertexInputAttributeDescription> attributes;
    };

    class VulkanRasterizationPipeline
    {
    public:
        VulkanRasterizationPipeline(
            VulkanContext* pVulkanContext,
            const std::string& vertexShaderPath,
            const std::string& fragmentShaderPath,
            std::vector<VulkanDescriptorSetLayout*>& descriptorSetLayouts,
            std::vector<VkFormat>& colorAttachments,
            VkFormat& depthAttachments,
            bool enableDepthTest,
            VkCompareOp depthCompareOp,
            std::vector<VkPushConstantRange>& pushConstantRanges,
            bool depthClipEnable = true,
            VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            const VertexInputDescription& vertexInput = {});

        ~VulkanRasterizationPipeline();

        VulkanRasterizationPipeline(const VulkanRasterizationPipeline&) = delete;
        VulkanRasterizationPipeline& operator=(const VulkanRasterizationPipeline&) = delete;

        VkPipeline       GetDeviceHandle()   const;
        VkPipelineLayout GetPipelineLayout() const;

    private:
        VkPipelineLayout pipelineLayout;
        VkPipeline       deviceHandle;
        VkDevice         device;
    };
}