#pragma once
#include <vector>
#include <memory>

#include <glm/glm.hpp>

#include "ruya_vulkan/vulkan_buffer.h"
#include "ruya_vulkan/vulkan_bottom_level_acceleration_structure.h"

#include "graphics_resource.h"

namespace ruya
{
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec3 bitangent;
        glm::vec2 texCoord;
        glm::vec2 pad;
    };

    class Mesh : public IGraphicsResource
    {
    public:
        Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
        ~Mesh();

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        Mesh(Mesh&&) = default;
        Mesh& operator=(Mesh&&) = default;

        VulkanBuffer* GetVertexBuffer() const;
        VulkanBuffer* GetIndexBuffer() const;

        uint32_t GetVertexCount() const;
        uint32_t GetIndexCount() const;

        VulkanBottomLevelAccelerationStructure* GetBLAS() const;

        void Load() override;
        void Unload() override;

    private:
        std::vector<Vertex> vertices; 
        std::vector<uint32_t> indices;
        std::unique_ptr<VulkanBuffer> vertexBuffer;
        std::unique_ptr<VulkanBuffer> indexBuffer;
        std::unique_ptr<VulkanBottomLevelAccelerationStructure> blas;
    };
}