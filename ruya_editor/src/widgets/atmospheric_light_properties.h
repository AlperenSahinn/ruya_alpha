#pragma once

#include "widget.h"

namespace editor
{
    class AtmosphericLightProperties : public Widget
    {
    public:
        AtmosphericLightProperties() = default;
        ~AtmosphericLightProperties() = default;

        AtmosphericLightProperties(const AtmosphericLightProperties&) = delete;
        AtmosphericLightProperties& operator=(const AtmosphericLightProperties&) = delete;

        void Draw() override;

        static bool visible;
    };
}