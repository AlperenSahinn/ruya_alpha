#pragma once
#include <cstdint>

#include <cereal/archives/json.hpp>

#include <core/ry_id.h>

namespace ruya
{
	struct RenderComponent
	{
		RyID renderGeometryID;
		RyID meshID;
		RyID materialID;
		bool draw = true;
	};

	template<typename Archive>
	void serialize(Archive& archive, RenderComponent& component)
	{
		archive(component.renderGeometryID, component.meshID, component.materialID, component.draw);
	}
}
