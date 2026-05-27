#include <mimalloc/mimalloc.h>

#include <core/utils.h>
#include <core/log.h>
#include <filesystem>

#include <engine.h>
#include <ruya_app.h>
#include "editor.h"

int main()
{
	RUYA_LOG_INFO("Mimalloc initilized. Mimalloc version: {}", mi_version());

	ruya::InitLogger();

	std::filesystem::path assetsDir = ruya::utils::GetAssetsDir();
	ruya::SetAssetsDir(assetsDir);

	std::unique_ptr<ruya::Engine> engineInstance = std::make_unique<ruya::Engine>();
	ruya::SetCurrentEngineInstance(std::move(engineInstance));
	engine->Init();

	std::unique_ptr<ruya::App> app = std::make_unique<RuyaApp>();
	engine->SetApp(std::move(app));

	editor::Editor* editor = new editor::Editor();

	engine->SetEditorUpdateFunction([editor]() { editor::EditorUpdate(editor); });
	engine->SetEditorDrawFunction(editor::EditorDraw);

	engine->Run();

	engine->WaitEngineShutdown();

	delete editor;

	engine->Shutdown();

	RUYA_LOG_INFO("Engine closing with zero errors.");

	ruya::ShutdownLogger();

	return 0;
}
