#pragma once
#include <memory>
#include <string>
#include <vector>

#include "volk/volk.h"

#include "vulkan_buffer.h"
#include "vulkan_descriptor_set_layout.h"

namespace ruya
{
	class VulkanContext;

	class VulkanRayTracingPipeline
	{
	public:
		VulkanRayTracingPipeline(
			VulkanContext* pVulkanContext,
			std::string rayGenerationShaderPath,
			std::vector<std::string> rayMissShaderPaths,
			std::string anyHitShaderPath,
			std::string closestHitShaderPath,
			std::vector<VulkanDescriptorSetLayout*>& descriptorSetLayouts);
		~VulkanRayTracingPipeline();

		VulkanRayTracingPipeline(const VulkanRayTracingPipeline&) = delete;
		VulkanRayTracingPipeline& operator=(const VulkanRayTracingPipeline&) = delete;

		VkPipeline GetDeviceHandle() const;
		VkPipelineLayout GetPipelineLayout() const;
		const VkStridedDeviceAddressRegionKHR& GetRayGenerationRegion() const;
		const VkStridedDeviceAddressRegionKHR& GetMissRegion() const;
		const VkStridedDeviceAddressRegionKHR& GetHitRegion() const;
		const VkStridedDeviceAddressRegionKHR& GetCallableRegion() const;

	private:
		std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
		VkPipelineLayout pipelineLayout;
		VkPipeline pipeline;

		std::unique_ptr<VulkanBuffer> shaderBindingTableBuffer;
		VkStridedDeviceAddressRegionKHR rayGenerationRegion;
		VkStridedDeviceAddressRegionKHR rayMissRegion;
		VkStridedDeviceAddressRegionKHR rayHitRegion;
		VkStridedDeviceAddressRegionKHR callableRegion;

		VkDevice device;
	};
}