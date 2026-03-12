#include "vulkan_ray_tracing_pipeline.h"

#include <core/math_helpers.h>

#include "vulkan_shader.h"
#include "vulkan_context.h"
#include "vulkan_helpers.h"

ruya::VulkanRayTracingPipeline::VulkanRayTracingPipeline(
	VulkanContext* pVulkanContext,
	std::string rayGenerationShaderPath, 
	std::vector<std::string> rayMissShaderPaths,
	std::string anyHitShaderPath,
	std::string closestHitShaderPath, 
	std::vector<VulkanDescriptorSetLayout*>& descriptorSetLayouts)
{
	device = pVulkanContext->GetDevice();

	uint32_t stageCount = 1;

	bool isMissShaderProvided = false;
	bool isAnyHitShaderProvided = false;;
	bool isClosestHitShaderProvided = false;;

	std::unique_ptr<VulkanShader> rayGenerationShader = std::make_unique<VulkanShader>(pVulkanContext, rayGenerationShaderPath.c_str());
	std::vector<std::unique_ptr<VulkanShader>> rayMissShaders;
	std::unique_ptr<VulkanShader> anyHitShader;
	std::unique_ptr<VulkanShader> closestHitShader;

	if (!rayMissShaderPaths.empty())
	{
		isMissShaderProvided = true;
		stageCount += rayMissShaderPaths.size();

		for (int i = 0; i < rayMissShaderPaths.size(); i++)
		{
			rayMissShaders.push_back(std::make_unique<VulkanShader>(pVulkanContext, rayMissShaderPaths[i].c_str()));
		}
	}

	if (!anyHitShaderPath.empty())
	{
		isAnyHitShaderProvided = true;
		stageCount++;
		anyHitShader = std::make_unique<VulkanShader>(pVulkanContext, anyHitShaderPath.c_str());
	}

	if (!closestHitShaderPath.empty())
	{
		isClosestHitShaderProvided = true;
		stageCount++;
		closestHitShader = std::make_unique<VulkanShader>(pVulkanContext, closestHitShaderPath.c_str());
	}

	std::vector<VkPipelineShaderStageCreateInfo> stages;

	VkPipelineShaderStageCreateInfo rayGenerationStage = {};
	rayGenerationStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	rayGenerationStage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	rayGenerationStage.module = rayGenerationShader->GetDeviceHandle();
	rayGenerationStage.pName = "main";
	stages.push_back(rayGenerationStage);

	for (int i = 0; i < rayMissShaders.size(); i++)
	{
		VkPipelineShaderStageCreateInfo rayMissStage = {};
		rayMissStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		rayMissStage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
		rayMissStage.module = rayMissShaders[i]->GetDeviceHandle();
		rayMissStage.pName = "main";
		stages.push_back(rayMissStage);
	}

	if (isAnyHitShaderProvided)
	{
		VkPipelineShaderStageCreateInfo anyHitStage = {};
		anyHitStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		anyHitStage.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		anyHitStage.module = anyHitShader->GetDeviceHandle();
		anyHitStage.pName = "main";
		stages.push_back(anyHitStage);
	}

	if (isClosestHitShaderProvided)
	{
		VkPipelineShaderStageCreateInfo closestHitStage = {};
		closestHitStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		closestHitStage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		closestHitStage.module = closestHitShader->GetDeviceHandle();
		closestHitStage.pName = "main";
		stages.push_back(closestHitStage);
	}

	VkRayTracingShaderGroupCreateInfoKHR rayGenerationGroup = {};
	rayGenerationGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	rayGenerationGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	rayGenerationGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
	rayGenerationGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
	rayGenerationGroup.generalShader = 0;
	rayGenerationGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
	shaderGroups.push_back(rayGenerationGroup);

	for (int i = 0; i < rayMissShaders.size(); i++)
	{
		VkRayTracingShaderGroupCreateInfoKHR rayMissGroup = {};
		rayMissGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		rayMissGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		rayMissGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
		rayMissGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
		rayMissGroup.generalShader = 1 + i;
		rayMissGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
		shaderGroups.push_back(rayMissGroup);
	}

	if (isAnyHitShaderProvided || isClosestHitShaderProvided)
	{
		VkRayTracingShaderGroupCreateInfoKHR hitGroup = {};
		hitGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		hitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;

		uint32_t currentIndex = 1 + rayMissShaders.size();

		if (isAnyHitShaderProvided) {
			hitGroup.anyHitShader = currentIndex++;
		}
		else {
			hitGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
		}

		if (isClosestHitShaderProvided) {
			hitGroup.closestHitShader = currentIndex;
		}
		else {
			hitGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
		}

		hitGroup.generalShader = VK_SHADER_UNUSED_KHR;
		hitGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

		shaderGroups.push_back(hitGroup);
	}

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = VK_NULL_HANDLE;

	std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts;

	for (VulkanDescriptorSetLayout* layout : descriptorSetLayouts)
	{
		vkDescriptorSetLayouts.push_back(layout->GetDeviceHandle());
	}

	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(vkDescriptorSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = vkDescriptorSetLayouts.data();
	CHECK_VKRESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

	VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCreateInfo = {};
	rayTracingPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rayTracingPipelineCreateInfo.stageCount = static_cast<uint32_t>(stages.size());
	rayTracingPipelineCreateInfo.pStages = stages.data();
	rayTracingPipelineCreateInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
	rayTracingPipelineCreateInfo.pGroups = shaderGroups.data();

	rayTracingPipelineCreateInfo.maxPipelineRayRecursionDepth = 17;
	rayTracingPipelineCreateInfo.layout = pipelineLayout;

	CHECK_VKRESULT(vkCreateRayTracingPipelinesKHR(device, {}, {}, 1, &rayTracingPipelineCreateInfo, nullptr, &pipeline));

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR physicalDeviceRayTracingPipelineProperties = {};
	physicalDeviceRayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

	VkPhysicalDeviceProperties2 physicalDeviceProperties2 = {};
	physicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	physicalDeviceProperties2.pNext = &physicalDeviceRayTracingPipelineProperties;

	vkGetPhysicalDeviceProperties2(pVulkanContext->GetPhysicalDevice(), &physicalDeviceProperties2);

	uint32_t missCount = rayMissShaders.size();
	uint32_t hitCount = (isAnyHitShaderProvided || isClosestHitShaderProvided) ? 1 : 0;
	uint32_t callCount = 0;
	auto handleCount = 1 + missCount + hitCount;
	uint32_t handleSize = physicalDeviceRayTracingPipelineProperties.shaderGroupHandleSize;

	uint32_t handleSizeAligned = math_helpers::AlignUp(handleSize, physicalDeviceRayTracingPipelineProperties.shaderGroupHandleAlignment);

	rayGenerationRegion.stride = math_helpers::AlignUp(handleSizeAligned, physicalDeviceRayTracingPipelineProperties.shaderGroupBaseAlignment);
	rayGenerationRegion.size = rayGenerationRegion.stride;
	rayMissRegion.stride = handleSizeAligned;
	rayMissRegion.size = math_helpers::AlignUp(missCount * handleSizeAligned, physicalDeviceRayTracingPipelineProperties.shaderGroupBaseAlignment);
	rayHitRegion.stride = handleSizeAligned;
	rayHitRegion.size = math_helpers::AlignUp(hitCount * handleSizeAligned, physicalDeviceRayTracingPipelineProperties.shaderGroupBaseAlignment);
	callableRegion.stride = 0;
	callableRegion.size = 0;
	callableRegion.deviceAddress = 0;

	uint32_t dataSize = handleCount * handleSize;
	std::vector<uint8_t> handles(dataSize);
	CHECK_VKRESULT(vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, 0, handleCount, dataSize, handles.data()));

	VkDeviceSize sbtSize = rayGenerationRegion.size + rayMissRegion.size + rayHitRegion.size + callableRegion.size;

	shaderBindingTableBuffer = std::make_unique<VulkanBuffer>(
		pVulkanContext,
		sbtSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
		VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU
	);

	VkBufferDeviceAddressInfo bufferDeviceAddressInfo = {};
	bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferDeviceAddressInfo.buffer = shaderBindingTableBuffer->GetDeviceHandle();

	VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(device, &bufferDeviceAddressInfo);
	rayGenerationRegion.deviceAddress = sbtAddress;
	rayMissRegion.deviceAddress = sbtAddress + rayGenerationRegion.size;
	rayHitRegion.deviceAddress = sbtAddress + rayGenerationRegion.size + rayMissRegion.size;

	auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

	uint8_t* pSBTBuffer = static_cast<uint8_t*>(shaderBindingTableBuffer->MapMemory());

	uint8_t* pData{ nullptr };
	uint32_t handleIdx{ 0 };

	pData = pSBTBuffer;
	memcpy(pData, getHandle(handleIdx++), handleSize);

	pData = pSBTBuffer + rayGenerationRegion.size;
	for (uint32_t c = 0; c < missCount; c++)
	{
		memcpy(pData, getHandle(handleIdx++), handleSize);
		pData += rayMissRegion.stride;
	}

	pData = pSBTBuffer + rayGenerationRegion.size + rayMissRegion.size;
	for (uint32_t c = 0; c < hitCount; c++)
	{
		memcpy(pData, getHandle(handleIdx++), handleSize);
		pData += rayHitRegion.stride;
	}

	shaderBindingTableBuffer->UnmapMemory();
}

ruya::VulkanRayTracingPipeline::~VulkanRayTracingPipeline()
{
	shaderBindingTableBuffer.reset();
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
}

VkPipeline ruya::VulkanRayTracingPipeline::GetDeviceHandle() const
{
	return pipeline;
}

VkPipelineLayout ruya::VulkanRayTracingPipeline::GetPipelineLayout() const
{
	return pipelineLayout;
}

const VkStridedDeviceAddressRegionKHR& ruya::VulkanRayTracingPipeline::GetRayGenerationRegion() const
{
	return rayGenerationRegion;
}

const VkStridedDeviceAddressRegionKHR& ruya::VulkanRayTracingPipeline::GetMissRegion() const
{
	return rayMissRegion;
}

const VkStridedDeviceAddressRegionKHR& ruya::VulkanRayTracingPipeline::GetHitRegion() const
{
	return rayHitRegion;
}

const VkStridedDeviceAddressRegionKHR& ruya::VulkanRayTracingPipeline::GetCallableRegion() const
{
	return callableRegion;
}
