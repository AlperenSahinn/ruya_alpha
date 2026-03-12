#pragma once
#include <stdint.h>
#include <string>

#include <SDL3/SDL.h>

namespace ruya
{
	struct WindowCreateInfo
	{
		std::string windowName = "RuyaEngine";
		uint32_t windowWidth = 1600;
		uint32_t windowHeight = 900;
	};

	class Window
	{
	public:
		Window(uint32_t windowWidth = 1600, uint32_t windowHeight = 900, std::string windowName = "RuyaEditor");
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		void PollEvents();
		bool ShouldClose() const;
		bool IsWindowResized() const;
		void SetWindowResized(bool b);
		bool IsWindowMinimized() const;

		uint32_t GetWindowWidth() const;
		uint32_t GetWindowHeight() const;

		SDL_Window* GetSDLWindow();

		void ResetDeltaTime();
		void UpdateDeltaTime();
		float GetDeltaTime() const;
		uint64_t GetFrameCount() const;

	private:
		void Create();
		void Destroy();

	private:
		std::string windowName;
		uint32_t windowWidth;
		uint32_t windowHeight;

		SDL_Window* sdlWindow;

		bool shouldClose;
		bool windowResized;
		bool windowMinimized;

		Uint64 now;
		Uint64 last;
		double deltaTime;
		uint64_t frameCount;
	};
}
