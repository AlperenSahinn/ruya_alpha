#include "mesh.h"

#include "engine.h"
#include <core/log.h>

#include "ruya_vulkan/vulkan_command_buffer.h"

ruya::Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
	this->vertices = vertices;
	this->indices = indices;
}

ruya::Mesh::~Mesh()
{

}

ruya::VulkanBuffer* ruya::Mesh::GetVertexBuffer() const
{
	return vertexBuffer.get();
}

ruya::VulkanBuffer* ruya::Mesh::GetIndexBuffer() const
{
	return indexBuffer.get();
}

uint32_t ruya::Mesh::GetVertexCount() const
{
	return static_cast<uint32_t>(vertices.size());
}

uint32_t ruya::Mesh::GetIndexCount() const
{
	return static_cast<uint32_t>(indices.size());
}

ruya::VulkanBottomLevelAccelerationStructure* ruya::Mesh::GetBLAS() const
{
	return blas.get();
}

void ruya::Mesh::Load()
{
	size_t vertexBufferSize = vertices.size() * sizeof(Vertex);

	std::unique_ptr<VulkanBuffer> vertexUploadBuffer = std::make_unique<VulkanBuffer>(engine->GetGraphics()->GetVulkanContext(), vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	vertexUploadBuffer->UploadData(vertices.data(), vertexBufferSize);

	vertexBuffer = std::make_unique<VulkanBuffer>(
		engine->GetGraphics()->GetVulkanContext(),
		vertexBufferSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VMA_MEMORY_USAGE_GPU_ONLY);

	engine->GetGraphics()->GetVulkanContext()->ImmediateSubmitCommand([&](VulkanCommandBuffer* commandBuffer)
		{
			VkBufferCopy bufferCopy = {};
			bufferCopy.size = vertexBufferSize;
			bufferCopy.srcOffset = 0;
			bufferCopy.dstOffset = 0;

			commandBuffer->CopyBufferToBuffer(*vertexUploadBuffer, vertexBuffer.get(), bufferCopy);
		}
	);

	size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	std::unique_ptr<VulkanBuffer> indexUploadBuffer = std::make_unique<VulkanBuffer>(engine->GetGraphics()->GetVulkanContext(), indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	indexUploadBuffer->UploadData(indices.data(), indexBufferSize);

	indexBuffer = std::make_unique<VulkanBuffer>(
		engine->GetGraphics()->GetVulkanContext(),
		indexBufferSize,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VMA_MEMORY_USAGE_GPU_ONLY);

	engine->GetGraphics()->GetVulkanContext()->ImmediateSubmitCommand([&](VulkanCommandBuffer* commandBuffer)
		{
			VkBufferCopy bufferCopy = {};
			bufferCopy.size = indexBufferSize;
			bufferCopy.srcOffset = 0;
			bufferCopy.dstOffset = 0;

			commandBuffer->CopyBufferToBuffer(*indexUploadBuffer, indexBuffer.get(), bufferCopy);
		}
	);

	blas = std::make_unique<VulkanBottomLevelAccelerationStructure>(engine->GetGraphics()->GetVulkanContext(), *vertexBuffer, vertices.size(), sizeof(Vertex), *indexBuffer, indices.size());
}

void ruya::Mesh::Unload()
{
	blas.reset();
	vertexBuffer.reset();
	indexBuffer.reset();
}