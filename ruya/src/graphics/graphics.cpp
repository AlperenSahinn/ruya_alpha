#include "graphics.h"

#include <engine.h>
#include <core/log.h>
#include <core/hash_code_generator.h>
#include <core/assert.h>
#include <assets_system/ry_asset_manager.h>

#include "ruya_vulkan/vulkan_descriptor_writer.h"

ruya::Graphics::Graphics()
{
	vulkanContext = std::make_unique<VulkanContext>();
	RUYA_LOG_INFO("[Graphics] Graphics instance created");
}

ruya::Graphics::~Graphics()
{
	vkDestroySampler(vulkanContext->GetDevice(), nearestSampler, nullptr);
	vkDestroySampler(vulkanContext->GetDevice(), linearSampler, nullptr);

	RUYA_LOG_INFO("[Graphics] Graphics instance destroyed");
}

void ruya::Graphics::Init(Window* window)
{
	vulkanContext->Init(window, true);

	CreateGeneralResources();

	frameBufferWidth = 1600;
	frameBufferHeight = 900;

	defaultPipeline = std::make_unique<GraphicsPipeline>(ASSETS_DIR + "/ruya_files/graphics_pipelines/default_pipeline.json");

	CreateDebugLinePipeline();
}

ruya::VulkanContext* ruya::Graphics::GetVulkanContext() const
{
	return vulkanContext.get();
}

void ruya::Graphics::BeginFrame()
{
	vulkanContext->BeginFrame();
}

void ruya::Graphics::Draw()
{
	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->GeneralMemoryBarrier(
		VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	SyncRenderItemTransforms();

	RenderData* renderData = engine->GetRenderDataReadBuffer();

	for (UpdateRenderItemTransformCommand& transformCommand : renderData->updateRenderItemTransformCommands)
		UpdateRenderItemTransform(transformCommand.renderItemRyID, transformCommand.transform);

	for (UpdateRenderItemMaterialCommand& updateRenderItemMaterialCommand : renderData->updateRenderItemMaterialCommand)
		UpdateRenderItemMaterial(updateRenderItemMaterialCommand.renderItemRyID, updateRenderItemMaterialCommand.materialUUID);

	UpdateDescriptors();

	VkExtent2D extent{ frameBufferWidth, frameBufferHeight };

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->SetViewPort(extent.width, extent.height);

	DestroyTLAS(frameIndex);
	CreateTLAS(frameIndex);

	//UpdateTLAS(vulkanContext->GetCurrentFrameResource()->GetCommandBuffer(), frameIndex);

	defaultPipeline->Dispatch();

	DispatchDebugLinePipeline();
}

void ruya::Graphics::BeginEditorDraw()
{
	VkImageSubresourceRange colorRange{};
	colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorRange.baseMipLevel = 0;
	colorRange.levelCount = 1;
	colorRange.baseArrayLayer = 0;
	colorRange.layerCount = 1;

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->ImageMemoryBarrier(
		defaultPipeline->GetOutputImage(frameIndex, "toneMapPassResultImage")->GetVulkanImage(),
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		colorRange
	);

	VkExtent2D extent = vulkanContext->GetSwapchainImageExtent();

	VkRenderingAttachmentInfo colorAttachmentInfo = {};
	colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachmentInfo.imageView = vulkanContext->GetCurrentSwapchainImageView();
	colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachmentInfo.clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	std::vector<VkRenderingAttachmentInfo> renderingAttachmentInfos = { colorAttachmentInfo };

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->SetViewPort(extent.width, extent.height);

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->BeginRenderPass(extent, renderingAttachmentInfos, nullptr);
}

void ruya::Graphics::EndEditorDraw()
{
	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->EndRenderPass();
}

void ruya::Graphics::EndFrame()
{
	vulkanContext->EndFrame();

	prevFrameIndex = frameIndex;
	frameIndex++;

	frameIndex = frameIndex % frameBufferCount;
}

bool ruya::Graphics::IsSwapchainValid()
{
	return vulkanContext->IsSwapchainValid();
}

void ruya::Graphics::WaitGraphicsDeviceIdle()
{
	vulkanContext->WaitDeviceIdle();
}

ruya::RyID ruya::Graphics::CreateMeshBuffer(UUID ryMeshUUID)
{	
	ENGINE_ASSERT_MSG(ryMeshUUID.IsValid(), "[Graphics] CreateMeshBuffer failed. ryMeshUUID is invalid.");

	if(uuid2MeshBufferRyID.contains(ryMeshUUID))
	{
		meshBuffersUsageCounts[uuid2MeshBufferRyID[ryMeshUUID]]++;
		return uuid2MeshBufferRyID[ryMeshUUID];
	}

	RyID ryID = RyID::Invalid();

	if (availableMeshBufferRyIDs.empty())
	{
		ryID = meshBufferRyIDCounter;
		meshBufferRyIDCounter = RyID(meshBufferRyIDCounter.GetRawID() + 1);
	}
	else
	{
		ryID = availableMeshBufferRyIDs.front();
		availableMeshBufferRyIDs.pop();
	}

	uuid2MeshBufferRyID.insert({ ryMeshUUID, ryID });
	ryID2MeshBufferUUID.insert({ ryID, ryMeshUUID });
	meshBuffersUsageCounts.insert({ ryID, 1 });
	meshBuffers.insert({ ryID, std::make_unique<MeshBuffer>() });

	meshBuffers[ryID]->SetInvalid();

	engine->QueueAsyncJob([this, ryMeshUUID, ryID]()
		{
			MeshBuffer* meshBuffer = meshBuffers[ryID].get();
			std::optional<RyMeshData> ryMesh = engine->GetAssetManager()->LoadGLTF(ryMeshUUID);
			meshBuffer->Load(ryMesh->vertices, ryMesh->indices);
			meshBuffer->SetValid();
		});

	return ryID;
}

ruya::MeshBuffer* ruya::Graphics::GetMeshBuffer(RyID meshBufferRyID)
{
	ENGINE_ASSERT_MSG(meshBufferRyID.IsValid(), "[Graphics] GetMeshBuffer failed. meshBufferRyID is invalid.");
	ENGINE_ASSERT_MSG(meshBuffers.contains(meshBufferRyID), "[Graphics] GetMeshBuffer failed. There is no MeshBuffer exist with this meshBufferRyID {}", std::to_string(meshBufferRyID).c_str());

	return meshBuffers[meshBufferRyID].get();
}

void ruya::Graphics::DestroyMeshBuffer(RyID meshBufferRyID)
{
	ENGINE_ASSERT_MSG(meshBufferRyID.IsValid(), "[Graphics] DestroyMeshBuffer failed. meshBufferRyID is invalid.");
	ENGINE_ASSERT_MSG(meshBuffers.contains(meshBufferRyID), "[Graphics] DestroyMeshBuffer failed. There is no MeshBuffer exist with this meshBufferRyID {}", std::to_string(meshBufferRyID).c_str());

	if (meshBuffersUsageCounts[meshBufferRyID] > 0) meshBuffersUsageCounts[meshBufferRyID]--;

	if(meshBuffersUsageCounts[meshBufferRyID] == 0)
	{
		meshBuffers[meshBufferRyID]->SetInvalid();

		UUID uuidTemp = ryID2MeshBufferUUID[meshBufferRyID];
		uuid2MeshBufferRyID.erase(uuidTemp);
		ryID2MeshBufferUUID.erase(meshBufferRyID);
		meshBuffersUsageCounts.erase(meshBufferRyID);
		availableMeshBufferRyIDs.push(meshBufferRyID);

		MeshBuffer* meshBufferptr = meshBuffers[meshBufferRyID].release();
		meshBuffers.erase(meshBufferRyID);

		engine->QueueAsyncJob([this, meshBufferptr]()
			{
				meshBufferptr->Unload();
				delete meshBufferptr;
			});
	}
}

ruya::RyID ruya::Graphics::CreateTexture2D(UUID image2DUUID)
{
	ENGINE_ASSERT_MSG(image2DUUID.IsValid(), "[Graphics] CreateTexture2D failed. image2DUUID is invalid.");

	if (uuid2Texture2DRyID.contains(image2DUUID))
	{
		texture2DUsageCounts[uuid2Texture2DRyID[image2DUUID]]++;
		return uuid2Texture2DRyID[image2DUUID];
	}

	RyID ryID = RyID::Invalid();

	if (availableTexture2DRyIDs.empty())
	{
		ryID = texture2DRyIDCounter;
		texture2DRyIDCounter = RyID(texture2DRyIDCounter.GetRawID() + 1);
	}
	else
	{
		ryID = availableTexture2DRyIDs.front();
		availableTexture2DRyIDs.pop();
	}

	uuid2Texture2DRyID.insert({ image2DUUID, ryID });
	ryID2Texture2DUUID.insert({ ryID, image2DUUID });
	texture2DUsageCounts.insert({ ryID, 1 });

	texture2Ds.insert({ ryID,std::make_unique<Texture2D>() });
	texture2Ds[ryID]->SetInvalid();

	engine->QueueAsyncJob([this, ryID, image2DUUID]()
		{
			ktxTexture2* ktxtet = engine->GetAssetManager()->LoadKTX2(image2DUUID);

			texture2Ds[ryID]->Load(ktxtet, Texture2DSampler::Linear);

			std::lock_guard<std::mutex> guard(descriptorUpdateQueueMutex);
			texture2DsDescriptorSetUpdateQueue.push(ryID);

			ktxTexture_Destroy(ktxTexture(ktxtet));
		});

	return ryID;
}

ruya::Texture2D* ruya::Graphics::GetTexture2D(RyID texture2DRyID)
{
	ENGINE_ASSERT_MSG(texture2DRyID.IsValid(), "[Graphics] GetTexture2D failed. texture2DRyID is invalid.");
	ENGINE_ASSERT_MSG(texture2Ds.contains(texture2DRyID), "[Graphics] GetTexture2D failed. There is no MeshBuffer exist with this texture2DRyID {}", std::to_string(texture2DRyID).c_str());

	return texture2Ds[texture2DRyID].get();
}

void ruya::Graphics::DestroyTexture2D(RyID texture2DRyID)
{
	ENGINE_ASSERT_MSG(texture2DRyID.IsValid(), "[Graphics] DestroyTexture2D failed. texture2DRyID is invalid.");
	ENGINE_ASSERT_MSG(texture2Ds.contains(texture2DRyID), "[Graphics] DestroyTexture2D failed. There is no Texture2D exist with this texture2DRyID {}", std::to_string(texture2DRyID).c_str());

	if (texture2DUsageCounts[texture2DRyID] > 0) texture2DUsageCounts[texture2DRyID]--;

	if (texture2DUsageCounts[texture2DRyID] == 0)
	{
		texture2Ds[texture2DRyID]->SetInvalid();

		UUID uuidTemp = ryID2Texture2DUUID[texture2DRyID];
		uuid2Texture2DRyID.erase(uuidTemp);
		ryID2Texture2DUUID.erase(texture2DRyID);
		texture2DUsageCounts.erase(texture2DRyID);
		availableTexture2DRyIDs.push(texture2DRyID);

		Texture2D* texture2Dptr = texture2Ds[texture2DRyID].release();
		texture2Ds.erase(texture2DRyID);

		engine->QueueAsyncJob([this, texture2Dptr]()
			{
				texture2Dptr->Unload();
				delete texture2Dptr;
			});
	}
}

ruya::RyID ruya::Graphics::CreateRenderMaterial(UUID materialUUID)
{
	ENGINE_ASSERT_MSG(materialUUID.IsValid(), "[Graphics] CreateRenderMaterial failed. materialUUID is invalid.");

	if (uuid2RenderMaterialRyID.contains(materialUUID))
	{
		renderMaterialUsageCounts[uuid2RenderMaterialRyID[materialUUID]]++;
		return uuid2RenderMaterialRyID[materialUUID];
	}

	RyID ryID = RyID::Invalid();

	if (availablerenderMaterialRyIDs.empty())
	{
		ryID = renderMaterialRyIDCounter;
		renderMaterialRyIDCounter = RyID(renderMaterialRyIDCounter.GetRawID() + 1);
	}
	else
	{
		ryID = availablerenderMaterialRyIDs.front();
		availablerenderMaterialRyIDs.pop();
	}

	std::optional<RyMaterial> material = engine->GetAssetManager()->LoadRyMaterial(materialUUID);

	RyID albedoTextureID = CreateTexture2D(material->albedoUUID);
	RyID normalTextureID = CreateTexture2D(material->normalUUID);
	RyID metallicRoughnessTextureID = CreateTexture2D(material->metallicRoughnessUUID);

	uuid2RenderMaterialRyID.insert({ materialUUID, ryID });
	ryID2RenderMaterialUUID.insert({ ryID, materialUUID });
	renderMaterialUsageCounts.insert({ ryID, 1 });

	renderMaterials.insert({ ryID,std::make_unique<RenderMaterial>(albedoTextureID, normalTextureID, metallicRoughnessTextureID) });
	renderMaterials[ryID]->SetInvalid();

	engine->QueueAsyncJob([this, ryID]()
		{
			renderMaterials[ryID]->Load();

			std::lock_guard<std::mutex> guard(descriptorUpdateQueueMutex);
			renderMaterialsDescriptorSetUpdateQueue.push(ryID);
		});

	return ryID;
}

ruya::RenderMaterial* ruya::Graphics::GetRenderMaterial(RyID renderMaterialRyID)
{
	ENGINE_ASSERT_MSG(renderMaterialRyID.IsValid(), "[Graphics] GetPBRPBROpaqueMaterial failed. renderMaterialRyID is invalid.");
	ENGINE_ASSERT_MSG(renderMaterials.contains(renderMaterialRyID), "[Graphics] GetPBRPBROpaqueMaterial failed. There is no PBROpaqueMaterial exist with this renderMaterialRyID {}", std::to_string(renderMaterialRyID).c_str());

	return renderMaterials[renderMaterialRyID].get();
}

void ruya::Graphics::DestroyRenderMaterial(RyID renderMaterialRyID)
{
	ENGINE_ASSERT_MSG(renderMaterialRyID.IsValid(), "[Graphics] DestroyRenderMaterial failed. renderMaterialRyID is invalid.");
	ENGINE_ASSERT_MSG(renderMaterials.contains(renderMaterialRyID), "[Graphics] DestroyRenderMaterial failed. There is no RenderMaterial exist with this materialID {}", std::to_string(renderMaterialRyID).c_str());

	if (renderMaterialUsageCounts[renderMaterialRyID] > 0) renderMaterialUsageCounts[renderMaterialRyID]--;

	if (renderMaterialUsageCounts[renderMaterialRyID] == 0)
	{
		DestroyTexture2D(renderMaterials[renderMaterialRyID]->GetAlbedoTexture2DRyID());
		DestroyTexture2D(renderMaterials[renderMaterialRyID]->GetNormalTexture2DRyID());
		DestroyTexture2D(renderMaterials[renderMaterialRyID]->GetMetallicRoughnessTexture2DRyID());

		UUID uuidTemp = ryID2RenderMaterialUUID[renderMaterialRyID];
		uuid2RenderMaterialRyID.erase(uuidTemp);
		ryID2RenderMaterialUUID.erase(renderMaterialRyID);
		renderMaterialUsageCounts.erase(renderMaterialRyID);
		availablerenderMaterialRyIDs.push(renderMaterialRyID);

		RenderMaterial* renderMaterialptr = renderMaterials[renderMaterialRyID].release();
		renderMaterials.erase(renderMaterialRyID);

		engine->QueueAsyncJob([this, renderMaterialptr]()
			{
				renderMaterialptr->Unload();
				delete renderMaterialptr;
			});
	}
}

ruya::RyID ruya::Graphics::CreateRenderItem(const glm::mat4& transform, UUID ryMeshUUID, UUID materialUUID)
{
	ENGINE_ASSERT_MSG(ryMeshUUID.IsValid(), "[Graphics] CreateRenderItem failed. ryMeshUUID is invalid.");
	ENGINE_ASSERT_MSG(materialUUID.IsValid(), "[Graphics] CreateRenderItem failed. materialUUID is invalid.");

	RyID ryID = RyID::Invalid();

	bool bPushBack = true;

	if (availableRenderItemIDs.empty())
	{
		ryID = renderItemRyIDCounter;
		renderItemRyIDCounter = RyID(ryID.GetRawID() + 1);
	}
	else
	{
		ryID = availableRenderItemIDs.front();
		availableRenderItemIDs.pop();
		bPushBack = false;
	}

	RyID meshBufferID = CreateMeshBuffer(ryMeshUUID);
	RyID materialID = CreateRenderMaterial(materialUUID);

	if (bPushBack)
	{
		renderItems.push_back({ transform, meshBufferID, materialID, true });
	}
	else
	{
		new (&renderItems[ryID.GetRawID()]) RenderItem{transform, meshBufferID, materialID, true};
	}

	renderItems[ryID.GetRawID()].SetInvalid();

	engine->QueueAsyncJob([this, ryID]()
		{
			renderItems[ryID.GetRawID()].Load();

			VulkanBottomLevelAccelerationStructure* blas = meshBuffers[renderItems[ryID.GetRawID()].GetMeshBufferRyID()]->GetBLAS();
			blasInstances.insert({ ryID, std::make_unique<VulkanBottomLevelAccelerationStructureInstance>(blas, static_cast<uint32_t>(ryID.GetRawID()), renderItems[ryID.GetRawID()].GetTransform()) });

			std::lock_guard<std::mutex> guard(descriptorUpdateQueueMutex);
			renderItemsDescriptorSetUpdateQueue.push(ryID);
		});

	return ryID;
}

ruya::RenderItem* ruya::Graphics::GetRenderItem(RyID renderItemRyID)
{
	ENGINE_ASSERT_MSG(renderItemRyID.IsValid(), "[Graphics] GetRenderItem failed. renderItemRyID is invalid.");
	ENGINE_ASSERT_MSG(renderItems.size() > renderItemRyID.GetRawID(), "[Graphics] GetRenderItem failed. There is no RenderItem exist with this id: {}", std::to_string(renderItemRyID).c_str());

	return &renderItems[renderItemRyID.GetRawID()];
}

void ruya::Graphics::UpdateRenderItemTransform(RyID renderItemRyID, const glm::mat4& transform)
{
	ENGINE_ASSERT_MSG(renderItemRyID.IsValid(), "[Graphics] UpdateRenderItemTransform failed. renderItemRyID is invalid.");
	ENGINE_ASSERT_MSG(renderItems.size() > renderItemRyID.GetRawID(), "[Graphics] UpdateRenderItemTransform failed. There is no RenderItem exist with this id: {}", std::to_string(renderItemRyID).c_str());

	renderItems[renderItemRyID.GetRawID()].UpdateTransform(transform);
	blasInstances[renderItemRyID]->transform = transform;

	renderItemNeedTransformUpdate.push(renderItemRyID);
}

void ruya::Graphics::UpdateRenderItemMaterial(RyID renderItemRyID, UUID materialUUID)
{
	ENGINE_ASSERT_MSG(renderItemRyID.IsValid(), "[Graphics] UpdateRenderItemMeshAndMaterial failed. renderItemRyID is invalid.");
	ENGINE_ASSERT_MSG(renderItems.size() > renderItemRyID.GetRawID(), "[Graphics] UpdateRenderItemMeshAndMaterial failed. There is no RenderItem exist with this id: {}", std::to_string(renderItemRyID).c_str());

	RyID oldMaterialRyID = renderItems[renderItemRyID.GetRawID()].GetRenderMaterialRyID();

	RyID materialID = CreateRenderMaterial(materialUUID);

	renderItems[renderItemRyID.GetRawID()].UpdateMaterial(materialID);

	renderItemNeedMaterialUpdate.push({ renderItemRyID, oldMaterialRyID });
}

void ruya::Graphics::SetRenderItemVisibility(RyID renderItemRyID, bool b)
{
	ENGINE_ASSERT_MSG(renderItemRyID.IsValid(), "[Graphics] SetRenderItemVisibility failed. renderItemRyID is invalid.");
	ENGINE_ASSERT_MSG(renderItems.size() > renderItemRyID.GetRawID(), "[Graphics] SetRenderItemVisibility failed. There is no RenderItem exist with this id: {}", std::to_string(renderItemRyID).c_str());

	renderItems[renderItemRyID.GetRawID()].SetVisibility(b);
}

void ruya::Graphics::DestroyRenderItem(RyID renderItemRyID)
{
	ENGINE_ASSERT_MSG(renderItemRyID.IsValid(), "[Graphics] DestroyRenderItem failed. renderItemRyID is invalid.");
	ENGINE_ASSERT_MSG(renderItems.size() > renderItemRyID.GetRawID(), "[Graphics] DestroyRenderItem failed. There is no RenderItem exist with this id: {}", std::to_string(renderItemRyID).c_str());

	DestroyMeshBuffer(renderItems[renderItemRyID.GetRawID()].GetMeshBufferRyID());
	DestroyRenderMaterial(renderItems[renderItemRyID.GetRawID()].GetRenderMaterialRyID());

	renderItems[renderItemRyID.GetRawID()].SetInvalid();

	std::unique_ptr<RenderItem> renderItemPtr = std::make_unique<RenderItem>(std::move(renderItems[renderItemRyID.GetRawID()]));
	renderItems[renderItemRyID.GetRawID()].~RenderItem();

	availableRenderItemIDs.push(renderItemRyID);

	engine->QueueAsyncJob([this, r = renderItemPtr.release(), renderItemRyID]() mutable
		{
			r->Unload();
			delete r;
			blasInstances.erase(renderItemRyID);

			for (size_t i = 0; i < frameBufferCount; ++i)
				rebuildTLASes[i].store(true, std::memory_order_release);
		});
}

std::vector<ruya::RenderItem>& ruya::Graphics::GetRenderItems()
{
	return renderItems;
}

size_t ruya::Graphics::GetRenderItemsCount()
{
	return renderItems.size();
}

void ruya::Graphics::SetFrameBufferExtent(uint32_t width, uint32_t height)
{
	frameBufferWidth = width;
	frameBufferHeight = height;

	WaitGraphicsDeviceIdle();

	defaultPipeline->RecreateFrameBuffers();
}

uint32_t ruya::Graphics::GetFrameBufferWidth() const
{
	return frameBufferWidth;
}

uint32_t ruya::Graphics::GetFrameBufferHeight() const
{
	return frameBufferHeight;
}

uint32_t ruya::Graphics::GetCurrentFrameIndex() const
{
	return frameIndex;
}

uint32_t ruya::Graphics::GetFrameBufferCount() const
{
	return frameBufferCount;
}

VkSampler ruya::Graphics::GetNearestSampler() const
{
	return nearestSampler;
}

VkSampler ruya::Graphics::GetLinearSampler() const
{
	return linearSampler;
}

void ruya::Graphics::CreateTLAS(uint32_t index)
{
	if (renderItems.size() > 0)
	{
		std::vector<std::pair<uint32_t, VulkanBottomLevelAccelerationStructureInstance*>> b;

		for (size_t s = 0; s < renderItems.size(); s++)
		{
			if (renderItems[s].IsValid())
				b.push_back({ static_cast<uint32_t>(s), blasInstances[RyID(s)].get() });
		}

		if(b.size() > 0)
		{
			tlases[index] = std::make_unique<VulkanTopLevelAccelerationStructure>(vulkanContext.get(), b, vulkanContext->GetCurrentFrameResource()->GetCommandBuffer());

			std::unique_ptr<VulkanDescriptorWriter> descriptorWriter = std::make_unique<VulkanDescriptorWriter>(vulkanContext.get());
			descriptorWriter->WriteDescriptorAccelerationStructure(0, tlases[index]->GetDeviceHandle());
			descriptorWriter->UpdateDescriptors(tlasDescriptorSets[index]->vkDescriptorSet);
		}
	}
}

void ruya::Graphics::UpdateTLAS(VulkanCommandBuffer* pCommandBuffer, uint32_t index)
{
	if (!tlases[frameIndex])
		return;

	if (renderItems.size() > 0)
	{
		std::vector<std::pair<uint32_t, VulkanBottomLevelAccelerationStructureInstance*>> b;

		for(size_t s = 0; s < renderItems.size(); s++)
		{
			if(renderItems[s].IsValid())
				b.push_back({ static_cast<uint32_t>(s), blasInstances[RyID(s)].get()});
		}

		pCommandBuffer->UpdateTLAS(tlases[index].get(), b);
	}
}

void ruya::Graphics::DestroyTLAS(uint32_t index)
{
	if (tlases[index])
	{
		tlases[index].reset();
	}
}

ruya::DescriptorSetLayout* ruya::Graphics::GetCameraDataBufferDescriptorSetLayout() const
{
	return cameraDataBufferDescriptorSetLayout.get();
}

ruya::DescriptorSetLayout* ruya::Graphics::GetDirectionalLightDataBufferDescriptorSetLayout() const
{
	return directionalLightDataBufferDescriptorSetLayout.get();
}

ruya::DescriptorSetLayout* ruya::Graphics::GetTexture2DsDescriptorSetLayout() const
{
	return texture2DsDescriptorSetLayout.get();
}

ruya::DescriptorSetLayout* ruya::Graphics::GetRenderMaterialsDescriptorSetLayout() const
{
	return renderMaterialsDescriptorSetLayout.get();
}

ruya::DescriptorSetLayout* ruya::Graphics::GetRenderItemsDescriptorSetLayout() const
{
	return renderItemsDescriptorSetLayout.get();
}

ruya::DescriptorSetLayout* ruya::Graphics::GetTLASDescriptorSetLayout() const
{
	return tlasDescriptorSetLayout.get();
}

bool ruya::Graphics::IsTLASValid()
{
	return tlases[frameIndex] != nullptr;
}

ruya::DescriptorSet* ruya::Graphics::GetTexture2DsDescriptorSet()
{
	return texture2DsDescriptorSet.get();
}

ruya::DescriptorSet* ruya::Graphics::GetRenderMaterialsDescriptorSet()
{
	return renderMaterialsDescriptorSet.get();
}

ruya::DescriptorSet* ruya::Graphics::GetRenderItemsDescriptorSet()
{
	return renderItemsDescriptorSet.get();
}

ruya::DescriptorSet* ruya::Graphics::GetCurrentFramesTLASDescriptorSet()
{
	return tlasDescriptorSets[frameIndex].get();
}

ruya::GraphicsPipeline* ruya::Graphics::GetStandartPipeline()
{
	return defaultPipeline.get();
}

ruya::Texture2D* ruya::Graphics::GetRenderTarget(uint32_t frameIndex, std::string renderTargetName)
{
	return defaultPipeline->GetOutputImage(frameIndex, renderTargetName);
}

void ruya::Graphics::CreateGeneralResources()
{
	frameBufferCount = vulkanContext->GetSwapchainImageCount();

	//Samplers
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.maxLod = 20;

	vkCreateSampler(vulkanContext->GetDevice(), &samplerCreateInfo, nullptr, &nearestSampler);

	samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.maxLod = 20;
	samplerCreateInfo.anisotropyEnable = VK_TRUE;
	samplerCreateInfo.maxAnisotropy = 16.0f;

	vkCreateSampler(vulkanContext->GetDevice(), &samplerCreateInfo, nullptr, &linearSampler);

	//Bindless descriptor pool
	std::vector<DescriptorPoolSizeRatio> descriptorPoolSizeRatio =
	{
		{ DescriptorType::STORAGE_BUFFER, kMaxMaterialCount + kMaxRenderItemCount},
		{ DescriptorType::COMBINED_IMAGE_SAMPLER, kMaxSampledImage2DCount },
		{ DescriptorType::ACCELERATION_STRUCTURE, static_cast<float>(frameBufferCount) }
	};

	bindlessDescriptorPool = std::make_unique<DescriptorPool>(descriptorPoolSizeRatio, frameBufferCount + 3);

	//Descriptor set layouts
	cameraDataBufferDescriptorSetLayout = std::make_unique<DescriptorSetLayout>();
	cameraDataBufferDescriptorSetLayout->AddBinding(DescriptorType::UNIFORM_BUFFER, 1);
	cameraDataBufferDescriptorSetLayout->Build();

	directionalLightDataBufferDescriptorSetLayout = std::make_unique<DescriptorSetLayout>();
	directionalLightDataBufferDescriptorSetLayout->AddBinding(DescriptorType::UNIFORM_BUFFER, 1);
	directionalLightDataBufferDescriptorSetLayout->Build();

	tlasDescriptorSetLayout = std::make_unique<DescriptorSetLayout>();
	tlasDescriptorSetLayout->AddBinding(DescriptorType::ACCELERATION_STRUCTURE, 1);
	tlasDescriptorSetLayout->Build();

	tlases.resize(frameBufferCount);
	rebuildTLASes = std::make_unique<std::atomic<bool>[]>(frameBufferCount);

	for (size_t i = 0; i < frameBufferCount; ++i) 
	{
		tlasDescriptorSets.push_back(bindlessDescriptorPool->AllocateDescriptorSet(tlasDescriptorSetLayout.get()));
		rebuildTLASes[i].store(false, std::memory_order_release);
	}

	CreateImage2DDescriptorSetLayout();
	CreateRenderMaterialDescriptorSetLayout();
	CreateRenderItemsDescriptorSetLayout();

	meshBufferRyIDCounter = RyID(0);
	texture2DRyIDCounter = RyID(0);
	renderMaterialRyIDCounter = RyID(0);
	renderItemRyIDCounter = RyID(0);

	frameIndex = 0;
}

void ruya::Graphics::CreateRenderItemsDescriptorSetLayout()
{
	renderItemsDescriptorSetLayout = std::make_unique<DescriptorSetLayout>();
	renderItemsDescriptorSetLayout->AddBinding(DescriptorType::STORAGE_BUFFER, kMaxRenderItemCount);
	renderItemsDescriptorSetLayout->Build();

	renderItemsDescriptorSet = bindlessDescriptorPool->AllocateDescriptorSet(renderItemsDescriptorSetLayout.get(), 1, true);

	renderItems.reserve(kMaxRenderItemCount);
}

void ruya::Graphics::CreateImage2DDescriptorSetLayout()
{
	texture2DsDescriptorSetLayout = std::make_unique<DescriptorSetLayout>();
	texture2DsDescriptorSetLayout->AddBinding(DescriptorType::COMBINED_IMAGE_SAMPLER, kMaxSampledImage2DCount);
	texture2DsDescriptorSetLayout->Build();

	texture2DsDescriptorSet = bindlessDescriptorPool->AllocateDescriptorSet(texture2DsDescriptorSetLayout.get(), 1, true);
}

void ruya::Graphics::CreateRenderMaterialDescriptorSetLayout()
{
	renderMaterialsDescriptorSetLayout = std::make_unique<DescriptorSetLayout>();
	renderMaterialsDescriptorSetLayout->AddBinding(DescriptorType::STORAGE_BUFFER, kMaxMaterialCount);
	renderMaterialsDescriptorSetLayout->Build();

	renderMaterialsDescriptorSet = bindlessDescriptorPool->AllocateDescriptorSet(renderMaterialsDescriptorSetLayout.get(), 1, true);
}

void ruya::Graphics::CreateDebugLinePipeline()
{
	debugLinePipeline = std::make_unique<DebugLinePipeline>();
}

void ruya::Graphics::DispatchDebugLinePipeline()
{
	debugLinePipeline->Dispatch(frameBufferWidth, frameBufferHeight, GetRenderTarget(frameIndex, "toneMapPassResultImage"), defaultPipeline->GetOutputImage(frameIndex, "gbufferDepth"));
}

void ruya::Graphics::UpdateDescriptors()
{
	std::lock_guard<std::mutex> guard(descriptorUpdateQueueMutex);

	while (!texture2DsDescriptorSetUpdateQueue.empty())
	{
		RyID ryID = texture2DsDescriptorSetUpdateQueue.front();
		texture2DsDescriptorSetUpdateQueue.pop();

		VkDescriptorImageInfo descriptorImageInfo = {};
		descriptorImageInfo.imageLayout = texture2Ds[ryID]->GetVulkanImage()->imageLayout;
		descriptorImageInfo.sampler = linearSampler;
		descriptorImageInfo.imageView = texture2Ds[ryID]->GetVulkanImage()->GetImageView();

		std::unique_ptr<VulkanDescriptorWriter> descriptorWriter = std::make_unique<VulkanDescriptorWriter>(vulkanContext.get());
		descriptorWriter->WriteAndUpdateDescriptorImageByIndex(texture2DsDescriptorSet->vkDescriptorSet, static_cast<uint32_t>(ryID.GetRawID()), descriptorImageInfo);

		texture2Ds[ryID]->SetValid();
	}

	while (!renderMaterialsDescriptorSetUpdateQueue.empty())
	{
		RyID ryID = renderMaterialsDescriptorSetUpdateQueue.front();
		renderMaterialsDescriptorSetUpdateQueue.pop();

		VkDescriptorBufferInfo descriptorBufferInfo = {};
		descriptorBufferInfo.buffer = renderMaterials[ryID]->GetRenderMaterialBuffer()->GetDeviceHandle();
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = VK_WHOLE_SIZE;

		std::unique_ptr<VulkanDescriptorWriter> descriptorWriter = std::make_unique<VulkanDescriptorWriter>(vulkanContext.get());
		descriptorWriter->WriteAndUpdateDescriptorBufferByIndex(renderMaterialsDescriptorSet->vkDescriptorSet, static_cast<uint32_t>(ryID.GetRawID()), descriptorBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		renderMaterials[ryID]->SetValid();
	}

	while (!renderItemsDescriptorSetUpdateQueue.empty())
	{
		RyID ryID = renderItemsDescriptorSetUpdateQueue.front();
		renderItemsDescriptorSetUpdateQueue.pop();

		UpdateRenderItemTransform(ryID, renderItems[ryID.GetRawID()].GetTransform());

		VkDescriptorBufferInfo descriptorBufferInfo = {};
		descriptorBufferInfo.buffer = renderItems[ryID.GetRawID()].GetRenderItemBufferDevice()->GetDeviceHandle();
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = VK_WHOLE_SIZE;

		std::unique_ptr<VulkanDescriptorWriter> descriptorWriter = std::make_unique<VulkanDescriptorWriter>(vulkanContext.get());
		descriptorWriter->WriteAndUpdateDescriptorBufferByIndex(renderItemsDescriptorSet->vkDescriptorSet, static_cast<uint32_t>(ryID.GetRawID()), descriptorBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		for (size_t i = 0; i < frameBufferCount; ++i)
			rebuildTLASes[i].store(true, std::memory_order_release);

		renderItems[ryID.GetRawID()].SetValid();
	}
}

void ruya::Graphics::SyncRenderItemTransforms()
{
	while(!renderItemNeedTransformUpdate.empty())
	{
		RyID ryID = renderItemNeedTransformUpdate.front();
		renderItemNeedTransformUpdate.pop();

		renderItems[ryID.GetRawID()].UpdateTransform(renderItems[ryID.GetRawID()].GetTransform());
		blasInstances[ryID]->transform = renderItems[ryID.GetRawID()].GetTransform();
	}

	while (!renderItemNeedMaterialUpdate.empty())
	{
		auto& pair = renderItemNeedMaterialUpdate.front();
		renderItemNeedMaterialUpdate.pop();

		renderItems[pair.first.GetRawID()].UpdateMaterial(renderItems[pair.first.GetRawID()].GetRenderMaterialRyID());

		DestroyRenderMaterial(pair.second);
	}
}
