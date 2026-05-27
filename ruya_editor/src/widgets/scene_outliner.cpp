#include "scene_outliner.h"

#include <algorithm>

#include <engine.h>
#include <app_framework/relationship_component.h>
#include <app_framework/id_component.h>
#include <app_framework/render_component.h>
#include <app_framework/transform_component.h>
#include <app_framework/physics_component.h>

#include "editor_widgets.h"
#include "../editor_style.h"

ruya::EntityID editor::SceneOutliner::selectedEntityFromSceneOutliner = 0;
ruya::RyID editor::SceneOutliner::selectedScene = ruya::RyID();
std::unordered_set<uint32_t> editor::SceneOutliner::selectedEntities;
std::unordered_set<uint32_t> editor::SceneOutliner::hiddenEntities;
std::unordered_set<uint32_t> editor::SceneOutliner::lockedEntities;
uint32_t editor::SceneOutliner::lastClickedEntity = 0;
bool editor::SceneOutliner::hasLastClickedEntity = false;

bool editor::SceneOutliner::IsEntityHidden(uint32_t entityId)
{
	return hiddenEntities.find(entityId) != hiddenEntities.end();
}

bool editor::SceneOutliner::IsEntityLocked(uint32_t entityId)
{
	return lockedEntities.find(entityId) != lockedEntities.end();
}

void editor::SceneOutliner::SetEntityHidden(uint32_t entityId, bool hidden)
{
	if (hidden) hiddenEntities.insert(entityId);
	else        hiddenEntities.erase(entityId);
}

void editor::SceneOutliner::SetEntityLocked(uint32_t entityId, bool locked)
{
	if (locked) lockedEntities.insert(entityId);
	else        lockedEntities.erase(entityId);
}

editor::SceneOutliner::EntityTypeStyle
editor::SceneOutliner::GetEntityTypeStyle(uint32_t entityId, ruya::RyID sceneId) const
{
	ruya::Scene* scene = engine->GetApp()->GetScene(sceneId);
	if (scene)
	{
#ifdef RUYA_HAS_LIGHT_COMPONENT
		if (scene->HasComponent<ruya::LightComponent>(entityId))
			return { ico::kLight, kColorHeaderLight };
#endif
		if (scene->HasComponent<ruya::RenderComponent>(entityId))
			return { ico::kRender, kColorMesh };

		if (scene->HasComponent<ruya::PhysicsComponent>(entityId))
			return { ico::kPhysics, kColorHeaderPhysics };
	}

	return { ico::kEntity, ImVec4(0.70f, 0.72f, 0.78f, 1.0f) };
}

bool editor::SceneOutliner::IsEntitySelected(uint32_t entityId)
{
	return selectedEntities.find(entityId) != selectedEntities.end();
}

void editor::SceneOutliner::ClearEntitySelection()
{
	selectedEntities.clear();
	selectedEntityFromSceneOutliner = 0;
	hasLastClickedEntity = false;
}

void editor::SceneOutliner::SelectEntity(uint32_t entityId, bool additive)
{
	if (additive)
	{
		auto it = selectedEntities.find(entityId);
		if (it != selectedEntities.end())
		{
			selectedEntities.erase(it);
			if (selectedEntityFromSceneOutliner == entityId)
			{
				selectedEntityFromSceneOutliner = selectedEntities.empty() ? 0 : *selectedEntities.begin();
			}
		}
		else
		{
			selectedEntities.insert(entityId);
			selectedEntityFromSceneOutliner = entityId;
		}
	}
	else
	{
		selectedEntities.clear();
		selectedEntities.insert(entityId);
		selectedEntityFromSceneOutliner = entityId;
	}
}

void editor::SceneOutliner::RequestDeleteEntities(const std::vector<uint32_t>& entities, ruya::RyID sceneId)
{
	if (entities.empty() || !sceneId.IsValid())
		return;

	pendingDeleteEntities = entities;
	pendingDeleteScene = sceneId;
	pendingDeleteRequest = true;
}

void editor::SceneOutliner::DrawDeleteConfirmationPopup()
{
	if (pendingDeleteRequest)
	{
		ImGui::OpenPopup("Delete Entities?");
		pendingDeleteRequest = false;
	}

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Delete Entities?", nullptr,
		ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
	{
		const size_t count = pendingDeleteEntities.size();

		if (count == 1)
		{
			ruya::Scene* scene = engine->GetApp()->GetScene(pendingDeleteScene);
			std::string entityName = "this entity";
			if (scene)
			{
				ruya::IdComponent* idComp = scene->TryGetComponent<ruya::IdComponent>(pendingDeleteEntities[0]);
				if (idComp && !idComp->name.empty())
					entityName = "\"" + idComp->name + "\"";
				else
					entityName = "Entity " + std::to_string(pendingDeleteEntities[0]);
			}
			ImGui::Text("Are you sure you want to delete %s?", entityName.c_str());
		}
		else
		{
			ImGui::Text("Are you sure you want to delete %zu entities?", count);
		}

		ImGui::TextDisabled("This action cannot be undone.");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		const float buttonWidth = 120.0f;
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float totalWidth = buttonWidth * 2.0f + spacing;
		const float avail = ImGui::GetContentRegionAvail().x;
		if (avail > totalWidth)
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - totalWidth) * 0.5f);

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.20f, 0.20f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.60f, 0.15f, 0.15f, 1.0f));

		bool confirmed = ImGui::Button(IconLabel(u8"\uE650", "Delete"), ImVec2(buttonWidth, 0));
		ImGui::PopStyleColor(3);

		if (confirmed || ImGui::IsKeyPressed(ImGuiKey_Enter, false))
		{
			ruya::Scene* scene = engine->GetApp()->GetScene(pendingDeleteScene);
			if (scene)
			{
				for (uint32_t entId : pendingDeleteEntities)
					DeleteEntity(static_cast<ruya::EntityID>(entId), pendingDeleteScene);
			}

			ClearEntitySelection();
			pendingDeleteEntities.clear();
			pendingDeleteScene = ruya::RyID();
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)) ||
			ImGui::IsKeyPressed(ImGuiKey_Escape, false))
		{
			pendingDeleteEntities.clear();
			pendingDeleteScene = ruya::RyID();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void editor::SceneOutliner::Draw()
{
	visibleEntityOrder.clear();

	const char* title = IconLabel(u8"\uEE48", "Scene Outliner");

	ImGui::Begin(title);

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
	float searchWidth = ImGui::GetContentRegionAvail().x;
	ImGui::SetNextItemWidth(searchWidth);
	ImGui::InputTextWithHint("##SceneSearch",
		IconLabel(u8"\uE30C", "Search entities..."),
		searchFilter, sizeof(searchFilter));
	ImGui::PopStyleVar();

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 12.0f);

	std::vector<ruya::RyID> sceneIds = engine->GetApp()->GetLoadedScenes();

	if (!sceneIds.empty())
	{
		bool selectedSceneStillLoaded = false;
		if (selectedScene.IsValid())
		{
			for (ruya::RyID id : sceneIds)
			{
				if (id == selectedScene)
				{
					selectedSceneStillLoaded = true;
					break;
				}
			}
		}

		if (!selectedScene.IsValid() || !selectedSceneStillLoaded)
		{
			selectedScene = sceneIds.front();
			ClearEntitySelection();
		}
	}

	for (ruya::RyID id : sceneIds)
	{
		std::string sceneLabel = engine->GetApp()->GetScene(id)->GetName();

		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_OpenOnDoubleClick |
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_DefaultOpen;

		if (selectedScene == id)
			flags |= ImGuiTreeNodeFlags_Selected;

		flags |= ImGuiTreeNodeFlags_FramePadding;

		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertFloat4ToU32(kColorScene));
		bool opened = ImGui::TreeNodeEx(
			(void*)(uint64_t)(id.GetRawID() + 100000),
			flags,
			"%s %s", IconStr(u8"\uE7AE"), sceneLabel.c_str()
		);
		ImGui::PopStyleColor();

		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		{
			selectedScene = id;
			ClearEntitySelection();
		}

		char sceneCtxMenuId[64];
		snprintf(sceneCtxMenuId, sizeof(sceneCtxMenuId), "SceneCtx_%u", id.GetRawID());
		if (ImGui::BeginPopupContextItem(sceneCtxMenuId))
		{
			selectedScene = id;

			ruya::Scene* scene = engine->GetApp()->GetScene(id);
			if (scene)
			{
				ImGui::TextDisabled("%s", scene->GetName().c_str());
				ImGui::Separator();
			}

			if (ImGui::MenuItem(IconLabel(u8"\uE872", "Unload Scene")))
			{
				engine->GetApp()->UnloadScene(id);
				selectedScene = ruya::RyID();
				ClearEntitySelection();
			}

			ImGui::Separator();

			if (ImGui::MenuItem(IconLabel(u8"\uE248", "Save Scene")))
			{
				ruya::UUID assetUUID = engine->GetApp()->GetSceneAssetUUID(id);
				if (assetUUID != ruya::UUID::Invalid())
				{
					ruya::Scene* sToSave = engine->GetApp()->GetScene(id);
					engine->GetAssetManager()->SaveScene(assetUUID, sToSave);
					RUYA_LOG_INFO("Scene saved successfully.");
				}
				else
				{
					RUYA_LOG_ERROR("Cannot save scene: no associated asset UUID found. Did you create it via Asset Browser?");
				}
			}

			ImGui::Separator();

			if (ImGui::MenuItem(IconLabel(u8"\uE145", "Create Entity")))
			{
				ruya::Scene* s = engine->GetApp()->GetScene(id);
				if (s)
				{
					auto newEntity = s->CreateEntity();
					SelectEntity(static_cast<uint32_t>(newEntity), false);
				}
			}

			ImGui::EndPopup();
		}

		if (id.IsValid() && ImGui::BeginDragDropTarget())
		{
			struct MeshDragPayload { ruya::UUID uuid; char name[128]; };
			struct MeshDragPayloadMulti { uint32_t count; ruya::UUID uuids[64]; char names[64][128]; };

			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MESH"))
			{
				const MeshDragPayload* meshPayload = static_cast<const MeshDragPayload*>(payload->Data);
				ruya::RyAsset* default_material = engine->GetAssetManager()->GetAssetByPath("ruya_files/materials/default_material.ryasset");
				ruya::Scene* scene = engine->GetApp()->GetScene(id);
				ruya::EntityID entityID = scene->CreateEntity();
				scene->AddComponent<ruya::RenderComponent>(entityID);
				ruya::RenderComponent* renderComponent = scene->TryGetComponent<ruya::RenderComponent>(entityID);
				renderComponent->meshUUID = meshPayload->uuid;
				renderComponent->materialUUID = default_material->uuid;
				scene->TryGetComponent<ruya::TransformComponent>(entityID)->position = engine->GetAssetManager()->GetGLTFInfo(renderComponent->meshUUID)->initialPosition;
				if (ruya::IdComponent* idComp = scene->TryGetComponent<ruya::IdComponent>(static_cast<uint32_t>(entityID)))
					idComp->name = meshPayload->name;
				selectedScene = id;
				SelectEntity(static_cast<uint32_t>(entityID), false);
			}

			else if (const ImGuiPayload* payloadMulti = ImGui::AcceptDragDropPayload("ASSET_MESH_MULTI"))
			{
				const MeshDragPayloadMulti* multi = static_cast<const MeshDragPayloadMulti*>(payloadMulti->Data);
				ruya::RyAsset* default_material = engine->GetAssetManager()->GetAssetByPath("ruya_files/materials/default_material.ryasset");
				ruya::Scene* scene = engine->GetApp()->GetScene(id);

				selectedScene = id;
				ClearEntitySelection();

				for (uint32_t i = 0; i < multi->count; ++i)
				{
					ruya::EntityID entityID = scene->CreateEntity();
					scene->AddComponent<ruya::RenderComponent>(entityID);
					ruya::RenderComponent* renderComponent = scene->TryGetComponent<ruya::RenderComponent>(entityID);
					renderComponent->meshUUID = multi->uuids[i];
					renderComponent->materialUUID = default_material->uuid;
					scene->TryGetComponent<ruya::TransformComponent>(entityID)->position = engine->GetAssetManager()->GetGLTFInfo(renderComponent->meshUUID)->initialPosition;
					if (ruya::IdComponent* idComp = scene->TryGetComponent<ruya::IdComponent>(static_cast<uint32_t>(entityID)))
						idComp->name = multi->names[i];

					selectedEntities.insert(static_cast<uint32_t>(entityID));
					selectedEntityFromSceneOutliner = static_cast<uint32_t>(entityID);
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (!id.IsValid())
		{
			if (opened)
				ImGui::TreePop();
			continue;
		}

		if (opened)
		{
			auto entities = engine->GetApp()->GetScene(id)->GetSceneViewByComponentsType<ruya::RelationshipComponent>();
			for (auto [entity, relationshipComponent] : entities.each())
			{
				if (relationshipComponent.parentEntity == entt::null)
				{
					DrawEntityNode(entity, id);
				}
			}
			ImGui::TreePop();
		}
	}

	if (ImGui::BeginPopupContextWindow("SceneOutlinerEmptyContextMenu",
		ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem(IconLabel(u8"\uE145", "Create Entity")))
		{
			if (selectedScene.IsValid())
			{
				auto scene = engine->GetApp()->GetScene(selectedScene);
				auto newEntity = scene->CreateEntity();
				SelectEntity(static_cast<uint32_t>(newEntity), false);
			}
		}

		ImGui::EndPopup();
	}

	if (pendingClickValid)
	{
		if (pendingClickedScene != selectedScene)
		{
			selectedScene = pendingClickedScene;
			ClearEntitySelection();
		}

		const bool ctrl = pendingClickCtrl;
		const bool shift = pendingClickShift;
		const uint32_t clickedId = pendingClickedEntity;

		if (shift && hasLastClickedEntity)
		{
			int anchorIdx = -1;
			int currentIdx = -1;
			for (int k = 0; k < (int)visibleEntityOrder.size(); ++k)
			{
				if (visibleEntityOrder[k] == lastClickedEntity) anchorIdx = k;
				if (visibleEntityOrder[k] == clickedId)         currentIdx = k;
			}

			if (anchorIdx >= 0 && currentIdx >= 0)
			{
				if (!ctrl)
					selectedEntities.clear();

				int a = std::min(anchorIdx, currentIdx);
				int b = std::max(anchorIdx, currentIdx);
				for (int k = a; k <= b; ++k)
					selectedEntities.insert(visibleEntityOrder[k]);

				selectedEntityFromSceneOutliner = clickedId;
			}
			else
			{
				SelectEntity(clickedId, ctrl);
				lastClickedEntity = clickedId;
				hasLastClickedEntity = true;
			}
		}
		else
		{
			SelectEntity(clickedId, ctrl);
			lastClickedEntity = clickedId;
			hasLastClickedEntity = true;
		}

		pendingClickValid = false;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
		!selectedEntities.empty() &&
		selectedScene.IsValid() &&
		ImGui::IsKeyPressed(ImGuiKey_Delete, false))
	{
		std::vector<uint32_t> targets(selectedEntities.begin(), selectedEntities.end());
		RequestDeleteEntities(targets, selectedScene);
	}

	DrawDeleteConfirmationPopup();

	ImGui::PopStyleVar();

	ImGui::End();
}

void editor::SceneOutliner::DrawEntityNode(ruya::EntityID entity, ruya::RyID sceneId)
{
	const uint32_t entityId = static_cast<uint32_t>(entity);

	ruya::IdComponent* idComp = engine->GetApp()->GetScene(sceneId)->TryGetComponent<ruya::IdComponent>(entityId);
	std::string entityName;
	if (idComp && !idComp->name.empty())
	{
		entityName = idComp->name;
	}
	else
	{
		entityName = "Entity " + std::to_string(entityId);
	}

	if (searchFilter[0] != '\0')
	{
		std::string lowerName = entityName;
		std::string lowerFilter = searchFilter;
		for (auto& c : lowerName) c = (char)tolower(c);
		for (auto& c : lowerFilter) c = (char)tolower(c);
		if (lowerName.find(lowerFilter) == std::string::npos)
			return;
	}

	visibleEntityOrder.push_back(entityId);

	ruya::RelationshipComponent* rel = engine->GetApp()->GetScene(sceneId)->TryGetComponent<ruya::RelationshipComponent>(entityId);

	const bool selected = (sceneId == selectedScene) && IsEntitySelected(entityId);
	const bool hidden = IsEntityHidden(entityId);
	const bool locked = IsEntityLocked(entityId);

	const EntityTypeStyle ts = GetEntityTypeStyle(entityId, sceneId);

	ImGui::PushID((int)entityId);

	ImGuiTreeNodeFlags flags =
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_FramePadding;

	if (rel->firstChildEntity == entt::null)
		flags |= ImGuiTreeNodeFlags_Leaf;

	if (selected)
		flags |= ImGuiTreeNodeFlags_Selected;

	const float iconBtn = ImGui::GetFrameHeight();

	const ImVec2 rowMin = ImGui::GetCursorScreenPos();
	const float  rowH = ImGui::GetFrameHeight();
	const float  fullW = ImGui::GetContentRegionAvail().x;
	const ImVec2 rowMax = ImVec2(rowMin.x + fullW, rowMin.y + rowH);

	const bool rowHovered = ImGui::IsMouseHoveringRect(rowMin, rowMax);

	ImDrawList* dl = ImGui::GetWindowDrawList();

	if (selected)
		dl->AddRectFilled(rowMin, rowMax, ImGui::ColorConvertFloat4ToU32(kRowSelBg), 3.0f);
	else if (rowHovered)
		dl->AddRectFilled(rowMin, rowMax, ImGui::ColorConvertFloat4ToU32(kRowHoverBg), 3.0f);

	bool pushedDim = false;
	if (hidden || locked)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, kRowLockedText);
		pushedDim = true;
	}

	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));

	ImVec4 iconColor = hidden
		? ImVec4(ts.color.x, ts.color.y, ts.color.z, 0.45f)
		: ts.color;

	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertFloat4ToU32(iconColor));
	bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entity, flags,
		"%s %s", reinterpret_cast<const char*>(ts.icon), entityName.c_str());
	ImGui::PopStyleColor();

	ImGui::PopStyleColor(3);

	const bool nodeClicked = ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
	const bool nodeToggledOpen = ImGui::IsItemToggledOpen();

	if (selected)
	{
		dl->AddRectFilled(
			ImVec2(rowMin.x, rowMin.y),
			ImVec2(rowMin.x + 3.0f, rowMax.y),
			ImGui::ColorConvertFloat4ToU32(kRowSelStripe), 0.0f);
	}

	if (pushedDim)
		ImGui::PopStyleColor();

	const bool showRowIcons = rowHovered || hidden || locked || selected;
	if (showRowIcons)
	{
		const float avail = ImGui::GetContentRegionMax().x;

		ImGui::SameLine(avail - iconBtn * 2.0f - 6.0f);
		if (IconToggleButton("##vis", ico::kEyeOpen, ico::kEyeClosed,
			!hidden, iconBtn, hidden ? "Hidden — click to show" : "Visible — click to hide"))
		{
			SetEntityHidden(entityId, !hidden);
		}

		ImGui::SameLine(avail - iconBtn - 2.0f);
		if (IconToggleButton("##lock", ico::kLockClosed, ico::kLockOpen,
			locked, iconBtn, locked ? "Locked — click to unlock" : "Unlocked — click to lock"))
		{
			SetEntityLocked(entityId, !locked);
		}
	}

	if (ImGui::BeginPopupContextItem())
	{
		const uint32_t clickedId = entityId;
		if (!IsEntitySelected(clickedId))
		{
			selectedScene = sceneId;
			SelectEntity(clickedId, false);
			lastClickedEntity = clickedId;
			hasLastClickedEntity = true;
		}

		ImGui::TextDisabled("%s", entityName.c_str());
		ImGui::Separator();

		if (ImGui::MenuItem(IconLabel(hidden ? ico::kEyeClosed : ico::kEyeOpen,
			hidden ? "Show" : "Hide")))
		{
			SetEntityHidden(clickedId, !hidden);
		}
		if (ImGui::MenuItem(IconLabel(locked ? ico::kLockClosed : ico::kLockOpen,
			locked ? "Unlock" : "Lock")))
		{
			SetEntityLocked(clickedId, !locked);
		}

		ImGui::Separator();

		if (ImGui::MenuItem(IconLabel(u8"\uE24C", "Duplicate")))
		{
		}

		if (ImGui::MenuItem(IconLabel(ico::kPlus, "Create Empty Child")))
		{
			auto scene = engine->GetApp()->GetScene(sceneId);
			if (scene)
			{
				ruya::EntityID childEntity = scene->CreateEntity();

				ruya::RelationshipComponent* childRel = scene->TryGetComponent<ruya::RelationshipComponent>(static_cast<uint32_t>(childEntity));
				ruya::RelationshipComponent* parentRel = scene->TryGetComponent<ruya::RelationshipComponent>(entityId);

				if (childRel && parentRel)
				{
					childRel->parentEntity = entity;

					childRel->nextEntity = parentRel->firstChildEntity;
					parentRel->firstChildEntity = childEntity;
				}

				SelectEntity(static_cast<uint32_t>(childEntity), false);
			}
		}

		ImGui::Separator();

		if (ImGui::MenuItem(IconLabel(u8"\uE650", "Delete")))
		{
			const uint32_t clickedIdLocal = entityId;
			const bool clickedIsSelected = IsEntitySelected(clickedIdLocal);

			std::vector<uint32_t> targets;
			if (clickedIsSelected && selectedEntities.size() > 1)
			{
				targets.assign(selectedEntities.begin(), selectedEntities.end());
			}
			else
			{
				targets.push_back(clickedIdLocal);
			}

			RequestDeleteEntities(targets, sceneId);
		}

		ImGui::EndPopup();
	}

	if (nodeClicked && !nodeToggledOpen)
	{
		ImGuiIO& io = ImGui::GetIO();
		pendingClickValid = true;
		pendingClickedEntity = entityId;
		pendingClickedScene = sceneId;
		pendingClickCtrl = io.KeyCtrl;
		pendingClickShift = io.KeyShift;
	}

	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
	{
		uint32_t dragId = entityId;
		ImGui::SetDragDropPayload("ENTITY_REPARENT", &dragId, sizeof(uint32_t));
		IconTextColored(ts.color, ts.icon, entityName.c_str());
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_REPARENT"))
		{
		}
		ImGui::EndDragDropTarget();
	}

	if (opened)
	{
		ruya::EntityID child = rel->firstChildEntity;
		while (child != entt::null)
		{
			DrawEntityNode(child, sceneId);
			child = engine->GetApp()->GetScene(sceneId)->TryGetComponent<ruya::RelationshipComponent>(
				static_cast<uint32_t>(child)
			)->nextEntity;
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

void editor::SceneOutliner::DeleteEntity(ruya::EntityID entity, ruya::RyID sceneId)
{
	if (entity == entt::null) return;

	auto sceneRef = engine->GetApp()->GetScene(sceneId);
	if (!sceneRef) return;

	if (ruya::RelationshipComponent* rel = sceneRef->TryGetComponent<ruya::RelationshipComponent>(entity))
	{
		ruya::EntityID child = rel->firstChildEntity;
		while (child != entt::null)
		{
			ruya::EntityID next = entt::null;
			if (ruya::RelationshipComponent* childRel = sceneRef->TryGetComponent<ruya::RelationshipComponent>(child))
				next = childRel->nextEntity;

			DeleteEntity(child, sceneId);
			child = next;
		}
	}

	if (ruya::RenderComponent* rc = sceneRef->TryGetComponent<ruya::RenderComponent>(entity))
	{
		engine->GetGraphics()->DestroyRenderItem(rc->renderItemID);
	}

	if (ruya::PhysicsComponent* pc = sceneRef->TryGetComponent<ruya::PhysicsComponent>(entity))
	{
		engine->GetPhysics()->DestroyRiggedBody(pc->riggidBodyRyID);
	}

	sceneRef->DestroyEntity(entity);
}