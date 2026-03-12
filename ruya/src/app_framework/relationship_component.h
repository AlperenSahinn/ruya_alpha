#pragma once
#include <cereal/archives/json.hpp>
#include <entt/entt.hpp>

namespace ruya
{
    struct RelationshipComponent
    {
        EntityID parentEntity{ entt::null };
        EntityID firstChildEntity{ entt::null };
        EntityID nextEntity{ entt::null };
    };

    template<typename Archive>
    void serialize(Archive& archive, RelationshipComponent& component)
    {
        archive(component.parentEntity, component.firstChildEntity, component.nextEntity);
    }
}