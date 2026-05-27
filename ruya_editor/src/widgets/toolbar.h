#pragma once
#include "widget.h"

namespace editor
{
	enum class GizmoMode
	{
		Select,
		Move,
		Rotate,
		Scale
	};

	enum class GizmoSpace
	{
		World,
		Local
	};

	class Toolbar : public Widget
	{
	public:
		Toolbar() = default;
		~Toolbar() = default;

		Toolbar(const Toolbar&) = delete;
		Toolbar& operator=(const Toolbar&) = delete;

		void Draw() override {}

		void DrawContents();

		static GizmoMode  GetMode()       { return mode; }
		static GizmoSpace GetSpace()      { return space; }
		static bool       IsSnapEnabled() { return snapEnabled; }
		static float      GetMoveSnap()   { return moveSnap; }
		static float      GetRotateSnap() { return rotateSnap; }
		static float      GetScaleSnap()  { return scaleSnap; }

	private:
		void DrawTransformTools();
		void DrawSpaceToggle();
		void DrawSnapControls();
		void DrawTransport();

	private:
		static GizmoMode  mode;
		static GizmoSpace space;
		static bool       snapEnabled;
		static float      moveSnap;
		static float      rotateSnap;
		static float      scaleSnap;

		bool  stepRequested = false;
	};
}
