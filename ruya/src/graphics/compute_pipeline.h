#pragma once
#include <memory>
#include <vector>

#include "ruya_vulkan/vulkan_compute_pipeline.h"

#include "descriptor_set_layout.h"
#include "texture_2d.h"
#include "descriptor_set.h"

namespace ruya
{
	class ComputePipeline
	{
	public:
		ComputePipeline(const std::string& computeShaderPathconst, std::vector<DescriptorSetLayout*>& pDescriptorSetLayouts, std::vector<VkPushConstantRange> pushConstantRanges = {});
		~ComputePipeline() = default;

		ComputePipeline(const ComputePipeline&) = delete;
		ComputePipeline& operator=(const ComputePipeline&) = delete;

		void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ, std::vector<Texture2D*> imageReads, std::vector<Texture2D*> imageWrites, const std::vector<DescriptorSet*>& pDescriptorSets, uint32_t pushConstantSize = 0, void* pushConstantData = nullptr);

	private:
		std::unique_ptr<VulkanComputePipeline> vulkanComputePipeline;
	};
}