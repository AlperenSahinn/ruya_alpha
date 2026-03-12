#include "vulkan_descriptor_writer.h"

#include "vulkan_context.h"

ruya::VulkanDescriptorWriter::VulkanDescriptorWriter(VulkanContext* pVulkanContext)
{
	device = pVulkanContext->GetDevice();
}

void ruya::VulkanDescriptorWriter::WriteDescriptorImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type)
{
	VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo{
		.sampler = sampler,
		.imageView = image,
		.imageLayout = layout
		});

	VkWriteDescriptorSet writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstBinding = binding;
	writeDescriptorSet.dstSet = VK_NULL_HANDLE;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = type;
	writeDescriptorSet.pImageInfo = &info;

	writeDescriptorSets.push_back(writeDescriptorSet);
}

void ruya::VulkanDescriptorWriter::WriteDescriptorSampler(int binding, VkSampler sampler, VkDescriptorType type)
{
	VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo{
	.sampler = sampler,
		});

	VkWriteDescriptorSet writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstBinding = binding;
	writeDescriptorSet.dstSet = VK_NULL_HANDLE;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = type;
	writeDescriptorSet.pImageInfo = &info;

	writeDescriptorSets.push_back(writeDescriptorSet);
}

void ruya::VulkanDescriptorWriter::WriteDescriptorImages(int binding, std::vector<VkDescriptorImageInfo> descriptorImageInfos, VkDescriptorType type)
{
	std::vector< VkDescriptorImageInfo>& imageInfos = multipleImageInfos.emplace_back(descriptorImageInfos);

	VkWriteDescriptorSet writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstBinding = binding;
	writeDescriptorSet.dstSet = VK_NULL_HANDLE;
	writeDescriptorSet.descriptorCount = imageInfos.size();
	writeDescriptorSet.descriptorType = type;
	writeDescriptorSet.pImageInfo = imageInfos.data();

	writeDescriptorSets.push_back(writeDescriptorSet);
}

void ruya::VulkanDescriptorWriter::WriteDescriptorBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type)
{
	VkDescriptorBufferInfo& info = bufferInfos.emplace_back(VkDescriptorBufferInfo{
		.buffer = buffer,
		.offset = offset,
		.range = size
		});

	VkWriteDescriptorSet writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstBinding = binding;
	writeDescriptorSet.dstSet = VK_NULL_HANDLE;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = type;
	writeDescriptorSet.pBufferInfo = &info;

	writeDescriptorSets.push_back(writeDescriptorSet);
}

void ruya::VulkanDescriptorWriter::WriteDescriptorBuffers(int binding, std::vector<VkDescriptorBufferInfo> descriptorBufferInfos, VkDescriptorType type)
{
	std::vector<VkDescriptorBufferInfo>& infos = meshBufferInfos.emplace_back(descriptorBufferInfos);

	VkWriteDescriptorSet writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstBinding = binding;
	writeDescriptorSet.dstSet = VK_NULL_HANDLE;
	writeDescriptorSet.descriptorCount = infos.size();
	writeDescriptorSet.descriptorType = type;
	writeDescriptorSet.pBufferInfo = infos.data();

	writeDescriptorSets.push_back(writeDescriptorSet);
}

void ruya::VulkanDescriptorWriter::WriteDescriptorAccelerationStructure(int binding, VkAccelerationStructureKHR accelerationStructureKHR)
{
	VkWriteDescriptorSetAccelerationStructureKHR writeDescriptorSetAccelerationStructureKHR = {};
	writeDescriptorSetAccelerationStructureKHR.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	writeDescriptorSetAccelerationStructureKHR.accelerationStructureCount = 1;
	writeDescriptorSetAccelerationStructureKHR.pAccelerationStructures = &accelerationStructureKHR;

	writeDescriptorSetAccelerationStructures.push_back(writeDescriptorSetAccelerationStructureKHR);

	VkWriteDescriptorSet writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.pNext = &writeDescriptorSetAccelerationStructures.back();
	writeDescriptorSet.dstBinding = binding;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	writeDescriptorSets.push_back(writeDescriptorSet);
}

void ruya::VulkanDescriptorWriter::UpdateDescriptors(VkDescriptorSet pDescriptorSet)
{
	if (writeDescriptorSets.size() <= 0)
		return;

	for (VkWriteDescriptorSet& write : writeDescriptorSets)
	{
		write.dstSet = pDescriptorSet;
	}

	vkUpdateDescriptorSets(device, (uint32_t)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
}

void ruya::VulkanDescriptorWriter::WriteAndUpdateDescriptorImageByIndex(VkDescriptorSet descriptorSet, uint32_t index, VkDescriptorImageInfo& descriptorImageInfo)
{
	VkWriteDescriptorSet writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstSet = descriptorSet;
	writeDescriptorSet.dstBinding = 0;
	writeDescriptorSet.dstArrayElement = index;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescriptorSet.pImageInfo = &descriptorImageInfo;

	vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
}

void ruya::VulkanDescriptorWriter::WriteAndUpdateDescriptorBufferByIndex(VkDescriptorSet descriptorSet, uint32_t index, VkDescriptorBufferInfo& descriptorBufferInfo, VkDescriptorType descriptorType)
{
	VkWriteDescriptorSet writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstSet = descriptorSet;
	writeDescriptorSet.dstBinding = 0;
	writeDescriptorSet.dstArrayElement = index;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = descriptorType;
	writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;
	writeDescriptorSet.pImageInfo = nullptr;
	writeDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
}
