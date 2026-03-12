#pragma once
#include <queue>
#include <vector>
#include <memory>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

#include <core/ry_id.h>
#include <window/window.h>

#include "ruya_vulkan/vulkan_context.h"
#include "ruya_vulkan/vulkan_buffer.h"
#include "ruya_vulkan/vulkan_descriptor_pool.h"
#include "ruya_vulkan/vulkan_descriptor_set_layout.h"
#include "ruya_vulkan/vulkan_rasterization_pipeline.h"
#include "ruya_vulkan/vulkan_compute_pipeline.h"
#include "ruya_vulkan/vulkan_top_level_acceleration_structure.h"
#include "ruya_vulkan/vulkan_ray_tracing_pipeline.h"

#include "graphics_job.h"
#include "frame_buffer.h"
#include "mesh.h"
#include "model.h"
#include "render_geometry.h"
#include "image_2d.h"
#include "material.h"
#include "render_command.h"

namespace ruya
{
	constexpr uint32_t kMaxRenderGeometryCount = 65536;
	constexpr uint32_t kMaxSampledImage2DCount = 8192;
	constexpr uint32_t kMaxMaterialCount = 8192;

	class Graphics
	{
	public:
		Graphics(Window* window);
		~Graphics();

		Graphics(const Graphics&) = delete;
		Graphics& operator=(const Graphics&) = delete;

		VulkanContext* GetVulkanContext() const;

		void RecordGraphicsJob(std::unique_ptr<GraphicsJob> graphicsJob);
		void SubmitGraphicsJobQueue();

		void BeginFrame();
		void Draw();
		void BeginEditorDraw();
		void EndEditorDraw();
		void EndFrame();

		bool IsSwapchainValid();

		void WaitGraphicsDeviceIdle();

		void LoadModel3D(RyID modelAssetID);
		Model3D* GetModel3D(RyID modelAssetID); 
		void UnloadModel3D(RyID modelAssetID);

		RyID CreateMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::string& name);
		void DestroyMesh(RyID meshID);

		RyID CreateImage2D(const std::string& imagePath, ImageType type, ImageSampler sampler, const std::string& name);
		void DestroyImage2D(RyID imageID);
		
		RyID CreatePBROpaqueMaterial(const std::string& name, RyID albedoImageId, RyID normalImageId, RyID metallicRoughnessImageId);
		void UpdatePBROpaqueMaterial(RyID materialId, RyID albedoImageId, RyID normalImageId, RyID metallicRoughnessImageId);
		void DestroyPBROpaqueMaterial(RyID materialId);

		RyID CreateRenderGeometry(const glm::mat4& transform, RyID meshId, RyID materialId);
		const RenderGeometry& GetRenderGeometry(RyID renderGeometryId);
		void UpdateRenderGeometry(RyID renderGeometryId, const glm::mat4& transform, RyID meshId, RyID materialId, bool draw);
		void DestroyRenderGeometry(RyID renderGeometryId);
		std::unordered_map<RyID, RenderGeometry>& GetRenderGeometries();

		void SetFrameBufferExtent(uint32_t width, uint32_t height);
		uint32_t GetFrameBufferWidth() const;
		uint32_t GetFrameBufferHeight() const;

		FrameBuffer* CurrentFrameBuffer() const;
		FrameBuffer* GetFrameBufferWithIndex(uint32_t index) const;
		uint32_t GetCurrentFrameIndex() const;
		uint32_t GetFrameBufferCount() const;

		VkSampler GetNearestSampler() const;
		VkSampler GetLinearSampler() const;

		VulkanDescriptorSetLayout* GetCameraDataBufferDescriptorSetLayout() const;
		VulkanDescriptorSetLayout* GetDirectionalLightDataBufferDescriptorSetLayout() const;
		VulkanDescriptorSetLayout* GetGBufferDescriptorSetLayout() const;
		VulkanDescriptorSetLayout* GetDownsampledGBufferDescriptorSetLayout() const;
		VulkanDescriptorSetLayout* GetDirectionalLightShadowMapImageDescriptorSetLayout() const;
		VulkanDescriptorSetLayout* GetLightPassImageDescriptorSetLayout() const;

		VulkanRasterizationPipeline* GetGBufferPipeline() const;

	private:
		void LoadMesh(RyID meshID);
		void UnloadMesh(RyID meshID);

		void LoadImage2D(RyID imageID);
		void UnloadImage2D(RyID imageID);

		void LoadPBROpaqueMaterial(RyID materialId);
		void UnloadPBROpaqueMaterial(RyID materialId);

		void CreateTLAS();
		void UpdateTLAS(VulkanCommandBuffer* pCommandBuffer);
		void DestroyTLAS();

		void CreateGeneralResources();

		void CreateFrameBuffers();
		void DestroyFrameBuffers();

		void CreateImage2DDescriptorSetLayout();
		void CreatePBROpaqueMaterialDescriptorSetLayout();
		void CreateRenderGeometriesDescriptorSetLayout();

		void CreateGBufferPipeline();
		void DispatchGBufferPipeline();

		void CreateDirectionalLightShadowPipeline();
		void DispatchDirectionalLightShadowPipeline();

		void CreateLightPassPipeline();
		void DispatchLightPassPipeline();

	private:
		//Vulkan Context
		std::unique_ptr<VulkanContext> vulkanContext;

		std::vector<std::unique_ptr<GraphicsJob>> recordedJobs;
		std::vector<std::unique_ptr<GraphicsJob>> jobsToDispatch;

		//Samplers
		VkSampler nearestSampler;
		VkSampler linearSampler;

		//Descriptor pool
		std::unique_ptr<VulkanDescriptorPool> bindlessDescriptorPool;

		//Descriptor set layouts
		std::unique_ptr<VulkanDescriptorSetLayout> cameraDataBufferDescriptorSetLayout;
		std::unique_ptr<VulkanDescriptorSetLayout> directionalLightDataBufferDescriptorSetLayout;
		std::unique_ptr<VulkanDescriptorSetLayout> gBufferDescriptorSetLayout;
		std::unique_ptr<VulkanDescriptorSetLayout> downsampledGBufferDescriptorSetLayout;
		std::unique_ptr<VulkanDescriptorSetLayout> directionalLightShadowMapImageDescriptorSetLayout;
		std::unique_ptr<VulkanDescriptorSetLayout> lightPassImageDescriptorSetLayout;

		//Models
		std::unordered_map<RyID, std::unique_ptr<Model3D>> model3Ds;
		std::unordered_map<RyID, uint32_t> model3DUsageCount;

		//Meshes
		std::unordered_map<RyID, std::unique_ptr<Mesh>> meshes;
		std::unordered_map<RyID, uint32_t> meshUsageCount;

		//Images
		std::unordered_map<RyID, std::unique_ptr<Image2D>> image2Ds;
		std::unordered_map<RyID, uint32_t> image2DUsageCount;
		std::unique_ptr<VulkanDescriptorSetLayout> image2DsDescriptorSetLayout;
		VkDescriptorSet image2DsDescriptorSet;
		uint32_t image2DsDescriptorImageInfosIndexCounter;
		std::queue<uint32_t> availableImage2DsDescriptorImageInfosIndexes;

		//Materials
		std::unordered_map<RyID, PBROpaqueMaterial> pbrOpaqueMaterials;
		std::unordered_map<RyID, std::unique_ptr<VulkanBuffer>> pbrOpaqueMaterialBuffers;
		std::unordered_map<RyID, uint32_t> pbrOpaqueMaterialsUsageCount;
		std::unique_ptr<VulkanDescriptorSetLayout> pbrOpaqueMaterialsDescriptorSetLayout;
		VkDescriptorSet pbrOpaqueMaterialsDescriptorSet;
		uint32_t pbrOpaqueMaterialsDescriptorBufferInfosIndexCounter;
		std::queue<uint32_t> availablePBROpaqueMaterialsDescriptorBufferInfosIndexes;

		//Render geometries
		std::unordered_map<RyID, RenderGeometry> renderGeometries;
		std::unordered_map<RyID, std::unique_ptr<VulkanBuffer>> renderGeometryBuffers;
		RyID nextRenderGeometryID;
		std::queue<RyID> availableRenderGeometryIds;
		std::unique_ptr<VulkanDescriptorSetLayout> renderGeometriesDescriptorSetLayout;
		VkDescriptorSet renderGeometriesDescriptorSet;

		//AccelerationStructure
		std::unique_ptr<VulkanTopLevelAccelerationStructure> tlas;
		std::unique_ptr<VulkanDescriptorSetLayout> tlasDescriptorSetLayout;
		VkDescriptorSet tlasDescriptorSet;
		std::unordered_map<RyID, std::unique_ptr<VulkanBottomLevelAccelerationStructureInstance>> blasInstances;

		//Frame buffer
		uint32_t frameBufferWidth;
		uint32_t frameBufferHeight;
		uint32_t frameBufferCount;
		std::vector<std::unique_ptr<FrameBuffer>> frameBuffers;
		uint32_t frameIndex;
		uint32_t prevFrameIndex;

		//GBuffer pass
		std::unique_ptr<VulkanRasterizationPipeline> gBufferPipeline;
		std::unique_ptr<VulkanBuffer> indirectDrawBuffer;

		//Shadow pass
		std::unique_ptr<VulkanRayTracingPipeline> directionalLightShadowPipeline;

		//Light Pass
		std::unique_ptr<VulkanComputePipeline> lightPassPipeline;
	};
}