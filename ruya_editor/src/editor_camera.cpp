#include "editor_camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp> 
#include <glm/gtx/transform.hpp>

#include <engine.h>
#include <window/input.h>

#include <widgets/viewport.h>

editor::EditorCameraSystem::EditorCameraSystem()
{
	editorCamera = std::make_unique<EditorCamera>();

	editorCamera->targetPosition = editorCamera->position;
	editorCamera->targetYaw = editorCamera->yaw;
	editorCamera->targetPitch = editorCamera->pitch;
}

editor::EditorCameraSystem::~EditorCameraSystem()
{
}

void editor::EditorCameraSystem::OnEngineUpdate()
{
	float dt = engine->GetWindow()->GetDeltaTime();

	if (Viewport::IsActive())
	{
		if (ruya::Input::IsKeyPressed(ruya::Input::KeyCode::W))
			editorCamera->targetPosition += editorCamera->cameraSpeed * editorCamera->front * dt;

		if (ruya::Input::IsKeyPressed(ruya::Input::KeyCode::S))
			editorCamera->targetPosition -= editorCamera->cameraSpeed * editorCamera->front * dt;

		if (ruya::Input::IsKeyPressed(ruya::Input::KeyCode::A))
			editorCamera->targetPosition -= glm::normalize(glm::cross(editorCamera->front, editorCamera->up)) * editorCamera->cameraSpeed * dt;

		if (ruya::Input::IsKeyPressed(ruya::Input::KeyCode::D))
			editorCamera->targetPosition += glm::normalize(glm::cross(editorCamera->front, editorCamera->up)) * editorCamera->cameraSpeed * dt;

		if (ruya::Input::IsKeyPressed(ruya::Input::KeyCode::E))
			editorCamera->targetPosition += editorCamera->cameraSpeed * glm::vec3(0.0f, 1.0f, 0.0f) * dt;

		if (ruya::Input::IsKeyPressed(ruya::Input::KeyCode::Q))
			editorCamera->targetPosition -= editorCamera->cameraSpeed * glm::vec3(0.0f, 1.0f, 0.0f) * dt;

		if (ruya::Input::IsMouseButtonPressed(ruya::Input::MouseButtonCode::Right))
		{
			float xoffset;
			float yoffset;

			ruya::Input::GetDeltaMouse(&xoffset, &yoffset);

			if (!editorCamera->wasRightMousePressed)
			{
				editorCamera->wasRightMousePressed = true;
				xoffset = 0.0f;
				yoffset = 0.0f;
			}

			ruya::Input::SetMouseVisible(false);
			ruya::Input::LockMouseToWindow(true);

			yoffset = -yoffset;

			xoffset *= editorCamera->mouseSensitivity;
			yoffset *= editorCamera->mouseSensitivity;

			editorCamera->targetYaw += xoffset;
			editorCamera->targetPitch += yoffset;

			if (editorCamera->targetPitch > 89.0f)
				editorCamera->targetPitch = 89.0f;
			if (editorCamera->targetPitch < -89.0f)
				editorCamera->targetPitch = -89.0f;
		}
		else
		{
			editorCamera->wasRightMousePressed = false;
			ruya::Input::SetMouseVisible(true);
			ruya::Input::LockMouseToWindow(false);
		}
	}
	else
	{
		editorCamera->wasRightMousePressed = false;
		ruya::Input::SetMouseVisible(true);
		ruya::Input::LockMouseToWindow(false);
	}

	float moveAlpha = 1.0f - expf(-editorCamera->moveSmoothness * dt);
	float lookAlpha = 1.0f - expf(-editorCamera->lookSmoothness * dt);

	editorCamera->position = glm::mix(editorCamera->position, editorCamera->targetPosition, moveAlpha);
	editorCamera->yaw = glm::mix(editorCamera->yaw, editorCamera->targetYaw, lookAlpha);
	editorCamera->pitch = glm::mix(editorCamera->pitch, editorCamera->targetPitch, lookAlpha);

	glm::vec3 front;
	front.x = cos(glm::radians(editorCamera->yaw)) * cos(glm::radians(editorCamera->pitch));
	front.y = sin(glm::radians(editorCamera->pitch));
	front.z = sin(glm::radians(editorCamera->yaw)) * cos(glm::radians(editorCamera->pitch));
	editorCamera->front = glm::normalize(front);

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

	renderData->camera.viewproj = renderData->camera.proj * renderData->camera.view;
	renderData->camera.invViewproj = glm::inverse(renderData->camera.viewproj);
}