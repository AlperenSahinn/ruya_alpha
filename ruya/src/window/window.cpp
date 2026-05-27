#include "window.h"

#include <ImGui/imgui_impl_sdl3.h>

#include <engine.h>

ruya::Window::Window(uint32_t windowWidth, uint32_t windowHeight, std::string windowName)
{
    this->windowWidth = windowWidth;
    this->windowHeight = windowHeight;
    this->windowName = windowName;

    shouldClose = false;
    windowResized = false;
    windowMinimized = false;

    frameRateMode = 1;
    frameRateChanged = false;

    Create();

    RUYA_LOG_INFO("Window initialized.");
}

ruya::Window::~Window()
{
    Destroy();
}

void ruya::Window::PollEvents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0)
    {
        ImGui_ImplSDL3_ProcessEvent(&e);

        switch (e.type)
        {
        case SDL_EVENT_QUIT:
            shouldClose = true;
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            windowResized = true;
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            windowMinimized = true;
            break;
        case SDL_EVENT_WINDOW_RESTORED:
            windowMinimized = false;
            windowResized = true;
            break;

        case SDL_EVENT_MOUSE_MOTION:
            break;

        default:
            break;
        }
    }
}

bool ruya::Window::ShouldClose() const
{
    return shouldClose;
}

bool ruya::Window::IsWindowResized() const
{
    return windowResized;
}

void ruya::Window::SetWindowResized(bool b)
{
    windowResized = b;

    int w;
    int h;

    SDL_GetWindowSize(sdlWindow, &w, &h);

    windowWidth = static_cast<uint32_t>(w);
    windowHeight = static_cast<uint32_t>(h);
}

bool ruya::Window::IsWindowMinimized() const
{
    return windowMinimized;
}

void ruya::Window::ChangeFrameRate(uint32_t frameRateMode)
{
    this->frameRateMode = frameRateMode;
    frameRateChanged = true;
}

bool ruya::Window::IsFrameRateChanged()
{
    return frameRateChanged;
}

void ruya::Window::SetFrameRateChanged(bool b)
{
    frameRateChanged = b;
}

uint32_t ruya::Window::GetFrameRateMode()
{
    return frameRateMode;
}

uint32_t ruya::Window::GetWindowWidth() const
{
    return windowWidth;
}

uint32_t ruya::Window::GetWindowHeight() const
{
    return windowHeight;
}

SDL_Window* ruya::Window::GetSDLWindow()
{
    return sdlWindow;
}

void ruya::Window::ResetDeltaTime()
{
    now = SDL_GetPerformanceCounter();
}

void ruya::Window::UpdateDeltaTime()
{
    last = now;
    now = SDL_GetPerformanceCounter();
    deltaTime = ((double)((now - last) * 1000.0 / SDL_GetPerformanceFrequency())) * 0.001;
    frameCount++;
}

float ruya::Window::GetDeltaTime() const
{
    return static_cast<float>(deltaTime);
}

uint64_t ruya::Window::GetFrameCount() const
{
    return frameCount;
}

void ruya::Window::Create()
{
    if (SDL_Init(SDL_INIT_VIDEO) != true)
    {
        RUYA_LOG_ERROR("[Window] SDL initialization failed: %s", SDL_GetError());
    }
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_VULKAN);

    sdlWindow = SDL_CreateWindow(windowName.c_str(), windowWidth, windowHeight, window_flags);

    if (!sdlWindow)
    {
        RUYA_LOG_ERROR("[Window] SDLWindow creation failed: %s", SDL_GetError());
    }
}

void ruya::Window::Destroy()
{
    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
}