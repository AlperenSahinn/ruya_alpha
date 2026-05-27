#include "vulkan_rasterization_pipeline.h"
#include "vulkan_helpers.h"
#include "vulkan_context.h"
#include "vulkan_shader.h"

ruya::VulkanRasterizationPipeline::VulkanRasterizationPipeline(
    VulkanContext* pVulkanContext,
    const std::string& vertexShaderPath,
    const std::string& fragmentShaderPath,
    std::vector<VulkanDescriptorSetLayout*>& descriptorSetLayouts,
    std::vector<VkFormat>& colorAttachments,
    VkFormat& depthAttachments,
    bool enableDepthTest,
    VkCompareOp depthCompareOp,
    std::vector<VkPushConstantRange>& pushConstantRanges,
    bool depthClipEnable,
    VkPrimitiveTopology topology,
    const VertexInputDescription& vertexInput)
{
    device = pVulkanContext->GetDevice();

    std::unique_ptr<VulkanShader> vertexShader = std::make_unique<VulkanShader>(pVulkanContext, vertexShaderPath.c_str());
    std::unique_ptr<VulkanShader> fragmentShader = std::make_unique<VulkanShader>(pVulkanContext, fragmentShaderPath.c_str());

    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingCreateInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    pipelineRenderingCreateInfo.pColorAttachmentFormats = colorAttachments.data();
    pipelineRenderingCreateInfo.depthAttachmentFormat = depthAttachments;

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
    vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageCreateInfo.pNext = nullptr;
    vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStageCreateInfo.module = vertexShader->GetDeviceHandle();
    vertexShaderStageCreateInfo.pName = "main";
    shaderStages.push_back(vertexShaderStageCreateInfo);

    VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
    fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStageCreateInfo.pNext = nullptr;
    fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStageCreateInfo.module = fragmentShader->GetDeviceHandle();
    fragmentShaderStageCreateInfo.pName = "main";
    shaderStages.push_back(fragmentShaderStageCreateInfo);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInput.bindings.size());
    vertexInputInfo.pVertexBindingDescriptions = vertexInput.bindings.empty() ? nullptr : vertexInput.bindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInput.attributes.size());
    vertexInputInfo.pVertexAttributeDescriptions = vertexInput.attributes.empty() ? nullptr : vertexInput.attributes.data();

    VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {};
    pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipelineInputAssemblyStateCreateInfo.topology = topology;
    pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {};
    pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    pipelineDynamicStateCreateInfo.dynamicStateCount = 2;
    pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStates;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationDepthClipStateCreateInfoEXT depthClipState = {};

    VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
    pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipelineRasterizationStateCreateInfo.lineWidth = 2.0f;
    pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;

    if (!depthClipEnable)
    {
        depthClipState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT;
        depthClipState.pNext = nullptr;
        depthClipState.depthClipEnable = VK_FALSE;
        pipelineRasterizationStateCreateInfo.pNext = &depthClipState;
    }

    VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {};
    pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipelineMultisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    pipelineMultisampleStateCreateInfo.minSampleShading = 1.0f;
    pipelineMultisampleStateCreateInfo.pSampleMask = VK_NULL_HANDLE;
    pipelineMultisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
    pipelineMultisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

    std::vector<VkPipelineColorBlendAttachmentState> pipelineColorBlendAttachmentStates;
    for (size_t i = 0; i < colorAttachments.size(); i++)
    {
        VkPipelineColorBlendAttachmentState state = {};
        state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        state.blendEnable = VK_FALSE;
        pipelineColorBlendAttachmentStates.push_back(state);
    }

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = static_cast<uint32_t>(colorAttachments.size());
    colorBlending.pAttachments = pipelineColorBlendAttachmentStates.data();

    VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {};
    pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    if (enableDepthTest)
    {
        pipelineDepthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
        pipelineDepthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
        pipelineDepthStencilStateCreateInfo.depthCompareOp = depthCompareOp;
        pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
        pipelineDepthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
        pipelineDepthStencilStateCreateInfo.minDepthBounds = 0.f;
        pipelineDepthStencilStateCreateInfo.maxDepthBounds = 1.f;
    }
    else
    {
        pipelineDepthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
        pipelineDepthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
        pipelineDepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
        pipelineDepthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
        pipelineDepthStencilStateCreateInfo.minDepthBounds = 0.f;
        pipelineDepthStencilStateCreateInfo.maxDepthBounds = 1.f;
    }

    std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts;
    for (VulkanDescriptorSetLayout* layout : descriptorSetLayouts)
        vkDescriptorSetLayouts.push_back(layout->GetDeviceHandle());

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
    pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.empty() ? nullptr : pushConstantRanges.data();
    pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(vkDescriptorSetLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts = vkDescriptorSetLayouts.empty() ? nullptr : vkDescriptorSetLayouts.data();

    CHECK_VKRESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &pipelineRenderingCreateInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
    pipelineInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &pipelineDepthStencilStateCreateInfo;
    pipelineInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
    pipelineInfo.layout = pipelineLayout;

    CHECK_VKRESULT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &deviceHandle));
}

ruya::VulkanRasterizationPipeline::~VulkanRasterizationPipeline()
{
    vkDestroyPipeline(device, deviceHandle, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
}

VkPipeline ruya::VulkanRasterizationPipeline::GetDeviceHandle() const { return deviceHandle; }
VkPipelineLayout ruya::VulkanRasterizationPipeline::GetPipelineLayout() const { return pipelineLayout; }