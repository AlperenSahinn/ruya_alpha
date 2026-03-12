#pragma once
#include <cstdint>

#include <cereal/archives/json.hpp>

#include <core/ry_id.h>

namespace ruya
{
	struct Model3DComponent
	{
		RyID modelAssetID;
		bool isLoaded = false;
	};

	template<typename Archive>
	void serialize(Archive& archive, Model3DComponent& component)
	{
		archive(component.modelAssetID);
	}
}
