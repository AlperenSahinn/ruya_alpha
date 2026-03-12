#include "engine.h"

#include <core/log.h>

void ruya::Engine::Init()
{
	engineRunning = false;
	engineState = EEngineState::EngineUpdate;

	ryAssetManager = std::make_unique<RyAssetManager>();
	ryAssetManager->Init();

	window = std::make_unique<Window>();
	graphics = std::make_unique<Graphics>(window.get());

	renderDataWriteBuffer = std::make_unique <RenderData>();
	renderDataReadBuffer = std::make_unique <RenderData>();

	bEditorMode = false;

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
	engineRunning = true;

	if (app != nullptr)
		app->Init();

	renderThread = std::thread(&ruya::Engine::OnRender, this);

	OnUpdate();
}

void ruya::Engine::WaitEngineShutdown()
{
	if (renderThread.joinable())
	{
		renderThread.join();
	}

	graphics->WaitGraphicsDeviceIdle();
}

void ruya::Engine::Shutdown()
{
	app.reset();
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

ruya::App* ruya::Engine::GetApp() const
{
	return app.get();
}

ruya::RyAssetManager* ruya::Engine::GetAssetManager() const
{
	return ryAssetManager.get();
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

void ruya::Engine::OnUpdate()
{
	window->ResetDeltaTime();

	while (engineRunning)
	{
		window->UpdateDeltaTime();
		window->PollEvents();

		if (engineState == EEngineState::GameStart)
		{
			app->OnStart();
			engineState = EEngineState::GameUpdate;
		}
		else if (engineState == EEngineState::GameUpdate)
		{
			app->OnUpdate();
		}
		else if (engineState == EEngineState::GameShutdown)
		{
			app->OnShutdown();
		}

		app->OnEngineUpdate();

		renderFinished.acquire();

		if (bEditorMode)
		{
			editorUpdateFunction();
		}

		if (window->ShouldClose())
		{
			engineRunning = false;
		}

		std::swap(renderDataReadBuffer, renderDataWriteBuffer);
		renderDataWriteBuffer.reset(new RenderData());
		graphics->SubmitGraphicsJobQueue();

		renderDataReady.release();
	}
}

void ruya::Engine::OnRender()
{
	while (engineRunning)
	{
		renderDataReady.acquire();

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

				graphics->EndFrame();
			}
		}

		renderFinished.release();
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
