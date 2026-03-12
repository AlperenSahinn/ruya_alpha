#pragma once

#include "Widget.h"

#include <volk/volk.h>

namespace editor
{
	class Viewport : public Widget
	{
	public:
		Viewport();
		~Viewport();

		Viewport(const Viewport&) = delete;
		Viewport& operator=(const Viewport&) = delete;

		void Draw() override;

		static bool IsActive();

	private:
		std::vector<VkDescriptorSet> drawTextures;
		static bool isActive;
	};
}