#pragma once
#include "widget.h"

#include "toolbar.h"
#include "status_bar.h"

namespace editor
{
	class DockSpace : public Widget
	{
	public:
		DockSpace() = default;
		~DockSpace() = default;

		DockSpace(const DockSpace&) = delete;
		DockSpace& operator=(const DockSpace&) = delete;

		void Draw() override;

	private:
		Toolbar   toolbar;
		StatusBar statusBar;
	};
}
