#include "job_system.h"

#include <cassert>
#include <iterator>

#include <tracy/tracy/Tracy.hpp>

#include "thread_affinity.h"

namespace ruya::job_system
{
    Counter::Counter()
        : value(0)
    {
    }

    Counter::~Counter()
    {
        ENGINE_ASSERT_MSG(value.load(std::memory_order_acquire) == 0,
            "[Job System] Counter destroyed while jobs still pending — did you forget to Wait()?");
    }

    bool Counter::IsBusy() const
    {
        return value.load(std::memory_order_acquire) > 0;
    }

    std::uint32_t Counter::Remaining() const
    {
        return value.load(std::memory_order_acquire);
    }

    JobScheduler::JobScheduler()
        : running(false)
    {
    }

    JobScheduler::~JobScheduler()
    {
        if (running.load(std::memory_order_acquire))
        {
            Shutdown();
        }
    }

    void JobScheduler::Init(std::uint32_t threadCount)
    {
        ENGINE_ASSERT_MSG(!running.load(std::memory_order_acquire),
            "[Job System] Init called twice without Shutdown");

        if (threadCount == 0)
        {
            unsigned hw = std::thread::hardware_concurrency();
            threadCount = (hw > 2) ? (hw - 2) : 1;
        }

        RUYA_LOG_INFO("[JobSystem] Worker thread count: {}", threadCount);

        running.store(true, std::memory_order_release);

        threads.reserve(threadCount);
        for (std::uint32_t i = 0; i < threadCount; ++i)
        {
            threads.emplace_back(&JobScheduler::WorkerLoop, this, i);
        }
    }

    void JobScheduler::Shutdown()
    {
        if (!running.load(std::memory_order_acquire))
        {
            return;
        }

        for (;;)
        {
            Job job;
            if (TryPopJob(job))
            {
                ExecuteJob(job);
            }
            else
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (queue.empty())
                {
                    break;
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            running.store(false, std::memory_order_release);
            queueCond.notify_all();
        }

        for (std::size_t i = 0; i < threads.size(); ++i)
        {
            if (threads[i].joinable())
            {
                threads[i].join();
            }
        }
        threads.clear();
    }

    void JobScheduler::PushJob(const Job& job)
    {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            ENGINE_ASSERT_MSG(queue.size() < kJobQueueCapacity,
                "[Job System] Job queue full — raise kJobQueueCapacity or reduce in-flight jobs");
            queue.push_back(job);
        }
        queueCond.notify_one();
    }

    bool JobScheduler::TryPopJob(Job& out)
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (queue.empty())
        {
            return false;
        }
        out = queue.front();
        queue.pop_front();
        return true;
    }

    void JobScheduler::ExecuteJob(Job& job)
    {
        job.entry(job.payload);
        job.counter->value.fetch_sub(1, std::memory_order_acq_rel);
    }

    void JobScheduler::WorkerLoop(uint32_t workerIndex)
    {
        PinThreadToCore(workerIndex + 1);

        std::string workerName = "Worker " + std::to_string(workerIndex + 1);
        tracy::SetThreadName(workerName.c_str());

        while (true)
        {
            Job job;
            bool hasJob = false;

            {
                std::unique_lock<std::mutex> lock(queueMutex);

                queueCond.wait(lock, [this]
                    {
                        return !queue.empty() || !running.load(std::memory_order_acquire);
                    });

                if (!queue.empty())
                {
                    job = queue.front();
                    queue.pop_front();
                    hasJob = true;
                }
                else
                {
                    return;
                }
            }

            if (hasJob)
            {
                ExecuteJob(job);
            }
        }
    }

    void JobScheduler::Wait(Counter& counter)
    {
        while (counter.IsBusy())
        {
            Job job;
            if (TryPopJob(job))
            {
                ExecuteJob(job);
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }

    bool JobScheduler::AnyBusy(Counter** counters, std::size_t count) const
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            if (counters[i] && counters[i]->IsBusy())
            {
                return true;
            }
        }
        return false;
    }
}