#pragma once
#include <unordered_map>

#include <core/uuid.h>

#include "widget.h"

namespace editor
{
    class MaterialEditor : public Widget
    {
    public:
    public:
        MaterialEditor() = default;
        ~MaterialEditor() = default;

        void Draw() override;
        static void OpenWindow(ruya::UUID materialID);

    private:
        static inline std::unordered_map<ruya::UUID, bool> openMaterialWindows;
    };
}