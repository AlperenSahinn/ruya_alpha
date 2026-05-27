#include "status_bar.h"

#include <engine.h>
#include <app_framework/id_component.h>

#include "editor_widgets.h"
#include "toolbar.h"
#include "scene_outliner.h"
#include "../editor_style.h"

void editor::StatusBar::DrawContents()
{
	const float h = ImGui::GetFrameHeight() + 4.0f;

	ImGui::PushStyleColor(ImGuiCol_ChildBg, kStatusBarBg);
	ImGui::PushStyleColor(ImGuiCol_Text, kStatusBarText);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 3.0f));

	if (ImGui::BeginChild("##EditorStatusBar", ImVec2(0.0f, h), false,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::AlignTextToFramePadding();

		ruya::RyID scene = SceneOutliner::selectedScene;
		const char* sceneName = "<no scene>";
		if (scene.IsValid())
		{
			ruya::Scene* s = engine->GetApp()->GetScene(scene);
			if (s) sceneName = s->GetName().c_str();
		}

		ImGui::PushStyleColor(ImGuiCol_Text, kStatusBarAccent);
		ImGui::TextUnformatted(IconStr(ico::kScene));
		ImGui::PopStyleColor();
		ImGui::SameLine(0, 5.0f);
		ImGui::TextUnformatted(sceneName);

		const size_t selCount = SceneOutliner::GetSelectedEntities().size();
		ImGui::SameLine(0, 18.0f);
		ImGui::TextUnformatted("|");
		ImGui::SameLine(0, 18.0f);
		ImGui::TextUnformatted(IconStr(ico::kSelectInfo));
		ImGui::SameLine(0, 5.0f);
		if (selCount == 0)
			ImGui::TextUnformatted("Nothing selected");
		else if (selCount == 1)
			ImGui::Text("1 entity selected");
		else
			ImGui::Text("%zu entities selected", selCount);

		const char* modeStr = "Select";
		const char8_t* modeIcon = ico::kCursor;
		switch (Toolbar::GetMode())
		{
		case GizmoMode::Move:   modeStr = "Move";   modeIcon = ico::kArrowsMove; break;
		case GizmoMode::Rotate: modeStr = "Rotate"; modeIcon = ico::kRotate;     break;
		case GizmoMode::Scale:  modeStr = "Scale";  modeIcon = ico::kScale;      break;
		default:                modeStr = "Select"; modeIcon = ico::kCursor;     break;
		}

		const bool isWorld   = (Toolbar::GetSpace() == GizmoSpace::World);
		const char* spaceStr = isWorld ? "World" : "Local";

		char snapBuf[48];
		if (Toolbar::IsSnapEnabled())
			snprintf(snapBuf, sizeof(snapBuf), "Snap: ON");
		else
			snprintf(snapBuf, sizeof(snapBuf), "Snap: off");

		char rightBuf[160];
		snprintf(rightBuf, sizeof(rightBuf), "%s %s    %s    %s",
			reinterpret_cast<const char*>(modeIcon), modeStr, spaceStr, snapBuf);

		float rightW = ImGui::CalcTextSize(rightBuf).x;
		ImGui::SameLine(ImGui::GetWindowWidth() - rightW - 14.0f);
		ImGui::TextUnformatted(rightBuf);
	}
	ImGui::EndChild();

	ImGui::PopStyleVar();
	ImGui::PopStyleColor(2);
}
