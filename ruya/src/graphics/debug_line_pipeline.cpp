#include "debug_line_pipeline.h"

#include <engine.h>

ruya::DebugLinePipeline::DebugLinePipeline()
{
    std::vector<VkFormat> colorFormats = { VK_FORMAT_R32G32B32A32_SFLOAT };
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    std::vector<VulkanDescriptorSetLayout*> layouts = {};

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcRange.offset = 0;
    pcRange.size = 64;
    std::vector<VkPushConstantRange> pushConstants = { pcRange };

    VertexInputDescription vertexInput;

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(DebugVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexInput.bindings.push_back(binding);

    VkVertexInputAttributeDescription posAttr{};
    posAttr.location = 0;
    posAttr.binding = 0;
    posAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    posAttr.offset = offsetof(DebugVertex, position);
    vertexInput.attributes.push_back(posAttr);

    VkVertexInputAttributeDescription colorAttr{};
    colorAttr.location = 1;
    colorAttr.binding = 0;
    colorAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    colorAttr.offset = offsetof(DebugVertex, color);
    vertexInput.attributes.push_back(colorAttr);

    std::string vertexShaderPath = ASSETS_DIR + "ruya_files/shaders/compiled/debug_line_vertex_shader.spv";
    std::string fragmentShaderPath = ASSETS_DIR + "ruya_files/shaders/compiled/debug_line_fragment_shader.spv";

    pipeline = std::make_unique<VulkanRasterizationPipeline>(
        engine->GetGraphics()->GetVulkanContext(),
        vertexShaderPath,
        fragmentShaderPath,
        layouts,
        colorFormats,
        depthFormat,
        true,
        VK_COMPARE_OP_LESS_OR_EQUAL,
        pushConstants,
        true,
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
        vertexInput);

    vertexBuffer = std::make_unique<VulkanBuffer>(
        engine->GetGraphics()->GetVulkanContext(),
        kMaxDebugVertices * sizeof(DebugVertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);
}

void ruya::DebugLinePipeline::Dispatch(uint32_t width, uint32_t height, Texture2D* colorAttachment, Texture2D* pDepthAttachment)
{
    std::vector<DebugVertex>& lines = engine->GetRenderDataReadBuffer()->debugVertexLines;

    if (lines.empty())
    {
        vertexCount = 0; return;
    }

    vertexCount = static_cast<uint32_t>(lines.size());
    vertexBuffer->UploadData(
        (void*)lines.data(),
        vertexCount * sizeof(DebugVertex));

    auto* cmd = engine->GetGraphics()->GetVulkanContext()->GetCurrentFrameResource()->GetCommandBuffer();

    VkImageSubresourceRange colorRange{};
    colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorRange.baseMipLevel = 0;
    colorRange.levelCount = 1;
    colorRange.baseArrayLayer = 0;
    colorRange.layerCount = 1;

    cmd->ImageMemoryBarrier(
        colorAttachment->GetVulkanImage(),
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        colorRange);

    VkImageSubresourceRange depthRange{};
    depthRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthRange.baseMipLevel = 0;
    depthRange.levelCount = 1;
    depthRange.baseArrayLayer = 0;
    depthRange.layerCount = 1;

    cmd->ImageMemoryBarrier(
        pDepthAttachment->GetVulkanImage(),
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        depthRange);

    VkRenderingAttachmentInfo colorInfo = {};
    colorInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorInfo.imageView = colorAttachment->GetVulkanImage()->GetImageView();
    colorInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depthInfo = {};
    depthInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthInfo.imageView = pDepthAttachment->GetVulkanImage()->GetImageView();
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    std::vector<VkRenderingAttachmentInfo> colorAttachments = { colorInfo };
    cmd->BeginRenderPass({ width, height }, colorAttachments, &depthInfo);

    cmd->BindRasterizationPipeline(*pipeline);
    cmd->BindVertexBuffer(*vertexBuffer);

    glm::mat4 viewProj = engine->GetRenderDataReadBuffer()->camera.viewproj;
    cmd->PushConstant(&viewProj, 64, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT);

    cmd->Draw(vertexCount, 1, 0, 0);

    cmd->EndRenderPass();
}