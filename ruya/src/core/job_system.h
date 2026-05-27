#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <deque>
#include <initializer_list>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#include <new>
#include <memory>

#include <core/assert.h>

namespace ruya::job_system
{
    class Counter;
    class JobScheduler;

    typedef void (*EntryFn)(void* payload);

    static const std::size_t kJobPayloadBytes = 48;
    static const std::size_t kJobQueueCapacity = 4096;

    class Counter
    {
    public:
        Counter();
        ~Counter();

        Counter(const Counter&) = delete;
        Counter& operator=(const Counter&) = delete;
        Counter(Counter&&) = delete;
        Counter& operator=(Counter&&) = delete;

        bool IsBusy() const;
        std::uint32_t Remaining() const;

    private:
        friend class JobScheduler;
        std::atomic<std::uint32_t> value;
    };

    struct alignas(64) Job
    {
        EntryFn entry;
        Counter* counter;
        std::byte payload[kJobPayloadBytes];
    };

    template <typename Fn>
    static void JobTrampoline(void* payload)
    {
        Fn* fn = reinterpret_cast<Fn*>(payload);
        (*fn)();
    }

    class JobScheduler
    {
    public:
        JobScheduler();
        ~JobScheduler();

        JobScheduler(const JobScheduler&) = delete;
        JobScheduler& operator=(const JobScheduler&) = delete;

        void Init(std::uint32_t threadCount = 0);
        void Shutdown();

        template <typename Fn>
        void Submit(Counter& counter, Fn&& fn)
        {
            typedef typename std::decay<Fn>::type FnType;

            ENGINE_STATIC_ASSERT_MSG(sizeof(FnType) <= kJobPayloadBytes,
                "[Job System] Lambda capture too large; reduce captures or increase kJobPayloadBytes");
            ENGINE_STATIC_ASSERT_MSG(std::is_trivially_destructible<FnType>::value,
                "[Job System] Lambda must be trivially destructible (capture by value, no std::function)");
            ENGINE_STATIC_ASSERT_MSG(std::is_trivially_copyable<FnType>::value,
                "[Job System] Lambda must be trivially copyable (no non-trivial captures)");

            counter.value.fetch_add(1, std::memory_order_relaxed);

            Job job;
            job.entry = &JobTrampoline<FnType>;
            job.counter = &counter;

            ::new (static_cast<void*>(job.payload)) FnType(std::forward<Fn>(fn));

            PushJob(job);
        }

        template <typename It>
        void SubmitBatch(Counter& counter, It first, It last)
        {
            typedef typename std::decay<decltype(*first)>::type FnType;

            ENGINE_STATIC_ASSERT_MSG(sizeof(FnType) <= kJobPayloadBytes,
                "[Job System] Lambda capture too large");
            ENGINE_STATIC_ASSERT_MSG(std::is_trivially_destructible<FnType>::value,
                "[Job System] Lambda must be trivially destructible");
            ENGINE_STATIC_ASSERT_MSG(std::is_trivially_copyable<FnType>::value,
                "[Job System] Lambda must be trivially copyable");

            const std::size_t count = static_cast<std::size_t>(std::distance(first, last));
            if (count == 0)
            {
                return;
            }

            counter.value.fetch_add(static_cast<std::uint32_t>(count), std::memory_order_relaxed);

            {
                std::lock_guard<std::mutex> lock(queueMutex);
                ENGINE_ASSERT_MSG(queue.size() + count <= kJobQueueCapacity,
                    "[Job System] Job queue would overflow — raise kJobQueueCapacity or reduce in-flight jobs");

                for (It it = first; it != last; ++it)
                {
                    Job job;
                    job.entry = &JobTrampoline<FnType>;
                    job.counter = &counter;
                    ::new (static_cast<void*>(job.payload)) FnType(*it);
                    queue.push_back(job);
                }
            }

            if (count == 1)
            {
                queueCond.notify_one();
            }
            else
            {
                queueCond.notify_all();
            }
        }

        template <typename Fn>
        void SubmitBatch(Counter& counter, std::initializer_list<Fn> fns)
        {
            SubmitBatch(counter, fns.begin(), fns.end());
        }

        void Wait(Counter& counter);

        std::uint32_t ThreadCount() const { return static_cast<std::uint32_t>(threads.size()); }

    private:
        void PushJob(const Job& job);
        bool TryPopJob(Job& out);
        void WorkerLoop(uint32_t workerIndex);
        void ExecuteJob(Job& job);

        bool AnyBusy(Counter** counters, std::size_t count) const;

        std::deque<Job> queue;
        mutable std::mutex queueMutex;
        std::condition_variable queueCond;

        std::vector<std::thread> threads;
        std::atomic<bool> running;
    };
}