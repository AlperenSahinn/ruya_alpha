#include "Input.h"

#include <SDL3/SDL.h>

#include <Engine.h>

#include "Window.h"

float ruya::Input::lastMousePosX = 0.0f;
float ruya::Input::lastMousePosY = 0.0f;
bool ruya::Input::firstMouseLock = true;

bool ruya::Input::IsKeyPressed(KeyCode keyCode)
{
    const bool* keyboardState = SDL_GetKeyboardState(NULL);

    switch (keyCode)
    {
    case KeyCode::W:
        return keyboardState[SDL_SCANCODE_W];
    case KeyCode::A:
        return keyboardState[SDL_SCANCODE_A];
    case KeyCode::S:
        return keyboardState[SDL_SCANCODE_S];
    case KeyCode::D:
        return keyboardState[SDL_SCANCODE_D];
    case KeyCode::E:
        return keyboardState[SDL_SCANCODE_E];
    case KeyCode::Q:
        return keyboardState[SDL_SCANCODE_Q];
    case KeyCode::F:
        return keyboardState[SDL_SCANCODE_F];
    default:
        return false;
    }
}

bool ruya::Input::IsMouseButtonPressed(MouseButtonCode mouseButtonCode)
{
    Uint32 mouseState = SDL_GetMouseState(NULL, NULL);

    switch (mouseButtonCode)
    {
    case MouseButtonCode::Left:
        return (mouseState & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
    case MouseButtonCode::Middle:
        return (mouseState & SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE)) != 0;
    case MouseButtonCode::Right:
        return (mouseState & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) != 0;
    default:
        return false;
    }
}

void ruya::Input::SetMouseVisible(bool b)
{
    if (b)
    {
        SDL_ShowCursor();
    }
    else
    {
        SDL_HideCursor();
    }
}

void ruya::Input::LockMouseToWindow(bool b)
{
    SDL_SetWindowRelativeMouseMode(engine->GetWindow()->GetSDLWindow(), b);

    if (b)
    {
        if (firstMouseLock)
        {
            firstMouseLock = false;
            SDL_GetMouseState(&lastMousePosX, &lastMousePosY);
        }

        SDL_WarpMouseInWindow(engine->GetWindow()->GetSDLWindow(), lastMousePosX, lastMousePosY);
    }

    else
    {
        if (!firstMouseLock)
        {
            firstMouseLock = true;
        }
    }
}

void ruya::Input::GetDeltaMouse(float* x, float* y)
{
    float xrel, yrel;
    SDL_GetRelativeMouseState(&xrel, &yrel);
    *x = xrel;
    *y = yrel;
}