#pragma once
#include <memory>
#include <vector>
#include <atomic>

#include "ruya_vulkan/vulkan_buffer.h"
#include "ruya_vulkan/vulkan_bottom_level_acceleration_structure.h"

#include "vertex.h"
#include "aabb.h"

namespace ruya
{
	class MeshBuffer
	{
	public:
		MeshBuffer() = default;
		~MeshBuffer() = default;

		MeshBuffer(const MeshBuffer&) = delete;
		MeshBuffer& operator=(const MeshBuffer&) = delete;

		void Load(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
		void Unload();

		VulkanBuffer* GetVertexBuffer() const { return vertexBuffer.get(); }
		VulkanBuffer* GetIndexBuffer() const { return indexBuffer.get(); }
		VulkanBottomLevelAccelerationStructure* GetBLAS() const { return blas.get(); }
		uint32_t GetIndexCount() const { return indexCount; }
		const AABB& GetAABB() const { return aabb; }

		void SetValid();
		void SetInvalid();
		bool IsValid();

	private:
		std::unique_ptr<VulkanBuffer> vertexBuffer;
		std::unique_ptr<VulkanBuffer> indexBuffer;
		std::unique_ptr<VulkanBottomLevelAccelerationStructure> blas;

		uint32_t indexCount;
		AABB aabb;

		std::atomic<bool> valid{ false };
	};
}