#pragma once
#include <thread>
#include <semaphore>
#include <stdint.h>
#include <string>
#include <functional>
#include <memory>
#include <filesystem>

#include <tracy/tracy/Tracy.hpp>

#include <core/log.h>
#include <window/window.h>
#include <assets_system/ry_asset_manager.h>
#include <graphics/graphics.h>
#include <physics/physics.h>
#include <graphics/render_data.h>
#include <app_framework/app.h>
#include <core/job_system.h>

namespace ruya
{
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
		Physics* GetPhysics() const;
		App* GetApp() const;
		RyAssetManager* GetAssetManager() const;
		job_system::JobScheduler* GetJobScheduler() const;

		void QueueAsyncJob(std::function<void()> func);

		RenderData* GetRenderDataWriteBuffer() const;
		RenderData* GetRenderDataReadBuffer() const;

		void SetEditorUpdateFunction(std::function<void()> func);
		void SetEditorDrawFunction(std::function<void()> func);

		void StartApp();
		void PauseApp();
		void ContinueApp();
		void ShutdownApp();
		AppState GetAppState();

	private:
		void OnUpdate();
		void AsyncJobWorkerLoop();

	private:
		bool engineRunning;

		std::unique_ptr<Window> window;
		std::unique_ptr<RyAssetManager> ryAssetManager;
		std::unique_ptr<Physics> physics;
		std::unique_ptr<Graphics> graphics;
		std::unique_ptr<App> app;
		AppState appState;

		std::unique_ptr<job_system::JobScheduler> jobScheduler;
		std::thread asyncJobWorker;
		std::queue<std::function<void()>> asyncJobQueue;
		std::mutex asyncJobQueueMutex;
		std::condition_variable asyncJobWorkerCondition;
		bool stopAsyncJobWorkerThread = false;

		std::unique_ptr<RenderData> renderDataWriteBuffer;
		std::unique_ptr<RenderData> renderDataReadBuffer;
		job_system::Counter renderSubmitWorkerCounter{};

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
