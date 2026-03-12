#include "frame_buffer.h"

#include <engine.h>

#include "ruya_vulkan/vulkan_descriptor_writer.h"

#include "camera.h"
#include "directional_light.h"

ruya::FrameBuffer::FrameBuffer(VulkanContext* pVulkanContext, uint32_t width, uint32_t height)
{
	cameraDataBuffer = std::make_unique<VulkanBuffer>(pVulkanContext, sizeof(Camera), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU);
	directionalLightDataBuffer = std::make_unique<VulkanBuffer>(pVulkanContext, sizeof(DirectionalLight), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU);
	gBuffer = std::make_unique<GBuffer>(pVulkanContext, width, height);
	directionalLightShadowMapImage = std::make_unique<VulkanImage>(pVulkanContext, VK_IMAGE_TYPE_2D, VkExtent3D{ width, height, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	lightPassImage = std::make_unique<VulkanImage>(pVulkanContext, VK_IMAGE_TYPE_2D, VkExtent3D{ width, height, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
}

ruya::FrameBuffer::~FrameBuffer()
{
}

void ruya::FrameBuffer::UpdateDescriptors(VulkanContext* pVulkanContext)
{
	engine->GetRenderDataReadBuffer()->camera.sampleCount = 4;

	cameraDataBufferDescriptorSet = engine->GetGraphics()->GetVulkanContext()->GetCurrentFrameResource()->GetDescriptorPool()->AllocateDescriptorSet(engine->GetGraphics()->GetCameraDataBufferDescriptorSetLayout()->GetDeviceHandle());
	std::unique_ptr<VulkanDescriptorWriter> descriptorWrite = std::make_unique<VulkanDescriptorWriter>(pVulkanContext);
	cameraDataBuffer->UploadData(&engine->GetRenderDataReadBuffer()->camera, sizeof(Camera));
	descriptorWrite->WriteDescriptorBuffer(0, cameraDataBuffer->GetDeviceHandle(), sizeof(Camera), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	descriptorWrite->UpdateDescriptors(cameraDataBufferDescriptorSet);
	descriptorWrite.reset();

	directionalLightDataBufferDescriptorSet = engine->GetGraphics()->GetVulkanContext()->GetCurrentFrameResource()->GetDescriptorPool()->AllocateDescriptorSet(engine->GetGraphics()->GetDirectionalLightDataBufferDescriptorSetLayout()->GetDeviceHandle());
	descriptorWrite = std::make_unique<VulkanDescriptorWriter>(pVulkanContext);
	directionalLightDataBuffer->UploadData(&engine->GetRenderDataReadBuffer()->directionalLight, sizeof(DirectionalLight));
	descriptorWrite->WriteDescriptorBuffer(0, directionalLightDataBuffer->GetDeviceHandle(), sizeof(DirectionalLight), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	descriptorWrite->UpdateDescriptors(directionalLightDataBufferDescriptorSet);
	descriptorWrite.reset();

	gBufferDescriptorSet = engine->GetGraphics()->GetVulkanContext()->GetCurrentFrameResource()->GetDescriptorPool()->AllocateDescriptorSet(engine->GetGraphics()->GetGBufferDescriptorSetLayout()->GetDeviceHandle());
	descriptorWrite = std::make_unique<VulkanDescriptorWriter>(pVulkanContext);
	descriptorWrite->WriteDescriptorImage(0, gBuffer->GetAlbedoDepth()->GetImageView(), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	descriptorWrite->WriteDescriptorImage(1, gBuffer->GetPositionMetallic()->GetImageView(), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	descriptorWrite->WriteDescriptorImage(2, gBuffer->GetNormalRoughness()->GetImageView(), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	descriptorWrite->WriteDescriptorImage(3, gBuffer->GetVertexNormals()->GetImageView(), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	descriptorWrite->UpdateDescriptors(gBufferDescriptorSet);
	descriptorWrite.reset();

	downsampledGBufferDescriptorSet = engine->GetGraphics()->GetVulkanContext()->GetCurrentFrameResource()->GetDescriptorPool()->AllocateDescriptorSet(engine->GetGraphics()->GetDownsampledGBufferDescriptorSetLayout()->GetDeviceHandle());
	descriptorWrite = std::make_unique<VulkanDescriptorWriter>(pVulkanContext);
	descriptorWrite->WriteDescriptorImage(0, gBuffer->GetPositionMetallicDownsample()->GetImageView(), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	descriptorWrite->WriteDescriptorImage(1, gBuffer->GetVertexNormalsDownsample()->GetImageView(), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	descriptorWrite->UpdateDescriptors(downsampledGBufferDescriptorSet);
	descriptorWrite.reset();

	directionalLightShadowMapImageDescriptorSet = engine->GetGraphics()->GetVulkanContext()->GetCurrentFrameResource()->GetDescriptorPool()->AllocateDescriptorSet(engine->GetGraphics()->GetDirectionalLightShadowMapImageDescriptorSetLayout()->GetDeviceHandle());
	descriptorWrite = std::make_unique<VulkanDescriptorWriter>(pVulkanContext);
	descriptorWrite->WriteDescriptorImage(0, directionalLightShadowMapImage->GetImageView(), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	descriptorWrite->UpdateDescriptors(directionalLightShadowMapImageDescriptorSet);
	descriptorWrite.reset();

	lightPassImageDescriptorSet = engine->GetGraphics()->GetVulkanContext()->GetCurrentFrameResource()->GetDescriptorPool()->AllocateDescriptorSet(engine->GetGraphics()->GetLightPassImageDescriptorSetLayout()->GetDeviceHandle());
	descriptorWrite = std::make_unique<VulkanDescriptorWriter>(pVulkanContext);
	descriptorWrite->WriteDescriptorImage(0, lightPassImage->GetImageView(), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	descriptorWrite->UpdateDescriptors(lightPassImageDescriptorSet);
	descriptorWrite.reset();
}

ruya::GBuffer* ruya::FrameBuffer::GetGBuffer() const
{
	return gBuffer.get();
}

ruya::VulkanImage* ruya::FrameBuffer::GetDirectionalLightShadowMapImage() const
{
	return directionalLightShadowMapImage.get();
}

ruya::VulkanImage* ruya::FrameBuffer::GetLightPassImage() const
{
	return lightPassImage.get();
}

VkDescriptorSet ruya::FrameBuffer::GetCameraDataBufferDescriptorSet() const
{
	return cameraDataBufferDescriptorSet;
}

VkDescriptorSet ruya::FrameBuffer::GetDirectionalLightDataBufferDescriptorSet() const
{
	return directionalLightDataBufferDescriptorSet;
}

VkDescriptorSet ruya::FrameBuffer::GetGBufferDescriptorSet() const
{
	return gBufferDescriptorSet;
}

VkDescriptorSet ruya::FrameBuffer::GetDownsampledGBufferDescriptorSet() const
{
	return downsampledGBufferDescriptorSet;
}

VkDescriptorSet ruya::FrameBuffer::GetDirectionalLightShadowMapImageDescriptorSet() const
{
	return directionalLightShadowMapImageDescriptorSet;
}

VkDescriptorSet ruya::FrameBuffer::GetLightPassImageDescriptorSet() const
{
	return lightPassImageDescriptorSet;
}

uint32_t ruya::FrameBuffer::GetWidth() const
{
	return frameWidth;
}

uint32_t ruya::FrameBuffer::GetHeight() const
{
	return frameHeight;
}