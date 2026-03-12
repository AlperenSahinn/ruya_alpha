#pragma once
#include <vector>
#include <memory>

#include <widgets/widget.h>

#include "editor_camera.h"

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
		static void SetThemeDark();

		void UpdateEditorCamera();

	private:
		void InitEditor();
		void ShutdownEditor();

	private:
		std::vector<std::unique_ptr<Widget>> widgets;
		std::unique_ptr<EditorCameraSystem> editorCameraSystem;
	};

	void EditorUpdate(Editor* pEditor);
	void EditorDraw();
}