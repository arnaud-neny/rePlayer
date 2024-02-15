#pragma once

#include <Containers/Array.h>

#include <atomic>
#include <functional>
#include <mutex>

namespace std
{
    class thread;
}
// namespace std

namespace core::thread
{
    class Workers
    {
    public:
        Workers(uint32_t numThreads, uint32_t maxJobs, const wchar_t* name);
        ~Workers();

        void AddJob(const std::function<void()>& callback);
        void FromJob(const std::function<void()>& callback);

        void Update();
        void Flush();

    private:
        struct Job
        {
            std::function<void()> callback;
            Job* next = nullptr;
        };

    private:
        void Run();
        Job WaitJob();
        Job NextJob();

        bool UpdateMainThreadJobs();

    private:
        std::thread* m_threads;
        uint32_t m_numThreads;

        std::atomic<bool> m_isClosing{ false };
        std::atomic<uint16_t> m_numRunningJobs{ 0 };

        std::mutex m_mutex;
        std::condition_variable m_event;

        Array<Job> m_jobs;
        uint32_t m_currentJob = 0;
        uint32_t m_freeJob = 0;

        std::atomic<Job*> m_mainThreadJobTail{ nullptr };
        std::atomic<Job*> m_mainThreadJobHead{ nullptr };
    };
}
// namespace core::thread