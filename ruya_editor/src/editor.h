#pragma once
#include <vector>
#include <memory>

#include <widgets/widget.h>

#include "editor_camera.h"

struct ImGuiStyle;

namespace editor
{
	class Editor
	{
	public:
		Editor();
		~Editor();

		Editor(const Editor&) = delete;
		Editor& operator=(const Editor&) = delete;

		std::vector<std::unique_ptr<Widget>>& GetWidgets();
		static void SetThemeModernDark();

		void UpdateEditorCamera();

		void UpdateDpiScale();

	private:
		void InitEditor();
		void ShutdownEditor();

		float GetCurrentDpiScale() const;

	private:
		std::vector<std::unique_ptr<Widget>> widgets;
		std::unique_ptr<EditorCameraSystem> editorCameraSystem;

		float currentDpiScale = 1.0f;
	};

	void EditorUpdate(Editor* pEditor);
	void EditorDraw();
}