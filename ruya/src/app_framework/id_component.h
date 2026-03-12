#pragma once
#include <string>

#include <cereal/types/string.hpp>
#include <cereal/archives/json.hpp>
#include <entt/entt.hpp>

namespace ruya
{
	struct IdComponent
	{
		std::string name = "New Entity";
		EntityID enttID{ entt::null };
	};

	template<typename Archive>
	void serialize(Archive& archive, IdComponent& component)
	{
		archive(component.name, component.enttID);
	}
}