#pragma once

namespace ruya
{
	class Input
	{
	public:
		enum class KeyCode
		{
			W,
			A,
			S,
			D,
			E,
			Q,
			F
		};

		enum class MouseButtonCode
		{
			Right,
			Middle,
			Left
		};

		static bool IsKeyPressed(KeyCode keyCode);
		static bool IsMouseButtonPressed(MouseButtonCode mouseButtonCode);
		static void SetMouseVisible(bool b);
		static void LockMouseToWindow(bool b);

		static void GetDeltaMouse(float* x, float* y);

	private:
		static float lastMousePosX;
		static float lastMousePosY;
		static bool firstMouseLock;
	};
}