#include "editor_widgets.h"
#include "editor_style.h"

#include <cstdio>
#include <cstring>
#include <cfloat>
#include <initializer_list>

extern ImFont* g_FontBold;
extern ImFont* g_FontLarge;

namespace editor
{
    // ─────────────────────────────────────────────────────────────────────────────
    // Internal property-table state
    // ─────────────────────────────────────────────────────────────────────────────
    namespace
    {
        thread_local bool  s_propTableOpen = false;
        thread_local float s_propLabelColW = 60.0f;

        static constexpr float kLabelPadding = 10.0f;
        static constexpr float kMinLabelW = 30.0f;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // HelpMarker / LabeledSeparator
    // ─────────────────────────────────────────────────────────────────────────────
    void HelpMarker(const char* description)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
            ImGui::TextUnformatted(description);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    void LabeledSeparator(const char* label)
    {
        ImGui::Spacing();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float width = ImGui::GetContentRegionAvail().x;
        float textWidth = ImGui::CalcTextSize(label).x;
        float lineY = pos.y + ImGui::GetTextLineHeight() * 0.5f;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(0.35f, 0.35f, 0.40f, 1.0f));
        dl->AddLine(ImVec2(pos.x, lineY), ImVec2(pos.x + 8.0f, lineY), col);
        dl->AddLine(ImVec2(pos.x + 16.0f + textWidth, lineY), ImVec2(pos.x + width, lineY), col);

        ImGui::SetCursorScreenPos(ImVec2(pos.x + 12.0f, pos.y));
        ImGui::TextDisabled("%s", label);
        ImGui::Spacing();
    }


    // ─────────────────────────────────────────────────────────────────────────────
    // Property table
    // ─────────────────────────────────────────────────────────────────────────────
    void BeginPropertyTable(std::initializer_list<const char*> labels)
    {
        float maxW = kMinLabelW;
        for (const char* l : labels)
        {
            float w = ImGui::CalcTextSize(l).x;
            if (w > maxW) maxW = w;
        }
        s_propLabelColW = maxW + kLabelPadding;
        s_propTableOpen = true;

        ImGui::BeginTable("##PT", 2,
            ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingFixedFit);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, s_propLabelColW);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    }

    void EndPropertyTable()
    {
        if (s_propTableOpen)
        {
            ImGui::EndTable();
            s_propTableOpen = false;
        }
    }

    bool PropertyRow(const char* label, const char* tooltip)
    {
        if (!s_propTableOpen) return false;

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        if (tooltip) HelpMarker(tooltip);

        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);

        ImGui::PushID(label);
        return true;
    }


    // ─────────────────────────────────────────────────────────────────────────────
    // Legacy single-row wrappers
    // ─────────────────────────────────────────────────────────────────────────────
    bool BeginPropertyRow(const char* label, const char* tooltip)
    {
        ImGui::PushID(label);

        const float w = ImGui::CalcTextSize(label).x + kLabelPadding;
        const float clamped = w < kMinLabelW ? kMinLabelW : w;

        if (ImGui::BeginTable("##PropRow", 2,
            ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, clamped);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            if (tooltip) HelpMarker(tooltip);

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            return true;
        }

        ImGui::PopID();
        return false;
    }

    void EndPropertyRow()
    {
        ImGui::EndTable();
        ImGui::PopID();
    }


    // ─────────────────────────────────────────────────────────────────────────────
    // Component header
    // ─────────────────────────────────────────────────────────────────────────────
    bool ComponentHeader(const char8_t* icon,
        const char* label,
        const ImVec4& accent,
        bool* outRemoveRequested,
        bool canRemove,
        bool defaultOpen)
    {
        ImGui::PushID(label);

        ImVec4 bg = ImVec4(accent.x * 0.18f, accent.y * 0.18f, accent.z * 0.18f, 1.0f);
        ImVec4 hover = ImVec4(accent.x * 0.28f, accent.y * 0.28f, accent.z * 0.28f, 1.0f);
        ImVec4 active = ImVec4(accent.x * 0.35f, accent.y * 0.35f, accent.z * 0.35f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Header, bg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hover);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, active);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

        char composed[160];
        snprintf(composed, sizeof(composed), "    %s  %s",
            reinterpret_cast<const char*>(icon), label);

        ImVec2 hdrStart = ImGui::GetCursorScreenPos();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_AllowOverlap |
            ImGuiTreeNodeFlags_FramePadding;
        if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;

        if (g_FontBold) ImGui::PushFont(g_FontBold);
        bool open = ImGui::CollapsingHeader(composed, flags);
        if (g_FontBold) ImGui::PopFont();

        float hdrEndY = ImGui::GetItemRectMax().y;

        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(hdrStart.x, hdrStart.y),
            ImVec2(hdrStart.x + 3.0f, hdrEndY),
            ImGui::ColorConvertFloat4ToU32(accent), 0.0f);

        if (canRemove)
        {
            const float btnW = 22.0f;
            ImVec2 cursorAfter = ImGui::GetCursorPos();

            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - btnW - 4.0f);
            ImGui::SetCursorPosY(cursorAfter.y - ImGui::GetItemRectSize().y
                - ImGui::GetStyle().ItemSpacing.y * 0.5f);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.10f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.18f));
            if (ImGui::Button(reinterpret_cast<const char*>(u8"\uE206"), ImVec2(btnW, 0)))
                ImGui::OpenPopup("##componentMenu");
            ImGui::PopStyleColor(3);

            if (ImGui::BeginPopup("##componentMenu"))
            {
                if (ImGui::MenuItem("Reset")) {}
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, kStatusError);
                if (ImGui::MenuItem("Remove Component"))
                    if (outRemoveRequested) *outRemoveRequested = true;
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }

            ImGui::SetCursorPos(cursorAfter);
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        ImGui::PopID();
        return open;
    }


    // ─────────────────────────────────────────────────────────────────────────────
    // DragVec3
    // ─────────────────────────────────────────────────────────────────────────────
    bool DragVec3(const char* label,
        float* values,
        float resetValue,
        float speed,
        float vMin, float vMax,
        const char* format)
    {
        if (!PropertyRow(label)) return false;

        bool changed = false;

        const ImGuiStyle& style = ImGui::GetStyle();
        const float lineH = ImGui::GetFontSize() + style.FramePadding.y * 2.0f;
        const ImVec2 btnSize = { lineH + 3.0f, lineH };
        const float  spacing = style.ItemInnerSpacing.x;
        const float  fullW = ImGui::GetContentRegionAvail().x;
        const float  dragW = (fullW - (btnSize.x * 3.0f) - (spacing * 5.0f)) / 3.0f;

        auto axis = [&](const char* tag,
            const ImVec4& base, const ImVec4& hov,
            float* v) -> bool
            {
                bool lc = false;
                ImGui::PushStyleColor(ImGuiCol_Button, base);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hov);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, base);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
                if (g_FontBold) ImGui::PushFont(g_FontBold);

                if (ImGui::Button(tag, btnSize)) { *v = resetValue; lc = true; }

                if (g_FontBold) ImGui::PopFont();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(4);

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Click to reset to %.2f", resetValue);

                ImGui::SameLine(0.0f, 0.0f);

                ImGui::PushItemWidth(dragW);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
                char id[16]; snprintf(id, sizeof(id), "##%s", tag);
                if (ImGui::DragFloat(id, v, speed, vMin, vMax, format)) lc = true;
                ImGui::PopStyleVar();
                ImGui::PopItemWidth();
                return lc;
            };

        if (axis("X", kAxisX, kAxisXHover, &values[0])) changed = true;
        ImGui::SameLine(0.0f, spacing);
        if (axis("Y", kAxisY, kAxisYHover, &values[1])) changed = true;
        ImGui::SameLine(0.0f, spacing);
        if (axis("Z", kAxisZ, kAxisZHover, &values[2])) changed = true;

        ImGui::PopID();
        return changed;
    }


    // ─────────────────────────────────────────────────────────────────────────────
    // Single-value rows — each calls PropertyRow (which PushID's) then PopID.
    // ─────────────────────────────────────────────────────────────────────────────
    bool DragFloat(const char* label, float* value,
        float speed, float vMin, float vMax,
        const char* format, const char* tooltip)
    {
        if (!PropertyRow(label, tooltip)) return false;
        bool r = ImGui::DragFloat("##v", value, speed, vMin, vMax, format);
        ImGui::PopID();
        return r;
    }

    bool DragInt(const char* label, int* value,
        float speed, int vMin, int vMax, const char* tooltip)
    {
        if (!PropertyRow(label, tooltip)) return false;
        bool r = ImGui::DragInt("##v", value, speed, vMin, vMax);
        ImGui::PopID();
        return r;
    }

    bool ColorEdit3(const char* label, float* color, const char* tooltip)
    {
        if (!PropertyRow(label, tooltip)) return false;
        bool r = ImGui::ColorEdit3("##c", color);
        ImGui::PopID();
        return r;
    }

    bool ColorEdit4(const char* label, float* color, const char* tooltip)
    {
        if (!PropertyRow(label, tooltip)) return false;
        bool r = ImGui::ColorEdit4("##c", color);
        ImGui::PopID();
        return r;
    }

    bool Checkbox(const char* label, bool* value, const char* tooltip)
    {
        if (!PropertyRow(label, tooltip)) return false;
        bool r = ImGui::Checkbox("##cb", value);
        ImGui::PopID();
        return r;
    }

    bool Combo(const char* label, int* current,
        const char* const items[], int itemCount, const char* tooltip)
    {
        if (!PropertyRow(label, tooltip)) return false;
        bool r = ImGui::Combo("##c", current, items, itemCount);
        ImGui::PopID();
        return r;
    }

    bool InputText(const char* label, char* buffer, size_t bufferSize, const char* tooltip)
    {
        if (!PropertyRow(label, tooltip)) return false;
        bool r = ImGui::InputText("##t", buffer, bufferSize);
        ImGui::PopID();
        return r;
    }

    void TextRow(const char* label, const char* value)
    {
        if (!PropertyRow(label)) return;
        ImGui::TextDisabled("%s", value);
        ImGui::PopID();
    }


    // ─────────────────────────────────────────────────────────────────────────────
    // Asset slot
    // ─────────────────────────────────────────────────────────────────────────────
    bool AssetSlot(const char* label,
        const char* assetDisplayName,
        bool hasAsset,
        const char* placeholder)
    {
        if (!PropertyRow(label)) return false;

        const ImVec4& bg = hasAsset ? kSlotFilledBg : kSlotEmptyBg;
        const ImVec4& border = hasAsset ? kSlotFilledBorder : kSlotEmptyBorder;
        const char* shown = hasAsset ? assetDisplayName : placeholder;

        ImGui::PushStyleColor(ImGuiCol_Button, bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(bg.x * 1.3f, bg.y * 1.3f, bg.z * 1.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, bg);
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));

        bool clicked = ImGui::Button("##slot", ImVec2(ImGui::GetContentRegionAvail().x, 0));

        if (ImGui::IsItemVisible())
        {
            ImVec2 rMin = ImGui::GetItemRectMin();
            ImVec2 rMax = ImGui::GetItemRectMax();
            ImVec2 textPos = ImVec2(rMin.x + ImGui::GetStyle().FramePadding.x,
                rMin.y + (rMax.y - rMin.y - ImGui::GetTextLineHeight()) * 0.5f);
            ImGui::GetWindowDrawList()->AddText(textPos,
                ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Text)),
                shown);
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(4);
        ImGui::PopID();
        return clicked;
    }


    // ─────────────────────────────────────────────────────────────────────────────
    // Status indicator
    // ─────────────────────────────────────────────────────────────────────────────
    void StatusIndicator(const char* label, bool active,
        const char* activeText, const char* inactiveText)
    {
        if (!PropertyRow(label)) return;

        ImGui::PushStyleColor(ImGuiCol_Text, active ? kStatusOn : kStatusOff);
        ImGui::TextUnformatted(active
            ? reinterpret_cast<const char*>(u8"\u25CF")
            : reinterpret_cast<const char*>(u8"\u25CB"));
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 6.0f);
        ImGui::TextDisabled("%s", active ? activeText : inactiveText);
        ImGui::PopID();
    }


    // ─────────────────────────────────────────────────────────────────────────────
    // Banner
    // ─────────────────────────────────────────────────────────────────────────────
    namespace
    {
        thread_local float  s_bannerHeight = 0.0f;
        thread_local ImVec2 s_bannerStart = ImVec2(0, 0);
    }

    void BeginBanner(float height)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + ImGui::GetContentRegionAvail().x, p0.y + height);

        dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(kBannerBg), 4.0f);
        dl->AddRect(p0, p1, ImGui::ColorConvertFloat4ToU32(kBannerBorder), 4.0f);

        s_bannerStart = p0;
        s_bannerHeight = height;

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Indent(10.0f);
    }

    void EndBanner()
    {
        ImGui::Unindent(10.0f);
        float endY = s_bannerStart.y + s_bannerHeight + 4.0f;
        ImGui::SetCursorScreenPos(ImVec2(s_bannerStart.x, endY));
        ImGui::Spacing();
    }


    // ─────────────────────────────────────────────────────────────────────────────
    // Primary button
    // ─────────────────────────────────────────────────────────────────────────────
    bool PrimaryButton(const char* label, const ImVec4& accent, float widthRatio)
    {
        const float windowW = ImGui::GetContentRegionAvail().x;
        const float btnW = windowW * widthRatio;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (windowW - btnW) * 0.5f);

        ImGui::PushStyleColor(ImGuiCol_Button,
            ImVec4(accent.x * 0.35f, accent.y * 0.35f, accent.z * 0.35f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(accent.x * 0.55f, accent.y * 0.55f, accent.z * 0.55f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
            ImVec4(accent.x * 0.70f, accent.y * 0.70f, accent.z * 0.70f, 1.00f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));

        if (g_FontBold) ImGui::PushFont(g_FontBold);
        bool clicked = ImGui::Button(label, ImVec2(btnW, 0));
        if (g_FontBold) ImGui::PopFont();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        return clicked;
    }


    // ─────────────────────────────────────────────────────────────────────────────
    // Icon toggle button
    // ─────────────────────────────────────────────────────────────────────────────
    bool IconToggleButton(const char* id,
        const char8_t* iconOn, const char8_t* iconOff,
        bool state, float size, const char* tooltip)
    {
        if (size <= 0.0f) size = ImGui::GetFrameHeight();
        ImGui::PushID(id);

        const bool hovered = ImGui::IsMouseHoveringRect(
            ImGui::GetCursorScreenPos(),
            ImVec2(ImGui::GetCursorScreenPos().x + size,
                ImGui::GetCursorScreenPos().y + size));

        ImVec4 col = state ? kIconBtnActive : kIconBtnDim;
        if (!state && !hovered) col.w = 0.45f;

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.14f));
        ImGui::PushStyleColor(ImGuiCol_Text, col);

        bool clicked = ImGui::Button(
            reinterpret_cast<const char*>(state ? iconOn : iconOff),
            ImVec2(size, size));

        ImGui::PopStyleColor(4);
        if (tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
        ImGui::PopID();
        return clicked;
    }


    // ─────────────────────────────────────────────────────────────────────────────
    // Toolbar primitives
    // ─────────────────────────────────────────────────────────────────────────────
    bool ToolButton(const char8_t* icon, const char* tooltip, bool active, float size)
    {
        if (size <= 0.0f) size = ImGui::GetFrameHeight();

        ImVec4 bg = active ? kToolBtnActiveBg : ImVec4(0, 0, 0, 0);
        ImVec4 bgHover = active ? kToolBtnActiveBg : kToolBtnHoverBg;
        ImVec4 txt = active ? ImVec4(1, 1, 1, 1) : ImVec4(0.82f, 0.84f, 0.88f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bgHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kToolBtnActiveBg);
        ImGui::PushStyleColor(ImGuiCol_Text, txt);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        bool clicked = ImGui::Button(reinterpret_cast<const char*>(icon), ImVec2(size, size));

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
        if (tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
        return clicked;
    }

    void ToolbarSeparator()
    {
        ImGui::SameLine(0.0f, 6.0f);
        ImVec2 p = ImGui::GetCursorScreenPos();
        float  h = ImGui::GetFrameHeight();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(p.x, p.y + 3.0f), ImVec2(p.x, p.y + h - 3.0f),
            ImGui::ColorConvertFloat4ToU32(kToolbarSeparator), 1.0f);
        ImGui::Dummy(ImVec2(1.0f, h));
        ImGui::SameLine(0.0f, 6.0f);
    }


    // ─────────────────────────────────────────────────────────────────────────────
    // Empty-state placeholder
    // ─────────────────────────────────────────────────────────────────────────────
    void EmptyState(const char8_t* bigIcon, const char* message, const char* hint)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        const char* icon = reinterpret_cast<const char*>(bigIcon);

        ImVec2 iconSize;
        if (g_FontLarge) { ImGui::PushFont(g_FontLarge); iconSize = ImGui::CalcTextSize(icon); ImGui::PopFont(); }
        else               iconSize = ImGui::CalcTextSize(icon);

        ImVec2 msgSize = ImGui::CalcTextSize(message);
        ImVec2 hintSize = hint ? ImGui::CalcTextSize(hint) : ImVec2(0, 0);

        float blockH = iconSize.y + 8.0f + msgSize.y + (hint ? 4.0f + hintSize.y : 0.0f);
        float startY = ImGui::GetCursorPosY() + (avail.y - blockH) * 0.5f;
        if (startY < ImGui::GetCursorPosY()) startY = ImGui::GetCursorPosY();

        ImGui::SetCursorPosY(startY);
        ImGui::SetCursorPosX((avail.x - iconSize.x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, kEmptyStateIcon);
        if (g_FontLarge) ImGui::PushFont(g_FontLarge);
        ImGui::TextUnformatted(icon);
        if (g_FontLarge) ImGui::PopFont();
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::SetCursorPosX((avail.x - msgSize.x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, kEmptyStateText);
        ImGui::TextUnformatted(message);
        ImGui::PopStyleColor();

        if (hint)
        {
            ImGui::SetCursorPosX((avail.x - hintSize.x) * 0.5f);
            ImGui::TextDisabled("%s", hint);
        }
    }
}