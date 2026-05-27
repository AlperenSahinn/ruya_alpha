#pragma once
#include <string>
#include <vector>
#include <memory>

#include "ruya_vulkan/vulkan_ray_tracing_pipeline.h"

#include "descriptor_set_layout.h"
#include "texture_2d.h"
#include "descriptor_set.h"

namespace ruya
{
	class RayTracingPipeline
	{
	public:
		RayTracingPipeline(
			const std::string& rayGenShaderPath,
			const std::string& anyHitShaderPath,
			const std::string& closestHitShaderPath,
			const std::vector<std::string>& rayMissShaderPaths,
			const std::vector<DescriptorSetLayout*>& pDescriptorSetLayouts);
		~RayTracingPipeline() = default;

		RayTracingPipeline(const RayTracingPipeline&) = delete;
		RayTracingPipeline& operator=(const RayTracingPipeline&) = delete;

		void Dispatch(
			uint32_t width, 
			uint32_t height, 
			std::vector<Texture2D*> imageReads,
			std::vector<Texture2D*> imageWrites,
			const std::vector<DescriptorSet*>& pDescriptorSets);

	private:
		std::unique_ptr<VulkanRayTracingPipeline> vulkanRayTracingPipeline;
	};
}
