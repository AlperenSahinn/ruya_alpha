#pragma once
#include "widget.h"

#include <core/ry_id.h>
#include <app_framework/scene.h>

#include <unordered_set>
#include <vector>

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

		static bool IsEntitySelected(uint32_t entityId);
		static void ClearEntitySelection();
		static void SelectEntity(uint32_t entityId, bool additive);
		static const std::unordered_set<uint32_t>& GetSelectedEntities() { return selectedEntities; }

		static void DeleteEntity(ruya::EntityID entity, ruya::RyID sceneId);

		static bool IsEntityHidden(uint32_t entityId);
		static bool IsEntityLocked(uint32_t entityId);
		static void SetEntityHidden(uint32_t entityId, bool hidden);
		static void SetEntityLocked(uint32_t entityId, bool locked);

	public:
		static ruya::EntityID selectedEntityFromSceneOutliner;
		static ruya::RyID selectedScene;

		static std::unordered_set<uint32_t> selectedEntities;

	private:
		static std::unordered_set<uint32_t> hiddenEntities;
		static std::unordered_set<uint32_t> lockedEntities;

		struct EntityTypeStyle { const char8_t* icon; ImVec4 color; };
		EntityTypeStyle GetEntityTypeStyle(uint32_t entityId, ruya::RyID sceneId) const;

	private:
		char searchFilter[128] = {};

		std::vector<uint32_t> visibleEntityOrder;
		static uint32_t lastClickedEntity;
		static bool hasLastClickedEntity;

		bool   pendingClickValid = false;
		uint32_t pendingClickedEntity = 0;
		ruya::RyID pendingClickedScene;
		bool   pendingClickCtrl = false;
		bool   pendingClickShift = false;
		bool pendingDeleteRequest = false;
		std::vector<uint32_t> pendingDeleteEntities;
		ruya::RyID pendingDeleteScene;

		void RequestDeleteEntities(const std::vector<uint32_t>& entities, ruya::RyID sceneId);
		void DrawDeleteConfirmationPopup();
	};
}