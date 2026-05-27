#pragma once
#include <glm/glm.hpp>
#include <nlohmann_json/json.hpp> 

#include <core/ry_id.h>
#include <physics/physics.h>

#include <core/nlohmann_glm.h>

namespace ruya
{
    struct PhysicsComponent
    {
        glm::vec3 centerPosition = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 halfWidhtHeightDepth = glm::vec3(0.5f, 0.5f, 0.5f);
        PhysicsMotionType motionType = PhysicsMotionType::Static;

        RyID riggidBodyRyID = RyID::Invalid();
        bool allocated = false;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(PhysicsComponent, centerPosition, halfWidhtHeightDepth, motionType)
    };
}