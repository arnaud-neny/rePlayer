#include "ReplayKlystrack.h"

#include <Audio/Audiotypes.inl.h>
#include <Core/String.h>
#include <ReplayDll.h>

#include <bit>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::klystrack,
        .name = "klystrack",
        .extensions = "kt",
        .about = "klystrack 1.7.6\nCopyright (c) 2009-2018 Tero Lindeman (kometbomb)",
        .settings = "klystrack",
        .load = ReplayKlystrack::Load
    };

    Replay* ReplayKlystrack::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto player = Scope([]() { return KSND_CreatePlayerUnregistered(kSampleRate); },
            [](auto* data) { if (data) { KSND_FreePlayer(data); } });
        if (player)
        {
            auto data = stream->Read();
            auto* song = KSND_LoadSongFromMemory(player, pCast<void>(data.Items()), data.NumItems<int>());
            if (song)
                return new ReplayKlystrack(player.Detach(), song);
        }

        return nullptr;
    }

    ReplayKlystrack::~ReplayKlystrack()
    {
        KSND_FreeSong(m_song);
        KSND_FreePlayer(m_player);
    }

    ReplayKlystrack::ReplayKlystrack(KPlayer* player, KSong* song)
        : Replay(eExtension::_kt, eReplay::klystrack)
        , m_player(player)
        , m_song(song)
    {
        KSND_GetSongInfo(m_song, &m_info);
        KSND_SetPlayerQuality(m_player, 4);

        // clear properties
        m_properties.Clear();
        // push empty property info
        auto* property = m_properties.Push();
        property->label = "Info";
        property->numColumns = 2;
        {
            property->Add("Title", Property::kIsNotEditable, m_info.song_title, Property::kIsEditable);

            char buf[16];
            sprintf(buf, "%d", m_info.n_channels);
            property->Add("Channels", Property::kIsNotEditable, buf, Property::kIsNotEditable);
            sprintf(buf, "%d", m_info.n_instruments);
            property->Add("Instruments", Property::kIsNotEditable, buf, Property::kIsNotEditable);
        }
        // push property instruments if any
        if (m_info.n_instruments)
        {
            property = m_properties.Push();
            property->label = "Instruments";
            property->numColumns = 1;
            for (int i = 0; i < m_info.n_instruments; ++i)
                property->Add(m_info.instrument_name[i], Property::kIsEditable);
        }
    }

    uint32_t ReplayKlystrack::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_hasLooped)
        {
            m_hasLooped = false;
            return 0;
        }
        auto* buffer = pCast<int16_t>(output + numSamples) - numSamples * 2;
        auto renderedSamples = uint32_t(KSND_FillBuffer(m_player, buffer, numSamples * sizeof(int16_t) * 2));
        output->Convert(buffer, renderedSamples);
        if (renderedSamples != numSamples)
            m_hasLooped = renderedSamples != 0;
        return renderedSamples;
    }

    void ReplayKlystrack::ResetPlayback()
    {
        KSND_PlaySong(m_player, m_song, 0);
        m_hasLooped = false;
    }

    void ReplayKlystrack::ApplySettings(const CommandBuffer metadata)
    {
        UnusedArg(metadata);
    }

    void ReplayKlystrack::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayKlystrack::GetDurationMs() const
    {
        return uint32_t(KSND_GetPlayTime(m_song, KSND_GetSongLength(m_song)));
    }

    uint32_t ReplayKlystrack::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayKlystrack::GetExtraInfo() const
    {
        std::string metadata;
        metadata = m_info.song_title;
        metadata += "\n";
        for (int i = 0; i < m_info.n_instruments; ++i)
        {
            metadata += "\n";
            metadata += m_info.instrument_name[i];
        }
        return metadata;
    }

    std::string ReplayKlystrack::GetInfo() const
    {
        std::string info;
        char buf[8];
        sprintf(buf, "%d", m_info.n_channels);
        info = buf;
        info += " channels\nklystron\nklystrack 1.7.6";
        return info;
    }

    const Replay::Properties& ReplayKlystrack::BuildProperties()
    {
        return m_properties;
    }
}
// namespace rePlayer