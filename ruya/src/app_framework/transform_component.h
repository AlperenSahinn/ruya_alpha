#pragma once
#include <glm/glm.hpp>
#include <cereal/archives/json.hpp>

namespace cereal
{
    template <class Archive>
    void serialize(Archive& archive, glm::vec3& vec)
    {
        archive(vec.x, vec.y, vec.z);
    }

    template <class Archive>
    void serialize(Archive& archive, glm::mat4& mat)
    {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                archive(mat[i][j]);
    }
}

namespace ruya
{
    struct TransformComponent
    {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f);
        glm::vec3 scale = glm::vec3(1.0f);
        glm::mat4 transform = glm::mat4(1.0f);
    };

    template<typename Archive>
    void serialize(Archive& archive, TransformComponent& component)
    {
        archive(component.position, component.rotation, component.scale, component.transform);
    }
}
