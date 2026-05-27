#include "toolbar.h"

#include <cstdio>

#include <engine.h>

#include "editor_widgets.h"
#include "../editor_style.h"

namespace editor
{
	GizmoMode  Toolbar::mode = GizmoMode::Select;
	GizmoSpace Toolbar::space = GizmoSpace::World;
	bool       Toolbar::snapEnabled = false;
	float      Toolbar::moveSnap = 1.0f;
	float      Toolbar::rotateSnap = 15.0f;
	float      Toolbar::scaleSnap = 0.1f;
}

namespace
{
	using namespace editor;

	const ImVec4 kColorStep = ImVec4(0.30f, 0.65f, 1.00f, 1.00f);
	const ImVec4 kColorDisabled = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);

	bool TransportButton(const char* icon, const char* tooltip, const ImVec4& color,
		bool enabled, float buttonWidth)
	{
		ImGui::BeginDisabled(!enabled);

		ImVec4 textColor = enabled ? color : kColorDisabled;
		ImVec4 bgIdle = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		ImVec4 bgHovered = enabled ? ImVec4(color.x, color.y, color.z, 0.20f) : ImVec4(0, 0, 0, 0);
		ImVec4 bgActive = enabled ? ImVec4(color.x, color.y, color.z, 0.35f) : ImVec4(0, 0, 0, 0);

		ImGui::PushStyleColor(ImGuiCol_Text, textColor);
		ImGui::PushStyleColor(ImGuiCol_Button, bgIdle);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bgHovered);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, bgActive);

		bool clicked = ImGui::Button(icon, ImVec2(buttonWidth, 0));

		ImGui::PopStyleColor(4);

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("%s%s", tooltip, enabled ? "" : "  (unavailable)");

		ImGui::EndDisabled();
		return clicked && enabled;
	}
}

void editor::Toolbar::DrawContents()
{
	const float barHeight = ImGui::GetFrameHeight() + 10.0f;

	ImGui::PushStyleColor(ImGuiCol_ChildBg, kToolbarBg);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));

	if (ImGui::BeginChild("##EditorToolbar", ImVec2(0.0f, barHeight), false,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		float vpad = (barHeight - ImGui::GetFrameHeight()) * 0.5f;
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + vpad);

		DrawTransformTools();
		ToolbarSeparator();
		DrawSpaceToggle();
		ToolbarSeparator();
		DrawSnapControls();

		DrawTransport();
	}
	ImGui::EndChild();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();

	ImVec2 p = ImGui::GetCursorScreenPos();
	float w = ImGui::GetContentRegionAvail().x;
	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(p.x, p.y), ImVec2(p.x + w, p.y),
		ImGui::ColorConvertFloat4ToU32(kToolbarSeparator), 1.0f);
}

void editor::Toolbar::DrawTransformTools()
{
	struct ToolDef { GizmoMode m; const char8_t* icon; const char* tip; };
	const ToolDef tools[] = {
		{ GizmoMode::Select, ico::kCursor,     "Select  (F)" },
		{ GizmoMode::Move,   ico::kArrowsMove, "Move  (G)"   },
		{ GizmoMode::Rotate, ico::kRotate,     "Rotate  (R)" },
		{ GizmoMode::Scale,  ico::kScale,      "Scale  (T)"  },
	};

	bool first = true;
	for (const ToolDef& t : tools)
	{
		if (!first) ImGui::SameLine(0.0f, 4.0f);
		first = false;
		if (ToolButton(t.icon, t.tip, mode == t.m))
			mode = t.m;
	}

	if (!ImGui::GetIO().WantTextInput)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_F, false)) mode = GizmoMode::Select;
		if (ImGui::IsKeyPressed(ImGuiKey_G, false)) mode = GizmoMode::Move;
		if (ImGui::IsKeyPressed(ImGuiKey_R, false)) mode = GizmoMode::Rotate;
		if (ImGui::IsKeyPressed(ImGuiKey_T, false)) mode = GizmoMode::Scale;
	}
}

void editor::Toolbar::DrawSpaceToggle()
{
	const bool isWorld = (space == GizmoSpace::World);
	const char8_t* icon = isWorld ? ico::kGlobe : ico::kCube3D;
	const char* label = isWorld ? "World" : "Local";

	ImGui::SameLine(0.0f, 4.0f);

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kToolBtnHoverBg);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, kToolBtnActiveBg);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

	char composed[32];
	snprintf(composed, sizeof(composed), "%s  %s",
		reinterpret_cast<const char*>(icon), label);

	if (ImGui::Button(composed))
		space = isWorld ? GizmoSpace::Local : GizmoSpace::World;

	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Coordinate space: %s  (click to switch)", label);
}

void editor::Toolbar::DrawSnapControls()
{
	ImGui::SameLine(0.0f, 4.0f);

	if (ToolButton(ico::kGridSnap, snapEnabled ? "Snapping ON  (click to disable)"
		: "Snapping OFF  (click to enable)", snapEnabled))
	{
		snapEnabled = !snapEnabled;
	}

	float* snapTarget = &moveSnap;
	const char* fmt = "%.2f";
	const char* tip = "Move snap (world units)";
	switch (mode)
	{
	case GizmoMode::Rotate: snapTarget = &rotateSnap; fmt = "%.0f\xC2\xB0"; tip = "Rotation snap (degrees)"; break;
	case GizmoMode::Scale:  snapTarget = &scaleSnap;  fmt = "%.2f";          tip = "Scale snap";              break;
	default:                snapTarget = &moveSnap;   fmt = "%.2f";          tip = "Move snap (world units)"; break;
	}

	ImGui::SameLine(0.0f, 6.0f);
	ImGui::BeginDisabled(!snapEnabled);
	ImGui::SetNextItemWidth(64.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::DragFloat("##snapval", snapTarget, 0.05f, 0.0f, 0.0f, fmt);
	ImGui::PopStyleVar();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", tip);
	ImGui::EndDisabled();
}

void editor::Toolbar::DrawTransport()
{
	const ruya::AppState state = engine->GetAppState();

	const bool isReady = (state == ruya::AppState::AppReady);
	const bool isRunning = (state == ruya::AppState::AppUpdate);
	const bool isPaused = (state == ruya::AppState::AppPause);
	const bool inTransit = (state == ruya::AppState::AppStart || state == ruya::AppState::AppShutdown);

	const bool playPauseEnabled = !inTransit && (isReady || isRunning || isPaused);
	const bool stepEnabled = !inTransit && isPaused;
	const bool stopEnabled = !inTransit && (isRunning || isPaused);

	const char* playIcon = IconStr(u8"\uE13A");
	const char* pauseIcon = IconStr(u8"\uE39E");
	const char* resumeIcon = IconStr(u8"\uE12A");
	const char* stepIcon = IconStr(u8"\uE8BE");
	const char* stopIcon = IconStr(u8"\uE45E");

	const char* playPauseIcon;
	const char* playPauseTooltip;
	ImVec4      playPauseColor;

	if (isRunning)
	{
		playPauseIcon = pauseIcon;  playPauseTooltip = "Pause";  playPauseColor = kColorPause;
	}
	else if (isPaused)
	{
		playPauseIcon = resumeIcon; playPauseTooltip = "Resume"; playPauseColor = kColorPlay;
	}
	else
	{
		playPauseIcon = playIcon;   playPauseTooltip = "Play";   playPauseColor = kColorPlay;
	}

	const float buttonWidth = 30.0f;
	const float spacing = 4.0f;
	const float groupWidth = (buttonWidth * 3) + (spacing * 2);

	const float windowWidth = ImGui::GetWindowSize().x;
	ImGui::SameLine((windowWidth - groupWidth) * 0.5f);

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

	if (TransportButton(playPauseIcon, playPauseTooltip, playPauseColor, playPauseEnabled, buttonWidth))
	{
		if (isReady)        engine->StartApp();
		else if (isRunning) engine->PauseApp();
		else if (isPaused)  engine->ContinueApp();
	}

	ImGui::SameLine(0, spacing);

	if (TransportButton(stepIcon, "Frame Step", kColorStep, stepEnabled, buttonWidth))
	{
		stepRequested = true;
		engine->ContinueApp();
	}

	ImGui::SameLine(0, spacing);

	if (TransportButton(stopIcon, "Stop", kColorStop, stopEnabled, buttonWidth))
		engine->ShutdownApp();

	ImGui::PopStyleVar();

	if (stepRequested && state == ruya::AppState::AppUpdate)
	{
		engine->PauseApp();
		stepRequested = false;
	}
}