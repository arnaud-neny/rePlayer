#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ReplayDll.h>

#include "ReplayWavPack.h"

#include "cli/utils.h"

#include <filesystem>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::WavPack,
        .name = "WavPack",
        .extensions = "wv",
        .about = "WavPack " PACKAGE_VERSION "\nCopyright (c) 1998-2024 David Bryant",
        .load = ReplayWavPack::Load
    };

    WavpackStreamReader64 ReplayWavPack::ms_reader = {
        ReadBytes, nullptr, GetPos, SetPosAbs, SetPosRel,
        PushBackByte, GetLength, CanSeek, nullptr, nullptr
    };

    Replay* ReplayWavPack::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto name = std::filesystem::path(stream->GetName());
        name.replace_extension("wvc");
        auto wvcStream = stream->Open(name.string());
        if (auto* context = WavpackOpenFileInputEx64(&ms_reader, stream, wvcStream.Get(), nullptr, OPEN_TAGS | OPEN_2CH_MAX | OPEN_DSD_AS_PCM, 0))
            return new ReplayWavPack(stream, wvcStream, context);
        return nullptr;
    }

    ReplayWavPack::~ReplayWavPack()
    {
        WavpackCloseFile(m_context);
    }

    uint32_t ReplayWavPack::GetSampleRate() const
    {
        return WavpackGetSampleRate(m_context);
    }

    ReplayWavPack::ReplayWavPack(io::Stream* stream, io::Stream* wvcStream, WavpackContext* context)
        : Replay(eExtension::_wv, eReplay::WavPack)
        , m_stream(stream)
        ,m_wvcStream(wvcStream)
        , m_context(context)
    {}

    int32_t ReplayWavPack::ReadBytes(void* id, void* data, int32_t bcount)
    {
        return int32_t(reinterpret_cast<io::Stream*>(id)->Read(data, bcount));
    }

    int ReplayWavPack::PushBackByte(void* id, int c)
    {
        auto status = reinterpret_cast<io::Stream*>(id)->Seek(-1, io::Stream::kSeekCurrent);
        if (status == Status::kOk)
            return c;
        return EOF;
    }

    int64_t ReplayWavPack::GetPos(void* id)
    {
        return int64_t(reinterpret_cast<io::Stream*>(id)->GetPosition());
    }

    int ReplayWavPack::SetPosAbs(void* id, int64_t pos)
    {
        return int(reinterpret_cast<io::Stream*>(id)->Seek(pos, io::Stream::kSeekBegin));
    }

    int ReplayWavPack::SetPosRel(void* id, int64_t delta, int mode)
    {
        return int(reinterpret_cast<io::Stream*>(id)->Seek(delta, mode == SEEK_CUR ? io::Stream::kSeekCurrent : (mode == SEEK_SET ? io::Stream::kSeekBegin : io::Stream::kSeekEnd)));
    }

    int64_t ReplayWavPack::GetLength(void* id)
    {
        return int64_t(reinterpret_cast<io::Stream*>(id)->GetSize());
    }

    int ReplayWavPack::CanSeek(void* id)
    {
        return reinterpret_cast<io::Stream*>(id)->GetSize() != 0;
    }

    uint32_t ReplayWavPack::Render(StereoSample* output, uint32_t numSamples)
    {
        auto* buffer = reinterpret_cast<int32_t*>(output);
        if (WavpackGetReducedChannels(m_context) == 1)
        {
            buffer += numSamples;
            numSamples = WavpackUnpackSamples(m_context, buffer, numSamples);
            if (WavpackGetMode(m_context) & MODE_FLOAT)
            {
                auto* floatBuffer = reinterpret_cast<float*>(buffer);
                for (uint32_t i = 0; i < numSamples; i++)
                    output[i].left = output[i].right = *floatBuffer++;
            }
            else
            {
                double scaler = 1.0 / (1ull << (WavpackGetBytesPerSample(m_context) * 8 - 1));
                for (uint32_t i = 0; i < numSamples; i++)
                    output[i].left = output[i].right = float(double(*buffer++) * scaler);
            }
        }
        else
        {
            numSamples = WavpackUnpackSamples(m_context, buffer, numSamples);
            if (!(WavpackGetMode(m_context) & MODE_FLOAT))
            {
                double scaler = 1.0 / (1ull << (WavpackGetBytesPerSample(m_context) * 8 - 1));
                for (uint32_t i = 0; i < numSamples; i++)
                {
                    output[i].left = float(double(*buffer++) * scaler);
                    output[i].right = float(double(*buffer++) * scaler);
                }
            }
        }
        m_bitrate = (uint32_t(WavpackGetInstantBitrate(m_context)) + 1023) / 1024;

        return numSamples;
    }

    uint32_t ReplayWavPack::Seek(uint32_t timeInMs)
    {
        auto numSamples = (int64_t(timeInMs) * GetSampleRate()) / 1000ll;
        WavpackSeekSample64(m_context, numSamples);
        return timeInMs;
    }

    void ReplayWavPack::ResetPlayback()
    {
        WavpackSeekSample(m_context, 0);
    }

    void ReplayWavPack::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayWavPack::SetSubsong(uint32_t)
    {}

    uint32_t ReplayWavPack::GetDurationMs() const
    {
        return uint32_t((1000ull * WavpackGetNumSamples64(m_context)) / GetSampleRate());
    }

    uint32_t ReplayWavPack::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayWavPack::GetExtraInfo() const
    {
        std::string info;
        for (int i = 0, e = WavpackGetNumTagItems(m_context); i < e; i++)
        {
            if (auto size = WavpackGetTagItemIndexed(m_context, i, nullptr, 0))
            {
                std::string item;
                item.resize(size);
                WavpackGetTagItemIndexed(m_context, i, item.data(), size + 1);
                std::string value;
                if (size = WavpackGetTagItem(m_context, item.c_str(), nullptr, 0))
                {
                    if (!info.empty())
                        info += "\n";
                    info += item;
                    info += ": ";
                    value.resize(size);
                    WavpackGetTagItem(m_context, item.c_str(), value.data(), size + 1);
                    info += value;
                }
            }
        }
        return info;
    }

    std::string ReplayWavPack::GetInfo() const
    {
        std::string info;
        char buf[128];
        auto numChannels = WavpackGetNumChannels(m_context);
        if (numChannels == 1)
            info = "1 channel\n";
        else
        {
            sprintf(buf, "%d channels\n", numChannels);
            info = buf;
        }
        sprintf(buf, "%d kb/s - %u hz\n", m_bitrate, WavpackGetSampleRate(m_context));
        info += buf;
        info += "WavPack " PACKAGE_VERSION;
        return info;
    }
}
// namespace rePlayer