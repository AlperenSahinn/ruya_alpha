#pragma once
#include <vector>
#include <memory>
#include <string>

#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_sdl3.h>

namespace editor
{
    // =================================================================================
    // Icon / text helpers
    // =================================================================================

    inline const char* IconStr(const char8_t* utf8Icon)
    {
        return reinterpret_cast<const char*>(utf8Icon);
    }

    inline const char* IconLabel(const char8_t* utf8Icon, const char* label)
    {
        thread_local char buffer[256];
        snprintf(buffer, sizeof(buffer), "%s %s",
            reinterpret_cast<const char*>(utf8Icon), label);
        return buffer;
    }

    inline void TextColored(const ImVec4& color, const char* text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }

    inline void IconTextColored(const ImVec4& iconColor, const char8_t* icon, const char* text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, iconColor);
        ImGui::TextUnformatted(reinterpret_cast<const char*>(icon));
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 4.0f);
        ImGui::TextUnformatted(text);
    }


    // =================================================================================
    // Layout
    // =================================================================================

    void HelpMarker(const char* description);
    void LabeledSeparator(const char* label);

    void BeginPropertyTable(std::initializer_list<const char*> labels);
    void EndPropertyTable();

    bool PropertyRow(const char* label, const char* tooltip = nullptr);

    bool BeginPropertyRow(const char* label, const char* tooltip = nullptr);
    void EndPropertyRow();


    // =================================================================================
    // Component / section header
    // =================================================================================
    bool ComponentHeader(const char8_t* icon,
        const char* label,
        const ImVec4& accentColor,
        bool* outRemoveRequested = nullptr,
        bool canRemove = false,
        bool defaultOpen = true);


    // =================================================================================
    // Vec3 control
    // =================================================================================
    bool DragVec3(const char* label,
        float* values,
        float resetValue = 0.0f,
        float speed = 0.1f,
        float vMin = 0.0f,
        float vMax = 0.0f,
        const char* format = "%.3f");


    // =================================================================================
    // Single-value rows
    // =================================================================================
    bool DragFloat(const char* label,
        float* value,
        float speed = 0.1f,
        float vMin = 0.0f,
        float vMax = 0.0f,
        const char* format = "%.3f",
        const char* tooltip = nullptr);

    bool DragInt(const char* label,
        int* value,
        float speed = 1.0f,
        int vMin = 0,
        int vMax = 0,
        const char* tooltip = nullptr);

    bool ColorEdit3(const char* label, float* color, const char* tooltip = nullptr);
    bool ColorEdit4(const char* label, float* color, const char* tooltip = nullptr);

    bool Checkbox(const char* label, bool* value, const char* tooltip = nullptr);

    bool Combo(const char* label,
        int* current,
        const char* const items[],
        int itemCount,
        const char* tooltip = nullptr);

    bool InputText(const char* label,
        char* buffer,
        size_t bufferSize,
        const char* tooltip = nullptr);

    void TextRow(const char* label, const char* value);


    // =================================================================================
    // Asset slot
    // =================================================================================
    bool AssetSlot(const char* label,
        const char* assetDisplayName,
        bool hasAsset,
        const char* placeholder = "None  (drop here)");


    // =================================================================================
    // Status indicator
    // =================================================================================
    void StatusIndicator(const char* label, bool active,
        const char* activeText = "Active",
        const char* inactiveText = "Inactive");


    // =================================================================================
    // Banner
    // =================================================================================
    void BeginBanner(float height = 56.0f);
    void EndBanner();


    // =================================================================================
    // Primary button
    // =================================================================================
    bool PrimaryButton(const char* label,
        const ImVec4& accent,
        float widthRatio = 0.8f);


    // =================================================================================
    // Icon toggle button
    // =================================================================================
    bool IconToggleButton(const char* id,
        const char8_t* iconOn,
        const char8_t* iconOff,
        bool state,
        float size = 0.0f,
        const char* tooltip = nullptr);


    // =================================================================================
    // Toolbar primitives
    // =================================================================================
    bool ToolButton(const char8_t* icon,
        const char* tooltip,
        bool active,
        float size = 0.0f);

    void ToolbarSeparator();


    // =================================================================================
    // Empty-state placeholder
    // =================================================================================
    void EmptyState(const char8_t* bigIcon, const char* message, const char* hint = nullptr);
}