#include "editor_camera.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp> 
#include <glm/gtx/transform.hpp>

#include <engine.h>
#include <window/input.h>

#include <widgets/viewport.h>

editor::EditorCameraSystem::EditorCameraSystem()
{
	editorCamera = std::make_unique<EditorCamera>();
}

editor::EditorCameraSystem::~EditorCameraSystem()
{
}

void editor::EditorCameraSystem::OnEngineUpdate()
{
	if (Viewport::IsActive())
	{
		if (ruya::Input::IsKeyPressed(ruya::Input::KeyCode::W))
			editorCamera->position = editorCamera->position + editorCamera->cameraSpeed * editorCamera->front * engine->GetWindow()->GetDeltaTime();

		if (ruya::Input::IsKeyPressed(ruya::Input::KeyCode::S))
			editorCamera->position = editorCamera->position - editorCamera->cameraSpeed * editorCamera->front * engine->GetWindow()->GetDeltaTime();

		if (ruya::Input::IsKeyPressed(ruya::Input::KeyCode::A))
			editorCamera->position = editorCamera->position - glm::normalize(glm::cross(editorCamera->front, editorCamera->up)) * editorCamera->cameraSpeed * engine->GetWindow()->GetDeltaTime();

		if (ruya::Input::IsKeyPressed(ruya::Input::KeyCode::D))
			editorCamera->position = editorCamera->position + glm::normalize(glm::cross(editorCamera->front, editorCamera->up)) * editorCamera->cameraSpeed * engine->GetWindow()->GetDeltaTime();

		if (ruya::Input::IsKeyPressed(ruya::Input::KeyCode::E))
			editorCamera->position = editorCamera->position + editorCamera->cameraSpeed * glm::vec3(0.0f, 1.0f, 0.0f) * engine->GetWindow()->GetDeltaTime();

		if (ruya::Input::IsKeyPressed(ruya::Input::KeyCode::Q))
			editorCamera->position = editorCamera->position - editorCamera->cameraSpeed * glm::vec3(0.0f, 1.0f, 0.0f) * engine->GetWindow()->GetDeltaTime();

		if (ruya::Input::IsMouseButtonPressed(ruya::Input::MouseButtonCode::Right))
		{
			float xoffset;
			float yoffset;

			ruya::Input::GetDeltaMouse(&xoffset, &yoffset);

			ruya::Input::SetMouseVisible(false);
			ruya::Input::LockMouseToWindow(true);

			yoffset = -yoffset;

			xoffset *= editorCamera->mouseSensitivity * engine->GetWindow()->GetDeltaTime();
			yoffset *= editorCamera->mouseSensitivity * engine->GetWindow()->GetDeltaTime();

			editorCamera->yaw += xoffset;
			editorCamera->pitch += yoffset;

			if (editorCamera->pitch > 89.0f)
				editorCamera->pitch = 89.0f;
			if (editorCamera->pitch < -89.0f)
				editorCamera->pitch = -89.0f;

			glm::vec3 front;
			front.x = cos(glm::radians(editorCamera->yaw)) * cos(glm::radians(editorCamera->pitch));
			front.y = sin(glm::radians(editorCamera->pitch));
			front.z = sin(glm::radians(editorCamera->yaw)) * cos(glm::radians(editorCamera->pitch));
			editorCamera->front = glm::normalize(front);
		}

		else
		{
			ruya::Input::SetMouseVisible(true);
			ruya::Input::LockMouseToWindow(false);
		}
	}

	else
	{
		ruya::Input::SetMouseVisible(true);
		ruya::Input::LockMouseToWindow(false);
	}

	ruya::RenderData* renderData = engine->GetRenderDataWriteBuffer();

	glm::vec3 target = editorCamera->front + editorCamera->position;
	glm::vec3 cameraPos = editorCamera->position;
	glm::vec3 up = editorCamera->up;

	editorCamera->width = static_cast<float>(engine->GetGraphics()->GetFrameBufferWidth());
	editorCamera->height = static_cast<float>(engine->GetGraphics()->GetFrameBufferHeight());

	renderData->camera.fov = editorCamera->fov;
	renderData->camera.width = editorCamera->width;
	renderData->camera.height = editorCamera->height;
	renderData->camera.cameraNear = editorCamera->cameraNear;
	renderData->camera.cameraFar = editorCamera->cameraFar;

	renderData->camera.view = glm::lookAt(cameraPos, target, up);
	renderData->camera.viewPos = glm::vec4(cameraPos, 1.0f);
	renderData->camera.proj = glm::perspective(
		glm::radians(editorCamera->fov),
		editorCamera->width / editorCamera->height,
		editorCamera->cameraNear,
		editorCamera->cameraFar
	);
	renderData->camera.proj[1][1] *= -1;
	renderData->camera.viewproj = renderData->camera.proj * renderData->camera.view;
}
