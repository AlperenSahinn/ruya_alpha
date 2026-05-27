#pragma once
#include <cstdint>
#include <atomic>

#include <core/ry_id.h>

#include "ruya_vulkan/vulkan_buffer.h"

namespace ruya
{
	class RenderMaterial
	{
		struct RenderMaterialBuffer
		{
			uint32_t albedoTextureDescriptorIndex;
			uint32_t normalTextureDescriptorIndex;
			uint32_t metallicRoughnessTextureDescriptorIndex;
			uint32_t pad;
		};

	public:
		RenderMaterial(RyID albedoTextureID, RyID normalTextureID, RyID metallicRoughnessTextureID);
		~RenderMaterial() = default;

		RenderMaterial(const RenderMaterial&) = delete;
		RenderMaterial& operator=(const RenderMaterial&) = delete;

		void Load();
		void Unload();

		VulkanBuffer* GetRenderMaterialBuffer() { return renderMaterialBuffer.get(); }
		RyID GetAlbedoTexture2DRyID() { return albedoTextureID; }
		RyID GetNormalTexture2DRyID() { return normalTextureID; }
		RyID GetMetallicRoughnessTexture2DRyID() { return metallicRoughnessTextureID; }

		void SetValid();
		void SetInvalid();
		bool IsValid();

	private:
		std::unique_ptr<VulkanBuffer> renderMaterialBuffer;
		RyID albedoTextureID;
		RyID normalTextureID;
		RyID metallicRoughnessTextureID;

		std::atomic<bool> valid{ false };
	};
}