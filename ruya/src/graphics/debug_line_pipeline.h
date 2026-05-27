#pragma once
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ruya_vulkan/vulkan_rasterization_pipeline.h"
#include "ruya_vulkan/vulkan_buffer.h"
#include "descriptor_set_layout.h"
#include "texture_2d.h"

namespace ruya
{
    struct DebugVertex 
    {
        glm::vec4 position;
        glm::vec4 color;
    };

    class DebugLinePipeline
    {
    public:
        DebugLinePipeline();
        ~DebugLinePipeline() = default;

        DebugLinePipeline(const DebugLinePipeline&) = delete;
        DebugLinePipeline& operator=(const DebugLinePipeline&) = delete;

        void Dispatch(
            uint32_t width, uint32_t height,
            Texture2D* colorAttachment,
            Texture2D* pDepthAttachment);

    private:
        std::unique_ptr<VulkanRasterizationPipeline> pipeline;
        std::unique_ptr<VulkanBuffer>                vertexBuffer;
        uint32_t                                     vertexCount = 0;

        static constexpr uint32_t kMaxDebugVertices = 1048576;
    };
}