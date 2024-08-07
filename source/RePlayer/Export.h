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
        void Cancel();

        uint32_t GetStatus(float& progress, uint32_t& duration) const;
        bool IsDone();

        uint32_t NumSongs() const;
        MusicID GetMusicId(uint32_t& index) const;

    private:
        struct Entry
        {
            SmartPtr<Song> song;
            SmartPtr<SongSheet> songSheet;
            MusicID id;
        };

    private:
        void Update();
        bool IsCancelled() const;

    private:
        Array<Entry> m_songs;

        uint32_t m_currentEntry = 0;
        float m_progress = 0.0f;
        uint32_t m_duration = 0;
        bool m_isCancelled = false;
        bool m_isJobDone = false;
    };

    inline uint32_t Export::NumSongs() const
    {
        return m_songs.NumItems();
    }

    inline MusicID Export::GetMusicId(uint32_t& index) const
    {
        return m_songs[index].id;
    }
}
// namespace rePlayer