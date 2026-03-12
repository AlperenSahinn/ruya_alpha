#pragma once

#include "widget.h"

namespace editor
{
	class Properties : public Widget
	{
	public:
		Properties() = default;
		~Properties() = default;

		Properties(const Properties&) = delete;
		Properties& operator=(const Properties&) = delete;

		void Draw() override;

	private:
		void UpdateTransformRecursiveUI(uint32_t entity);
	};
}