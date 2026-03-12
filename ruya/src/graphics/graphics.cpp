#include "graphics.h"

#include <engine.h>
#include <core/log.h>
#include <core/hash_code_generator.h>
#include <core/assert.h>

#include "ruya_vulkan/vulkan_descriptor_writer.h"

ruya::Graphics::Graphics(Window* window)
{
	vulkanContext = std::make_unique<VulkanContext>();
	vulkanContext->Init(window, true);

	CreateGeneralResources();
	SetFrameBufferExtent(2560, 1440);
	CreateGBufferPipeline();
	CreateDirectionalLightShadowPipeline();
	CreateLightPassPipeline();

	RUYA_LOG_INFO("[Graphics] Graphics instance created");
}

ruya::Graphics::~Graphics()
{
	vkDestroySampler(vulkanContext->GetDevice(), nearestSampler, nullptr);
	vkDestroySampler(vulkanContext->GetDevice(), linearSampler, nullptr);

	RUYA_LOG_INFO("[Graphics] Graphics instance destroyed");
}

ruya::VulkanContext* ruya::Graphics::GetVulkanContext() const
{
	return vulkanContext.get();
}

void ruya::Graphics::RecordGraphicsJob(std::unique_ptr<GraphicsJob> graphicsJob)
{
	recordedJobs.push_back(std::move(graphicsJob));
}

void ruya::Graphics::SubmitGraphicsJobQueue()
{
	std::swap(recordedJobs, jobsToDispatch);
	recordedJobs.clear();
}

void ruya::Graphics::BeginFrame()
{
	vulkanContext->BeginFrame();

	for (std::unique_ptr<GraphicsJob>& graphicsJob : jobsToDispatch)
	{
		graphicsJob->Execute();
	}
}

void ruya::Graphics::Draw()
{
	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->GeneralMemoryBarrier(
		VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
		VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	if(tlas)
		UpdateTLAS(vulkanContext->GetCurrentFrameResource()->GetCommandBuffer());

	frameBuffers[frameIndex]->UpdateDescriptors(vulkanContext.get());

	VkExtent2D extent{ frameBufferWidth, frameBufferHeight };

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->SetViewPort(extent.width, extent.height);

	DispatchGBufferPipeline();

	if(tlas)
	{
		DispatchDirectionalLightShadowPipeline();
	}

	DispatchLightPassPipeline();
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
		frameBuffers[frameIndex]->GetLightPassImage(),
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
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

void ruya::Graphics::LoadModel3D(RyID modelAssetID)
{
	ENGINE_ASSERT_MSG(modelAssetID.IsValid(), "[Graphics] LoadModel3D failed. modelAssetID is invalid.");
	ENGINE_ASSERT_MSG(modelAssetID.IsValid(), "[Graphics] LoadModel3D failed. modelAssetID is invalid.");

	if (model3Ds.contains(modelAssetID))
	{
		model3DUsageCount[modelAssetID]++;
		return;
	}

	model3Ds.insert({ modelAssetID, std::make_unique<Model3D>(modelAssetID) });
	model3DUsageCount.insert({ modelAssetID, 1 });
}

ruya::Model3D* ruya::Graphics::GetModel3D(RyID modelAssetID)
{
	ENGINE_ASSERT_MSG(modelAssetID.IsValid(), "[Graphics] GetModel3D failed. modelAssetID is invalid.");
	ENGINE_ASSERT_MSG(model3Ds.contains(modelAssetID), "[Graphics] GetModel3D failed.There is no Model3D exist with this modelAssetID {}", std::to_string(modelAssetID).c_str());

	return model3Ds[modelAssetID].get();
}

void ruya::Graphics::UnloadModel3D(RyID modelAssetID)
{
	ENGINE_ASSERT_MSG(modelAssetID.IsValid(), "[Graphics] UnloadModel3D failed. modelAssetID is invalid.");
	ENGINE_ASSERT_MSG(model3Ds.contains(modelAssetID), "[Graphics] UnloadModel3D failed.There is no Model3D exist with this modelAssetID {}", std::to_string(modelAssetID).c_str());

	model3DUsageCount[modelAssetID]--;

	if (model3DUsageCount[modelAssetID] == 0)
	{
		model3Ds.erase(modelAssetID);
		model3DUsageCount.erase(modelAssetID);
	}
}

ruya::RyID ruya::Graphics::CreateMesh(const std::vector<Vertex>& vertices, const std::vector <uint32_t>& indices, const std::string& name)
{
	RyID id(hash_code_generator::XXHash64(name));

	if(meshes.contains(id))
	{
		RUYA_LOG_WARN("There is a Mesh with this name.");
		return id;
	}

	std::unique_ptr<GraphicsJob> graphicsJob = CreateGraphicsJob();
	
	graphicsJob->AddJob([this, id, vertices, indices]() {
		meshes.insert({ id, std::make_unique<Mesh>(vertices, indices) });
		meshUsageCount.insert({ id, 0 });
		});

	RecordGraphicsJob(std::move(graphicsJob));

	return id;
}

void ruya::Graphics::DestroyMesh(RyID meshID)
{
	ENGINE_ASSERT_MSG(meshID.IsValid(), "[Graphics] DestroyMesh failed. meshID is invalid.");

	std::unique_ptr<GraphicsJob> graphicsJob = CreateGraphicsJob();

	graphicsJob->AddJob([this, meshID]() {
		ENGINE_ASSERT_MSG(meshes.contains(meshID), "[Graphics] DestroyMesh failed. There is no Mesh exist with this meshID {}", std::to_string(meshID).c_str());
		meshes[meshID]->Unload();
		meshes.erase(meshID);
		meshUsageCount.erase(meshID);
		});

	RecordGraphicsJob(std::move(graphicsJob));
}

void ruya::Graphics::LoadMesh(RyID meshID)
{
	ENGINE_ASSERT_MSG(meshID.IsValid(), "[Graphics] LoadMesh failed. meshID is invalid.");
	ENGINE_ASSERT_MSG(meshes.contains(meshID), "[Graphics] LoadMesh failed. There is no Mesh exist with this meshID {}", std::to_string(meshID).c_str());

	meshUsageCount[meshID]++;
	if (meshUsageCount[meshID] == 1)
	{
		meshes[meshID]->Load();
	}
}

void ruya::Graphics::UnloadMesh(RyID meshID)
{
	ENGINE_ASSERT_MSG(meshID.IsValid(), "[Graphics] UnloadMesh failed. meshID is invalid.");
	ENGINE_ASSERT_MSG(meshes.contains(meshID), "[Graphics] UnloadMesh failed. There is no Mesh exist with this meshID {}", std::to_string(meshID).c_str());

	if (meshUsageCount[meshID] == 0)
		return;

	meshUsageCount[meshID]--;

	if (meshUsageCount[meshID] == 0)
	{
		meshes[meshID]->Unload();
	}
}

ruya::RyID ruya::Graphics::CreateImage2D(const std::string& imagePath, ImageType type, ImageSampler sampler, const std::string& name)
{
	RyID id(hash_code_generator::XXHash64(name));

	if (image2Ds.contains(id))
	{
		RUYA_LOG_WARN("There is a Image2D with this name.");
		return id;
	}

	std::unique_ptr<GraphicsJob> graphicsJob = CreateGraphicsJob();

	graphicsJob->AddJob([this, id, imagePath, type, sampler]() {
		image2Ds.insert({ id, std::make_unique<Image2D>(imagePath, type, sampler) });
		image2DUsageCount.insert({ id, 0 });
		});

	RecordGraphicsJob(std::move(graphicsJob));

	return id;
}

void ruya::Graphics::DestroyImage2D(RyID imageID)
{
	ENGINE_ASSERT_MSG(imageID.IsValid(), "[Graphics] DestroyImage2D failed. imageID is invalid.");

	std::unique_ptr<GraphicsJob> graphicsJob = CreateGraphicsJob();

	graphicsJob->AddJob([this, imageID]() {
		ENGINE_ASSERT_MSG(image2Ds.contains(imageID), "[Graphics] DestroyImage2D failed. There is no Image2D exist with this imageID {}", std::to_string(imageID).c_str());
		image2Ds[imageID]->Unload();
		availableImage2DsDescriptorImageInfosIndexes.push(image2Ds[imageID]->GetDescriptorIndex());
		image2Ds.erase(imageID);
		image2DUsageCount.erase(imageID);
		});

	RecordGraphicsJob(std::move(graphicsJob));
}

void ruya::Graphics::LoadImage2D(RyID imageID)
{
	ENGINE_ASSERT_MSG(imageID.IsValid(), "[Graphics] LoadImage2D failed. imageID is invalid.");
	ENGINE_ASSERT_MSG(image2Ds.contains(imageID), "[Graphics] LoadImage2D failed. There is no Image2D exist with this imageID {}", std::to_string(imageID).c_str());

	image2DUsageCount[imageID]++;

	if (image2DUsageCount[imageID] == 1)
	{
		image2Ds[imageID]->Load();

		VkDescriptorImageInfo descriptorImageInfo = {};
		descriptorImageInfo.imageLayout = image2Ds[imageID]->GetVulkanImage()->imageLayout;
		descriptorImageInfo.sampler = linearSampler;
		descriptorImageInfo.imageView = image2Ds[imageID]->GetVulkanImage()->GetImageView();

		uint32_t descriptorIndex;

		if (availableImage2DsDescriptorImageInfosIndexes.empty())
		{
			descriptorIndex = image2DsDescriptorImageInfosIndexCounter;
			image2DsDescriptorImageInfosIndexCounter++;
		}
		else
		{
			descriptorIndex = availableImage2DsDescriptorImageInfosIndexes.front();
			availableImage2DsDescriptorImageInfosIndexes.pop();
		}

		image2Ds[imageID]->SetDescriptorIndex(descriptorIndex);

		std::unique_ptr<VulkanDescriptorWriter> descriptorWriter = std::make_unique<VulkanDescriptorWriter>(vulkanContext.get());
		descriptorWriter->WriteAndUpdateDescriptorImageByIndex(image2DsDescriptorSet, descriptorIndex, descriptorImageInfo);
	}
}

void ruya::Graphics::UnloadImage2D(RyID imageID)
{
	ENGINE_ASSERT_MSG(imageID.IsValid(), "[Graphics] UnloadImage2D failed. imageID is invalid.");
	ENGINE_ASSERT_MSG(image2Ds.contains(imageID), "[Graphics] UnloadImage2D failed. There is no Image2D exist with this imageID {}", std::to_string(imageID).c_str());

	if (image2DUsageCount[imageID] == 0)
		return;

	image2DUsageCount[imageID]--;

	if (image2DUsageCount[imageID] == 0)
	{
		uint32_t descriptorIndex = image2Ds[imageID]->GetDescriptorIndex();
		image2Ds[imageID]->Unload();
		availableImage2DsDescriptorImageInfosIndexes.push(descriptorIndex);
	}
}

ruya::RyID ruya::Graphics::CreatePBROpaqueMaterial(const std::string& name, RyID albedoImageId, RyID normalImageId, RyID metallicRoughnessImageId)
{
	RyID id(hash_code_generator::XXHash64(name));

	if (pbrOpaqueMaterials.contains(id))
	{
		RUYA_LOG_WARN("There is a PBROpaqueMaterial with this name.");
		return id;
	}

	std::unique_ptr<GraphicsJob> graphicsJob = CreateGraphicsJob();

	graphicsJob->AddJob([this, id, albedoImageId, normalImageId, metallicRoughnessImageId]() {
		PBROpaqueMaterial pbrOpaqueMaterial = {};
		pbrOpaqueMaterial.albedoImageId = albedoImageId;
		pbrOpaqueMaterial.normalImageId = normalImageId;
		pbrOpaqueMaterial.metallicRoughnessImageId = metallicRoughnessImageId;

		pbrOpaqueMaterials.insert({ id, std::move(pbrOpaqueMaterial) });
		pbrOpaqueMaterialsUsageCount.insert({ id, 0 });
		});

	RecordGraphicsJob(std::move(graphicsJob));

	return id;
}

void ruya::Graphics::UpdatePBROpaqueMaterial(RyID materialId, RyID albedoImageId, RyID normalImageId, RyID metallicRoughnessImageId)
{
	ENGINE_ASSERT_MSG(materialId.IsValid(), "[Graphics] UpdatePBROpaqueMaterial failed. materialId is invalid.");

	std::unique_ptr<GraphicsJob> graphicsJob = CreateGraphicsJob();

	graphicsJob->AddJob([this, materialId, albedoImageId, normalImageId, metallicRoughnessImageId]() {
		ENGINE_ASSERT_MSG(pbrOpaqueMaterials.contains(materialId), "[Graphics] UpdatePBROpaqueMaterial failed. There is no PBROpaqueMaterial exist with this materialId {}", std::to_string(materialId).c_str());
		PBROpaqueMaterial& pbrOpaqueMaterial = pbrOpaqueMaterials[materialId];
		pbrOpaqueMaterial.albedoImageId = albedoImageId;
		pbrOpaqueMaterial.normalImageId = normalImageId;
		pbrOpaqueMaterial.metallicRoughnessImageId = metallicRoughnessImageId;
		});

	RecordGraphicsJob(std::move(graphicsJob));
}

void ruya::Graphics::DestroyPBROpaqueMaterial(RyID materialId)
{
	ENGINE_ASSERT_MSG(materialId.IsValid(), "[Graphics] DestroyPBROpaqueMaterial failed. materialId is invalid.");

	std::unique_ptr<GraphicsJob> graphicsJob = CreateGraphicsJob();

	graphicsJob->AddJob([this, materialId]() {
		ENGINE_ASSERT_MSG(pbrOpaqueMaterials.contains(materialId), "[Graphics] DestroyPBROpaqueMaterial failed. There is no PBROpaqueMaterial exist with this materialId {}", std::to_string(materialId).c_str());
		PBROpaqueMaterial& pbrOpaqueMaterial = pbrOpaqueMaterials[materialId];

		UnloadImage2D(pbrOpaqueMaterial.albedoImageId);
		UnloadImage2D(pbrOpaqueMaterial.normalImageId);
		UnloadImage2D(pbrOpaqueMaterial.metallicRoughnessImageId);

		availablePBROpaqueMaterialsDescriptorBufferInfosIndexes.push(pbrOpaqueMaterial.descriptorIndex);
		pbrOpaqueMaterials.erase(materialId);
		});

	RecordGraphicsJob(std::move(graphicsJob));
}

void ruya::Graphics::LoadPBROpaqueMaterial(RyID materialId)
{
	ENGINE_ASSERT_MSG(materialId.IsValid(), "[Graphics] LoadPBROpaqueMaterial failed. materialId is invalid.");
	ENGINE_ASSERT_MSG(pbrOpaqueMaterials.contains(materialId), "[Graphics] LoadPBROpaqueMaterial failed. There is no PBROpaqueMaterial exist with this materialId {}", std::to_string(materialId).c_str());

	pbrOpaqueMaterialsUsageCount[materialId]++;

	if (pbrOpaqueMaterialsUsageCount[materialId] == 1)
	{
		PBROpaqueMaterial& pbrOpaqueMaterial = pbrOpaqueMaterials[materialId];

		LoadImage2D(pbrOpaqueMaterial.albedoImageId);
		LoadImage2D(pbrOpaqueMaterial.normalImageId);
		LoadImage2D(pbrOpaqueMaterial.metallicRoughnessImageId);

		PBROpaqueMaterialGPU pbrOpaqueMaterialGPU;
		pbrOpaqueMaterialGPU.albedoImageDescriptorIndex = image2Ds[pbrOpaqueMaterial.albedoImageId]->GetDescriptorIndex();
		pbrOpaqueMaterialGPU.normalImageDescriptorIndex = image2Ds[pbrOpaqueMaterial.normalImageId]->GetDescriptorIndex();
		pbrOpaqueMaterialGPU.metallicRoughnessImageDescriptorIndex = image2Ds[pbrOpaqueMaterial.metallicRoughnessImageId]->GetDescriptorIndex();

		pbrOpaqueMaterialBuffers.insert(
			{
				materialId,
				std::make_unique<VulkanBuffer>(
					vulkanContext.get(),
					sizeof(PBROpaqueMaterialGPU),
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
					VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY)
			}
		);

		VkDescriptorBufferInfo descriptorBufferInfo = {};
		descriptorBufferInfo.buffer = pbrOpaqueMaterialBuffers[materialId]->GetDeviceHandle();
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = VK_WHOLE_SIZE;

		uint32_t descriptorIndex;

		if (availablePBROpaqueMaterialsDescriptorBufferInfosIndexes.empty())
		{
			descriptorIndex = pbrOpaqueMaterialsDescriptorBufferInfosIndexCounter;
			pbrOpaqueMaterialsDescriptorBufferInfosIndexCounter++;
		}
		else
		{
			descriptorIndex = availablePBROpaqueMaterialsDescriptorBufferInfosIndexes.front();
			availablePBROpaqueMaterialsDescriptorBufferInfosIndexes.pop();
		}

		pbrOpaqueMaterial.descriptorIndex = descriptorIndex;

		std::unique_ptr<VulkanDescriptorWriter> descriptorWriter = std::make_unique<VulkanDescriptorWriter>(vulkanContext.get());
		descriptorWriter->WriteAndUpdateDescriptorBufferByIndex(pbrOpaqueMaterialsDescriptorSet, descriptorIndex, descriptorBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		std::unique_ptr<VulkanBuffer> uploadBuffer = std::make_unique<VulkanBuffer>(vulkanContext.get(), sizeof(PBROpaqueMaterialGPU), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		uploadBuffer->UploadData(&pbrOpaqueMaterialGPU, sizeof(PBROpaqueMaterialGPU));

		vulkanContext->ImmediateSubmitCommand([&](VulkanCommandBuffer* commandBuffer)
			{
				VkBufferCopy bufferCopy = {};
				bufferCopy.size = sizeof(PBROpaqueMaterialGPU);
				bufferCopy.srcOffset = 0;
				bufferCopy.dstOffset = 0;

				commandBuffer->CopyBufferToBuffer(*uploadBuffer, pbrOpaqueMaterialBuffers[materialId].get(), bufferCopy);
			}
		);
	}
}

void ruya::Graphics::UnloadPBROpaqueMaterial(RyID materialId)
{
	ENGINE_ASSERT_MSG(materialId.IsValid(), "[Graphics] UnloadPBROpaqueMaterial failed. materialId is invalid.");
	ENGINE_ASSERT_MSG(pbrOpaqueMaterials.contains(materialId), "[Graphics] UnloadPBROpaqueMaterial failed. There is no PBROpaqueMaterial exist with this materialId {}", std::to_string(materialId).c_str());

	if (pbrOpaqueMaterialsUsageCount[materialId] == 0)
		return;

	pbrOpaqueMaterialsUsageCount[materialId]--;

	if (pbrOpaqueMaterialsUsageCount[materialId] == 0)
	{
		PBROpaqueMaterial& pbrOpaqueMaterial = pbrOpaqueMaterials[materialId];

		UnloadImage2D(pbrOpaqueMaterial.albedoImageId);
		UnloadImage2D(pbrOpaqueMaterial.normalImageId);
		UnloadImage2D(pbrOpaqueMaterial.metallicRoughnessImageId);

		pbrOpaqueMaterialBuffers.erase(materialId);
		availablePBROpaqueMaterialsDescriptorBufferInfosIndexes.push(pbrOpaqueMaterial.descriptorIndex);
	}
}

ruya::RyID ruya::Graphics::CreateRenderGeometry(const glm::mat4& transform, RyID meshId, RyID materialId)
{
	RyID id;

	if (availableRenderGeometryIds.empty())
	{
		id = nextRenderGeometryID;

		nextRenderGeometryID = RyID(id.GetRawID() + 1);
	}
	else
	{
		id = availableRenderGeometryIds.front();
		availableRenderGeometryIds.pop();
	}

	std::unique_ptr<GraphicsJob> graphicsJob = CreateGraphicsJob();

	graphicsJob->AddJob([this, id, transform, meshId, materialId]() {
		if (!meshes.contains(meshId))
		{
			RUYA_LOG_ERROR("[Graphics] Create render geometry failed. There is no mesh exist with this id: %s", std::to_string(meshId).c_str());
			return 0;
		}

		LoadMesh(meshId);
		LoadPBROpaqueMaterial(materialId);

		RenderGeometry renderGeometry = {};
		renderGeometry.transform = transform;
		renderGeometry.vertexBufferAddress = meshes[meshId]->GetVertexBuffer()->GetDeviceAddress();
		renderGeometry.indexBufferAddress = meshes[meshId]->GetIndexBuffer()->GetDeviceAddress();
		renderGeometry.materialDescriptorIndex = pbrOpaqueMaterials[materialId].descriptorIndex;
		renderGeometry.meshId = meshId;
		renderGeometry.materialId = materialId;
		renderGeometries.insert({ id,  renderGeometry });

		renderGeometry = renderGeometries[id];

		renderGeometryBuffers.insert(
			{
				id,
				std::make_unique<VulkanBuffer>(
					vulkanContext.get(),
					sizeof(RenderGeometryGPU),
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
					VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU)
			}
		);

		RenderGeometryGPU renderGeometryGPU = {};
		renderGeometryGPU.transform = renderGeometry.transform;
		renderGeometryGPU.vertexBufferAddress = renderGeometry.vertexBufferAddress;
		renderGeometryGPU.indexBufferAddress = renderGeometry.indexBufferAddress;
		renderGeometryGPU.materialDescriptorIndex = renderGeometry.materialDescriptorIndex;

		renderGeometryBuffers[id]->UploadData(&renderGeometryGPU, sizeof(RenderGeometryGPU));

		VkDescriptorBufferInfo descriptorBufferInfo = {};
		descriptorBufferInfo.buffer = renderGeometryBuffers[id]->GetDeviceHandle();
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = VK_WHOLE_SIZE;

		std::unique_ptr<VulkanDescriptorWriter> descriptorWriter = std::make_unique<VulkanDescriptorWriter>(vulkanContext.get());
		descriptorWriter->WriteAndUpdateDescriptorBufferByIndex(renderGeometriesDescriptorSet, static_cast<uint32_t>(id.GetRawID()), descriptorBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		blasInstances.insert({ id, std::make_unique<VulkanBottomLevelAccelerationStructureInstance>(
			meshes[renderGeometry.meshId]->GetBLAS(), static_cast<uint32_t>(id.GetRawID()), renderGeometry.transform) });

		DestroyTLAS();
		CreateTLAS();
		});

	RecordGraphicsJob(std::move(graphicsJob));

	return id;
}

const ruya::RenderGeometry& ruya::Graphics::GetRenderGeometry(RyID renderGeometryId)
{
	return renderGeometries[renderGeometryId];
}

void ruya::Graphics::UpdateRenderGeometry(RyID renderGeometryId, const glm::mat4& transform, RyID meshId, RyID materialId, bool draw)
{
	std::unique_ptr<GraphicsJob> graphicsJob = CreateGraphicsJob();

	graphicsJob->AddJob([this, renderGeometryId, transform, meshId, materialId, draw]() {
		if (!renderGeometries.contains(renderGeometryId))
		{
			RUYA_LOG_ERROR("[Graphics] Update render geometry failed. There is no render geometry exist with this id: %s", std::to_string(renderGeometryId).c_str());
			return;
		}

		if (!meshes.contains(meshId))
		{
			RUYA_LOG_ERROR("[Graphics] Update render geometry failed. There is no mesh exist with this id: %s", std::to_string(meshId).c_str());
			return;
		}

		if (!pbrOpaqueMaterials.contains(materialId))
		{
			RUYA_LOG_ERROR("[Graphics] Update render geometry failed. There is no material exist with this id: %s", std::to_string(materialId).c_str());
			return;
		}

		RenderGeometry& renderGeometry = renderGeometries[renderGeometryId];

		if (materialId != renderGeometry.materialId)
		{
			UnloadPBROpaqueMaterial(renderGeometry.materialId);
			LoadPBROpaqueMaterial(materialId);
		}

		if (meshId != renderGeometry.meshId)
		{
			UnloadMesh(renderGeometry.meshId);
			LoadMesh(meshId);
		}

		renderGeometry.transform = transform;
		renderGeometry.vertexBufferAddress = meshes[meshId]->GetVertexBuffer()->GetDeviceAddress();
		renderGeometry.indexBufferAddress = meshes[meshId]->GetIndexBuffer()->GetDeviceAddress();
		renderGeometry.materialDescriptorIndex = pbrOpaqueMaterials[materialId].descriptorIndex;
		renderGeometry.meshId = meshId;
		renderGeometry.materialId = materialId;
		renderGeometry.draw = draw;

		RenderGeometryGPU renderGeometryGPU = {};
		renderGeometryGPU.transform = renderGeometry.transform;
		renderGeometryGPU.vertexBufferAddress = renderGeometry.vertexBufferAddress;
		renderGeometryGPU.indexBufferAddress = renderGeometry.indexBufferAddress;
		renderGeometryGPU.materialDescriptorIndex = renderGeometry.materialDescriptorIndex;

		renderGeometryBuffers[renderGeometryId]->UploadData(&renderGeometryGPU, sizeof(RenderGeometryGPU));
		});

	RecordGraphicsJob(std::move(graphicsJob));
}

void ruya::Graphics::DestroyRenderGeometry(RyID renderGeometryId)
{
	std::unique_ptr<GraphicsJob> graphicsJob = CreateGraphicsJob();

	graphicsJob->AddJob([this, renderGeometryId]() {
		if (!renderGeometries.contains(renderGeometryId))
		{
			RUYA_LOG_ERROR("[Graphics] Destroy render geometry failed. There is no render geometry exist with this id: %s", std::to_string(renderGeometryId).c_str());
			return;
		}

		UnloadPBROpaqueMaterial(renderGeometries[renderGeometryId].materialId);
		UnloadMesh(renderGeometries[renderGeometryId].meshId);

		renderGeometryBuffers.erase(renderGeometryId);
		renderGeometries.erase(renderGeometryId);
		availableRenderGeometryIds.push(renderGeometryId);
		blasInstances.erase(renderGeometryId);

		DestroyTLAS();
		CreateTLAS();
		});

	RecordGraphicsJob(std::move(graphicsJob));
}

std::unordered_map<ruya::RyID, ruya::RenderGeometry>& ruya::Graphics::GetRenderGeometries()
{
	return renderGeometries;
}

void ruya::Graphics::SetFrameBufferExtent(uint32_t width, uint32_t height)
{
	frameBufferWidth = width;
	frameBufferHeight = height;

	DestroyFrameBuffers();
	CreateFrameBuffers();
}

uint32_t ruya::Graphics::GetFrameBufferWidth() const
{
	return frameBufferWidth;
}

uint32_t ruya::Graphics::GetFrameBufferHeight() const
{
	return frameBufferHeight;
}

ruya::FrameBuffer* ruya::Graphics::CurrentFrameBuffer() const
{
	return frameBuffers[frameIndex].get();
}

ruya::FrameBuffer* ruya::Graphics::GetFrameBufferWithIndex(uint32_t index) const
{
	return frameBuffers[index].get();
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

void ruya::Graphics::CreateTLAS()
{
	if (renderGeometries.size() > 0)
	{
		tlasDescriptorSet = bindlessDescriptorPool->AllocateDescriptorSet(tlasDescriptorSetLayout->GetDeviceHandle());

		tlas = std::make_unique<VulkanTopLevelAccelerationStructure>(vulkanContext.get(), blasInstances);

		std::unique_ptr<VulkanDescriptorWriter> descriptorWriter = std::make_unique<VulkanDescriptorWriter>(vulkanContext.get());
		descriptorWriter->WriteDescriptorAccelerationStructure(0, tlas->GetDeviceHandle());
		descriptorWriter->UpdateDescriptors(tlasDescriptorSet);
	}
}

void ruya::Graphics::UpdateTLAS(VulkanCommandBuffer* pCommandBuffer)
{
	for (auto& pair : renderGeometries)
	{
		RyID id = pair.first;
		const RenderGeometry renderGeometry = pair.second;
		blasInstances[id]->transform = renderGeometry.transform;
		blasInstances[id]->blas = meshes[renderGeometries[RyID(blasInstances[id]->instanceCustomIndex)].meshId]->GetBLAS();
	}

	pCommandBuffer->UpdateTLAS(tlas.get(), blasInstances);
}

void ruya::Graphics::DestroyTLAS()
{
	if(tlas)
	{
		WaitGraphicsDeviceIdle();
		bindlessDescriptorPool->FreeDescriptorSet(tlasDescriptorSet);
		tlas.reset();
	}
}

ruya::VulkanDescriptorSetLayout* ruya::Graphics::GetCameraDataBufferDescriptorSetLayout() const
{
	return cameraDataBufferDescriptorSetLayout.get();
}

ruya::VulkanDescriptorSetLayout* ruya::Graphics::GetDirectionalLightDataBufferDescriptorSetLayout() const
{
	return directionalLightDataBufferDescriptorSetLayout.get();
}

ruya::VulkanDescriptorSetLayout* ruya::Graphics::GetGBufferDescriptorSetLayout() const
{
	return gBufferDescriptorSetLayout.get();
}

ruya::VulkanDescriptorSetLayout* ruya::Graphics::GetDownsampledGBufferDescriptorSetLayout() const
{
	return downsampledGBufferDescriptorSetLayout.get();
}

ruya::VulkanDescriptorSetLayout* ruya::Graphics::GetDirectionalLightShadowMapImageDescriptorSetLayout() const
{
	return directionalLightShadowMapImageDescriptorSetLayout.get();
}

ruya::VulkanDescriptorSetLayout* ruya::Graphics::GetLightPassImageDescriptorSetLayout() const
{
	return lightPassImageDescriptorSetLayout.get();
}

ruya::VulkanRasterizationPipeline* ruya::Graphics::GetGBufferPipeline() const
{
	return gBufferPipeline.get();
}

void ruya::Graphics::CreateGeneralResources()
{
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

	vkCreateSampler(vulkanContext->GetDevice(), &samplerCreateInfo, nullptr, &linearSampler);

	//Indirect draw buffer
	indirectDrawBuffer = std::make_unique<VulkanBuffer>(
		vulkanContext.get(),
		kMaxRenderGeometryCount * sizeof(VkDrawIndirectCommand),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU);

	//Bindless descriptor pool
	std::vector<VulkanDescriptorPoolSizeRatio> descriptorPoolSizeRatio =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kMaxMaterialCount + kMaxRenderGeometryCount},
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxSampledImage2DCount },
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 }
	};

	bindlessDescriptorPool = std::make_unique<VulkanDescriptorPool>(vulkanContext.get(), descriptorPoolSizeRatio, 4);

	//Descriptor set layouts
	cameraDataBufferDescriptorSetLayout = std::make_unique<VulkanDescriptorSetLayout>(vulkanContext.get());
	cameraDataBufferDescriptorSetLayout->AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
	cameraDataBufferDescriptorSetLayout->Build(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

	directionalLightDataBufferDescriptorSetLayout = std::make_unique<VulkanDescriptorSetLayout>(vulkanContext.get());
	directionalLightDataBufferDescriptorSetLayout->AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
	directionalLightDataBufferDescriptorSetLayout->Build(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

	gBufferDescriptorSetLayout = std::make_unique<VulkanDescriptorSetLayout>(vulkanContext.get());
	gBufferDescriptorSetLayout->AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);
	gBufferDescriptorSetLayout->AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);
	gBufferDescriptorSetLayout->AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);
	gBufferDescriptorSetLayout->AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);
	gBufferDescriptorSetLayout->Build(VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);

	downsampledGBufferDescriptorSetLayout = std::make_unique<VulkanDescriptorSetLayout>(vulkanContext.get());
	downsampledGBufferDescriptorSetLayout->AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);
	downsampledGBufferDescriptorSetLayout->AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);
	downsampledGBufferDescriptorSetLayout->Build(VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);

	directionalLightShadowMapImageDescriptorSetLayout = std::make_unique<VulkanDescriptorSetLayout>(vulkanContext.get());
	directionalLightShadowMapImageDescriptorSetLayout->AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);
	directionalLightShadowMapImageDescriptorSetLayout->Build(VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);

	lightPassImageDescriptorSetLayout = std::make_unique<VulkanDescriptorSetLayout>(vulkanContext.get());
	lightPassImageDescriptorSetLayout->AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);
	lightPassImageDescriptorSetLayout->Build(VK_SHADER_STAGE_COMPUTE_BIT);

	tlasDescriptorSetLayout = std::make_unique<VulkanDescriptorSetLayout>(vulkanContext.get());
	tlasDescriptorSetLayout->AddBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1);
	tlasDescriptorSetLayout->Build(VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);

	CreateImage2DDescriptorSetLayout();
	CreatePBROpaqueMaterialDescriptorSetLayout();
	CreateRenderGeometriesDescriptorSetLayout();
}

void ruya::Graphics::CreateFrameBuffers()
{
	for(uint32_t i = 0; i < vulkanContext->GetSwapchainImageCount(); i++)
	{
		frameBuffers.push_back(std::make_unique<FrameBuffer>(vulkanContext.get(), frameBufferWidth, frameBufferHeight));
	}

	frameBufferCount = static_cast<uint32_t>(frameBuffers.size());
	frameIndex = 0;
	prevFrameIndex = 0;
}

void ruya::Graphics::DestroyFrameBuffers()
{
	frameBuffers.clear();
}

void ruya::Graphics::CreateRenderGeometriesDescriptorSetLayout()
{
	renderGeometriesDescriptorSetLayout = std::make_unique<VulkanDescriptorSetLayout>(vulkanContext.get());
	renderGeometriesDescriptorSetLayout->AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kMaxRenderGeometryCount);

	std::vector<VkDescriptorBindingFlags> bindingFlags =
	{
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
	};

	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();

	renderGeometriesDescriptorSetLayout->Build(
		VK_SHADER_STAGE_VERTEX_BIT |
		VK_SHADER_STAGE_FRAGMENT_BIT |
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
		VK_SHADER_STAGE_COMPUTE_BIT,
		&descriptorSetLayoutBindingFlagsCreateInfo,
		VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);

	renderGeometriesDescriptorSet = bindlessDescriptorPool->AllocateDescriptorSet(renderGeometriesDescriptorSetLayout->GetDeviceHandle(), true, kMaxRenderGeometryCount);

	nextRenderGeometryID = RyID(0);
}

void ruya::Graphics::CreateImage2DDescriptorSetLayout()
{
	image2DsDescriptorSetLayout = std::make_unique<VulkanDescriptorSetLayout>(vulkanContext.get());
	image2DsDescriptorSetLayout->AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxSampledImage2DCount);

	std::vector<VkDescriptorBindingFlags> bindingFlags =
	{
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
	};

	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();

	image2DsDescriptorSetLayout->Build(
		VK_SHADER_STAGE_VERTEX_BIT |
		VK_SHADER_STAGE_FRAGMENT_BIT |
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
		VK_SHADER_STAGE_COMPUTE_BIT,
		&descriptorSetLayoutBindingFlagsCreateInfo,
		VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);

	image2DsDescriptorSet = bindlessDescriptorPool->AllocateDescriptorSet(image2DsDescriptorSetLayout->GetDeviceHandle(), true, kMaxSampledImage2DCount);

	image2DsDescriptorImageInfosIndexCounter = 0;
}

void ruya::Graphics::CreatePBROpaqueMaterialDescriptorSetLayout()
{
	pbrOpaqueMaterialsDescriptorSetLayout = std::make_unique<VulkanDescriptorSetLayout>(vulkanContext.get());
	pbrOpaqueMaterialsDescriptorSetLayout->AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kMaxMaterialCount);

	std::vector<VkDescriptorBindingFlags> bindingFlags =
	{
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
	};

	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();

	pbrOpaqueMaterialsDescriptorSetLayout->Build(
		VK_SHADER_STAGE_VERTEX_BIT |
		VK_SHADER_STAGE_FRAGMENT_BIT |
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
		VK_SHADER_STAGE_COMPUTE_BIT,
		&descriptorSetLayoutBindingFlagsCreateInfo,
		VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);

	pbrOpaqueMaterialsDescriptorSet = bindlessDescriptorPool->AllocateDescriptorSet(pbrOpaqueMaterialsDescriptorSetLayout->GetDeviceHandle(), true, kMaxMaterialCount);

	pbrOpaqueMaterialsDescriptorBufferInfosIndexCounter = 0;
}

void ruya::Graphics::CreateGBufferPipeline()
{
	std::vector<VkFormat> colorAttachmentFormats = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT };

	VkFormat depthAttachment = VK_FORMAT_D32_SFLOAT;

	std::vector<VulkanDescriptorSetLayout*>descriptorSetLayouts;
	descriptorSetLayouts.push_back(image2DsDescriptorSetLayout.get());
	descriptorSetLayouts.push_back(pbrOpaqueMaterialsDescriptorSetLayout.get());
	descriptorSetLayouts.push_back(renderGeometriesDescriptorSetLayout.get());
	descriptorSetLayouts.push_back(cameraDataBufferDescriptorSetLayout.get());

	std::vector<VkPushConstantRange> pushConstantRanges;

	gBufferPipeline = std::make_unique<VulkanRasterizationPipeline>(
		vulkanContext.get(),
		ASSETS_DIR + "ruya_files/shaders/compiled/gbuffer_vertex_shader.spv",
		ASSETS_DIR + "ruya_files/shaders/compiled/gbuffer_fragment_shader.spv",
		descriptorSetLayouts,
		colorAttachmentFormats,
		depthAttachment,
		true,
		VK_COMPARE_OP_LESS_OR_EQUAL,
		pushConstantRanges);
}

void ruya::Graphics::DispatchGBufferPipeline()
{
	VkExtent2D extent{ frameBufferWidth, frameBufferHeight };

	VkRenderingAttachmentInfo albedoDepthAttachmentInfo = {};
	albedoDepthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	albedoDepthAttachmentInfo.imageView = frameBuffers[frameIndex]->GetGBuffer()->GetAlbedoDepth()->GetImageView();
	albedoDepthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	albedoDepthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	albedoDepthAttachmentInfo.clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	albedoDepthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingAttachmentInfo positionMetallicAttachmentInfo = {};
	positionMetallicAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	positionMetallicAttachmentInfo.imageView = frameBuffers[frameIndex]->GetGBuffer()->GetPositionMetallic()->GetImageView();
	positionMetallicAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	positionMetallicAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	positionMetallicAttachmentInfo.clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	positionMetallicAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingAttachmentInfo normalRoughnessAttachmentInfo = {};
	normalRoughnessAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	normalRoughnessAttachmentInfo.imageView = frameBuffers[frameIndex]->GetGBuffer()->GetNormalRoughness()->GetImageView();
	normalRoughnessAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	normalRoughnessAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	normalRoughnessAttachmentInfo.clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	normalRoughnessAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingAttachmentInfo vertexNormalsAttachmentInfo = {};
	vertexNormalsAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	vertexNormalsAttachmentInfo.imageView = frameBuffers[frameIndex]->GetGBuffer()->GetVertexNormals()->GetImageView();
	vertexNormalsAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	vertexNormalsAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	vertexNormalsAttachmentInfo.clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	vertexNormalsAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingAttachmentInfo depthAttachmentInfo = {};
	depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachmentInfo.imageView = frameBuffers[frameIndex]->GetGBuffer()->GetDepth()->GetImageView();
	depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachmentInfo.clearValue.depthStencil = { 1.0f, 0 };
	depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkImageSubresourceRange colorRange{};
	colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorRange.baseMipLevel = 0;
	colorRange.levelCount = 1;
	colorRange.baseArrayLayer = 0;
	colorRange.layerCount = 1;

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->ImageMemoryBarrier(
		frameBuffers[frameIndex]->GetGBuffer()->GetAlbedoDepth(),
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		colorRange
	);

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->ImageMemoryBarrier(
		frameBuffers[frameIndex]->GetGBuffer()->GetPositionMetallic(),
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		colorRange
	);

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->ImageMemoryBarrier(
		frameBuffers[frameIndex]->GetGBuffer()->GetNormalRoughness(),
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		colorRange
	);

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->ImageMemoryBarrier(
		frameBuffers[frameIndex]->GetGBuffer()->GetVertexNormals(),
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		colorRange
	);

	VkImageSubresourceRange depthRange{};
	depthRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthRange.baseMipLevel = 0;
	depthRange.levelCount = 1;
	depthRange.baseArrayLayer = 0;
	depthRange.layerCount = 1;

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->ImageMemoryBarrier(
		frameBuffers[frameIndex]->GetGBuffer()->GetDepth(),
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		depthRange
	);

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->SetViewPort(extent.width, extent.height);

	std::vector<VkRenderingAttachmentInfo> renderingAttachmentInfos = { albedoDepthAttachmentInfo,  positionMetallicAttachmentInfo, normalRoughnessAttachmentInfo, vertexNormalsAttachmentInfo };

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->BeginRenderPass(extent, renderingAttachmentInfos, &depthAttachmentInfo);

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->BindRasterizationPipeline(*gBufferPipeline);

	std::vector<VkDescriptorSet> descriptorSets;
	descriptorSets.push_back(image2DsDescriptorSet);
	descriptorSets.push_back(pbrOpaqueMaterialsDescriptorSet);
	descriptorSets.push_back(renderGeometriesDescriptorSet);
	descriptorSets.push_back(frameBuffers[frameIndex]->GetCameraDataBufferDescriptorSet());

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->BindDescriptorSets(descriptorSets, gBufferPipeline->GetPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS);

	std::vector<VkDrawIndirectCommand> drawCmds;
	drawCmds.reserve(renderGeometries.size());

	for (auto& pair : renderGeometries)
	{
		RyID renderGeometryID = pair.first;
		RenderGeometry& renderGeometry = pair.second;

		if(renderGeometryID.IsValid() && renderGeometry.draw && renderGeometry.meshId.IsValid())
		{
			VkDrawIndirectCommand cmd{};
			cmd.vertexCount = meshes[renderGeometry.meshId]->GetIndexCount();
			cmd.instanceCount = 1;
			cmd.firstVertex = 0;
			cmd.firstInstance = static_cast<uint32_t>(renderGeometryID.GetRawID());
			drawCmds.push_back(cmd);
		}
	}

	uint32_t drawCount = static_cast<uint32_t>(drawCmds.size());

	if (drawCount > 0)
	{
		indirectDrawBuffer->UploadData(drawCmds.data(), drawCount * sizeof(VkDrawIndirectCommand));
		vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->DrawIndirect(*indirectDrawBuffer.get(), drawCount, sizeof(VkDrawIndirectCommand));
	}

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->EndRenderPass();
}

void ruya::Graphics::CreateDirectionalLightShadowPipeline()
{
	std::string rayGenShaderPath = ASSETS_DIR + "ruya_files/shaders/compiled/rt_directional_light_shadow_ray_generation.spv";
	std::string rayMissShaderPath = ASSETS_DIR + "ruya_files/shaders/compiled/rt_directional_light_shadow_ray_miss.spv";

	std::vector<VulkanDescriptorSetLayout*>descriptorSetLayouts;
	descriptorSetLayouts.push_back(directionalLightDataBufferDescriptorSetLayout.get());
	descriptorSetLayouts.push_back(gBufferDescriptorSetLayout.get());
	descriptorSetLayouts.push_back(directionalLightShadowMapImageDescriptorSetLayout.get());
	descriptorSetLayouts.push_back(tlasDescriptorSetLayout.get());

	std::vector<std::string> missShaders;
	missShaders.push_back(rayMissShaderPath);

	directionalLightShadowPipeline = std::make_unique<VulkanRayTracingPipeline>(
		vulkanContext.get(),
		rayGenShaderPath,
		missShaders,
		"",
		"",
		descriptorSetLayouts);
}

void ruya::Graphics::DispatchDirectionalLightShadowPipeline()
{
	VkImageSubresourceRange colorRange{};
	colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorRange.baseMipLevel = 0;
	colorRange.levelCount = 1;
	colorRange.baseArrayLayer = 0;
	colorRange.layerCount = 1;

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->ImageMemoryBarrier(
		frameBuffers[frameIndex]->GetGBuffer()->GetAlbedoDepth(),
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		colorRange
	);

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->ImageMemoryBarrier(
		frameBuffers[frameIndex]->GetGBuffer()->GetPositionMetallic(),
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		colorRange
	);

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->ImageMemoryBarrier(
		frameBuffers[frameIndex]->GetGBuffer()->GetNormalRoughness(),
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		colorRange
	);

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->ImageMemoryBarrier(
		frameBuffers[frameIndex]->GetDirectionalLightShadowMapImage(),
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		colorRange
	);

	std::vector<VkDescriptorSet> descriptorSets;
	descriptorSets.push_back(frameBuffers[frameIndex]->GetDirectionalLightDataBufferDescriptorSet());
	descriptorSets.push_back(frameBuffers[frameIndex]->GetGBufferDescriptorSet());
	descriptorSets.push_back(frameBuffers[frameIndex]->GetDirectionalLightShadowMapImageDescriptorSet());
	descriptorSets.push_back(tlasDescriptorSet);

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->TraceRays(directionalLightShadowPipeline.get(), descriptorSets, frameBufferWidth, frameBufferHeight);
}

void ruya::Graphics::CreateLightPassPipeline()
{
	std::vector<VulkanDescriptorSetLayout*>descriptorSetLayouts;
	descriptorSetLayouts.push_back(cameraDataBufferDescriptorSetLayout.get());
	descriptorSetLayouts.push_back(directionalLightDataBufferDescriptorSetLayout.get());
	descriptorSetLayouts.push_back(gBufferDescriptorSetLayout.get());
	descriptorSetLayouts.push_back(directionalLightShadowMapImageDescriptorSetLayout.get());
	descriptorSetLayouts.push_back(lightPassImageDescriptorSetLayout.get());

	std::vector<VkPushConstantRange> pushConstantRanges;

	lightPassPipeline = std::make_unique<VulkanComputePipeline>(
		vulkanContext.get(),
		ASSETS_DIR + "ruya_files/shaders/compiled/light_pass_compute_shader.spv",
		descriptorSetLayouts,
		pushConstantRanges);
}

void ruya::Graphics::DispatchLightPassPipeline()
{
	VkImageSubresourceRange colorRange{};
	colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorRange.baseMipLevel = 0;
	colorRange.levelCount = 1;
	colorRange.baseArrayLayer = 0;
	colorRange.layerCount = 1;

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->ImageMemoryBarrier(
		frameBuffers[frameIndex]->GetGBuffer()->GetAlbedoDepth(),
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		colorRange
	);

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->ImageMemoryBarrier(
		frameBuffers[frameIndex]->GetGBuffer()->GetPositionMetallic(),
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		colorRange
	);

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->ImageMemoryBarrier(
		frameBuffers[frameIndex]->GetGBuffer()->GetNormalRoughness(),
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		colorRange
	);

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->ImageMemoryBarrier(
		frameBuffers[frameIndex]->GetGBuffer()->GetVertexNormals(),
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		colorRange
	);

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->ImageMemoryBarrier(
		frameBuffers[frameIndex]->GetDirectionalLightShadowMapImage(),
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		colorRange
	);

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->ImageMemoryBarrier(
		frameBuffers[frameIndex]->GetLightPassImage(),
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		colorRange
	);

	std::vector<VkDescriptorSet> descriptorSets;
	descriptorSets.push_back(frameBuffers[frameIndex]->GetCameraDataBufferDescriptorSet());
	descriptorSets.push_back(frameBuffers[frameIndex]->GetDirectionalLightDataBufferDescriptorSet());
	descriptorSets.push_back(frameBuffers[frameIndex]->GetGBufferDescriptorSet());
	descriptorSets.push_back(frameBuffers[frameIndex]->GetDirectionalLightShadowMapImageDescriptorSet());
	descriptorSets.push_back(frameBuffers[frameIndex]->GetLightPassImageDescriptorSet());

	vulkanContext->GetCurrentFrameResource()->GetCommandBuffer()->DispatchComputePipeline(lightPassPipeline.get(), descriptorSets, frameBufferWidth, frameBufferHeight, 1);
}
