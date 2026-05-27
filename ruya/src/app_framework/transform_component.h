#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann_json/json.hpp> 

#include <core/nlohmann_glm.h> 

namespace ruya
{
    struct TransformComponent
    {
        glm::vec3 position = glm::vec3(0.0f);
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 scale = glm::vec3(1.0f);
        glm::mat4 transform = glm::mat4(1.0f);

        glm::vec3 updatePosition = glm::vec3(0.0f);
        glm::quat updateRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 updateScale = glm::vec3(1.0f);
        glm::mat4 updateTransform = glm::mat4(1.0f);

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(TransformComponent, position, rotation, scale)
    };
}