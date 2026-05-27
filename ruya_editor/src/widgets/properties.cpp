#include "properties.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <engine.h>
#include <app_framework/id_component.h>
#include <app_framework/relationship_component.h>
#include <app_framework/transform_component.h>
#include <app_framework/render_component.h>
#include <app_framework/physics_component.h>

#include "../editor_style.h"
#include "editor_widgets.h"
#include "scene_outliner.h"

extern ImFont* g_FontBold;

void editor::Properties::Draw()
{
	const char* propertiesTitle = IconLabel(u8"\uE5D4", "Properties");

	ruya::RyID scene = SceneOutliner::selectedScene;

	ImGui::Begin(propertiesTitle);

	if (!scene.IsValid() || engine->GetApp()->GetScene(scene)->IsEmpty())
	{
		EmptyState(u8"\uE2C8", "No entity selected",
			"Pick an entity in the Scene Outliner to inspect it.");
		ImGui::End();
		return;
	}

	uint32_t entity = SceneOutliner::selectedEntityFromSceneOutliner;
	auto sceneRef = engine->GetApp()->GetScene(scene);

	ruya::IdComponent* idComponent = sceneRef->TryGetComponent<ruya::IdComponent>(entity);
	if (idComponent)
	{
		const char8_t* typeIcon = ico::kEntity;
		ImVec4 typeColor = ImVec4(0.70f, 0.72f, 0.78f, 1.0f);
		const char* typeName = "Entity";
		if (sceneRef->HasComponent<ruya::RenderComponent>(entity))
		{
			typeIcon = ico::kRender; typeColor = kColorMesh; typeName = "Mesh";
		}
		else if (sceneRef->HasComponent<ruya::PhysicsComponent>(entity))
		{
			typeIcon = ico::kPhysics; typeColor = kColorHeaderPhysics; typeName = "Physics Body";
		}

		BeginBanner(60.0f);

		ImGui::AlignTextToFramePadding();
		TextColored(typeColor, IconStr(typeIcon));
		ImGui::SameLine();
		ImGui::TextUnformatted("Name:");
		ImGui::SameLine();

		char nameBuffer[128];
		strncpy(nameBuffer, idComponent->name.c_str(), sizeof(nameBuffer) - 1);
		nameBuffer[sizeof(nameBuffer) - 1] = '\0';

		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 20.0f);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.07f, 0.08f, 0.09f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.11f, 0.12f, 0.14f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.13f, 0.14f, 0.17f, 1.0f));
		if (g_FontBold) ImGui::PushFont(g_FontBold);
		if (ImGui::InputText("##EntityName", nameBuffer, sizeof(nameBuffer)))
			idComponent->name = nameBuffer;
		if (g_FontBold) ImGui::PopFont();
		ImGui::PopStyleColor(3);
		ImGui::PopItemWidth();

		ImGui::TextDisabled("%s  \u2022  EntityID: %s",
			typeName, std::to_string(idComponent->enttID).c_str());

		EndBanner();
	}

	// ─────────────────────────────────────────────────────────────────
	// Transform
	// ─────────────────────────────────────────────────────────────────
	if (ruya::TransformComponent* tc = sceneRef->TryGetComponent<ruya::TransformComponent>(entity))
	{
		if (ComponentHeader(u8"\uE0A4", "Transform", kColorHeaderTransform))
		{
			ImGui::Indent(8.0f);

			bool changed = false;

			BeginPropertyTable({ "Position", "Rotation", "Scale" });

			if (DragVec3("Position", glm::value_ptr(tc->position), 0.0f, 0.1f)) changed = true;

			glm::vec3 eulerRotation = glm::degrees(glm::eulerAngles(tc->rotation));
			if (DragVec3("Rotation", glm::value_ptr(eulerRotation), 0.0f, 0.5f))
			{
				glm::vec3 radiansVec = glm::radians(eulerRotation);
				tc->rotation = glm::quat(radiansVec);
				changed = true;
			}

			if (DragVec3("Scale", glm::value_ptr(tc->scale), 1.0f, 0.01f)) changed = true;

			EndPropertyTable();

			if (changed) UpdateTransformRecursive(entity);

			const float btnW = 130.0f;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - btnW);
			if (ImGui::Button("Reset Transform", ImVec2(btnW, 0)))
			{
				tc->position = glm::vec3(0.0f);
				tc->rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
				tc->scale = glm::vec3(1.0f);
				UpdateTransformRecursive(entity);
			}

			ImGui::Unindent(8.0f);
		}
	}

	// ─────────────────────────────────────────────────────────────────
	// Render
	// ─────────────────────────────────────────────────────────────────
	if (ruya::RenderComponent* rc = sceneRef->TryGetComponent<ruya::RenderComponent>(entity))
	{
		bool removeRender = false;
		if (ComponentHeader(u8"\uE1DA", "Render", kColorHeaderRender, &removeRender, true))
		{
			ImGui::Indent(8.0f);

			BeginPropertyTable({ "Render Item", "Mesh", "Material", "Visible" });

			TextRow("Render Item", std::to_string(rc->renderItemID).c_str());

			ruya::RyAsset* meshAsset = engine->GetAssetManager()->TryGetAsset(rc->meshUUID);
			AssetSlot("Mesh",
				meshAsset ? meshAsset->assetName.c_str() : "None",
				meshAsset != nullptr,
				"None  (drop mesh here)");

			ruya::RyAsset* matAsset = engine->GetAssetManager()->TryGetAsset(rc->materialUUID);
			AssetSlot("Material",
				matAsset ? matAsset->assetName.c_str() : "None",
				matAsset != nullptr,
				"None  (drop material here)");

			Checkbox("Visible", &rc->draw);

			EndPropertyTable();

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MATERIAL"))
				{
					if (rc->materialUUID != *(const ruya::UUID*)payload->Data)
					{
						IM_ASSERT(payload->DataSize == sizeof(ruya::UUID));
						const ruya::UUID newMaterial = *(const ruya::UUID*)payload->Data;

						const auto& selected = SceneOutliner::GetSelectedEntities();
						if (selected.size() > 1)
						{
							for (uint32_t entId : selected)
							{
								ruya::RenderComponent* otherRc = sceneRef->TryGetComponent<ruya::RenderComponent>(entId);
								if (!otherRc) continue;
								if (otherRc->materialUUID == newMaterial) continue;

								otherRc->materialUUID = newMaterial;
								engine->GetRenderDataWriteBuffer()->updateRenderItemMaterialCommand.push_back(
									{ otherRc->renderItemID, newMaterial });
							}
						}
						else
						{
							rc->materialUUID = newMaterial;
							engine->GetRenderDataWriteBuffer()->updateRenderItemMaterialCommand.push_back(
								{ rc->renderItemID, newMaterial });
						}
					}
				}
				ImGui::EndDragDropTarget();
			}

			if (SceneOutliner::GetSelectedEntities().size() > 1)
			{
				ImGui::TextDisabled("%s  Material drop applies to %zu selected",
					IconStr(u8"\uE88F"),
					SceneOutliner::GetSelectedEntities().size());
			}

			ImGui::Unindent(8.0f);
		}
	}

	// ─────────────────────────────────────────────────────────────────
	// Physics
	// ─────────────────────────────────────────────────────────────────
	if (ruya::PhysicsComponent* pc = sceneRef->TryGetComponent<ruya::PhysicsComponent>(entity))
	{
		bool removePhysics = false;
		if (ComponentHeader(u8"\uE680", "Physics", kColorHeaderPhysics, &removePhysics, true))
		{
			ImGui::Indent(8.0f);

			ruya::Physics* physics = engine->GetPhysics();

			BeginPropertyTable({ "Center", "Half Extents", "Motion Type" });

			if (DragVec3("Center", &pc->centerPosition.x, 0.0f, 0.1f))
			{
				ruya::TransformComponent* tComp = sceneRef->TryGetComponent<ruya::TransformComponent>(entity);
				glm::vec3 worldPos = pc->centerPosition;
				if (tComp)
				{
					glm::vec3 rotatedOffset = tComp->rotation * pc->centerPosition;
					worldPos = tComp->position + rotatedOffset;
				}
				physics->SetRiggedBodyPosition(pc->riggidBodyRyID, worldPos);
			}

			if (DragVec3("Half Extents", &pc->halfWidhtHeightDepth.x, 0.5f, 0.1f))
				physics->SetRiggedBodySize(pc->riggidBodyRyID, pc->halfWidhtHeightDepth);

			{
				const char* motionTypeNames[] = { "Static", "Kinematic", "Dynamic" };

				const auto& selectedForMotion = SceneOutliner::GetSelectedEntities();
				bool motionMixed = false;
				if (selectedForMotion.size() > 1)
				{
					bool firstSeen = false;
					ruya::PhysicsMotionType reference = pc->motionType;
					for (uint32_t entId : selectedForMotion)
					{
						ruya::PhysicsComponent* other = sceneRef->TryGetComponent<ruya::PhysicsComponent>(entId);
						if (!other) continue;
						if (!firstSeen) { reference = other->motionType; firstSeen = true; continue; }
						if (other->motionType != reference) { motionMixed = true; break; }
					}
				}

				int  currentMotion = static_cast<int>(pc->motionType);
				bool comboChanged = false;

				if (motionMixed)
				{
					PropertyRow("Motion Type");
					int displayIdx = -1;
					if (ImGui::BeginCombo("##MotionTypeMixed", "Mixed"))
					{
						for (int n = 0; n < IM_ARRAYSIZE(motionTypeNames); ++n)
						{
							if (ImGui::Selectable(motionTypeNames[n], displayIdx == n))
							{
								currentMotion = n;
								comboChanged = true;
							}
							if (displayIdx == n) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
					ImGui::PopID();
				}
				else
				{
					comboChanged = Combo("Motion Type", &currentMotion,
						motionTypeNames, IM_ARRAYSIZE(motionTypeNames));
				}

				if (comboChanged)
				{
					ruya::PhysicsMotionType newMotion = static_cast<ruya::PhysicsMotionType>(currentMotion);
					if (selectedForMotion.size() > 1)
					{
						for (uint32_t entId : selectedForMotion)
						{
							if (ruya::PhysicsComponent* other = sceneRef->TryGetComponent<ruya::PhysicsComponent>(entId))
							{
								other->motionType = newMotion;
								physics->SetRiggedBodyMotionType(other->riggidBodyRyID, newMotion);
							}
						}
					}
					else
					{
						pc->motionType = newMotion;
						physics->SetRiggedBodyMotionType(pc->riggidBodyRyID, pc->motionType);
					}
				}
			}

			EndPropertyTable();

			if (SceneOutliner::GetSelectedEntities().size() > 1)
			{
				ImGui::TextDisabled("%s  Applies to %zu selected",
					IconStr(u8"\uE88F"),
					SceneOutliner::GetSelectedEntities().size());
			}

			if (ImGui::TreeNodeEx("Debug Info", ImGuiTreeNodeFlags_SpanAvailWidth))
			{
				BeginPropertyTable({ "Body RyID", "Allocated" });
				TextRow("Body RyID", std::to_string(pc->riggidBodyRyID).c_str());
				StatusIndicator("Allocated", pc->allocated, "Allocated", "Not allocated");
				EndPropertyTable();
				ImGui::TreePop();
			}

			ImGui::Unindent(8.0f);
		}
	}

	// ─────────────────────────────────────────────────────────────────
	// Add Component
	// ─────────────────────────────────────────────────────────────────
	ImGui::Spacing();

	if (PrimaryButton(IconLabel(u8"\uE8C6", "Add Component"), kAccent, 0.8f))
		ImGui::OpenPopup("AddComponentPopup");

	if (ImGui::BeginPopup("AddComponentPopup"))
	{
		ImGui::TextDisabled("  Add Component");
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::MenuItem(IconLabel(u8"\uE680", "Physics Component")))
		{
			const auto& selected = SceneOutliner::GetSelectedEntities();
			std::vector<uint32_t> targets;
			if (selected.size() > 1) targets.assign(selected.begin(), selected.end());
			else targets.push_back(entity);

			for (uint32_t entId : targets)
			{
				if (sceneRef->HasComponent<ruya::PhysicsComponent>(entId)) continue;
				sceneRef->AddComponent<ruya::PhysicsComponent>(entId);
				ruya::PhysicsComponent* pcNew = sceneRef->TryGetComponent<ruya::PhysicsComponent>(entId);
				ruya::TransformComponent* tcNew = sceneRef->TryGetComponent<ruya::TransformComponent>(entId);
				if (pcNew && tcNew) pcNew->centerPosition = tcNew->position;
			}
		}

		if (ImGui::MenuItem(IconLabel(u8"\uE1DA", "Render Component")))
		{
		}

		ImGui::EndPopup();
	}

	ImGui::End();
}

void editor::Properties::UpdateTransformRecursive(ruya::EntityID entity)
{
	if (entity == entt::null) return;

	ruya::RyID scene = SceneOutliner::selectedScene;

	ruya::TransformComponent* transformComp = engine->GetApp()->GetScene(scene)->TryGetComponent<ruya::TransformComponent>(entity);
	ruya::RelationshipComponent* relationshipComp = engine->GetApp()->GetScene(scene)->TryGetComponent<ruya::RelationshipComponent>(entity);

	glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), transformComp->position)
		* glm::toMat4(transformComp->rotation)
		* glm::scale(glm::mat4(1.0f), transformComp->scale);

	if (relationshipComp->parentEntity != entt::null)
	{
		ruya::TransformComponent* parentTransformComp = engine->GetApp()->GetScene(scene)->TryGetComponent<ruya::TransformComponent>(
			static_cast<uint32_t>(relationshipComp->parentEntity));
		transformComp->transform = parentTransformComp->transform * localTransform;
	}
	else
	{
		transformComp->transform = localTransform;
	}

	if (ruya::RenderComponent* renderComponent = engine->GetApp()->GetScene(scene)->TryGetComponent<ruya::RenderComponent>(entity))
	{
		ruya::RenderData* renderData = engine->GetRenderDataWriteBuffer();
		if (renderComponent->renderItemID.IsValid() && renderComponent->allocated)
		{
			renderData->updateRenderItemTransformCommands.push_back(
				{ renderComponent->renderItemID, transformComp->transform });
		}
	}

	if (ruya::PhysicsComponent* physicsComponent = engine->GetApp()->GetScene(scene)->TryGetComponent<ruya::PhysicsComponent>(entity))
	{
		if (physicsComponent->allocated || physicsComponent->riggidBodyRyID.IsValid())
		{
			glm::vec3 rotatedOffset = transformComp->rotation * physicsComponent->centerPosition;
			engine->GetPhysics()->SetRiggedBodyPositionAndRotation(
				physicsComponent->riggidBodyRyID,
				transformComp->position + rotatedOffset,
				transformComp->rotation);
		}
	}

	if (relationshipComp->firstChildEntity != entt::null)
		UpdateTransformRecursive(relationshipComp->firstChildEntity);

	if (relationshipComp->nextEntity != entt::null)
		UpdateTransformRecursive(relationshipComp->nextEntity);
}