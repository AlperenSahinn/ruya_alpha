#include "render_item.h"

#include <engine.h>

ruya::RenderItem::RenderItem(const glm::mat4& transform, RyID meshBufferRyID, RyID materialRyID, bool draw)
{
	this->transform = transform;
	this->meshBufferID = meshBufferRyID;
	this->materialID = materialRyID;
	this->draw = draw;
	valid = false;
}

void ruya::RenderItem::Load()
{
	Graphics* graphics = engine->GetGraphics();

	RenderItemBuffer temp{};
	temp.transform = transform;
	temp.prevTransform = transform;
	temp.transformInverse = glm::inverse(transform);
	temp.vertexBufferAddress = graphics->GetMeshBuffer(meshBufferID)->GetVertexBuffer()->GetDeviceAddress();
	temp.indexBufferAddress = graphics->GetMeshBuffer(meshBufferID)->GetIndexBuffer()->GetDeviceAddress();
	temp.materialDescriptorIndex = static_cast<uint32_t>(materialID.GetRawID());

	for(size_t i = 0; i < graphics->GetFrameBufferCount(); ++i)
	{
		renderItemBufferHosts.push_back(std::make_unique<VulkanBuffer>(
			graphics->GetVulkanContext(),
			sizeof(RenderItemBuffer),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU));

		renderItemBufferHosts[i]->UploadData(&temp, sizeof(RenderItemBuffer));
	}

	renderItemBufferDevice = std::make_unique<VulkanBuffer>(
		graphics->GetVulkanContext(),
		sizeof(RenderItemBuffer),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY);
}

void ruya::RenderItem::Unload()
{
	renderItemBufferHosts.clear();
	renderItemBufferDevice.reset();
}

void ruya::RenderItem::UpdateTransform(const glm::mat4& transform)
{
	Graphics* graphics = engine->GetGraphics();

	RenderItemBuffer temp{};
	temp.prevTransform = this->transform;
	temp.transform = transform;
	temp.transformInverse = glm::inverse(transform);
	temp.vertexBufferAddress = graphics->GetMeshBuffer(meshBufferID)->GetVertexBuffer()->GetDeviceAddress();
	temp.indexBufferAddress = graphics->GetMeshBuffer(meshBufferID)->GetIndexBuffer()->GetDeviceAddress();
	temp.materialDescriptorIndex = static_cast<uint32_t>(materialID.GetRawID());

	renderItemBufferHosts[graphics->GetCurrentFrameIndex()]->UploadData(&temp, sizeof(RenderItemBuffer));

	this->transform = transform;

	VkBufferCopy bufferCopy = {};
	bufferCopy.size = sizeof(RenderItemBuffer);
	bufferCopy.srcOffset = 0;
	bufferCopy.dstOffset = 0;

	graphics->GetVulkanContext()->GetCurrentFrameResource()->GetCommandBuffer()->CopyBufferToBuffer(*renderItemBufferHosts[graphics->GetCurrentFrameIndex()], renderItemBufferDevice.get(), bufferCopy);
}

void ruya::RenderItem::UpdateMaterial(RyID materialRyID)
{
	Graphics* graphics = engine->GetGraphics();

	materialID = materialRyID;

	RenderItemBuffer temp{};
	temp.prevTransform = this->transform;
	temp.transform = transform;
	temp.transformInverse = glm::inverse(transform);
	temp.vertexBufferAddress = graphics->GetMeshBuffer(meshBufferID)->GetVertexBuffer()->GetDeviceAddress();
	temp.indexBufferAddress = graphics->GetMeshBuffer(meshBufferID)->GetIndexBuffer()->GetDeviceAddress();
	temp.materialDescriptorIndex = static_cast<uint32_t>(materialRyID.GetRawID());

	renderItemBufferHosts[graphics->GetCurrentFrameIndex()]->UploadData(&temp, sizeof(RenderItemBuffer));

	VkBufferCopy bufferCopy = {};
	bufferCopy.size = sizeof(RenderItemBuffer);
	bufferCopy.srcOffset = 0;
	bufferCopy.dstOffset = 0;

	graphics->GetVulkanContext()->GetCurrentFrameResource()->GetCommandBuffer()->CopyBufferToBuffer(*renderItemBufferHosts[graphics->GetCurrentFrameIndex()], renderItemBufferDevice.get(), bufferCopy);
}

void ruya::RenderItem::SetValid()
{
	valid.store(true, std::memory_order_release);
}

void ruya::RenderItem::SetInvalid()
{
	valid.store(false, std::memory_order_release);
}

bool ruya::RenderItem::IsValid()
{
	Graphics* graphics = engine->GetGraphics();

	return valid.load(std::memory_order_acquire) && graphics->GetMeshBuffer(meshBufferID)->IsValid() && graphics->GetRenderMaterial(materialID)->IsValid();
}
