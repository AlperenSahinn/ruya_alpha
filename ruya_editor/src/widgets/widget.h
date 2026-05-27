#pragma once
#include <vector>
#include <memory>

#include <imgui/imgui.h>

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
            pChildren.push_back(std::move(widget));
        }

    private:
        std::vector<std::unique_ptr<Widget>> pChildren;
    };
}