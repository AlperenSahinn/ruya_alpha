#pragma once
#include <glm/glm.hpp>
#include <nlohmann_json/json.hpp>

#include <core/nlohmann_glm.h>

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

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Vertex, position, normal, tangent, bitangent, texCoord)
    };
}