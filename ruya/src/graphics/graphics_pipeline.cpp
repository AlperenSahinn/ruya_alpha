#include "graphics_pipeline.h"

#include <fstream>
#include <sstream>
#include <algorithm>

#include <core/assert.h>
#include <engine.h>

#include "ruya_vulkan/vulkan_descriptor_writer.h"

ruya::GraphicsPipeline::GraphicsPipeline(std::string graphicsPipelineFilePath)
{
    std::ifstream file(graphicsPipelineFilePath);
    ENGINE_ASSERT_MSG(file.is_open(),
        "[GraphicsPipeline] GraphicsPipeline file could not be opened: {}", graphicsPipelineFilePath);

    if (!file.is_open())
    {
        return;
    }

    nlohmann::json j;
    try
    {
        file >> j;
    }
    catch (const nlohmann::json::parse_error& e)
    {
        ENGINE_ASSERT_MSG(false,
            "[GraphicsPipeline] GraphicsPipeline JSON parse error in {}: {}",
            graphicsPipelineFilePath, e.what());
        return;
    }

    try
    {
        j.get_to(*this);
    }
    catch (const nlohmann::json::exception& e)
    {
        ENGINE_ASSERT_MSG(false,
            "[GraphicsPipeline] GraphicsPipeline deserialization failed for {}: {}",
            graphicsPipelineFilePath, e.what());
    }

    CreatePipeline();

    RUYA_LOG_INFO("[GraphicsPipeline] Graphics pipeline created");
}

ruya::GraphicsPipeline::~GraphicsPipeline()
{

}

void ruya::GraphicsPipeline::RecreateFrameBuffers()
{
    frameBuffers.clear();

    for (uint32_t i = 0; i < engine->GetGraphics()->GetFrameBufferCount(); ++i)
    {
        frameBuffers.push_back(CreateGraphicsPipelineFrameBuffer());
    }
}

void ruya::GraphicsPipeline::Dispatch()
{
    GraphicsPipelineFrameBuffer* frameBuffer = frameBuffers[engine->GetGraphics()->GetCurrentFrameIndex()].get();

    VulkanCommandBuffer* commandBuffer = engine->GetGraphics()->GetVulkanContext()->GetCurrentFrameResource()->GetCommandBuffer();

    std::unique_ptr<VulkanDescriptorWriter> frameBufferDescriptorWriter = std::make_unique<VulkanDescriptorWriter>(engine->GetGraphics()->GetVulkanContext());

    frameBuffer->cameraDataBufferDescriptorSet->vkDescriptorSet = engine->GetGraphics()->GetVulkanContext()->GetCurrentFrameResource()->GetDescriptorPool()->AllocateDescriptorSet(engine->GetGraphics()->GetCameraDataBufferDescriptorSetLayout()->GetVulkanDescriptorSetLayout()->GetDeviceHandle());
    frameBuffer->cameraDataBuffer->UploadData(&engine->GetRenderDataReadBuffer()->camera, sizeof(Camera));
    frameBufferDescriptorWriter->WriteDescriptorBuffer(0, frameBuffer->cameraDataBuffer->GetDeviceHandle(), sizeof(Camera), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    frameBufferDescriptorWriter->UpdateDescriptors(frameBuffer->cameraDataBufferDescriptorSet->vkDescriptorSet);
    frameBufferDescriptorWriter.reset();

    frameBuffer->directionalLightDataBufferDescriptorSet->vkDescriptorSet = engine->GetGraphics()->GetVulkanContext()->GetCurrentFrameResource()->GetDescriptorPool()->AllocateDescriptorSet(engine->GetGraphics()->GetDirectionalLightDataBufferDescriptorSetLayout()->GetVulkanDescriptorSetLayout()->GetDeviceHandle());
    frameBufferDescriptorWriter = std::make_unique<VulkanDescriptorWriter>(engine->GetGraphics()->GetVulkanContext());
    frameBuffer->directionalLightDataBuffer->UploadData(&engine->GetRenderDataReadBuffer()->directionalLight, sizeof(AtmosphericLight));
    frameBufferDescriptorWriter->WriteDescriptorBuffer(0, frameBuffer->directionalLightDataBuffer->GetDeviceHandle(), sizeof(AtmosphericLight), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    frameBufferDescriptorWriter->UpdateDescriptors(frameBuffer->directionalLightDataBufferDescriptorSet->vkDescriptorSet);
    frameBufferDescriptorWriter.reset();

    const uint32_t frameCount = static_cast<uint32_t>(frameBuffers.size());
    const uint32_t currentFrameIdx = engine->GetGraphics()->GetCurrentFrameIndex();
    const uint32_t prevFrameIdx = (frameCount >= 2)
        ? ((currentFrameIdx + frameCount - 1) % frameCount)
        : currentFrameIdx;

    for (uint32_t i = 0; i < pipelineBlocks.size(); ++i)
    {
        const GraphicsPipelineBlock& pipelineBlock = pipelineBlocks[i];

        if (pipelinesDescriptorSetLayouts.contains(i))
        {
            DescriptorSet* descriptorSet = frameBuffer->texture2DDescriptorSets[i].get();
            descriptorSet->vkDescriptorSet = engine->GetGraphics()->GetVulkanContext()->GetCurrentFrameResource()->GetDescriptorPool()->AllocateDescriptorSet(pipelinesDescriptorSetLayouts[i]->GetVulkanDescriptorSetLayout()->GetDeviceHandle());

            std::unique_ptr<VulkanDescriptorWriter> descriptorWriter = std::make_unique<VulkanDescriptorWriter>(engine->GetGraphics()->GetVulkanContext());

            uint32_t binding = 0;
            for (uint32_t j = 0; j < pipelineBlock.pipelineImageDescriptors.size(); ++j)
            {
                const PipelineImageDescriptor& imageDescriptor = pipelineBlock.pipelineImageDescriptors[j];
                const std::string& name = imageDescriptor.pipelineImageName;
                PipelineImageDescriptorType type = imageDescriptor.type;

                GraphicsPipelineFrameBuffer* sourceFrameBuffer =
                    (imageDescriptor.frameIndex == PipelineImageDescriptorFrameIndex::PrevFrame)
                    ? frameBuffers[prevFrameIdx].get()
                    : frameBuffer;

                if (type == PipelineImageDescriptorType::Storage)
                {
                    descriptorWriter->WriteDescriptorImage(binding, sourceFrameBuffer->texture2Ds[name]->GetVulkanImage()->GetImageView(), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
                    binding++;
                }
                else if (type == PipelineImageDescriptorType::Sampled)
                {
                    VkSampler sampler = (imageDescriptor.filter == PipelineImageDescriptorFilter::Nearest)
                        ? engine->GetGraphics()->GetNearestSampler()
                        : engine->GetGraphics()->GetLinearSampler();

                    descriptorWriter->WriteDescriptorImage(binding, sourceFrameBuffer->texture2Ds[name]->GetVulkanImage()->GetImageView(), sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                    binding++;
                }
            }

            descriptorWriter->UpdateDescriptors(descriptorSet->vkDescriptorSet);
        }
    }

    for (uint32_t i = 0; i < pipelineBlocks.size(); ++i)
    {
        const GraphicsPipelineBlock& pipelineBlock = pipelineBlocks[i];

        std::vector<DescriptorSet*> ds;

        for (uint32_t j = 0; j < pipelineBlock.standartDescriptorTypes.size(); ++j)
        {
            StandartDescriptorType standartDescriptorType = pipelineBlock.standartDescriptorTypes[j];
            if (standartDescriptorType == StandartDescriptorType::CameraData) ds.push_back(frameBuffer->cameraDataBufferDescriptorSet.get());
            else if (standartDescriptorType == StandartDescriptorType::AtmosphericLightData) ds.push_back(frameBuffer->directionalLightDataBufferDescriptorSet.get());
            else if (standartDescriptorType == StandartDescriptorType::Texture2Ds) ds.push_back(engine->GetGraphics()->GetTexture2DsDescriptorSet());
            else if (standartDescriptorType == StandartDescriptorType::RenderMaterials) ds.push_back(engine->GetGraphics()->GetRenderMaterialsDescriptorSet());
            else if (standartDescriptorType == StandartDescriptorType::RenderItems) ds.push_back(engine->GetGraphics()->GetRenderItemsDescriptorSet());
            else if (standartDescriptorType == StandartDescriptorType::TLAS) ds.push_back(engine->GetGraphics()->GetCurrentFramesTLASDescriptorSet());
        }

        if (pipelineBlock.type == GraphicsPipelineBlockType::Rasterization)
        {
            std::vector<Texture2D*> colorAttachmentImages;
            Texture2D* depthAttachment = nullptr;

            for (uint32_t j = 0; j < pipelineBlock.pipelineImageDescriptors.size(); ++j)
            {
                const std::string& name = pipelineBlock.pipelineImageDescriptors[j].pipelineImageName;
                PipelineImageDescriptorType type = pipelineBlock.pipelineImageDescriptors[j].type;

                if (type == PipelineImageDescriptorType::ColorAttachment) colorAttachmentImages.push_back(frameBuffer->texture2Ds[name].get());
                else if (type == PipelineImageDescriptorType::DepthAttachment) depthAttachment = frameBuffer->texture2Ds[name].get();
            }

            if (frameBuffer->texture2DDescriptorSets.contains(i))
            {
                ds.push_back(frameBuffer->texture2DDescriptorSets[i].get());
            }

            std::vector<RyID> ids;

            std::vector<RenderItem>& renderItems = engine->GetGraphics()->GetRenderItems();

            for (size_t s = 0; s < renderItems.size(); ++s)
            {
                RyID RenderItemID = RyID(s);
                RenderItem& renderItem = renderItems[s];

                if (RenderItemID.IsValid() && renderItem.GetVisibility() && renderItem.IsValid())
                {
                    ids.push_back(RenderItemID);
                }
            }

            uint32_t rasterWidth = engine->GetGraphics()->GetFrameBufferWidth();
            uint32_t rasterHeight = engine->GetGraphics()->GetFrameBufferHeight();

            switch (pipelineBlock.dispatchSize)
            {
            case GraphicsPipelineBlockDispatchSize::Full:
                break;
            case GraphicsPipelineBlockDispatchSize::Half:
                rasterWidth = std::max(1u, rasterWidth / 2);
                rasterHeight = std::max(1u, rasterHeight / 2);
                break;
            case GraphicsPipelineBlockDispatchSize::Quarter:
                rasterWidth = std::max(1u, rasterWidth / 4);
                rasterHeight = std::max(1u, rasterHeight / 4);
                break;
            case GraphicsPipelineBlockDispatchSize::Custom:
                rasterWidth = pipelineBlock.customDispatchSize.width;
                rasterHeight = pipelineBlock.customDispatchSize.height;
                break;
            }

            rasterizationPipelines[i]->Dispatch(
                rasterWidth,
                rasterHeight,
                colorAttachmentImages,
                depthAttachment,
                ds,
                ids);
        }

        else if (pipelineBlock.type == GraphicsPipelineBlockType::RayTracing)
        {
            if (!engine->GetGraphics()->IsTLASValid())
                return;

            if (frameBuffer->texture2DDescriptorSets.contains(i))
            {
                ds.push_back(frameBuffer->texture2DDescriptorSets[i].get());
            }

            std::vector<Texture2D*> imageReads;
            std::vector<Texture2D*> imageWrites;

            for (uint32_t j = 0; j < pipelineBlock.pipelineImageDescriptors.size(); ++j)
            {
                const PipelineImageDescriptor& imageDescriptor = pipelineBlock.pipelineImageDescriptors[j];
                const std::string& name = imageDescriptor.pipelineImageName;
                PipelineImageDescriptorUsage usage = imageDescriptor.usage;
                PipelineImageDescriptorType type = imageDescriptor.type;

                GraphicsPipelineFrameBuffer* sourceFrameBuffer =
                    (imageDescriptor.frameIndex == PipelineImageDescriptorFrameIndex::PrevFrame)
                    ? frameBuffers[prevFrameIdx].get()
                    : frameBuffer;

                if (usage == PipelineImageDescriptorUsage::Read) imageReads.push_back(sourceFrameBuffer->texture2Ds[name].get());
                else if (usage == PipelineImageDescriptorUsage::Write) imageWrites.push_back(sourceFrameBuffer->texture2Ds[name].get());
            }

            uint32_t rtWidth = engine->GetGraphics()->GetFrameBufferWidth();
            uint32_t rtHeight = engine->GetGraphics()->GetFrameBufferHeight();

            switch (pipelineBlock.dispatchSize)
            {
            case GraphicsPipelineBlockDispatchSize::Full:
                break;
            case GraphicsPipelineBlockDispatchSize::Half:
                rtWidth = std::max(1u, rtWidth / 2);
                rtHeight = std::max(1u, rtHeight / 2);
                break;
            case GraphicsPipelineBlockDispatchSize::Quarter:
                rtWidth = std::max(1u, rtWidth / 4);
                rtHeight = std::max(1u, rtHeight / 4);
                break;
            case GraphicsPipelineBlockDispatchSize::Custom:
                rtWidth = pipelineBlock.customDispatchSize.width;
                rtHeight = pipelineBlock.customDispatchSize.height;
                break;
            }

            rayTracingPipelines[i]->Dispatch(rtWidth, rtHeight, imageReads, imageWrites, ds);
        }

        else if (pipelineBlock.type == GraphicsPipelineBlockType::Compute)
        {
            if (frameBuffer->texture2DDescriptorSets.contains(i))
            {
                ds.push_back(frameBuffer->texture2DDescriptorSets[i].get());
            }

            std::vector<Texture2D*> imageReads;
            std::vector<Texture2D*> imageWrites;

            for (uint32_t j = 0; j < pipelineBlock.pipelineImageDescriptors.size(); ++j)
            {
                const PipelineImageDescriptor& imageDescriptor = pipelineBlock.pipelineImageDescriptors[j];
                const std::string& name = imageDescriptor.pipelineImageName;
                PipelineImageDescriptorUsage usage = imageDescriptor.usage;
                PipelineImageDescriptorType type = imageDescriptor.type;

                GraphicsPipelineFrameBuffer* sourceFrameBuffer =
                    (imageDescriptor.frameIndex == PipelineImageDescriptorFrameIndex::PrevFrame)
                    ? frameBuffers[prevFrameIdx].get()
                    : frameBuffer;

                if (usage == PipelineImageDescriptorUsage::Read) imageReads.push_back(sourceFrameBuffer->texture2Ds[name].get());
                else if (usage == PipelineImageDescriptorUsage::Write) imageWrites.push_back(sourceFrameBuffer->texture2Ds[name].get());
            }

            uint32_t coverageWidth = engine->GetGraphics()->GetFrameBufferWidth();
            uint32_t coverageHeight = engine->GetGraphics()->GetFrameBufferHeight();
            uint32_t coverageDepth = 1;

            switch (pipelineBlock.dispatchSize)
            {
            case GraphicsPipelineBlockDispatchSize::Full:
                break;
            case GraphicsPipelineBlockDispatchSize::Half:
                coverageWidth = std::max(1u, coverageWidth / 2);
                coverageHeight = std::max(1u, coverageHeight / 2);
                break;
            case GraphicsPipelineBlockDispatchSize::Quarter:
                coverageWidth = std::max(1u, coverageWidth / 4);
                coverageHeight = std::max(1u, coverageHeight / 4);
                break;
            case GraphicsPipelineBlockDispatchSize::Custom:
                coverageWidth = pipelineBlock.customDispatchSize.width;
                coverageHeight = pipelineBlock.customDispatchSize.height;
                coverageDepth = pipelineBlock.customDispatchSize.depth;
                break;
            }

            const uint32_t tgW = std::max(1u, pipelineBlock.computeShaderThreadGroupDimensions.width);
            const uint32_t tgH = std::max(1u, pipelineBlock.computeShaderThreadGroupDimensions.height);
            const uint32_t tgD = std::max(1u, pipelineBlock.computeShaderThreadGroupDimensions.depth);

            const uint32_t groupX = (coverageWidth + tgW - 1) / tgW;
            const uint32_t groupY = (coverageHeight + tgH - 1) / tgH;
            const uint32_t groupZ = (coverageDepth + tgD - 1) / tgD;

            computePipelines[i]->Dispatch(groupX, groupY, groupZ, imageReads, imageWrites, ds);
        }

        else if (pipelineBlock.type == GraphicsPipelineBlockType::BlitImage)
        {
            auto srcIt = frameBuffer->texture2Ds.find(pipelineBlock.blitSourceImageName);
            auto dstIt = frameBuffer->texture2Ds.find(pipelineBlock.blitDestinationImageName);

            ENGINE_ASSERT_MSG(srcIt != frameBuffer->texture2Ds.end(),
                "[GraphicsPipeline] Blit source image '{}' not found",
                pipelineBlock.blitSourceImageName);
            ENGINE_ASSERT_MSG(dstIt != frameBuffer->texture2Ds.end(),
                "[GraphicsPipeline] Blit destination image '{}' not found",
                pipelineBlock.blitDestinationImageName);

            if (srcIt == frameBuffer->texture2Ds.end() || dstIt == frameBuffer->texture2Ds.end())
                continue;

            VulkanImage* sourceImage = srcIt->second->GetVulkanImage();
            VulkanImage* destinationImage = dstIt->second->GetVulkanImage();

            VkFilter vkFilter = (pipelineBlock.blitFilter == BlitImageFilter::Nearest)
                ? VK_FILTER_NEAREST
                : VK_FILTER_LINEAR;

            commandBuffer->BlitImage(sourceImage, destinationImage, vkFilter);
        }
    }
}

ruya::Texture2D* ruya::GraphicsPipeline::GetOutputImage(uint32_t frameIndex, std::string name)
{
    return frameBuffers[frameIndex]->texture2Ds[name].get();
}

std::unordered_map<std::string, uint32_t>& ruya::GraphicsPipeline::GetRenderTargetsMap()
{
    return pipelineImagesMap;
}

void ruya::GraphicsPipeline::CreatePipeline()
{
    for (uint32_t i = 0; i < pipelineImages.size(); ++i)
    {
        pipelineImagesMap.insert({ pipelineImages[i].name, i });
    }

    for (uint32_t i = 0; i < pipelineBlocks.size(); ++i)
    {
        const GraphicsPipelineBlock& pipelineBlock = pipelineBlocks[i];

        if (pipelineBlock.type == GraphicsPipelineBlockType::BlitImage)
            continue;

        bool hasDescriptor = false;
        for (uint32_t j = 0; j < pipelineBlock.pipelineImageDescriptors.size(); ++j)
        {
            if (pipelineBlock.pipelineImageDescriptors[j].type != PipelineImageDescriptorType::ColorAttachment && pipelineBlock.pipelineImageDescriptors[j].type != PipelineImageDescriptorType::DepthAttachment)
            {
                hasDescriptor = true;
                break;
            }
        }

        if (hasDescriptor)
        {
            std::unique_ptr<DescriptorSetLayout> descriptorSetLayout = std::make_unique<DescriptorSetLayout>();

            for (uint32_t j = 0; j < pipelineBlock.pipelineImageDescriptors.size(); ++j)
            {
                const PipelineImageDescriptor& desc = pipelineBlock.pipelineImageDescriptors[j];

                if (desc.type == PipelineImageDescriptorType::Storage)
                {
                    descriptorSetLayout->AddBinding(DescriptorType::STORAGE_IMAGE, 1);
                }
                else if (desc.type == PipelineImageDescriptorType::Sampled)
                {
                    descriptorSetLayout->AddBinding(DescriptorType::COMBINED_IMAGE_SAMPLER, 1);
                }
            }

            descriptorSetLayout->Build();
            pipelinesDescriptorSetLayouts.insert({ i, std::move(descriptorSetLayout) });
        }
    }

    for (uint32_t i = 0; i < pipelineBlocks.size(); ++i)
    {
        const GraphicsPipelineBlock& pipelineBlock = pipelineBlocks[i];

        if (pipelineBlock.type == GraphicsPipelineBlockType::BlitImage)
            continue;

        std::vector<DescriptorSetLayout*> dsl;

        for (uint32_t j = 0; j < pipelineBlock.standartDescriptorTypes.size(); ++j)
        {
            if (pipelineBlock.standartDescriptorTypes[j] == StandartDescriptorType::CameraData) dsl.push_back(engine->GetGraphics()->GetCameraDataBufferDescriptorSetLayout());
            else if (pipelineBlock.standartDescriptorTypes[j] == StandartDescriptorType::AtmosphericLightData) dsl.push_back(engine->GetGraphics()->GetDirectionalLightDataBufferDescriptorSetLayout());
            else if (pipelineBlock.standartDescriptorTypes[j] == StandartDescriptorType::Texture2Ds) dsl.push_back(engine->GetGraphics()->GetTexture2DsDescriptorSetLayout());
            else if (pipelineBlock.standartDescriptorTypes[j] == StandartDescriptorType::RenderMaterials) dsl.push_back(engine->GetGraphics()->GetRenderMaterialsDescriptorSetLayout());
            else if (pipelineBlock.standartDescriptorTypes[j] == StandartDescriptorType::RenderItems) dsl.push_back(engine->GetGraphics()->GetRenderItemsDescriptorSetLayout());
            else if (pipelineBlock.standartDescriptorTypes[j] == StandartDescriptorType::TLAS) dsl.push_back(engine->GetGraphics()->GetTLASDescriptorSetLayout());
        }

        auto dslIt = pipelinesDescriptorSetLayouts.find(i);
        if (dslIt != pipelinesDescriptorSetLayouts.end())
        {
            dsl.push_back(dslIt->second.get());
        }

        if (pipelineBlock.type == GraphicsPipelineBlockType::Rasterization)
        {
            std::vector<ColorFormat> colorAttachmentFormats;
            ColorFormat depthAttachmentFormat = ColorFormat::D32_SFLOAT;

            for (uint32_t j = 0; j < pipelineBlock.pipelineImageDescriptors.size(); ++j)
            {
                const PipelineImageDescriptor& desc = pipelineBlock.pipelineImageDescriptors[j];

                if (desc.type != PipelineImageDescriptorType::ColorAttachment)
                    continue;

                auto imageIt = pipelineImagesMap.find(desc.pipelineImageName);
                ENGINE_ASSERT_MSG(imageIt != pipelineImagesMap.end(),
                    "[GraphicsPipeline] Pipeline image '{}' not found", desc.pipelineImageName);
                if (imageIt == pipelineImagesMap.end()) continue;

                PipelineImageFormat pipelineImageFormat = pipelineImages[imageIt->second].format;

                if (pipelineImageFormat == PipelineImageFormat::R32G32B32A32_SFLOAT) colorAttachmentFormats.push_back(ColorFormat::R32G32B32A32_SFLOAT);
                else if (pipelineImageFormat == PipelineImageFormat::R16G16B16A16_SFLOAT) colorAttachmentFormats.push_back(ColorFormat::R16G16B16A16_SFLOAT);
                else if (pipelineImageFormat == PipelineImageFormat::R8G8B8A8_UNORM) colorAttachmentFormats.push_back(ColorFormat::R8G8B8A8_UNORM);
                else if (pipelineImageFormat == PipelineImageFormat::R8_UNORM) colorAttachmentFormats.push_back(ColorFormat::R8_UNORM);
                else if (pipelineImageFormat == PipelineImageFormat::R32_UINT) colorAttachmentFormats.push_back(ColorFormat::R32_UINT);
                else
                {
                    ENGINE_ASSERT_MSG(false,
                        "[GraphicsPipeline] Unsupported color attachment format for image '{}'",
                        desc.pipelineImageName);
                }
            }

            std::string vertexShaderPath;
            std::string fragmentShaderPath;

            for (uint32_t j = 0; j < pipelineBlock.shaders.size(); ++j)
            {
                const GraphicsPipelineShader& graphicsPipelineShader = pipelineBlock.shaders[j];

                if (graphicsPipelineShader.type == GraphicsPipelineShaderType::Vertex) vertexShaderPath = ASSETS_DIR + graphicsPipelineShader.path;
                else if (graphicsPipelineShader.type == GraphicsPipelineShaderType::Fragment) fragmentShaderPath = ASSETS_DIR + graphicsPipelineShader.path;
            }

            std::unique_ptr<RasterizationPipeline> rasterizationPipeline = std::make_unique<RasterizationPipeline>(
                vertexShaderPath,
                fragmentShaderPath,
                dsl,
                colorAttachmentFormats,
                depthAttachmentFormat);

            rasterizationPipelines.insert({ i, std::move(rasterizationPipeline) });
        }

        else if (pipelineBlock.type == GraphicsPipelineBlockType::RayTracing)
        {
            std::string rayGenShaderPath = "";
            std::vector<std::string> rayMissShaderPaths;
            std::string rayAnyHitShaderPath = "";
            std::string rayClosestHitShaderPath = "";

            for (uint32_t j = 0; j < pipelineBlock.shaders.size(); ++j)
            {
                const GraphicsPipelineShader& graphicsPipelineShader = pipelineBlock.shaders[j];

                if (graphicsPipelineShader.type == GraphicsPipelineShaderType::RayGeneration) rayGenShaderPath = ASSETS_DIR + graphicsPipelineShader.path;
                else if (graphicsPipelineShader.type == GraphicsPipelineShaderType::RayMiss) rayMissShaderPaths.push_back(ASSETS_DIR + graphicsPipelineShader.path);
                else if (graphicsPipelineShader.type == GraphicsPipelineShaderType::RayAnyHit) rayAnyHitShaderPath = ASSETS_DIR + graphicsPipelineShader.path;
                else if (graphicsPipelineShader.type == GraphicsPipelineShaderType::RayClosestHit) rayClosestHitShaderPath = ASSETS_DIR + graphicsPipelineShader.path;
            }

            std::unique_ptr<RayTracingPipeline> rayTracingPipeline = std::make_unique<RayTracingPipeline>(
                rayGenShaderPath,
                rayAnyHitShaderPath,
                rayClosestHitShaderPath,
                rayMissShaderPaths,
                dsl);

            rayTracingPipelines.insert({ i, std::move(rayTracingPipeline) });
        }

        else if (pipelineBlock.type == GraphicsPipelineBlockType::Compute)
        {
            ENGINE_ASSERT_MSG(!pipelineBlock.shaders.empty(),
                "[GraphicsPipeline] Compute pipeline block has no shader");
            if (pipelineBlock.shaders.empty()) continue;

            std::unique_ptr<ComputePipeline> computePipeline = std::make_unique<ComputePipeline>(
                ASSETS_DIR + pipelineBlock.shaders[0].path,
                dsl);

            computePipelines.insert({ i, std::move(computePipeline) });
        }
    }

    for (int i = 0; i < engine->GetGraphics()->GetFrameBufferCount(); ++i)
    {
        frameBuffers.push_back(CreateGraphicsPipelineFrameBuffer());
    }
}

std::unique_ptr<ruya::GraphicsPipelineFrameBuffer> ruya::GraphicsPipeline::CreateGraphicsPipelineFrameBuffer()
{
    std::unique_ptr<GraphicsPipelineFrameBuffer> graphicsPipelineFrameBuffer = std::make_unique<GraphicsPipelineFrameBuffer>();

    graphicsPipelineFrameBuffer->cameraDataBuffer = std::make_unique<VulkanBuffer>(engine->GetGraphics()->GetVulkanContext(), sizeof(Camera), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU);
    graphicsPipelineFrameBuffer->cameraDataBufferDescriptorSet = std::make_unique<DescriptorSet>();

    graphicsPipelineFrameBuffer->directionalLightDataBuffer = std::make_unique<VulkanBuffer>(engine->GetGraphics()->GetVulkanContext(), sizeof(AtmosphericLight), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU);
    graphicsPipelineFrameBuffer->directionalLightDataBufferDescriptorSet = std::make_unique<DescriptorSet>();

    const uint32_t fbWidth = engine->GetGraphics()->GetFrameBufferWidth();
    const uint32_t fbHeight = engine->GetGraphics()->GetFrameBufferHeight();

    for (uint32_t i = 0; i < pipelineImages.size(); ++i)
    {
        const PipelineImage& pipelineImage = pipelineImages[i];

        uint32_t width = fbWidth;
        uint32_t height = fbHeight;

        if (pipelineImage.dimension == PipelineImageDimension::Full)
        {
            width = fbWidth;
            height = fbHeight;
        }
        else if (pipelineImage.dimension == PipelineImageDimension::Half)
        {
            width = fbWidth / 2;
            height = fbHeight / 2;
        }
        else if (pipelineImage.dimension == PipelineImageDimension::Quarter)
        {
            width = fbWidth / 4;
            height = fbHeight / 4;
        }
        else
        {
            ENGINE_ASSERT_MSG(false,
                "[GraphicsPipeline] Unknown PipelineImageDimension for image '{}'",
                pipelineImage.name);
        }

        ColorFormat colorFormat = ColorFormat::R32G32B32A32_SFLOAT;
        bool formatRecognized = true;

        if (pipelineImage.format == PipelineImageFormat::R32G32B32A32_SFLOAT) colorFormat = ColorFormat::R32G32B32A32_SFLOAT;
        else if (pipelineImage.format == PipelineImageFormat::R16G16B16A16_SFLOAT) colorFormat = ColorFormat::R16G16B16A16_SFLOAT;
        else if (pipelineImage.format == PipelineImageFormat::R8G8B8A8_UNORM) colorFormat = ColorFormat::R8G8B8A8_UNORM;
        else if (pipelineImage.format == PipelineImageFormat::D32_SFLOAT) colorFormat = ColorFormat::D32_SFLOAT;
        else if (pipelineImage.format == PipelineImageFormat::R8_UNORM) colorFormat = ColorFormat::R8_UNORM;
        else if (pipelineImage.format == PipelineImageFormat::R16_SFLOAT) colorFormat = ColorFormat::R16_SFLOAT;
        else if (pipelineImage.format == PipelineImageFormat::R32_UINT) colorFormat = ColorFormat::R32_UINT;
        else formatRecognized = false;

        ENGINE_ASSERT_MSG(formatRecognized,
            "[GraphicsPipeline] Unsupported PipelineImageFormat for image '{}'",
            pipelineImage.name);

        std::unique_ptr<Texture2D> texture2D = std::make_unique<Texture2D>(width, height, colorFormat);

        graphicsPipelineFrameBuffer->texture2Ds.insert({ pipelineImage.name, std::move(texture2D) });
    }

    for (uint32_t i = 0; i < pipelineBlocks.size(); ++i)
    {
        const GraphicsPipelineBlock& pipelineBlock = pipelineBlocks[i];

        if (pipelineBlock.type == GraphicsPipelineBlockType::BlitImage)
            continue;

        bool hasDescriptor = false;
        for (uint32_t j = 0; j < pipelineBlock.pipelineImageDescriptors.size(); ++j)
        {
            if (pipelineBlock.pipelineImageDescriptors[j].type != PipelineImageDescriptorType::ColorAttachment && pipelineBlock.pipelineImageDescriptors[j].type != PipelineImageDescriptorType::DepthAttachment)
            {
                hasDescriptor = true;
                break;
            }
        }

        if (hasDescriptor)
        {
            graphicsPipelineFrameBuffer->texture2DDescriptorSets.insert({ i, std::make_unique<DescriptorSet>() });
        }
    }

    return graphicsPipelineFrameBuffer;
}