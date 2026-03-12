#pragma once

#include "widget.h"

namespace editor
{
	class DirectionalLightProperties : public Widget
	{
	public:
		DirectionalLightProperties() = default;
		~DirectionalLightProperties() = default;

		DirectionalLightProperties(const DirectionalLightProperties&) = delete;
		DirectionalLightProperties& operator=(const DirectionalLightProperties&) = delete;

		void Draw() override;

		static bool visible;
	};
}