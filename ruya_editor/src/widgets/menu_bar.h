#pragma once
#include "widget.h"

namespace editor
{
	class MenuBar : public Widget
	{
	public:
		MenuBar() = default;
		~MenuBar() = default;

		MenuBar(const MenuBar&) = delete;
		MenuBar& operator=(const MenuBar&) = delete;

		void Draw() override;

	private:
		float timer = 0.0f;
		float cachedMs = 0.0f;
		float cachedFps = 0.0f;
		char buffer[64] = {};

		static constexpr int kFpsHistorySize = 60;
		float fpsHistory[kFpsHistorySize] = {};
		int fpsHistoryIndex = 0;

		bool stepRequested = false;
	};
}