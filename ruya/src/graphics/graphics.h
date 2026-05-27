#pragma once
#include <queue>
#include <vector>
#include <memory>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

#include <core/ry_id.h>
#include <core/uuid.h>
#include <window/window.h>

#include "ruya_vulkan/vulkan_context.h"
#include "ruya_vulkan/vulkan_buffer.h"
#include "ruya_vulkan/vulkan_descriptor_pool.h"
#include "ruya_vulkan/vulkan_descriptor_set_layout.h"
#include "ruya_vulkan/vulkan_rasterization_pipeline.h"
#include "ruya_vulkan/vulkan_compute_pipeline.h"
#include "ruya_vulkan/vulkan_top_level_acceleration_structure.h"
#include "ruya_vulkan/vulkan_ray_tracing_pipeline.h"

#include "mesh_buffer.h"
#include "texture_2d.h"
#include "render_material.h"
#include "render_item.h"

#include "rasterization_pipeline.h"
#include "descriptor_set.h"
#include "descriptor_set_layout.h"
#include "descriptor_pool.h"
#include "compute_pipeline.h"
#include "ray_tracing_pipeline.h"
#include "debug_line_pipeline.h"
#include "graphics_pipeline.h"

namespace ruya
{
	constexpr uint32_t kMaxRenderItemCount = 65536;
	constexpr uint32_t kMaxSampledImage2DCount = 8192;
	constexpr uint32_t kMaxMaterialCount = 8192;

	class Graphics
	{
	public:
		Graphics();
		~Graphics();

		Graphics(const Graphics&) = delete;
		Graphics& operator=(const Graphics&) = delete;

		void Init(Window* window);

		VulkanContext* GetVulkanContext() const;

		void BeginFrame();
		void Draw();
		void BeginEditorDraw();
		void EndEditorDraw();
		void EndFrame();

		bool IsSwapchainValid();

		void WaitGraphicsDeviceIdle();

		RyID CreateMeshBuffer(UUID ryMeshUUID);
		MeshBuffer* GetMeshBuffer(RyID meshBufferRyID);
		void DestroyMeshBuffer(RyID meshBufferRyID);

		RyID CreateTexture2D(UUID image2DUUID);
		Texture2D* GetTexture2D(RyID texture2DRyID);
		void DestroyTexture2D(RyID texture2DRyID);

		RyID CreateRenderMaterial(UUID materialUUID);
		RenderMaterial* GetRenderMaterial(RyID renderMaterialRyID);
		void DestroyRenderMaterial(RyID renderMaterialRyID);

		RyID CreateRenderItem(const glm::mat4& transform, UUID ryMeshUUID, UUID materialUUID);
		RenderItem* GetRenderItem(RyID renderItemRyID);
		void UpdateRenderItemTransform(RyID renderItemRyID, const glm::mat4& transform);
		void UpdateRenderItemMaterial(RyID renderItemRyID, UUID materialUUID);
		void SetRenderItemVisibility(RyID renderItemRyID, bool b);
		void DestroyRenderItem(RyID renderItemRyID);
		std::vector<RenderItem>& GetRenderItems();
		size_t GetRenderItemsCount();

		void SetFrameBufferExtent(uint32_t width, uint32_t height);
		uint32_t GetFrameBufferWidth() const;
		uint32_t GetFrameBufferHeight() const;

		uint32_t GetCurrentFrameIndex() const;
		uint32_t GetFrameBufferCount() const;

		VkSampler GetNearestSampler() const;
		VkSampler GetLinearSampler() const;

		DescriptorSetLayout* GetCameraDataBufferDescriptorSetLayout() const;
		DescriptorSetLayout* GetDirectionalLightDataBufferDescriptorSetLayout() const;
		DescriptorSetLayout* GetTexture2DsDescriptorSetLayout() const;
		DescriptorSetLayout* GetRenderMaterialsDescriptorSetLayout() const;
		DescriptorSetLayout* GetRenderItemsDescriptorSetLayout() const;
		DescriptorSetLayout* GetTLASDescriptorSetLayout() const;

		bool IsTLASValid();

		DescriptorSet* GetTexture2DsDescriptorSet();
		DescriptorSet* GetRenderMaterialsDescriptorSet();
		DescriptorSet* GetRenderItemsDescriptorSet();
		DescriptorSet* GetCurrentFramesTLASDescriptorSet();

		GraphicsPipeline* GetStandartPipeline();
		Texture2D* GetRenderTarget(uint32_t frameIndex, std::string renderTargetName);

	private:
		void CreateTLAS(uint32_t index);
		void UpdateTLAS(VulkanCommandBuffer* pCommandBuffer, uint32_t index);
		void DestroyTLAS(uint32_t index);

		void CreateGeneralResources();

		void CreateFrameBuffers();
		void DestroyFrameBuffers();

		void CreateImage2DDescriptorSetLayout();
		void CreateRenderMaterialDescriptorSetLayout();
		void CreateRenderItemsDescriptorSetLayout();

		void CreateDebugLinePipeline();
		void DispatchDebugLinePipeline();

		void UpdateDescriptors();
		void SyncRenderItemTransforms();

	private:
		//Vulkan Context
		std::unique_ptr<VulkanContext> vulkanContext;

		//Samplers
		VkSampler nearestSampler;
		VkSampler linearSampler;

		//Descriptor pool
		std::unique_ptr<DescriptorPool> bindlessDescriptorPool;

		//Descriptor set layouts
		std::unique_ptr<DescriptorSetLayout> cameraDataBufferDescriptorSetLayout;
		std::unique_ptr<DescriptorSetLayout> directionalLightDataBufferDescriptorSetLayout;

		//MeshBuffers
		std::unordered_map<UUID, RyID> uuid2MeshBufferRyID;
		std::unordered_map<RyID, UUID> ryID2MeshBufferUUID;
		std::unordered_map<RyID, std::unique_ptr<MeshBuffer>> meshBuffers;
		std::unordered_map<RyID, uint32_t> meshBuffersUsageCounts;
		RyID meshBufferRyIDCounter;
		std::queue<RyID> availableMeshBufferRyIDs;

		//Texture2Ds
		std::unordered_map<UUID, RyID> uuid2Texture2DRyID;
		std::unordered_map<RyID, UUID> ryID2Texture2DUUID;
		std::unordered_map<RyID, std::unique_ptr<Texture2D>> texture2Ds;
		std::unordered_map<RyID, uint32_t> texture2DUsageCounts;
		std::unique_ptr<DescriptorSetLayout> texture2DsDescriptorSetLayout;
		std::unique_ptr<DescriptorSet> texture2DsDescriptorSet;
		RyID texture2DRyIDCounter;
		std::queue<RyID> availableTexture2DRyIDs;
		std::queue<RyID> texture2DsDescriptorSetUpdateQueue;

		//Materials
		std::unordered_map<UUID, RyID> uuid2RenderMaterialRyID;
		std::unordered_map<RyID, UUID> ryID2RenderMaterialUUID;
		std::unordered_map<RyID, std::unique_ptr<RenderMaterial>> renderMaterials;
		std::unordered_map<RyID, uint32_t> renderMaterialUsageCounts;
		std::unique_ptr<DescriptorSetLayout> renderMaterialsDescriptorSetLayout;
		std::unique_ptr<DescriptorSet> renderMaterialsDescriptorSet;
		RyID renderMaterialRyIDCounter;
		std::queue<RyID> availablerenderMaterialRyIDs;
		std::queue<RyID> renderMaterialsDescriptorSetUpdateQueue;

		//RenderItems
		std::vector<RenderItem> renderItems;
		std::unique_ptr<DescriptorSetLayout> renderItemsDescriptorSetLayout;
		std::unique_ptr<DescriptorSet> renderItemsDescriptorSet;
		RyID renderItemRyIDCounter;
		std::queue<RyID> availableRenderItemIDs;
		std::queue<RyID> renderItemsDescriptorSetUpdateQueue;
		std::queue<RyID> renderItemNeedTransformUpdate;
		std::queue<std::pair<RyID, RyID>> renderItemNeedMaterialUpdate;

		std::mutex descriptorUpdateQueueMutex;

		//AccelerationStructure
		std::vector<std::unique_ptr<VulkanTopLevelAccelerationStructure>> tlases;
		std::unique_ptr<DescriptorSetLayout> tlasDescriptorSetLayout;
		std::vector<std::unique_ptr<DescriptorSet>> tlasDescriptorSets;
		std::unordered_map<RyID, std::unique_ptr<VulkanBottomLevelAccelerationStructureInstance>> blasInstances;
		std::unique_ptr<std::atomic<bool>[]> rebuildTLASes;

		//Frame
		uint32_t frameBufferWidth;
		uint32_t frameBufferHeight;
		uint32_t frameBufferCount;
		uint32_t frameIndex;
		uint32_t prevFrameIndex;

		//Graphics Pipelines
		std::unique_ptr<GraphicsPipeline> defaultPipeline;

		//DebugLine pass
		std::unique_ptr<DebugLinePipeline> debugLinePipeline;
	};
}