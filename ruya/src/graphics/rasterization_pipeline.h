#pragma once
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#include "ruya_vulkan/vulkan_rasterization_pipeline.h"
#include "ruya_vulkan/vulkan_buffer.h"

#include "descriptor_set_layout.h"
#include "color_format.h"
#include "texture_2d.h"
#include "descriptor_set.h"
#include "render_item.h"

namespace ruya
{
	class RasterizationPipeline
	{
	public:
		RasterizationPipeline(
			const std::string& vertexShader, 
			const std::string& fragmentShader, 
			const std::vector<DescriptorSetLayout*>& pDescriptorSetLayouts,
			const std::vector<ColorFormat>& colorAttachmentFormats,
			ColorFormat depthFormat);
		~RasterizationPipeline() = default;

		RasterizationPipeline(const RasterizationPipeline&) = delete;
		RasterizationPipeline& operator=(const RasterizationPipeline&) = delete;

		void Dispatch(
			uint32_t frameBufferWidth, 
			uint32_t frameBufferHeight,
			const std::vector<Texture2D*>& pColorAttachments,
			Texture2D* pDepthAttachment,
			const std::vector<DescriptorSet*>& pDescriptorSets,
			const std::vector<RyID>& renderGeometryInstanceIDs);

	private:
		std::unique_ptr<VulkanRasterizationPipeline> vulkanRasterizationPipeline;
		std::unique_ptr<VulkanBuffer> vulkanIndirectDrawBuffer;
	};
}