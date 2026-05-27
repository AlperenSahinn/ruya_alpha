#pragma once

#include <app_framework/scene.h>

#include "widget.h"

namespace editor
{
    class Properties : public Widget
    {
    public:
        Properties() = default;
        ~Properties() = default;

        Properties(const Properties&) = delete;
        Properties& operator=(const Properties&) = delete;

        void Draw() override;

    private:
        void UpdateTransformRecursive(ruya::EntityID entity);
    };
}