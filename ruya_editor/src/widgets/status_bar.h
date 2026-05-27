#pragma once
#include "widget.h"

namespace editor
{
	class StatusBar : public Widget
	{
	public:
		StatusBar() = default;
		~StatusBar() = default;

		StatusBar(const StatusBar&) = delete;
		StatusBar& operator=(const StatusBar&) = delete;

		void Draw() override {}

		void DrawContents();
	};
}
