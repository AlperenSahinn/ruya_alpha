#include "directional_light_properties.h"

#include <engine.h>
#include <app_framework/lighting_system.h>

bool editor::DirectionalLightProperties::visible = false;

void editor::DirectionalLightProperties::Draw()
{
	if (visible)
	{
		const char8_t* utf8_directionalLightPropertiesIcon = u8"\uE540 Directional Light Properties";
		const char* directionalLightPropertiesIcon = reinterpret_cast<const char*>(utf8_directionalLightPropertiesIcon);

		if (ruya::LightingSystem* lightingSystem = engine->GetApp()->GetScene(engine->GetApp()->GetLoadedScenes()[0])->TryGetSceneSystem<ruya::LightingSystem>("LightingSystem"))
		{
			if (ImGui::Begin(directionalLightPropertiesIcon, &visible))
			{
				auto* directionalLight = lightingSystem->GetDirectionalLight();

				ImGui::Text("Light Direction");
				ImGui::DragFloat3("Direction", &directionalLight->sunlightDirection.x, 0.01f, -1.0f, 1.0f);

				ImGui::Text("Light Strength");
				ImGui::DragFloat("Direct Light Strength", &directionalLight->sunlightColor.w, 0.01f, 0.0f, 100.0f);

				ImGui::Text("Ambient Light Strength");
				ImGui::DragFloat("Ambient Light Strength", &directionalLight->ambientColor.w, 0.01f, 0.0f, 100.0f);
			}
			ImGui::End();
		}
	}
}
