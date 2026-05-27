#include "render_material.h"

#include <engine.h>

ruya::RenderMaterial::RenderMaterial(RyID albedoTextureID, RyID normalTextureID, RyID metallicRoughnessTextureID)
{
	this->albedoTextureID = albedoTextureID;
	this->normalTextureID = normalTextureID;
	this->metallicRoughnessTextureID = metallicRoughnessTextureID;
}

void ruya::RenderMaterial::Load()
{
	renderMaterialBuffer = std::make_unique<VulkanBuffer>(
		engine->GetGraphics()->GetVulkanContext(),
		sizeof(RenderMaterialBuffer),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY);

	RenderMaterialBuffer temp;
	temp.albedoTextureDescriptorIndex = static_cast<uint32_t>(albedoTextureID.GetRawID());
	temp.normalTextureDescriptorIndex = static_cast<uint32_t>(normalTextureID.GetRawID());
	temp.metallicRoughnessTextureDescriptorIndex = static_cast<uint32_t>(metallicRoughnessTextureID.GetRawID());

	std::unique_ptr<VulkanBuffer> uploadBuffer = std::make_unique<VulkanBuffer>(engine->GetGraphics()->GetVulkanContext(), sizeof(RenderMaterialBuffer), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	uploadBuffer->UploadData(&temp, sizeof(RenderMaterialBuffer));

	ImmediateSubmitTransferList tl;
	tl.buffers.push_back({
		renderMaterialBuffer.get(),
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		VK_ACCESS_SHADER_READ_BIT
		});

	engine->GetGraphics()->GetVulkanContext()->ImmediateSubmitCommand([&](VulkanCommandBuffer* commandBuffer)
		{
			VkBufferCopy bufferCopy = {};
			bufferCopy.size = sizeof(RenderMaterialBuffer);
			bufferCopy.srcOffset = 0;
			bufferCopy.dstOffset = 0;

			commandBuffer->CopyBufferToBuffer(*uploadBuffer, renderMaterialBuffer.get(), bufferCopy);
		}, tl);
}

void ruya::RenderMaterial::Unload()
{
	renderMaterialBuffer.reset();
}

void ruya::RenderMaterial::SetValid()
{
	valid.store(true, std::memory_order_release);
}

void ruya::RenderMaterial::SetInvalid()
{
	valid.store(false, std::memory_order_release);
}

bool ruya::RenderMaterial::IsValid()
{
	Graphics* graphics = engine->GetGraphics();

	return valid.load(std::memory_order_acquire) && graphics->GetTexture2D(albedoTextureID)->IsValid() && graphics->GetTexture2D(normalTextureID)->IsValid() && graphics->GetTexture2D(metallicRoughnessTextureID)->IsValid();
}