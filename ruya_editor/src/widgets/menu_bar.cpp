#include "menu_bar.h"

#include <engine.h>

#include "directional_light_properties.h"

#include <editor.h>

void editor::MenuBar::Draw()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open"))
            {
            }

            if (ImGui::MenuItem("Save"))
            {
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo"))
            {
            }

            if (ImGui::MenuItem("Redo"))
            {
            }
            ImGui::EndMenu();
        }

		if (ImGui::BeginMenu("Graphics"))
		{
			if (ImGui::BeginMenu("Lighting"))
			{
				if (ImGui::MenuItem("Atmospheric Light Properties")) 
                {
                    if(!DirectionalLightProperties::visible)
                    {
                        DirectionalLightProperties::visible = true;
                    }
                }
                ImGui::EndMenu();
			}
            ImGui::EndMenu();
		}

        if (ImGui::BeginMenu("Preferences"))
        {
            ImGui::EndMenu();
        }

        const char8_t* utf8_playIcon = u8"\uE3D0";
        const char* playIcon = reinterpret_cast<const char*>(utf8_playIcon);

        const char8_t* utf8_pauseIcon = u8"\uE39E";
        const char* pauseIcon = reinterpret_cast<const char*>(utf8_pauseIcon);

        const char8_t* utf8_playPauseIcon = u8"\uE8BE";
        const char* playPauseIcon = reinterpret_cast<const char*>(utf8_playPauseIcon);

        const char8_t* utf8_stopIcon = u8"\uE46C";
        const char* stopIcon = reinterpret_cast<const char*>(utf8_stopIcon);

        float windowWidth = ImGui::GetWindowSize().x;

        float buttonWidth = 80.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float totalButtonsWidth = (buttonWidth * 3) + (spacing * 2);

        ImGui::SameLine((windowWidth - totalButtonsWidth) * 0.5f);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.20f, 0.85f, 0.30f, 1.00f));
        if (ImGui::Button(playIcon)) {}
        ImGui::PopStyleColor();

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.20f, 1.00f));
        if (ImGui::Button(pauseIcon)) {}
        ImGui::PopStyleColor();

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.60f, 0.95f, 1.00f));
        if (ImGui::Button(playPauseIcon)) {}
        ImGui::PopStyleColor();

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.25f, 0.25f, 1.00f));
        if (ImGui::Button(stopIcon)) {}
        ImGui::PopStyleColor();

        float dt = engine->GetWindow()->GetDeltaTime();
        timer += dt;

        if (timer >= 0.1f)
        {
            cachedMs = dt * 1000.0f;
            cachedFps = (dt > 0.0f) ? (1.0f / dt) : 0.0f;

            snprintf(buffer, sizeof(buffer), "ms: %.2f | fps: %.1f", cachedMs, cachedFps);

            timer = 0.0f;
        }

        ImVec4 color;
        if (cachedFps >= 59.0f)
            color = ImVec4(0.20f, 0.85f, 0.25f, 1.0f);
        else if (cachedFps >= 29.0f)
            color = ImVec4(0.95f, 0.75f, 0.20f, 1.0f);
        else
            color = ImVec4(0.90f, 0.20f, 0.20f, 1.0f);

        float textWidth = ImGui::CalcTextSize(buffer).x;
        ImGui::SameLine(windowWidth - textWidth - 10);

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(buffer);
        ImGui::PopStyleColor();

        ImGui::EndMainMenuBar();
    }
}
