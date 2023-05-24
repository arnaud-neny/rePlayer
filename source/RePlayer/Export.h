#pragma once

#include <Containers/Array.h>
#include <Containers/SmartPtr.h>
#include <Database/Types/MusicID.h>

namespace rePlayer
{
    using namespace core;

    class Song;
    struct SongSheet;

    class Export
    {
    public:
        Export();
        ~Export();

        void Enqueue(MusicID musicId);
        bool Start();

        void GetStatus(MusicID& musicId, float& progress, uint32_t& duration) const;
        bool IsDone();

    private:
        struct Entry
        {
            SmartPtr<Song> song;
            SmartPtr<SongSheet> songSheet;
            MusicID id;
        };

    private:
        static uint32_t ThreadFunc(uint32_t* lpdwParam);
        void Update();

    private:
        void* m_threadHandle = nullptr;

        Array<Entry> m_songs;

        uint32_t m_currentEntry = 0;
        float m_progress = 0.0f;
        uint32_t m_duration = 0;
    };
}
// namespace rePlayer