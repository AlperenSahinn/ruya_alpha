#pragma once

namespace ruya
{
	class IGraphicsResource
	{
	public:
		virtual ~IGraphicsResource() = default;

		virtual void Load() = 0;
		virtual void Unload() = 0;
	};
}