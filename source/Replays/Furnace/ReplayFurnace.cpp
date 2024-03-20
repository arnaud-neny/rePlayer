// Furnace is generated from original repository using:
// cmake .. -DBUILD_GUI=OFF -DUSE_RTMIDI=OFF -DUSE_SDL2=OFF -DUSE_SNDFILE=OFF -DUSE_BACKWARD=OFF -DWITH_JACK=OFF -DWITH_PORTAUDIO=OFF -DWITH_DEMOS=OFF -DWITH_INSTRUMENTS=OFF -DWITH_WAVETABLES=OFF
// plus some minor changes to avoid memory leak and useless libraries:
// - configEngine.cpp
// - engine.cpp
// - engine.h
// - fileOps.cpp
// - fileutills.cpp
#include "ReplayFurnace.h"

#include <Audio/Audiotypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

void reportError(String) {}

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::Furnace, .isThreadSafe = false,
        .name = "Furnace",
        .extensions = "fur;dmf;ftm",
        .about = "Furnace " DIV_VERSION "\nCopyright (c) 2021-2024 tildearrow and contributors",
        .load = ReplayFurnace::Load,
    };

    Replay* ReplayFurnace::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto size = stream->GetSize();
        auto data = new uint8_t[size];
        stream->Read(data, size);
        auto* engine = new DivEngine;
        engine->preInit();
        if (engine->load(data, size) && engine->init())
        {
            engine->initDispatch(true);
            engine->renderSamplesP();
            engine->play();
            return new ReplayFurnace(engine);
        }
        engine->quit();
        delete engine;

        return nullptr;
    }

    ReplayFurnace::~ReplayFurnace()
    {
        m_engine->quit();
        delete m_engine;
    }

    ReplayFurnace::ReplayFurnace(DivEngine* engine)
        : Replay(engine->song.isDMF ? eExtension::_dmf : (engine->song.version == DIV_VERSION_FTM ? eExtension::_ftm : eExtension::_fur), eReplay::Furnace)
        , m_engine(engine)
    {}

    uint32_t ReplayFurnace::GetSampleRate() const
    {
        return uint32_t(m_engine->getAudioDescGot().rate);
    }

    uint32_t ReplayFurnace::Render(StereoSample* output, uint32_t numSamples)
    {
        float* samples[] = { m_samples[0], m_samples[1] };
        uint32_t numSamplesRendered = 0;
        uint32_t numRemainingSamples = m_numRemainingSamples;
        while (numSamples)
        {
            if (numRemainingSamples == 0)
            {
                m_engine->nextBuf(nullptr, samples, 0, 2, kMaxSamples);
                numRemainingSamples = kMaxSamples;
                m_lastLoopPos = m_engine->lastLoopPos;
            }
            auto numSamplesAvailable = numRemainingSamples;
            if (m_lastLoopPos > -1)
            {
                numSamplesAvailable = m_lastLoopPos - (kMaxSamples - numRemainingSamples);
                if (numSamplesAvailable == 0)
                {
                    if (numSamplesRendered == 0)
                        m_lastLoopPos = -1;
                    break;
                }
            }
            auto numSamplesToCopy = Min(numSamples, numSamplesAvailable);
            for (uint32_t i = 0; i < numSamplesToCopy; i++)
            {
                output[i].left = samples[0][kMaxSamples - numRemainingSamples + i];
                output[i].right = samples[1][kMaxSamples - numRemainingSamples + i];
            }
            output += numSamplesToCopy;
            numSamples -= numSamplesToCopy;
            numRemainingSamples -= numSamplesToCopy;
            numSamplesRendered += numSamplesToCopy;
        }
        m_numRemainingSamples = numRemainingSamples;
        return numSamplesRendered;
    }

    void ReplayFurnace::ResetPlayback()
    {
        m_engine->changeSongP(m_subsongIndex);
        m_engine->play();
        m_position = 0;
        m_lastLoopPos = -1;
        m_numRemainingSamples = 0;
    }

    void ReplayFurnace::ApplySettings(const CommandBuffer metadata)
    {
        (void)metadata;
    }

    void ReplayFurnace::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayFurnace::GetDurationMs() const
    {
        return 0;
    }

    uint32_t ReplayFurnace::GetNumSubsongs() const
    {
        return uint32_t(m_engine->song.subsong.size());
    }

    std::string ReplayFurnace::GetSubsongTitle() const
    {
        return m_engine->song.subsong[m_subsongIndex]->name;
    }

    std::string ReplayFurnace::GetExtraInfo() const
    {
        std::string metadata;
        if (!m_engine->song.name.empty() || !m_engine->song.subsong[m_subsongIndex]->name.empty())
        {
            metadata  = "Title  : ";
            metadata += m_engine->song.name;
            if (!m_engine->song.name.empty() && !m_engine->song.subsong[m_subsongIndex]->name.empty())
                metadata += " / ";
            metadata += m_engine->song.subsong[m_subsongIndex]->name;
        }
        if (!m_engine->song.author.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Artist : ";
            metadata += m_engine->song.author;
        }
        if (!m_engine->song.systemName.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "System : ";
            metadata += m_engine->song.systemName;
        }
        if (!m_engine->song.notes.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += m_engine->song.notes;
        }
        if (!m_engine->song.subsong[m_subsongIndex]->notes.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += m_engine->song.subsong[m_subsongIndex]->notes;
        }
        return metadata;
    }

    std::string ReplayFurnace::GetInfo() const
    {
        std::string info;
        char buf[16];
        sprintf(buf, "%d", m_engine->getTotalChannelCount());
        info = buf;
        info += " channels\n";
        if (m_engine->song.version == DIV_VERSION_FTM)
            info += "Famitracker";
        else
        {
            info += m_engine->song.isDMF ? "DefleMask v" : "Furnace v";
            sprintf(buf, "%d", m_engine->song.version);
            info += buf;
        }
        info += "\nFurnace " DIV_VERSION;
        return info;
    }
}
// namespace rePlayer