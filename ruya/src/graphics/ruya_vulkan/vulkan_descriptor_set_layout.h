#pragma once
#include <vector>

#include <volk/volk.h>

namespace ruya
{
	class VulkanContext;

	class VulkanDescriptorSetLayout
	{
	public:
		VulkanDescriptorSetLayout(VulkanContext* pVulkanContext);
		~VulkanDescriptorSetLayout();

		VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout&) = delete;
		VulkanDescriptorSetLayout& operator=(const VulkanDescriptorSetLayout&) = delete;

		void AddBinding(uint32_t binding, VkDescriptorType type, uint32_t count);
		void Build(VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags descriptorSetLayoutCreateFlags = 0);

		VkDescriptorSetLayout GetDeviceHandle() const;

	private:
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		VkDescriptorSetLayout deviceHandle;
		VkDevice device;
	};
}