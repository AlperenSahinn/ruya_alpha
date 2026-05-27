#pragma once
#include <string>
#include <unordered_map>

#include <volk/volk.h>

#include "Widget.h"

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
		void RecreateDrawTextures();
		void DestroyDrawTextures();

		static bool isActive;
		std::unordered_map<std::string, std::vector<VkDescriptorSet>> renderTargetDescriptorSets;
		std::string selectedRenderTarget;
	};
}