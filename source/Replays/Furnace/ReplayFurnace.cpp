// Furnace is generated from original repository using:
// cmake .. -DBUILD_GUI=OFF -DUSE_RTMIDI=OFF -DUSE_SDL2=OFF -DUSE_SNDFILE=OFF -DUSE_BACKWARD=OFF -DWITH_JACK=OFF -DWITH_PORTAUDIO=OFF -DWITH_DEMOS=OFF -DWITH_INSTRUMENTS=OFF -DWITH_WAVETABLES=OFF
// plus some minor changes to avoid memory leak and useless libraries:
// - configEngine.cpp
// - engine.cpp
// - engine.h
// - fileOpsCommon.cpp
// - fileutils.cpp
// - ftm.cpp
// - nes.cpp
// - extern\vgsound_emu\src\scc\scc.cpp
// - tia.cpp
#include "ReplayFurnace.h"

#include <Audio/Audiotypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

#include "furnace/src/engine/fileOps/fileOpsCommon.h"

void reportError(String) {}

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::Furnace, .isThreadSafe = false,
        .name = "Furnace",
        .extensions = "fur;dmf;ftm;dnm;0cc;eft;tfm;tfe",
        .about = "Furnace " DIV_VERSION "\nCopyright (c) 2021-2025 tildearrow and contributors",
        .load = ReplayFurnace::Load,
    };

    Replay* ReplayFurnace::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto size = stream->GetSize();
        auto data = new uint8_t[size_t(size)];
        stream->Read(data, size);
        uint64_t magic;
        memcpy(&magic, data, sizeof(magic));
        auto* engine = new DivEngine;
        initLog(stdout); // could be nice to have an option to not link with logs...
        engine->preInit();
        if (engine->load(data, size_t(size), stream->GetName().c_str()) && engine->init())
        {
            engine->initDispatch(true);
            engine->renderSamplesP();
            engine->play();
            return new ReplayFurnace(engine, stream->GetName().c_str(), magic);
        }
        engine->quit(false);
        delete engine;

        return nullptr;
    }

    ReplayFurnace::~ReplayFurnace()
    {
        m_engine->quit(false);
        delete m_engine;
    }

    ReplayFurnace::ReplayFurnace(DivEngine* engine, const char* name, uint64_t magic)
        : Replay(engine->song.isDMF ? eExtension::_dmf : ((engine->song.systemName == "Sega Genesis/Mega Drive or TurboSound FM") ? (memcmp(&magic, DIV_TFM_MAGIC, sizeof(magic)) == 0) ? eExtension::_tfm : eExtension::_tfe : (engine->song.version == DIV_VERSION_FTM ? GetFamitrackerExtension(name) : eExtension::_fur)), eReplay::Furnace)
        , m_engine(engine)
    {
        uint8_t sysCount[DIV_MAX_CHIPS] = {};
        DivSystem sysId[DIV_MAX_CHIPS];
        uint8_t numSys = 0;

        for (uint8_t i = 0; i < engine->song.systemLen; ++i)
        {
            uint8_t curSys = 0;
            for (; curSys < numSys; ++curSys)
            {
                if (sysId[curSys] == engine->song.system[i])
                    break;
            }
            sysId[curSys] = engine->song.system[i];
            if (sysCount[curSys]++ == 0)
                numSys++;
        }
        for (uint8_t i = 0; i < numSys; ++i)
        {
            if (i > 0)
                m_systems += '+';

            if (sysCount[i] > 1)
            {
                char buf[8];
                sprintf(buf, "[%dx]", sysCount[i]);
                m_systems += buf;
            }
            m_systems += engine->getSystemDef(sysId[i])->name;
        }
    }

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
            info += "Famitracker: ";
        else if (m_engine->song.isDMF)
            info += "DefleMask: ";
        info += m_systems;
        info += "\nFurnace " DIV_VERSION;
        return info;
    }

    eExtension ReplayFurnace::GetFamitrackerExtension(const char* name)
    {
        auto c = strrchr(name, '.');
        if (c)
        {
            if (_stricmp(c + 1, "dnm") == 0)
                return eExtension::_dnm;
            else if(_stricmp(c + 1, "0cc") == 0)
                return eExtension::_0cc;
            else if (_stricmp(c + 1, "eft") == 0)
                return eExtension::_eft;
        }
        return eExtension::_ftm;
    }
}
// namespace rePlayer