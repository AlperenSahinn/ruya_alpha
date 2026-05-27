#include "atmospheric_light_properties.h"

#include <cmath>
#include <algorithm>

#include <engine.h>
#include <app_framework/lighting_system.h>

#include "../editor_style.h"
#include "editor_widgets.h"

bool editor::AtmosphericLightProperties::visible = false;

void editor::AtmosphericLightProperties::Draw()
{
    if (!visible)
        return;

    const char* title = IconLabel(u8"\uE540", "Atmospheric Light Properties");

    if (engine->GetApp()->GetLoadedScenes().empty())
        return;

    ruya::LightingSystem* lightingSystem =
        engine->GetApp()->GetScene(engine->GetApp()->GetLoadedScenes()[0])
        ->TryGetSceneSystem<ruya::LightingSystem>("LightingSystem");

    if (!lightingSystem) return;

    if (!ImGui::Begin(title, &visible))
    {
        ImGui::End();
        return;
    }

    ImGui::SetColorEditOptions(ImGuiColorEditFlags_PickerHueWheel);

    auto* dl = lightingSystem->GetDirectionalLight();

    // ─────────────────────────────────────────────────────────────────
    // Sun
    // ─────────────────────────────────────────────────────────────────
    if (ComponentHeader(u8"\uE472", "Sun", kColorHeaderLight))
    {
        ImGui::Indent(8.0f);

        static int s_directionMode = 0;

        static const char* kDirModes[] = { "Spherical (Elev/Azim)", "Cartesian (XYZ)" };
        BeginPropertyTable({ "Input Mode" });
        Combo("Input Mode", &s_directionMode, kDirModes, IM_ARRAYSIZE(kDirModes),
            "Sun direction input style. Spherical: elevation + azimuth angles. Cartesian: raw XYZ vector.");
        EndPropertyTable();

        if (s_directionMode == 0)
        {
            float dx = dl->sunDirection.x;
            float dy = dl->sunDirection.y;
            float dz = dl->sunDirection.z;
            float len = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (len > 1e-6f) { dx /= len; dy /= len; dz /= len; }
            else { dx = 0.0f; dy = 1.0f; dz = 0.0f; }

            float elevationDeg = std::asin(std::clamp(dy, -1.0f, 1.0f))
                * (180.0f / 3.14159265358979323846f);
            float azimuthDeg = std::atan2(dx, dz)
                * (180.0f / 3.14159265358979323846f);

            bool changed = false;

            BeginPropertyTable({ "Elevation", "Azimuth", "Sun Color", "Intensity" });

            changed |= DragFloat("Elevation", &elevationDeg,
                0.5f, -90.0f, 90.0f, "%.1f deg",
                "Vertical angle. 0 = horizon, 90 = zenith (overhead), -90 = straight down.");
            changed |= DragFloat("Azimuth", &azimuthDeg,
                0.5f, -180.0f, 180.0f, "%.1f deg",
                "Horizontal angle. 0 = +Z (north), 90 = +X (east).");

            if (changed)
            {
                float e = elevationDeg * (3.14159265358979323846f / 180.0f);
                float a = azimuthDeg * (3.14159265358979323846f / 180.0f);
                float ce = std::cos(e);
                dl->sunDirection.x = ce * std::sin(a);
                dl->sunDirection.y = std::sin(e);
                dl->sunDirection.z = ce * std::cos(a);
            }

            ColorEdit3("Sun Color", &dl->sunColor.x);
            DragFloat("Intensity", &dl->sunColor.w,
                0.1f, 0.0f, 100.0f, "%.2f",
                "HDR multiplier for sun brightness (e.g. 15.0)");

            EndPropertyTable();
        }
        else
        {
            BeginPropertyTable({ "Direction", "Sun Color", "Intensity" });

            DragVec3("Direction", &dl->sunDirection.x,
                0.0f, 0.01f, -1.0f, 1.0f, "%.3f");

            ColorEdit3("Sun Color", &dl->sunColor.x);
            DragFloat("Intensity", &dl->sunColor.w,
                0.1f, 0.0f, 100.0f, "%.2f",
                "HDR multiplier for sun brightness (e.g. 15.0)");

            EndPropertyTable();

            ImGui::TextDisabled("%s Normalized sun direction vector", IconStr(u8"\uE88F"));
        }

        ImGui::Unindent(8.0f);
    }

    // ─────────────────────────────────────────────────────────────────
    // Sky & Atmosphere
    // ─────────────────────────────────────────────────────────────────
    if (ComponentHeader(u8"\uE1AA", "Sky & Atmosphere", kColorHeaderLight))
    {
        ImGui::Indent(8.0f);

        BeginPropertyTable({ "Sky Exposure", "Sun Disk Size", "Sun Sharpness" });

        DragFloat("Sky Exposure", &dl->skyParams.z, 0.01f, 0.0f, 10.0f, "%.2f");
        DragFloat("Sun Disk Size", &dl->skyParams.x, 0.0001f, 0.9f, 1.0f, "%.4f",
            "Angular size of the sun disk in the sky (closer to 1.0 = smaller).");
        DragFloat("Sun Sharpness", &dl->skyParams.y, 0.0001f, 0.9f, 1.0f, "%.4f",
            "Edge falloff of the sun disk.");

        EndPropertyTable();

        ImGui::Unindent(8.0f);
    }

    // ─────────────────────────────────────────────────────────────────
    // Scattering
    // ─────────────────────────────────────────────────────────────────
    if (ComponentHeader(u8"\uEAAC", "Scattering", kColorHeaderLight))
    {
        ImGui::Indent(8.0f);

        BeginPropertyTable({ "Mie Beta", "Mie Asymmetry (G)", "Rayleigh Height", "Mie Height" });

        DragFloat("Mie Beta", &dl->scatterParams.x, 0.000001f, 0.0f, 0.0001f, "%.6f",
            "Aerosol/particle scattering coefficient. Default: 0.000021");
        DragFloat("Mie Asymmetry (G)", &dl->scatterParams.y, 0.01f, -0.99f, 0.99f, "%.2f",
            "Forward (positive) or backward (negative) light scattering. Default: 0.76");
        DragFloat("Rayleigh Height", &dl->scatterParams.z, 10.0f, 100.0f, 20000.0f, "%.0f m",
            "Atmospheric scale height for Rayleigh (air molecule) scattering.");
        DragFloat("Mie Height", &dl->scatterParams.w, 10.0f, 10.0f, 10000.0f, "%.0f m",
            "Atmospheric scale height for Mie (aerosol) scattering.");

        EndPropertyTable();

        ImGui::Unindent(8.0f);
    }

    // ─────────────────────────────────────────────────────────────────
    // Environment & Fog
    // ─────────────────────────────────────────────────────────────────
    if (ComponentHeader(u8"\uE53C", "Environment & Fog", kColorHeaderLight))
    {
        ImGui::Indent(8.0f);

        BeginPropertyTable({ "Ground Albedo", "Reflectance", "Fog Density" });

        ColorEdit3("Ground Albedo", &dl->groundAlbedo.x,
            "Color of the ground used for bottom-up bounce lighting.");
        DragFloat("Reflectance", &dl->groundAlbedo.w, 0.01f, 0.0f, 1.0f, "%.2f",
            "How much light the ground bounces back into the atmosphere.");
        DragFloat("Fog Density", &dl->skyParams.w, 0.0001f, 0.0f, 0.1f, "%.4f",
            "Density of distance-based atmospheric fog. 0.0 disables fog entirely.");

        EndPropertyTable();

        ImGui::Unindent(8.0f);
    }

    ImGui::End();
}