#include "engine.h"

#include <core/log.h>
#include <core/thread_affinity.h>

void ruya::Engine::Init()
{
	engineRunning = false;

	ryAssetManager = std::make_unique<RyAssetManager>();

	window = std::make_unique<Window>();
	graphics = std::make_unique<Graphics>();
	graphics->Init(window.get());

	physics = std::make_unique<Physics>();

	renderDataWriteBuffer = std::make_unique <RenderData>();
	renderDataReadBuffer = std::make_unique <RenderData>();

	bEditorMode = false;

	jobScheduler = std::make_unique<job_system::JobScheduler>();

	RUYA_LOG_INFO("Engine instance initilized.");
}

void ruya::Engine::SetApp(std::unique_ptr<App> app)
{
	this->app = std::move(app);
	ryAssetManager->ScanAssets();
	RUYA_LOG_INFO("Application acquired.");
}

ruya::App* ruya::Engine::GetApp()
{
	return app.get();
}

void ruya::Engine::Run()
{
	tracy::SetThreadName("Main Thread (Worker 0)");

	PinThreadToCore(0);

	engineRunning = true;

	if (app != nullptr)
		app->Init();

	jobScheduler->Init();

	asyncJobWorker = std::thread(&ruya::Engine::AsyncJobWorkerLoop, this);

	OnUpdate();
}

void ruya::Engine::WaitEngineShutdown()
{
	jobScheduler->Wait(renderSubmitWorkerCounter);

	{
		std::lock_guard<std::mutex> lock(asyncJobQueueMutex);
		stopAsyncJobWorkerThread = true;
	}
	asyncJobWorkerCondition.notify_all();

	asyncJobWorker.join();

	graphics->WaitGraphicsDeviceIdle();
}

void ruya::Engine::Shutdown()
{
	jobScheduler.reset();
	app.reset();
	physics.reset();
	graphics.reset();
	window.reset();
	ryAssetManager.reset();
}

ruya::Window* ruya::Engine::GetWindow()
{
	return window.get();
}

ruya::Graphics* ruya::Engine::GetGraphics()
{
	return graphics.get();
}

ruya::Physics* ruya::Engine::GetPhysics() const
{
	return physics.get();
}

ruya::App* ruya::Engine::GetApp() const
{
	return app.get();
}

ruya::RyAssetManager* ruya::Engine::GetAssetManager() const
{
	return ryAssetManager.get();
}

ruya::job_system::JobScheduler* ruya::Engine::GetJobScheduler() const
{
	return jobScheduler.get();
}

void ruya::Engine::QueueAsyncJob(std::function<void()> func)
{
	{
		std::lock_guard<std::mutex> lock(asyncJobQueueMutex);
		asyncJobQueue.push(std::move(func));
	}
	asyncJobWorkerCondition.notify_one();
}

ruya::RenderData* ruya::Engine::GetRenderDataWriteBuffer() const
{
	return renderDataWriteBuffer.get();
}

ruya::RenderData* ruya::Engine::GetRenderDataReadBuffer() const
{
	return renderDataReadBuffer.get();
}

void ruya::Engine::SetEditorUpdateFunction(std::function<void()> func)
{
	bEditorMode = true;
	editorUpdateFunction = func;
}

void ruya::Engine::SetEditorDrawFunction(std::function<void()> func)
{
	editorDrawFunction = func;
}

void ruya::Engine::StartApp()
{
	if (appState == AppState::AppReady)
		appState = AppState::AppStart;
}

void ruya::Engine::PauseApp()
{
	if (appState == AppState::AppUpdate)
		appState = AppState::AppPause;
}

void ruya::Engine::ContinueApp()
{
	if (appState == AppState::AppPause)
		appState = AppState::AppUpdate;
}

void ruya::Engine::ShutdownApp()
{
	if (appState == AppState::AppUpdate || appState == AppState::AppPause)
		appState = AppState::AppShutdown;
}

ruya::AppState ruya::Engine::GetAppState()
{
	return appState;
}

void ruya::Engine::OnUpdate()
{
	window->ResetDeltaTime();

	appState = AppState::AppReady;

	while (engineRunning)
	{
		window->UpdateDeltaTime();
		window->PollEvents();

		if (appState == AppState::AppStart)
		{
			job_system::Counter counter{};
			jobScheduler->Submit(counter, [p = app.get()] {p->OnStart();});
			jobScheduler->Wait(counter);
			appState = AppState::AppUpdate;
		}
		else if (appState == AppState::AppUpdate)
		{
			job_system::Counter counter{};
			jobScheduler->Submit(counter, [p = app.get()] {p->OnUpdate();});
			jobScheduler->Wait(counter);
		}
		else if (appState == AppState::AppShutdown)
		{
			job_system::Counter counter{};
			jobScheduler->Submit(counter, [p = app.get()] {p->OnShutdown();});
			jobScheduler->Wait(counter);
			appState = AppState::AppReady;
		}

		{
			job_system::Counter counter{};
			jobScheduler->Submit(counter, [p = app.get()] {p->OnEngineUpdate();});
			jobScheduler->Wait(counter);
		}

		if (window->ShouldClose())
		{
			engineRunning = false;
		}

		std::swap(renderDataReadBuffer, renderDataWriteBuffer);
		renderDataWriteBuffer.reset(new RenderData());

		jobScheduler->Wait(renderSubmitWorkerCounter);

		if (bEditorMode)
		{
			editorUpdateFunction();
		}

		if (!window->IsWindowMinimized())
		{
			graphics->BeginFrame();

			if (graphics->IsSwapchainValid())
			{
				graphics->Draw();

				if (bEditorMode)
				{
					graphics->BeginEditorDraw();
					editorDrawFunction();
					graphics->EndEditorDraw();
				}

				jobScheduler->Submit(renderSubmitWorkerCounter, [p = graphics.get()] {p->EndFrame();});
			}
		}

		FrameMark;
	}
}

void ruya::Engine::AsyncJobWorkerLoop()
{
	tracy::SetThreadName("Async Thread");

	while (true)
	{
		std::function<void()> func;

		{
			std::unique_lock<std::mutex> lock(asyncJobQueueMutex);

			asyncJobWorkerCondition.wait(lock, [this]() {return !asyncJobQueue.empty() || stopAsyncJobWorkerThread;});

			if (stopAsyncJobWorkerThread && asyncJobQueue.empty())
			{
				return;
			}

			func = std::move(asyncJobQueue.front());
			asyncJobQueue.pop();
		}

		if (func)
		{
			func();
		}
	}
}

static std::unique_ptr<ruya::Engine> globalEngineInstance;

void ruya::SetCurrentEngineInstance(std::unique_ptr<ruya::Engine> engineInstance)
{
	globalEngineInstance = std::move(engineInstance);
}

ruya::Engine* ruya::GetCurrentEngineInstance()
{
	return globalEngineInstance.get();
}

static std::unique_ptr<std::string> globalAssetsDir;

void ruya::SetAssetsDir(const std::filesystem::path path)
{
	globalAssetsDir = std::make_unique<std::string>(path.string());
	RUYA_LOG_INFO("Assets directory acquired. Path: {}", *globalAssetsDir);
}

const std::string& ruya::GetAssetsDir()
{
	return *globalAssetsDir;
}
