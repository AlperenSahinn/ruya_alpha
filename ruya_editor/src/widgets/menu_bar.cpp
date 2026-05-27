#include "menu_bar.h"

#include <engine.h>
#include <editor.h>

#include "atmospheric_light_properties.h"
#include "editor_widgets.h"
#include "../editor_style.h"

void editor::MenuBar::Draw()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem(IconLabel(u8"\uE24A", "Open"), "Ctrl+O"))
            {
            }

            if (ImGui::MenuItem(IconLabel(u8"\uE248", "Save"), "Ctrl+S"))
            {
            }

            if (ImGui::MenuItem(IconLabel(u8"\uE248", "Save As..."), "Ctrl+Shift+S"))
            {
            }

            ImGui::Separator();

            if (ImGui::MenuItem(IconLabel(u8"\uE4F6", "Exit")))
            {
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem(IconLabel(u8"\uE07E", "Undo"), "Ctrl+Z"))
            {
            }

            if (ImGui::MenuItem(IconLabel(u8"\uE080", "Redo"), "Ctrl+Y"))
            {
            }

            ImGui::Separator();

            if (ImGui::MenuItem(IconLabel(u8"\uE124", "Preferences")))
            {
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            if (ImGui::BeginMenu(IconLabel(u8"\uE8BC", "Frame Rate")))
            {
                static int frameRateMode = 1;

                if (ImGui::MenuItem("Unlimited", nullptr, frameRateMode == 0))
                {
                    frameRateMode = 0;
                    engine->GetWindow()->ChangeFrameRate(0);
                }

                if (ImGui::MenuItem("V-Sync", nullptr, frameRateMode == 1))
                {
                    frameRateMode = 1;
                    engine->GetWindow()->ChangeFrameRate(1);
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Graphics"))
        {
            if (ImGui::BeginMenu(IconLabel(u8"\uE2DC", "Lighting")))
            {
                if (ImGui::MenuItem(IconLabel(u8"\uE540", "Atmospheric Light")))
                {
                    if (!AtmosphericLightProperties::visible)
                    {
                        AtmosphericLightProperties::visible = true;
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem(IconLabel(ico::kScene, "Scene Outliner"), nullptr, nullptr);
            ImGui::MenuItem(IconLabel(ico::kName, "Properties"), nullptr, nullptr);
            ImGui::MenuItem(IconLabel(ico::kFolder, "Asset Browser"), nullptr, nullptr);
            ImGui::MenuItem(IconLabel(ico::kCube3D, "Viewport"), nullptr, nullptr);
            ImGui::EndMenu();
        }

        const float windowWidth = ImGui::GetWindowSize().x;

        float dt = engine->GetWindow()->GetDeltaTime();
        timer += dt;

        if (timer >= 0.1f)
        {
            cachedMs = dt * 1000.0f;
            cachedFps = (dt > 0.0f) ? (1.0f / dt) : 0.0f;

            fpsHistory[fpsHistoryIndex] = cachedFps;
            fpsHistoryIndex = (fpsHistoryIndex + 1) % kFpsHistorySize;

            snprintf(buffer, sizeof(buffer), "%.1f fps  %.2f ms", cachedFps, cachedMs);
            timer = 0.0f;
        }

        ImVec4 fpsColor;
        if (cachedFps >= 59.0f)
            fpsColor = kFpsGood;
        else if (cachedFps >= 29.0f)
            fpsColor = kFpsOk;
        else
            fpsColor = kFpsBad;

        float textWidth = ImGui::CalcTextSize(buffer).x;
        float sparklineWidth = 50.0f;
        float totalFpsWidth = sparklineWidth + 6.0f + textWidth;

        ImGui::SameLine(windowWidth - totalFpsWidth - 12.0f);

        ImVec2 sparkPos = ImGui::GetCursorScreenPos();
        sparkPos.y += 2.0f;
        float sparkH = ImGui::GetFrameHeight() - 4.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        float maxFps = 1.0f;
        for (int i = 0; i < kFpsHistorySize; i++)
            if (fpsHistory[i] > maxFps) maxFps = fpsHistory[i];

        for (int i = 0; i < kFpsHistorySize - 1; i++)
        {
            int idx0 = (fpsHistoryIndex + i) % kFpsHistorySize;
            int idx1 = (fpsHistoryIndex + i + 1) % kFpsHistorySize;
            float x0 = sparkPos.x + (float)i / (kFpsHistorySize - 1) * sparklineWidth;
            float x1 = sparkPos.x + (float)(i + 1) / (kFpsHistorySize - 1) * sparklineWidth;
            float y0 = sparkPos.y + sparkH - (fpsHistory[idx0] / maxFps) * sparkH;
            float y1 = sparkPos.y + sparkH - (fpsHistory[idx1] / maxFps) * sparkH;
            ImU32 lineCol = ImGui::ColorConvertFloat4ToU32(ImVec4(fpsColor.x, fpsColor.y, fpsColor.z, 0.6f));
            dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), lineCol, 1.0f);
        }

        ImGui::Dummy(ImVec2(sparklineWidth, 0));
        ImGui::SameLine(0, 6.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, fpsColor);
        ImGui::TextUnformatted(buffer);
        ImGui::PopStyleColor();

        ImGui::EndMainMenuBar();
    }
}