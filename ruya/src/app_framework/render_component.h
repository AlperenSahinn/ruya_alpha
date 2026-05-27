#pragma once
#include <cstdint>

#include <nlohmann_json/json.hpp>

#include <core/ry_id.h>
#include <core/uuid.h>

namespace ruya
{
	struct RenderComponent
	{
		UUID meshUUID = UUID::Invalid();
		UUID materialUUID = UUID::Invalid();
		bool draw = true;

		RyID renderItemID = RyID::Invalid();
		bool allocated = false;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(RenderComponent, meshUUID, materialUUID, draw)
	};
}