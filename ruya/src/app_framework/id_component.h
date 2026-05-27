#pragma once
#include <string>
#include <nlohmann_json/json.hpp>

namespace ruya
{
	struct IdComponent
	{
		std::string name = "New Entity";
		EntityID enttID{ entt::null };

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(IdComponent, name, enttID)
	};
}