// Core
#include <Core/Log.h>
#include <Thread/Thread.h>

#include "Workers.h"

// Windows
#include <windows.h>

// stl
#include <thread>

namespace core::thread
{
    Workers::Workers(uint32_t numThreads, uint32_t maxJobs, const wchar_t* name)
        : m_threads(new std::thread[numThreads])
        , m_numThreads(numThreads)
        , m_jobs(maxJobs, 0)
    {
        wchar_t format[16] = L"%s Worker %01u";
        if (numThreads <= 10)
            ;
        else if (numThreads <= 100)
            format[12] = L'2';
        else
        {
            assert(0 && "That's a lot of workers...");
            format[13] = L'0';
            format[14] = L'u';
            format[15] = 0;
        }

        for (uint32_t i = 0; i < numThreads; i++)
        {
            m_threads[i] = std::thread([this]() { Run(); });
            wchar_t desc[128];
            swprintf(desc, 128, format, name, i);
            ::SetThreadDescription(m_threads[i].native_handle(), desc);
        }
    }

    Workers::~Workers()
    {
        m_isClosing.store(true);
        m_event.notify_all();
        for (uint32_t i = 0; i < m_numThreads; i++)
            m_threads[i].join();
        delete[] m_threads;
    }

    void Workers::AddJob(const std::function<void()>& callback)
    {
        m_mutex.lock();

        auto currentJob = m_currentJob;
        auto freeJob = m_freeJob;
        auto maxJobs = m_jobs.NumItems();
        if (freeJob - currentJob == maxJobs)
        {
            m_currentJob = currentJob %= maxJobs;
            freeJob = (freeJob % maxJobs) + maxJobs;
            m_jobs.Resize(maxJobs * 2);
            for (uint32_t i = 0; i < currentJob; i++)
                m_jobs[i + maxJobs] = std::move(m_jobs[i]);
            maxJobs *= 2;
        }
        m_jobs[freeJob % maxJobs] = { callback };
        m_freeJob = freeJob + 1;

        m_mutex.unlock();
        m_event.notify_one();
    }

    void Workers::FromJob(const std::function<void()>& callback)
    {
        assert(GetCurrentId() == thread::ID::kWorker);
        if (GetCurrentId() == thread::ID::kWorker)
        {
            auto* job = new Job;
            job->callback = callback;
            auto* prev = m_mainThreadJobHead.exchange(job);
            if (prev == nullptr)
                m_mainThreadJobTail.store(job);
            else
                std::atomic_ref(prev->next).store(job);
        }
        else
            Log::Error("OnEndJob can only be used inside a running job");
    }

    void Workers::Update()
    {
        UpdateMainThreadJobs();
    }

    void Workers::Flush()
    {
        for (bool isDone = false; !isDone;)
        {
            isDone = m_numRunningJobs.load() == 0;
            m_mutex.lock();
            isDone |= m_freeJob == m_currentJob;
            m_mutex.unlock();
            isDone |= UpdateMainThreadJobs() == false;
            thread::Sleep(0);
        }
    }

    void Workers::Run()
    {
        thread::SetCurrentId(ID::kWorker);
        while (!m_isClosing.load())
        {
            // waiting jobs
            for (Job job = WaitJob(); job.callback; job = NextJob())
            {
                ++m_numRunningJobs;
                job.callback();
                --m_numRunningJobs;
            }
        }
    }

    Workers::Job Workers::WaitJob()
    {
        Job job;

        std::unique_lock lk(m_mutex);
        m_event.wait(lk);

        auto currentJob = m_currentJob;
        if (currentJob != m_freeJob)
        {
            job = std::move(m_jobs[currentJob % m_jobs.NumItems()]);
            m_currentJob = currentJob + 1;
        }

        return job;
    }

    Workers::Job Workers::NextJob()
    {
        Job job;

        std::lock_guard<std::mutex> lk(m_mutex);

        auto currentJob = m_currentJob;
        if (currentJob != m_freeJob)
        {
            job = std::move(m_jobs[currentJob % m_jobs.NumItems()]);
            m_currentJob = currentJob + 1;
        }

        return job;
    }

    bool Workers::UpdateMainThreadJobs()
    {
        if (auto* tail = m_mainThreadJobTail.exchange(nullptr))
        {
            auto* head = m_mainThreadJobHead.exchange(nullptr);
            for (;;)
            {
                tail->callback();
                if (tail == head)
                {
                    delete tail;
                    return true;
                }
                auto* next = tail->next;
                for (; next == nullptr;) // bad luck
                {
                    thread::Sleep(0);
                    next = std::atomic_ref(tail->next).load();
                }
                delete tail;
                tail = next;
            }
        }
        return false;
    }
}
// namespace core::thread