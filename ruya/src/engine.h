#pragma once
#include <thread>
#include <semaphore>
#include <stdint.h>
#include <string>
#include <functional>
#include <memory>
#include <filesystem>

#include <core/log.h>
#include <window/window.h>
#include <graphics/graphics.h>
#include <graphics/render_data.h>
#include <app_framework/app.h>
#include <assets_system/ry_asset_manager.h>

namespace ruya
{
	enum class EEngineState
	{
		EngineUpdate,
		GameStart,
		GameUpdate,
		GamePause,
		GameShutdown,
	};

	class Engine
	{
	public:
		Engine() = default;
		~Engine() = default;

		Engine(const Engine&) = delete;
		Engine& operator=(const Engine&) = delete;

		void Init();

		void SetApp(std::unique_ptr<App> app);
		App* GetApp();

		void Run();
		void WaitEngineShutdown();
		void Shutdown();

		Window* GetWindow();
		Graphics* GetGraphics();
		App* GetApp() const;
		RyAssetManager* GetAssetManager() const;

		RenderData* GetRenderDataWriteBuffer() const;
		RenderData* GetRenderDataReadBuffer() const;

		void SetEditorUpdateFunction(std::function<void()> func);
		void SetEditorDrawFunction(std::function<void()> func);

	private:
		void OnUpdate();
		void OnRender();

	private:
		bool engineRunning;
		EEngineState engineState;

		std::unique_ptr<Window> window;
		std::unique_ptr<Graphics> graphics;
		std::unique_ptr<RyAssetManager> ryAssetManager;
		std::unique_ptr<App> app;

		std::thread renderThread;
		std::counting_semaphore<1> renderDataReady{ 0 };
		std::counting_semaphore<1> renderFinished{ 1 };
		std::unique_ptr<RenderData> renderDataWriteBuffer;
		std::unique_ptr<RenderData> renderDataReadBuffer;

		bool bEditorMode;
		std::function<void()> editorUpdateFunction;
		std::function<void()> editorDrawFunction;

	};

	void SetCurrentEngineInstance(std::unique_ptr<ruya::Engine> engineInstance);
	Engine* GetCurrentEngineInstance();

	void SetAssetsDir(const std::filesystem::path path);
	const std::string& GetAssetsDir();
}

#define engine (ruya::GetCurrentEngineInstance())
#define ASSETS_DIR (ruya::GetAssetsDir())
