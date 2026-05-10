#include "ReplayJayTrax.h"

#include <Audio/Audiotypes.inl.h>
#include <Core/String.h>
#include <ReplayDll.h>

#include <bit>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::JayTrax,
        .name = "JayTrax",
        .extensions = "jxs",
        .about = "JayTrax\nCopyright (c) Reinier van Vliet",
        .settings = "JayTrax",
        .load = ReplayJayTrax::Load
    };

    Replay* ReplayJayTrax::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto soundEngine = Scope([]() { return new SoundEngine; },
            [](auto* data) { if (data) { data->FreeSong(); delete data; } });
        soundEngine->Allocate();
        auto data = stream->Read();
        if (soundEngine->LoadSongFromMemory(pCast<unsigned char>(data.Items())))
            return nullptr;

        return new ReplayJayTrax(soundEngine.Detach(), stream);
    }

    ReplayJayTrax::~ReplayJayTrax()
    {
        m_displayEngine.FreeSong();
        m_soundEngine->FreeSong();
        delete m_soundEngine;
    }

    ReplayJayTrax::ReplayJayTrax(SoundEngine* soundEngine, io::Stream* stream)
        : Replay(eExtension::_jxs, eReplay::JayTrax)
        , m_soundEngine(soundEngine)
    {
        auto data = stream->Read();
        m_displayEngine.Allocate();
        m_displayEngine.LoadSongFromMemory(pCast<unsigned char>(data.Items()));
    }

    uint32_t ReplayJayTrax::Render(StereoSample* output, uint32_t numSamples)
    {
        auto* buffer = pCast<int16_t>(output + numSamples) - numSamples * 2;
        numSamples = m_soundEngine->RenderChannels1(buffer, numSamples, kSampleRate);
        output->Convert(buffer, numSamples);

        return numSamples;
    }

    uint32_t ReplayJayTrax::Seek(uint32_t timeInMs)
    {
        m_soundEngine->PlaySubSong(m_subsongIndex);
        m_displayEngine.PlaySubSong(m_subsongIndex);

        for (auto numSamples = (uint64_t(timeInMs) * kSampleRate) / 1000; numSamples;)
        {
            m_displayEngine.RenderChannels1(nullptr, int(numSamples), kSampleRate);
            numSamples -= m_soundEngine->RenderChannels1(nullptr, int(numSamples), kSampleRate);
        }

        return timeInMs;
    }

    void ReplayJayTrax::ResetPlayback()
    {
        m_soundEngine->PlaySubSong(m_subsongIndex);
        m_displayEngine.PlaySubSong(m_subsongIndex);
    }

    void ReplayJayTrax::ApplySettings(const CommandBuffer metadata)
    {
        UnusedArg(metadata);
    }

    void ReplayJayTrax::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
        // clear properties
        m_properties.Clear();
        // push empty property info
        auto* property = m_properties.Push();
        property->label = "Info";
        property->numColumns = 2;
        // push property instruments if any
        if (m_soundEngine->m_SongPack.nrofinst)
        {
            property = m_properties.Push();
            property->label = "Instruments";
            uint32_t columns = 0;
            for (int i = 0; i < m_soundEngine->m_SongPack.nrofinst; ++i)
            {
                if (m_soundEngine->m_Instruments[i]->samplelength)
                {
                    columns |= 1 << 0;
                    for (int j = 0; j < sizeof(m_soundEngine->m_Instruments[i]->samplename); ++j)
                    {
                        if (m_soundEngine->m_Instruments[i]->samplename[j] == 0)
                            break;
                        if (m_soundEngine->m_Instruments[i]->samplename[j] != ' ')
                        {
                            columns |= 1 << 1;
                            break;
                        }
                    }
                }
            }
            property->numColumns = std::popcount(columns) + 1;
            for (int i = 0; i < m_soundEngine->m_SongPack.nrofinst; ++i)
            {
                property->Add(m_soundEngine->m_Instruments[i]->instname, Property::kIsEditable);
                if (columns & (1 << 0))
                {
                    if (columns & (1 << 1))
                        property->Add(m_soundEngine->m_Instruments[i]->samplename, Property::kIsEditable);

                    char buf[16];
                    if (m_soundEngine->m_Instruments[i]->samplelength)
                    {
                        sprintf(buf, "%uBytes", m_soundEngine->m_Instruments[i]->samplelength);
                        property->Add(buf, Property::kIsNotEditable);
                    }
                    else
                    {
                        buf[0] = 0;
                        property->Add(buf, Property::kIsNotEditable);
                    }
                }
            }
        }
        // push property patterns if any
        if (m_soundEngine->m_SongPack.nrofpats)
        {
            property = m_properties.Push();
            property->label = "Patterns";
            property->numColumns = 1;
            for (int i = 0; i < m_soundEngine->m_SongPack.nrofpats; ++i)
                property->Add(m_soundEngine->m_PatternNames[i], Property::kIsEditable);
        }
    }

    uint32_t ReplayJayTrax::GetDurationMs() const
    {
        uint32_t length = 0;
        for (;;)
        {
            auto numSamples = m_soundEngine->RenderChannels1(nullptr, 65536, kSampleRate);
            if (numSamples == 0)
                break;
            length += numSamples;
        }
        return (length * 1000) / kSampleRate;
    }

    uint32_t ReplayJayTrax::GetNumSubsongs() const
    {
        return m_soundEngine->m_SongPack.nrofsongs;
    }

    std::string ReplayJayTrax::GetExtraInfo() const
    {
        std::string metadata;
        metadata = m_soundEngine->m_Songs[m_subsongIndex]->name;
        metadata += "\n";
        for (int i = 0; i < m_soundEngine->m_SongPack.nrofinst; ++i)
        {
            metadata += "\n";
            metadata += m_soundEngine->m_Instruments[i]->instname;
        }
        return metadata;
    }

    std::string ReplayJayTrax::GetInfo() const
    {
        std::string info;
        char buf[8];
        sprintf(buf, "%d", m_soundEngine->m_NrOfChannels);
        info = buf;
        info += " channels\nCrossX Audio\nJayTrax";
        return info;
    }

    const Replay::Properties& ReplayJayTrax::BuildProperties()
    {
        // build realtime basic info
        auto& property = m_properties[0];
        property.numEntries = 0;
        property.data.Clear();

        property.Add("Title", Property::kIsNotEditable, m_displayEngine.m_Songs[m_subsongIndex]->name, Property::kIsEditable);

        char buf[16];
        sprintf(buf, "%d", m_displayEngine.m_Songs[m_subsongIndex]->nrofchans);
        property.Add("Channels", Property::kIsNotEditable, buf, Property::kIsNotEditable);
        sprintf(buf, "%d", m_displayEngine.m_Songs[m_subsongIndex]->songspd);
        property.Add("Speed", Property::kIsNotEditable, buf, Property::kIsNotEditable);
        sprintf(buf, "%d", m_displayEngine.m_Songs[m_subsongIndex]->groove);
        property.Add("Groove", Property::kIsNotEditable, buf, Property::kIsNotEditable);
        sprintf(buf, "%d", m_displayEngine.m_PlayPosition);
        property.Add("Order", Property::kIsNotEditable, buf, Property::kIsNotEditable);
        sprintf(buf, "%d", m_displayEngine.m_PlayStep);
        property.Add("Row", Property::kIsNotEditable, buf, Property::kIsNotEditable);
        std::string patterns;
        for (uint16_t i = 0; i < m_displayEngine.m_NrOfChannels; ++i)
        {
            auto pat = m_displayEngine.m_Songs[m_subsongIndex]->songchans[i][m_displayEngine.m_ChannelData[i]->songpos].patnr;
            sprintf(buf, "%d,", pat);
            patterns += buf;
        }
        patterns.pop_back();
        property.Add("Patterns", Property::kIsNotEditable, patterns.c_str(), Property::kIsNotEditable);

        return m_properties;
    }

    Replay::Patterns ReplayJayTrax::UpdatePatterns(uint32_t numSamples, uint32_t numLines, uint32_t charWidth, uint32_t spaceWidth, Patterns::Flags flags)
    {
        UnusedArg(numLines, charWidth, spaceWidth, flags);
        while (numSamples)
            numSamples -= m_displayEngine.RenderChannels1(nullptr, int(numSamples), kSampleRate);
        return {};
    }
}
// namespace rePlayer