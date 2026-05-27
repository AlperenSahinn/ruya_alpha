#include "mesh_buffer.h"

#include <engine.h>

void ruya::MeshBuffer::Load(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
{
    auto* ctx = engine->GetGraphics()->GetVulkanContext();

    size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    auto vertexUploadBuffer = std::make_unique<VulkanBuffer>(
        ctx, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    vertexUploadBuffer->UploadData(vertices.data(), vertexBufferSize);

    auto indexUploadBuffer = std::make_unique<VulkanBuffer>(
        ctx, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    indexUploadBuffer->UploadData(indices.data(), indexBufferSize);

    vertexBuffer = std::make_unique<VulkanBuffer>(
        ctx, vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_GPU_ONLY);

    indexBuffer = std::make_unique<VulkanBuffer>(
        ctx, indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_GPU_ONLY);

    ctx->ImmediateSubmitCommand([&](VulkanCommandBuffer* cb)
        {
            VkBufferCopy vbCopy = { 0, 0, vertexBufferSize };
            cb->CopyBufferToBuffer(*vertexUploadBuffer, vertexBuffer.get(), vbCopy);

            VkBufferCopy ibCopy = { 0, 0, indexBufferSize };
            cb->CopyBufferToBuffer(*indexUploadBuffer, indexBuffer.get(), ibCopy);
        });

    blas = std::make_unique<VulkanBottomLevelAccelerationStructure>(
        ctx, *vertexBuffer, vertices.size(), sizeof(Vertex), *indexBuffer, indices.size());

    indexCount = indices.size();

    if (!vertices.empty()) {
        aabb.min = glm::vec3(std::numeric_limits<float>::max());
        aabb.max = glm::vec3(std::numeric_limits<float>::lowest());
        for (const auto& vertex : vertices) {
            aabb.min = glm::min(aabb.min, vertex.position);
            aabb.max = glm::max(aabb.max, vertex.position);
        }
    }
    else {
        aabb.min = aabb.max = glm::vec3(0.0f);
    }
}

void ruya::MeshBuffer::Unload()
{
	blas.reset();
	indexBuffer.reset();
	vertexBuffer.reset();
}

void ruya::MeshBuffer::SetValid()
{
    valid.store(true, std::memory_order_release);
}

void ruya::MeshBuffer::SetInvalid()
{
    valid.store(false, std::memory_order_release);
}

bool ruya::MeshBuffer::IsValid()
{
    return valid.load(std::memory_order_acquire);
}