#pragma once
#include <core/uuid.h>
#include <nlohmann_json/json.hpp>

namespace ruya
{
	struct RyMaterial
	{
		UUID albedoUUID = UUID::Invalid();
		UUID normalUUID = UUID::Invalid();
		UUID metallicRoughnessUUID = UUID::Invalid();

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(RyMaterial, albedoUUID, normalUUID, metallicRoughnessUUID)
	};
}