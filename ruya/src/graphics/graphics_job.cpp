#include "graphics_job.h"

void ruya::GraphicsJob::AddJob(std::function<void()> fn)
{
	jobs.emplace_back(std::move(fn));
}

void ruya::GraphicsJob::Execute()
{
	for (auto& fn : jobs)
	{
		fn();
	}
}
