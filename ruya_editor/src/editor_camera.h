#pragma once
#include <memory>

#include <glm/glm.hpp>

#include <app_framework/scene_system.h>

namespace editor
{
	struct EditorCamera
	{
		glm::vec3 targetPosition = glm::vec3(0.0f, 0.0f, 3.0f);
		float targetYaw = -90.0f;
		float targetPitch = 0.0f;
		bool wasRightMousePressed = false;
		float moveSmoothness = 10.0f;
		float lookSmoothness = 15.0f;

		float cameraSpeed = 20.0f;
		float mouseSensitivity = 0.25f;

		float yaw = -90.0f;
		float pitch = 0.0f;

		glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);;
		glm::vec3 rotation = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 front = glm::vec3(
			cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
			sin(glm::radians(pitch)),
			sin(glm::radians(yaw)) * cos(glm::radians(pitch))
		);

		float fov = 60.0f;
		float width = 1600.0f;
		float height = 900.0f;
		float cameraNear = 0.1f;
		float cameraFar = 10000.0f;
	};

	class EditorCameraSystem : public ruya::SceneSystem
	{
	public:
		EditorCameraSystem();
		~EditorCameraSystem();

		void OnEngineUpdate() override;

	private:
		std::unique_ptr<EditorCamera> editorCamera;
	};
}