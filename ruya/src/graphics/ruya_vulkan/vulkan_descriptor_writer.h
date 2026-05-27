#pragma once
#include <deque>
#include <vector>

#include <volk/volk.h>

namespace ruya
{
	class VulkanContext;

	class VulkanDescriptorWriter
	{
	public:
		VulkanDescriptorWriter(VulkanContext* pVulkanContext);
		~VulkanDescriptorWriter() = default;

		VulkanDescriptorWriter(const VulkanDescriptorWriter&) = delete;
		VulkanDescriptorWriter& operator=(const VulkanDescriptorWriter&) = delete;

		void WriteDescriptorImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
		void WriteDescriptorSampler(int binding, VkSampler sampler, VkDescriptorType type);
		void WriteDescriptorImages(int binding, std::vector<VkDescriptorImageInfo> descriptorImageInfos, VkDescriptorType type);
		void WriteDescriptorBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);
		void WriteDescriptorBuffers(int binding, std::vector<VkDescriptorBufferInfo> descriptorBufferInfos, VkDescriptorType type);
		void WriteDescriptorAccelerationStructure(int binding, VkAccelerationStructureKHR accelerationStructureKHR);
		void UpdateDescriptors(VkDescriptorSet& pDescriptorSet);

		void WriteAndUpdateDescriptorImageByIndex(VkDescriptorSet descriptorSet, uint32_t index, VkDescriptorImageInfo& descriptorImageInfo);
		void WriteAndUpdateDescriptorBufferByIndex(VkDescriptorSet descriptorSet, uint32_t index, VkDescriptorBufferInfo& descriptorBufferInfo, VkDescriptorType descriptorType);

	private:
		std::deque<VkDescriptorImageInfo> imageInfos;
		std::deque<std::vector<VkDescriptorImageInfo>> multipleImageInfos;
		std::deque<VkDescriptorBufferInfo> bufferInfos;
		std::deque<std::vector<VkDescriptorBufferInfo>> meshBufferInfos;
		std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		std::vector<VkWriteDescriptorSetAccelerationStructureKHR> writeDescriptorSetAccelerationStructures;

		VkDevice device;
	};
}