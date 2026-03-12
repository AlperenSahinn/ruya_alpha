#pragma once
#include "widget.h"

#include <core/ry_id.h>
#include <app_framework/scene.h>

namespace editor
{
	class SceneOutliner : public Widget
	{
	public:
		SceneOutliner() = default;
		~SceneOutliner() = default;

		SceneOutliner(const SceneOutliner&) = delete;
		SceneOutliner& operator=(const SceneOutliner&) = delete;

		void Draw() override;
		void DrawEntityNode(ruya::EntityID entity, ruya::RyID sceneId);

	public:
		static ruya::EntityID selectedEntityFromSceneOutliner;
		static ruya::RyID selectedScene;
	};
}