#pragma once

#include <ImGui/imgui.h>

namespace editor
{
    // ── Axis Colors ────────────────────────────────────────────────────
    inline constexpr ImVec4 kAxisX = ImVec4(0.92f, 0.20f, 0.22f, 1.00f);
    inline constexpr ImVec4 kAxisY = ImVec4(0.36f, 0.80f, 0.32f, 1.00f);
    inline constexpr ImVec4 kAxisZ = ImVec4(0.22f, 0.48f, 0.95f, 1.00f);
    inline constexpr ImVec4 kAxisXHover = ImVec4(1.00f, 0.32f, 0.34f, 1.00f);
    inline constexpr ImVec4 kAxisYHover = ImVec4(0.46f, 0.90f, 0.42f, 1.00f);
    inline constexpr ImVec4 kAxisZHover = ImVec4(0.32f, 0.58f, 1.00f, 1.00f);
    inline constexpr ImVec4 kAxisXDark = ImVec4(0.60f, 0.10f, 0.12f, 1.00f);
    inline constexpr ImVec4 kAxisYDark = ImVec4(0.15f, 0.45f, 0.14f, 1.00f);
    inline constexpr ImVec4 kAxisZDark = ImVec4(0.10f, 0.22f, 0.55f, 1.00f);

    // ── Asset Type Colors ──────────────────────────────────────────────
    inline constexpr ImVec4 kColorFolder = ImVec4(0.95f, 0.78f, 0.28f, 1.00f);
    inline constexpr ImVec4 kColorMesh = ImVec4(0.30f, 0.75f, 0.95f, 1.00f);
    inline constexpr ImVec4 kColorTexture = ImVec4(0.60f, 0.85f, 0.35f, 1.00f);
    inline constexpr ImVec4 kColorMaterial = ImVec4(0.92f, 0.48f, 0.70f, 1.00f);
    inline constexpr ImVec4 kColorScene = ImVec4(0.75f, 0.58f, 1.00f, 1.00f);
    inline constexpr ImVec4 kColorInvalid = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);

    // ── Component Header Colors ────────────────────────────────────────
    inline constexpr ImVec4 kColorHeaderId = ImVec4(0.45f, 0.45f, 0.55f, 0.65f);
    inline constexpr ImVec4 kColorHeaderTransform = ImVec4(0.18f, 0.50f, 0.82f, 0.70f);
    inline constexpr ImVec4 kColorHeaderRender = ImVec4(0.82f, 0.42f, 0.18f, 0.70f);
    inline constexpr ImVec4 kColorHeaderPhysics = ImVec4(0.25f, 0.72f, 0.42f, 0.70f);
    inline constexpr ImVec4 kColorHeaderLight = ImVec4(0.95f, 0.82f, 0.30f, 0.70f);

    // ── Transport Colors ───────────────────────────────────────────────
    inline constexpr ImVec4 kColorPlay = ImVec4(0.20f, 0.85f, 0.30f, 1.00f);
    inline constexpr ImVec4 kColorPause = ImVec4(0.95f, 0.75f, 0.20f, 1.00f);
    inline constexpr ImVec4 kColorResume = ImVec4(0.30f, 0.60f, 0.95f, 1.00f);
    inline constexpr ImVec4 kColorStop = ImVec4(0.90f, 0.25f, 0.25f, 1.00f);

    // ── FPS Colors ─────────────────────────────────────────────────────
    inline constexpr ImVec4 kFpsGood = ImVec4(0.20f, 0.85f, 0.25f, 1.0f);
    inline constexpr ImVec4 kFpsOk = ImVec4(0.95f, 0.75f, 0.20f, 1.0f);
    inline constexpr ImVec4 kFpsBad = ImVec4(0.90f, 0.20f, 0.20f, 1.0f);

    // ── Accent ─────────────────────────────────────────────────────────
    inline constexpr ImVec4 kAccent = ImVec4(0.40f, 0.55f, 0.95f, 1.00f);
    inline constexpr ImVec4 kAccentHover = ImVec4(0.50f, 0.65f, 1.00f, 1.00f);

    // ── Status / Indicator Colors ──────────────────────────────────────
    inline constexpr ImVec4 kStatusOn = ImVec4(0.45f, 0.85f, 0.45f, 1.00f);
    inline constexpr ImVec4 kStatusOff = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
    inline constexpr ImVec4 kStatusWarn = ImVec4(0.95f, 0.72f, 0.25f, 1.00f);
    inline constexpr ImVec4 kStatusError = ImVec4(0.95f, 0.35f, 0.35f, 1.00f);

    // ── Slot / Drop Target ─────────────────────────────────────────────
    inline constexpr ImVec4 kSlotEmptyBg = ImVec4(0.13f, 0.13f, 0.15f, 1.00f);
    inline constexpr ImVec4 kSlotEmptyBorder = ImVec4(0.35f, 0.35f, 0.40f, 0.50f);
    inline constexpr ImVec4 kSlotFilledBg = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
    inline constexpr ImVec4 kSlotFilledBorder = ImVec4(0.35f, 0.55f, 0.85f, 0.60f);

    // ── Banner (entity name strip etc.) ────────────────────────────────
    inline constexpr ImVec4 kBannerBg = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    inline constexpr ImVec4 kBannerBorder = ImVec4(0.25f, 0.26f, 0.30f, 1.00f);

    // ── Layout constants ───────────────────────────────────────────────
    inline constexpr float kLabelColumnWidth = 120.0f;

    // ── Outliner row colors ────────────────────────────────────────────
    inline constexpr ImVec4 kRowSelStripe   = ImVec4(0.40f, 0.55f, 0.95f, 1.00f);  // left accent on selected row
    inline constexpr ImVec4 kRowHoverBg      = ImVec4(1.00f, 1.00f, 1.00f, 0.045f); // faint hover wash
    inline constexpr ImVec4 kRowSelBg        = ImVec4(0.40f, 0.55f, 0.95f, 0.16f);  // selected row fill
    inline constexpr ImVec4 kIconBtnDim      = ImVec4(0.55f, 0.57f, 0.62f, 1.00f);  // eye/lock idle
    inline constexpr ImVec4 kIconBtnActive   = ImVec4(0.92f, 0.93f, 0.95f, 1.00f);  // eye/lock active
    inline constexpr ImVec4 kRowLockedText   = ImVec4(0.55f, 0.56f, 0.60f, 1.00f);  // dimmed text when locked

    // ── Toolbar ────────────────────────────────────────────────────────
    inline constexpr float  kToolbarHeight   = 34.0f;
    inline constexpr ImVec4 kToolbarBg        = ImVec4(0.075f, 0.078f, 0.085f, 1.00f);
    inline constexpr ImVec4 kToolBtnActiveBg  = ImVec4(0.38f, 0.52f, 0.92f, 0.85f);  // pressed/selected tool
    inline constexpr ImVec4 kToolBtnHoverBg   = ImVec4(0.40f, 0.55f, 0.95f, 0.30f);
    inline constexpr ImVec4 kToolbarSeparator = ImVec4(0.22f, 0.23f, 0.26f, 1.00f);

    // ── Status bar ─────────────────────────────────────────────────────
    inline constexpr float  kStatusBarHeight  = 24.0f;
    inline constexpr ImVec4 kStatusBarBg       = ImVec4(0.065f, 0.068f, 0.075f, 1.00f);
    inline constexpr ImVec4 kStatusBarBorder   = ImVec4(0.18f, 0.19f, 0.21f, 1.00f);
    inline constexpr ImVec4 kStatusBarText     = ImVec4(0.62f, 0.64f, 0.70f, 1.00f);
    inline constexpr ImVec4 kStatusBarAccent    = ImVec4(0.45f, 0.60f, 1.00f, 1.00f);

    // ── Empty-state ────────────────────────────────────────────────────
    inline constexpr ImVec4 kEmptyStateIcon   = ImVec4(0.30f, 0.32f, 0.38f, 1.00f);
    inline constexpr ImVec4 kEmptyStateText   = ImVec4(0.45f, 0.47f, 0.53f, 1.00f);

    namespace ico
    {
        inline constexpr const char8_t* kScene        = u8"\uE7AE"; // scene root
        inline constexpr const char8_t* kEntity       = u8"\uE8F4"; // generic entity (cube)
        inline constexpr const char8_t* kTransform    = u8"\uE0A4"; // transform / move
        inline constexpr const char8_t* kRender       = u8"\uE1DA"; // render (cube)
        inline constexpr const char8_t* kPhysics      = u8"\uE680"; // physics
        inline constexpr const char8_t* kLight        = u8"\uE472"; // light / sun
        inline constexpr const char8_t* kMaterial     = u8"\uE18E"; // material
        inline constexpr const char8_t* kFolder       = u8"\uE24A"; // folder
        inline constexpr const char8_t* kName         = u8"\uE2C8"; // name tag
        inline constexpr const char8_t* kInfo         = u8"\uE2CE"; // info
        inline constexpr const char8_t* kSearch       = u8"\uE30C"; // magnifier
        inline constexpr const char8_t* kDots         = u8"\uE1FC"; // vertical ellipsis
        inline constexpr const char8_t* kPlus         = u8"\uE3D4"; // create / add
        inline constexpr const char8_t* kSave         = u8"\uE248"; // save (floppy)

        inline constexpr const char8_t* kEyeOpen      = u8"\uE220"; // visible
        inline constexpr const char8_t* kEyeClosed    = u8"\uE222"; // hidden
        inline constexpr const char8_t* kLockClosed   = u8"\uE2FE"; // locked
        inline constexpr const char8_t* kLockOpen     = u8"\uE300"; // unlocked 
        inline constexpr const char8_t* kCursor       = u8"\uE1DC"; // select tool
        inline constexpr const char8_t* kArrowsMove   = u8"\uE0A4"; // move (reuse transform glyph)
        inline constexpr const char8_t* kRotate       = u8"\uE096"; // rotate
        inline constexpr const char8_t* kScale        = u8"\uE0A6"; // scale/resize
        inline constexpr const char8_t* kGlobe        = u8"\uE288"; // world space
        inline constexpr const char8_t* kCube3D       = u8"\uE1DA"; // local space
        inline constexpr const char8_t* kGridSnap     = u8"\uE680"; // grid / magnet
        inline constexpr const char8_t* kSelectInfo   = u8"\uE2CE"; // selection info
    }

    // ── Modern Dark Theme ──────────────────────────────────────────────
    inline void ApplyModernDarkTheme()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        // Rounding
        style.WindowRounding = 3.0f;
        style.ChildRounding = 3.0f;
        style.FrameRounding = 3.0f;
        style.PopupRounding = 3.0f;
        style.ScrollbarRounding = 6.0f;
        style.GrabRounding = 3.0f;
        style.TabRounding = 3.0f;

        // Spacing
        style.WindowPadding = ImVec2(8.0f, 8.0f);
        style.FramePadding = ImVec2(6.0f, 4.0f);
        style.ItemSpacing = ImVec2(8.0f, 5.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.CellPadding = ImVec2(8.0f, 4.0f);
        style.IndentSpacing = 18.0f;
        style.ScrollbarSize = 12.0f;
        style.GrabMinSize = 8.0f;

        // Borders
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.TabBorderSize = 0.0f;

        // Alignment
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.ColorButtonPosition = ImGuiDir_Right;
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

        style.Alpha = 1.0f;
        style.DisabledAlpha = 0.55f;

        // ── Colors ─────────────────────────────────────────────────────
        ImVec4* c = style.Colors;

        ImVec4 bg0 = ImVec4(0.055f, 0.058f, 0.065f, 1.0f);  // Deepest
        ImVec4 bg1 = ImVec4(0.085f, 0.088f, 0.095f, 1.0f);  // Window
        ImVec4 bg2 = ImVec4(0.120f, 0.125f, 0.135f, 1.0f);  // Child / Frame
        ImVec4 bg3 = ImVec4(0.160f, 0.165f, 0.175f, 1.0f);  // Hover
        ImVec4 border = ImVec4(0.190f, 0.195f, 0.205f, 1.0f);

        ImVec4 accent = ImVec4(0.38f, 0.52f, 0.92f, 1.00f);
        ImVec4 accentH = ImVec4(0.45f, 0.60f, 1.00f, 1.00f);
        ImVec4 accentA = ImVec4(0.30f, 0.42f, 0.78f, 1.00f);

        c[ImGuiCol_Text] = ImVec4(0.92f, 0.93f, 0.95f, 1.00f);
        c[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.42f, 0.48f, 1.00f);
        c[ImGuiCol_WindowBg] = bg1;
        c[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        c[ImGuiCol_PopupBg] = ImVec4(bg0.x, bg0.y, bg0.z, 0.96f);
        c[ImGuiCol_Border] = border;
        c[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        c[ImGuiCol_FrameBg] = bg2;
        c[ImGuiCol_FrameBgHovered] = bg3;
        c[ImGuiCol_FrameBgActive] = ImVec4(accent.x * 0.5f, accent.y * 0.5f, accent.z * 0.5f, 0.60f);
        c[ImGuiCol_TitleBg] = bg0;
        c[ImGuiCol_TitleBgActive] = bg0;
        c[ImGuiCol_TitleBgCollapsed] = bg0;
        c[ImGuiCol_MenuBarBg] = ImVec4(bg0.x + 0.02f, bg0.y + 0.02f, bg0.z + 0.02f, 1.0f);
        c[ImGuiCol_ScrollbarBg] = bg0;
        c[ImGuiCol_ScrollbarGrab] = bg2;
        c[ImGuiCol_ScrollbarGrabHovered] = bg3;
        c[ImGuiCol_ScrollbarGrabActive] = accent;
        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = accentH;
        c[ImGuiCol_Button] = bg2;
        c[ImGuiCol_ButtonHovered] = ImVec4(accent.x * 0.4f, accent.y * 0.4f, accent.z * 0.4f, 0.65f);
        c[ImGuiCol_ButtonActive] = accentA;
        c[ImGuiCol_Header] = bg2;
        c[ImGuiCol_HeaderHovered] = ImVec4(accent.x * 0.35f, accent.y * 0.35f, accent.z * 0.35f, 0.55f);
        c[ImGuiCol_HeaderActive] = ImVec4(accent.x * 0.45f, accent.y * 0.45f, accent.z * 0.45f, 0.65f);
        c[ImGuiCol_Separator] = border;
        c[ImGuiCol_SeparatorHovered] = accent;
        c[ImGuiCol_SeparatorActive] = accentH;
        c[ImGuiCol_ResizeGrip] = ImVec4(accent.x, accent.y, accent.z, 0.15f);
        c[ImGuiCol_ResizeGripHovered] = ImVec4(accent.x, accent.y, accent.z, 0.40f);
        c[ImGuiCol_ResizeGripActive] = ImVec4(accent.x, accent.y, accent.z, 0.70f);
        c[ImGuiCol_Tab] = ImVec4(bg0.x, bg0.y, bg0.z, 1.0f);
        c[ImGuiCol_TabHovered] = ImVec4(accent.x * 0.45f, accent.y * 0.45f, accent.z * 0.45f, 0.55f);
        c[ImGuiCol_TabActive] = ImVec4(
            bg1.x + (accent.x - bg1.x) * 0.16f,
            bg1.y + (accent.y - bg1.y) * 0.16f,
            bg1.z + (accent.z - bg1.z) * 0.16f, 1.0f);
        c[ImGuiCol_TabUnfocused] = ImVec4(bg0.x, bg0.y, bg0.z, 1.0f);
        c[ImGuiCol_TabUnfocusedActive] = ImVec4(bg1.x + 0.015f, bg1.y + 0.015f, bg1.z + 0.015f, 1.0f);
        c[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.40f);
        c[ImGuiCol_DockingEmptyBg] = bg0;
        c[ImGuiCol_PlotLines] = ImVec4(0.50f, 0.58f, 0.70f, 1.0f);
        c[ImGuiCol_PlotLinesHovered] = accent;
        c[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.50f, 0.25f, 1.0f);
        c[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.35f, 1.0f);
        c[ImGuiCol_TableHeaderBg] = bg0;
        c[ImGuiCol_TableBorderStrong] = border;
        c[ImGuiCol_TableBorderLight] = ImVec4(border.x * 0.7f, border.y * 0.7f, border.z * 0.7f, 1.0f);
        c[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        c[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.02f);
        c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.30f);
        c[ImGuiCol_DragDropTarget] = ImVec4(accent.x, accent.y, accent.z, 0.80f);
        c[ImGuiCol_NavHighlight] = accent;
        c[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.12f);
        c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.50f);
        c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);
    }
}