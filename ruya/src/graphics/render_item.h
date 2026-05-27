#pragma once
#include <atomic>
#include <memory>

#include <glm/glm.hpp>
#include <core/ry_id.h>
#include "ruya_vulkan/vulkan_buffer.h"
#include "ruya_vulkan/vulkan_top_level_acceleration_structure.h"

namespace ruya
{
	struct RenderItemBuffer
	{
		glm::mat4 transform;
		glm::mat4 transformInverse;
		glm::mat4 prevTransform;
		VkDeviceAddress vertexBufferAddress;
		VkDeviceAddress indexBufferAddress;
		uint32_t materialDescriptorIndex;
		uint32_t pad[3];
	};

	class RenderItem
	{

	public:
		RenderItem() = default;
		RenderItem(const glm::mat4& transform, RyID meshBufferRyID, RyID materialRyID, bool draw);
		~RenderItem() = default;

		RenderItem(const RenderItem&) = delete;
		RenderItem& operator=(const RenderItem&) = delete;

		RenderItem(RenderItem&& other) noexcept :
			renderItemBufferHosts(std::move(other.renderItemBufferHosts)),
			renderItemBufferDevice(std::move(other.renderItemBufferDevice)),
			transform(std::move(other.transform)),
			meshBufferID(std::move(other.meshBufferID)),
			materialID(std::move(other.materialID)),
			draw(std::move(other.draw)),
			valid(other.valid.load(std::memory_order_relaxed))
		{
			other.valid.store(false, std::memory_order_relaxed);
		}

		void Load();
		void Unload();

		bool GetVisibility() { return draw; }
		void SetVisibility(bool b) { draw = b; }

		void UpdateTransform(const glm::mat4& transform);
		void UpdateMaterial(RyID materialRyID);

		VulkanBuffer* GetRenderItemBufferDevice() { return renderItemBufferDevice.get(); }

		RyID GetMeshBufferRyID() const { return meshBufferID; }
		glm::mat4 GetTransform() { return transform; }
		RyID GetRenderMaterialRyID() const { return materialID; }

		void SetValid();
		void SetInvalid();
		bool IsValid();

	private:
		std::vector<std::unique_ptr<VulkanBuffer>> renderItemBufferHosts;
		std::unique_ptr<VulkanBuffer> renderItemBufferDevice;
		glm::mat4 transform;
		RyID meshBufferID;
		RyID materialID;
		bool draw;
		std::atomic<bool> valid{ false };
	};
}