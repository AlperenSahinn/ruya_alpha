#pragma once
#include <vector>
#include <memory>

#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_sdl3.h>

namespace editor
{
	class Widget
	{
	public:
		Widget() = default;
		virtual ~Widget() = default;

		Widget(const Widget&) = delete;
		Widget& operator=(const Widget&) = delete;

		virtual void Draw() = 0;

		void AddChild(std::unique_ptr<Widget> widget)
		{
			pChilderen.push_back(std::move(widget));
		}

	private:
		std::vector<std::unique_ptr<Widget>> pChilderen;
	};
}