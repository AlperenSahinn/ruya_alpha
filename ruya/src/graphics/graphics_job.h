#pragma once
#include <functional>
#include <vector>
#include <memory>

namespace ruya
{
	class GraphicsJob
	{
	public:
		GraphicsJob() = default;
		~GraphicsJob() = default;

		void AddJob(std::function<void()> fn);
		void Execute();

	private:
		std::vector<std::function<void()>> jobs;
	};

	inline std::unique_ptr<GraphicsJob> CreateGraphicsJob()
	{
		return std::make_unique<GraphicsJob>();
	}
}