#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include <nlohmann_json/json.hpp>
#include <magic_enum/magic_enum.hpp>

#include <core/enum_serializer.h>

#include "texture_2d.h"
#include "descriptor_set_layout.h"
#include "descriptor_set.h"
#include "rasterization_pipeline.h"
#include "ray_tracing_pipeline.h"
#include "compute_pipeline.h"

namespace ruya
{
	enum class PipelineImageFormat
	{
		R32G32B32A32_SFLOAT,
		R16G16B16A16_SFLOAT,
		R32G32B32_SFLOAT,
		R16G16B16_SFLOAT,
		R8G8B8A8_UNORM,
		D32_SFLOAT,
		R8_UNORM,
		R16_SFLOAT,
		R32_UINT
	};
	MAKE_ENUM_JSON_SERIALIZABLE(PipelineImageFormat);

	enum class PipelineImageDimension
	{
		Full,
		Half,
		Quarter
	};
	MAKE_ENUM_JSON_SERIALIZABLE(PipelineImageDimension);

	struct PipelineImage
	{
		std::string name = "";
		PipelineImageFormat format = PipelineImageFormat::R32G32B32A32_SFLOAT;
		PipelineImageDimension dimension = PipelineImageDimension::Full;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(PipelineImage, name, format, dimension)
	};

	enum class PipelineImageDescriptorType
	{
		Storage,
		Sampled,
		ColorAttachment,
		DepthAttachment
	};
	MAKE_ENUM_JSON_SERIALIZABLE(PipelineImageDescriptorType);

	enum class PipelineImageDescriptorUsage
	{
		Read,
		Write
	};
	MAKE_ENUM_JSON_SERIALIZABLE(PipelineImageDescriptorUsage);

	enum class PipelineImageDescriptorFrameIndex
	{
		CurrentFrame,
		PrevFrame
	};
	MAKE_ENUM_JSON_SERIALIZABLE(PipelineImageDescriptorFrameIndex);

	enum class PipelineImageDescriptorFilter
	{
		Linear,
		Nearest
	};
	MAKE_ENUM_JSON_SERIALIZABLE(PipelineImageDescriptorFilter);

	struct PipelineImageDescriptor
	{
		std::string pipelineImageName = "";
		PipelineImageDescriptorType type = PipelineImageDescriptorType::Storage;
		PipelineImageDescriptorUsage usage = PipelineImageDescriptorUsage::Read;
		PipelineImageDescriptorFrameIndex frameIndex = PipelineImageDescriptorFrameIndex::CurrentFrame;
		PipelineImageDescriptorFilter filter = PipelineImageDescriptorFilter::Linear;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(PipelineImageDescriptor, pipelineImageName, type, usage, frameIndex, filter)
	};

	enum class GraphicsPipelineBlockType
	{
		Rasterization,
		RayTracing,
		Compute,
		BlitImage
	};
	MAKE_ENUM_JSON_SERIALIZABLE(GraphicsPipelineBlockType);

	enum class StandartDescriptorType
	{
		CameraData,
		AtmosphericLightData,
		Texture2Ds,
		RenderMaterials,
		RenderItems,
		TLAS,
	};
	MAKE_ENUM_JSON_SERIALIZABLE(StandartDescriptorType);

	enum class GraphicsPipelineShaderType
	{
		Vertex,
		Fragment,
		RayGeneration,
		RayMiss,
		RayAnyHit,
		RayClosestHit,
		Compute,
	};
	MAKE_ENUM_JSON_SERIALIZABLE(GraphicsPipelineShaderType);

	struct GraphicsPipelineShader
	{
		GraphicsPipelineShaderType type = GraphicsPipelineShaderType::Compute;
		std::string path = "";

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(GraphicsPipelineShader, type, path)
	};

	enum class GraphicsPipelineBlockDispatchSize
	{
		Full,
		Half,
		Quarter,
		Custom
	};
	MAKE_ENUM_JSON_SERIALIZABLE(GraphicsPipelineBlockDispatchSize);

	enum class BlitImageFilter
	{
		Nearest,
		Linear
	};
	MAKE_ENUM_JSON_SERIALIZABLE(BlitImageFilter);

	struct CustomDispatchSize
	{
		uint32_t width = 128;
		uint32_t height = 1;
		uint32_t depth = 1;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CustomDispatchSize, width, height, depth)
	};

	struct ComputeShaderThreadGroupDimensions
	{
		uint32_t width = 128;
		uint32_t height = 1;
		uint32_t depth = 1;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ComputeShaderThreadGroupDimensions, width, height, depth)
	};

	struct GraphicsPipelineBlock
	{
		GraphicsPipelineBlockType type = GraphicsPipelineBlockType::Compute;
		std::vector<StandartDescriptorType> standartDescriptorTypes;
		std::vector<PipelineImageDescriptor> pipelineImageDescriptors;

		GraphicsPipelineBlockDispatchSize dispatchSize = GraphicsPipelineBlockDispatchSize::Full;
		CustomDispatchSize customDispatchSize{};
		ComputeShaderThreadGroupDimensions computeShaderThreadGroupDimensions{};

		std::vector<GraphicsPipelineShader> shaders;

		std::string blitSourceImageName = "";
		std::string blitDestinationImageName = "";
		BlitImageFilter blitFilter = BlitImageFilter::Linear;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(GraphicsPipelineBlock,
			type,
			standartDescriptorTypes,
			pipelineImageDescriptors,
			dispatchSize,
			customDispatchSize,
			computeShaderThreadGroupDimensions,
			shaders,
			blitSourceImageName,
			blitDestinationImageName,
			blitFilter)
	};

	struct GraphicsPipelineFrameBuffer
	{
		std::unique_ptr<VulkanBuffer> cameraDataBuffer;
		std::unique_ptr<DescriptorSet> cameraDataBufferDescriptorSet;

		std::unique_ptr<VulkanBuffer> directionalLightDataBuffer;
		std::unique_ptr<DescriptorSet> directionalLightDataBufferDescriptorSet;

		std::unordered_map<std::string, std::unique_ptr<Texture2D>> texture2Ds;
		std::unordered_map<uint32_t, std::unique_ptr<DescriptorSet>> texture2DDescriptorSets;
	};

	class GraphicsPipeline
	{
	public:
		GraphicsPipeline() = default;
		GraphicsPipeline(std::string graphicsPipelineFilePath);
		~GraphicsPipeline();

		GraphicsPipeline(const GraphicsPipeline&) = delete;
		GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

		void RecreateFrameBuffers();

		void Dispatch();

		Texture2D* GetOutputImage(uint32_t frameIndex, std::string name);

		std::unordered_map<std::string, uint32_t>& GetRenderTargetsMap();

	private:
		void CreatePipeline();
		std::unique_ptr<GraphicsPipelineFrameBuffer> CreateGraphicsPipelineFrameBuffer();

	private:
		std::vector<PipelineImage> pipelineImages;
		std::vector<GraphicsPipelineBlock> pipelineBlocks;

		std::unordered_map<std::string, uint32_t> pipelineImagesMap;

		std::unordered_map<uint32_t, std::unique_ptr<DescriptorSetLayout>> pipelinesDescriptorSetLayouts;

		std::unordered_map<uint32_t, std::unique_ptr<RasterizationPipeline>> rasterizationPipelines;
		std::unordered_map<uint32_t, std::unique_ptr<RayTracingPipeline>> rayTracingPipelines;
		std::unordered_map<uint32_t, std::unique_ptr<ComputePipeline>> computePipelines;

		std::vector<std::unique_ptr<GraphicsPipelineFrameBuffer>> frameBuffers;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(GraphicsPipeline, pipelineImages, pipelineBlocks)
	};
}