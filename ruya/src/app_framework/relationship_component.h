#pragma once
#include <nlohmann_json/json.hpp>
#include <entt/entt.hpp>

#include "entity_id.h"

namespace ruya
{
    struct RelationshipComponent
    {
        EntityID parentEntity{ entt::null };
        EntityID firstChildEntity{ entt::null };
        EntityID nextEntity{ entt::null };

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(RelationshipComponent, parentEntity, firstChildEntity, nextEntity)
    };
}