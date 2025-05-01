#include "ReplayAAC.h"

#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ReplayDll.h>

namespace rePlayer
{

    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::AAC,
        .name = "Advanced Audio Coding",
        .extensions = "aac",
        .about = "Freeware Advanced Audio Codec " FAAD2_VERSION "\nCopyright (c) 2003-2005 M. Bakker, Nero AG, http://www.nero.com",
        .load = ReplayAAC::Load
    };

    Replay* ReplayAAC::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto* replay = new ReplayAAC(stream);
        if (replay->Init() == Status::kOk)
            return replay;

        delete replay;
        return nullptr;
    }

    ReplayAAC::~ReplayAAC()
    {
        NeAACDecClose(m_hDecoder);
        while (m_bitRate.tail)
        {
            auto* next = m_bitRate.tail->next;
            delete m_bitRate.tail;
            m_bitRate.tail = next;
        }
        while (m_bitRate.free)
        {
            auto* next = m_bitRate.free->next;
            delete m_bitRate.free;
            m_bitRate.free = next;
        }
    }

    ReplayAAC::ReplayAAC(io::Stream* stream)
        : Replay(eExtension::_aac, eReplay::AAC)
        , m_stream(stream)
        , m_isSeekable(stream->Seek(0, io::Stream::kSeekEnd) == Status::kOk)
    {
        stream->Seek(0, io::Stream::kSeekBegin);
    }

    bool ReplayAAC::IsSeekable() const
    {
        return m_isSeekable;
    }

    bool ReplayAAC::IsStreaming() const
    {
        return !m_isSeekable;
    }

    uint32_t ReplayAAC::Render(StereoSample* output, uint32_t numSamples)
    {
        auto remainingSamples = numSamples;
        while (remainingSamples)
        {
            if (m_remainingSamples == 0)
            {
                if (m_bytesIntoBuffer == 0)
                    return numSamples - remainingSamples;

                NeAACDecFrameInfo frameInfo;
                m_sampleBuffer = reinterpret_cast<StereoSample*>(NeAACDecDecode(m_hDecoder, &frameInfo, m_buffer, m_bytesIntoBuffer));

                AdvanceBuffer(frameInfo.bytesconsumed);
                FillBuffer();

                if (m_sampleBuffer == nullptr || frameInfo.error > 0)
                    return numSamples - remainingSamples;
                m_numSamples = m_remainingSamples = frameInfo.samples / frameInfo.channels;
                assert(m_sampleRate == frameInfo.samplerate && frameInfo.channels == 2);

                // compute the bitrate per second
                auto* bitRate = m_bitRate.free;
                if (bitRate == nullptr)
                    bitRate = new BitRateData;
                else
                {
                    m_bitRate.free = bitRate->next;
                    bitRate->next = nullptr;
                }
                m_bitRate.head->next = bitRate;
                m_bitRate.head = bitRate;

                bitRate->numFrames = m_remainingSamples;
                bitRate->numBits = frameInfo.bytesconsumed * 8;
                m_bitRate.numFrames += bitRate->numFrames;
                m_bitRate.numBits += bitRate->numBits;
                while (m_bitRate.numFrames > frameInfo.samplerate)
                {
                    if (m_bitRate.tail->next == nullptr)
                        break;
                    auto* tail = m_bitRate.tail;
                    m_bitRate.tail = tail->next;
                    m_bitRate.numFrames -= tail->numFrames;
                    m_bitRate.numBits -= tail->numBits;
                    tail->next = m_bitRate.free;
                    m_bitRate.free = tail;
                }
                m_bitRate.average = uint32_t((m_bitRate.numBits * frameInfo.samplerate + 1000 * m_bitRate.numFrames - 1) / (1000 * m_bitRate.numFrames));
            }
            else
            {
                auto numSamplesToCopy = Min(remainingSamples, m_remainingSamples);
                memcpy(output, m_sampleBuffer, sizeof(StereoSample) * numSamplesToCopy);
                output += numSamplesToCopy;
                m_sampleBuffer += numSamplesToCopy;
                remainingSamples -= numSamplesToCopy;
                m_remainingSamples -= numSamplesToCopy;
                m_position += numSamplesToCopy;
            }
        }

        return numSamples;
    }

    uint32_t ReplayAAC::Seek(uint32_t timeInMs)
    {
        m_position = m_position + m_remainingSamples - m_numSamples;
        m_sampleBuffer = m_sampleBuffer + m_remainingSamples - m_numSamples;
        m_remainingSamples = m_numSamples;

        auto numSamples = (uint64_t(timeInMs) * m_sampleRate) / 1000;
        auto remainingSamples = uint32_t(numSamples);
        if (numSamples < m_position)
            ResetPlayback();
        else
            remainingSamples = uint32_t(numSamples - m_position);

        while (remainingSamples)
        {
            if (m_remainingSamples == 0)
            {
                if (DecodeFrame() != Status::kOk)
                    break;
            }
            else
            {
                auto numSamplesToCopy = Min(remainingSamples, m_remainingSamples);
                m_sampleBuffer += numSamplesToCopy;
                remainingSamples -= numSamplesToCopy;
                m_remainingSamples -= numSamplesToCopy;
                m_position += numSamplesToCopy;
            }
        }
        numSamples -= m_remainingSamples;

        return uint32_t((numSamples * 1000) / m_sampleRate);
    }

    void ReplayAAC::ResetPlayback()
    {
        while (m_bitRate.tail->next)
        {
            auto* next = m_bitRate.tail->next;
            m_bitRate.tail->next = m_bitRate.free;
            m_bitRate.free = m_bitRate.tail;
            m_bitRate.tail = next;
        }
        m_bitRate.tail->numBits = 0;
        m_bitRate.tail->numFrames = 0;
        m_bitRate.head = m_bitRate.tail;
        m_position = 0;
        m_numSamples = m_remainingSamples = 0;
        NeAACDecClose(m_hDecoder);
        Init();
    }

    void ReplayAAC::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayAAC::SetSubsong(uint32_t)
    {}

    uint32_t ReplayAAC::GetDurationMs() const
    {
        if (m_isSeekable)
        {
            auto stream = m_stream->Clone();

            auto* replay = new ReplayAAC(stream);

            auto bytesRead = stream->Read(replay->m_buffer, sizeof(replay->m_buffer));
            replay->m_bytesIntoBuffer = uint32_t(bytesRead);
            replay->m_bytesConsumed = 0;

            replay->m_isEof = bytesRead != sizeof(replay->m_buffer);

            int32_t tagSize = 0;
            if (!memcmp(replay->m_buffer, "ID3", 3))
            {
                // high bit is not used
                tagSize = (replay->m_buffer[6] << 21) | (replay->m_buffer[7] << 14) | (replay->m_buffer[8] << 7) | (replay->m_buffer[9] << 0);

                tagSize += 10;
                replay->AdvanceBuffer(tagSize);
                replay->FillBuffer();
            }

            if ((replay->m_buffer[0] == 0xFF) && ((replay->m_buffer[1] & 0xF6) == 0xF0))
            {
                size_t frames = 0;

                // Read all frames to ensure correct time and bitrate
                for (;; frames++)
                {
                    replay->FillBuffer();

                    if (replay->m_bytesIntoBuffer > 7)
                    {
                        // check syncword
                        if (!((replay->m_buffer[0] == 0xFF) && ((replay->m_buffer[1] & 0xF6) == 0xF0)))
                            break;

                        auto frameLength = (uint32_t(replay->m_buffer[3] & 0x3) << 11) | (uint32_t(replay->m_buffer[4]) << 3) | (replay->m_buffer[5] >> 5);
                        if (frameLength == 0)
                            break;

                        if (frameLength > replay->m_bytesIntoBuffer)
                            break;

                        replay->AdvanceBuffer(frameLength);
                    }
                    else
                        break;
                }
                delete replay;

                if (frames)
                    return uint32_t((frames * 1024 * 1000) / m_sampleRate);
            }
            else if (memcmp(replay->m_buffer, "ADIF", 4) == 0)
            {
                auto skipSize = (replay->m_buffer[4] & 0x80) ? 9 : 0;
                auto bitrate = (uint32_t(replay->m_buffer[4 + skipSize] & 0x0F) << 19) |
                    (uint32_t(replay->m_buffer[5 + skipSize]) << 11) |
                    (uint32_t(replay->m_buffer[6 + skipSize]) << 3) |
                    (uint32_t(replay->m_buffer[7 + skipSize]) & 0xE0);

                return uint32_t((stream->GetSize() * 8 * 1000) / bitrate);
            }
        }
        return 0;
    }

    uint32_t ReplayAAC::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayAAC::GetStreamingTitle() const
    {
        std::string title;
        title = m_stream->GetTitle();
        return title;
    }

    std::string ReplayAAC::GetExtraInfo() const
    {
        std::string metadata;
        metadata = m_stream->GetInfo();
        return metadata;
    }

    std::string ReplayAAC::GetInfo() const
    {
        std::string info;
        info = "2 channels\n";
        char buf[32];
        sprintf(buf, "%u kb/s - %u hz", m_bitRate.average.load(), m_sampleRate);
        info += buf;
        if (IsStreaming())
        {
            sprintf(buf, " (%.2f KB cache)", m_stream->GetAvailableSize() / 1024.0);
            info += buf;
        }
        info += "\nFAAD " FAAD2_VERSION;
        return info;
    }

    Status ReplayAAC::Init()
    {
        m_stream->Seek(0, io::Stream::kSeekBegin);
        core::Scope scope = {
            [stream = m_stream.Get()]() { stream->EnableLatency(true); },
            [stream = m_stream.Get()]() { stream->EnableLatency(false); },
        };

        auto bytesRead = m_stream->Read(m_buffer, sizeof(m_buffer));
        m_bytesIntoBuffer = uint32_t(bytesRead);
        m_bytesConsumed = 0;

        m_isEof = bytesRead != sizeof(m_buffer);

        uint32_t tagSize = 0;
        if (!memcmp(m_buffer, "ID3", 3))
        {
            /* high bit is not used */
            tagSize = (uint32_t(m_buffer[6]) << 21) | (uint32_t(m_buffer[7]) << 14) | (uint32_t(m_buffer[8]) << 7) | (uint32_t(m_buffer[9]) << 0);

            tagSize += 10;
            AdvanceBuffer(tagSize);
            FillBuffer();
        }

        if (!m_isSeekable)
            LookForHeader();

        m_hDecoder = NeAACDecOpen();
        auto config = NeAACDecGetCurrentConfiguration(m_hDecoder);
        config->outputFormat = FAAD_FMT_FLOAT;
        NeAACDecSetConfiguration(m_hDecoder, config);

        uint8_t channels;
        auto bytesUsed = NeAACDecInit(m_hDecoder, m_buffer, m_bytesIntoBuffer, reinterpret_cast<unsigned long*>(&m_sampleRate), &channels);
        if (bytesUsed < 0 || channels != 2)
            return Status::kFail;

        AdvanceBuffer(bytesUsed);
        FillBuffer();

        NeAACDecPostSeekReset(m_hDecoder, 1);

        return DecodeFrame();
    }

    Status ReplayAAC::DecodeFrame()
    {
        if (m_bytesIntoBuffer == 0)
            return Status::kFail;

        NeAACDecFrameInfo frameInfo;
        m_sampleBuffer = reinterpret_cast<StereoSample*>(NeAACDecDecode(m_hDecoder, &frameInfo, m_buffer, m_bytesIntoBuffer));

        AdvanceBuffer(frameInfo.bytesconsumed);
        FillBuffer();

        if (m_sampleBuffer == nullptr || frameInfo.error > 0)
            return Status::kFail;
        if (m_sampleRate != frameInfo.samplerate || frameInfo.channels != 2)
            return Status::kFail;
        m_numSamples = m_remainingSamples = frameInfo.samples / frameInfo.channels;

        // compute the bitrate per second
        auto* bitRate = m_bitRate.free;
        if (bitRate == nullptr)
            bitRate = new BitRateData;
        else
        {
            m_bitRate.free = bitRate->next;
            bitRate->next = nullptr;
        }
        m_bitRate.head->next = bitRate;
        m_bitRate.head = bitRate;

        bitRate->numFrames = m_remainingSamples;
        bitRate->numBits = frameInfo.bytesconsumed * 8;
        m_bitRate.numFrames += bitRate->numFrames;
        m_bitRate.numBits += bitRate->numBits;
        while (m_bitRate.numFrames > frameInfo.samplerate)
        {
            if (m_bitRate.tail->next == nullptr)
                break;
            auto* tail = m_bitRate.tail;
            m_bitRate.tail = tail->next;
            m_bitRate.numFrames -= tail->numFrames;
            m_bitRate.numBits -= tail->numBits;
            tail->next = m_bitRate.free;
            m_bitRate.free = tail;
        }
        m_bitRate.average = uint32_t((m_bitRate.numBits * frameInfo.samplerate + 1024 * m_bitRate.numFrames - 1) / (1024 * m_bitRate.numFrames));

        return Status::kOk;
    }

    void ReplayAAC::AdvanceBuffer(uint32_t bytes)
    {
        while ((m_bytesIntoBuffer > 0) && (bytes > 0))
        {
            uint32_t chunk = bytes;
            if (m_bytesIntoBuffer < chunk)
                chunk = uint32_t(m_bytesIntoBuffer);

            bytes -= chunk;
            m_bytesConsumed = chunk;
            m_bytesIntoBuffer -= chunk;

            if (m_bytesIntoBuffer == 0)
                FillBuffer();
        }
    }

    void ReplayAAC::FillBuffer()
    {
        if (m_bytesConsumed > 0)
        {
            if (m_bytesIntoBuffer)
                memmove(m_buffer, m_buffer + m_bytesConsumed, m_bytesIntoBuffer);

            if (!m_isEof)
            {
                auto bytesRead = m_stream->Read(m_buffer + m_bytesIntoBuffer, m_bytesConsumed);

                m_isEof = bytesRead != m_bytesConsumed;

                m_bytesIntoBuffer += (unsigned long)bytesRead;
            }

            m_bytesConsumed = 0;

            if (m_bytesIntoBuffer > 3)
            {
                if (memcmp(m_buffer, "TAG", 3) == 0)
                    m_bytesIntoBuffer = 0;
            }
            if (m_bytesIntoBuffer > 11)
            {
                if (memcmp(m_buffer, "LYRICSBEGIN", 11) == 0)
                    m_bytesIntoBuffer = 0;
            }
            if (m_bytesIntoBuffer > 8)
            {
                if (memcmp(m_buffer, "APETAGEX", 8) == 0)
                    m_bytesIntoBuffer = 0;
            }
        }
    }

    void ReplayAAC::LookForHeader()
    {
        for (uint32_t i = 0; !m_isEof;)
        {
            if (m_bytesIntoBuffer > 4)
            {
                if (((m_buffer[0 + i] == 0xff) && ((m_buffer[1 + i] & 0xf6) == 0xf0)) ||
                    (m_buffer[0 + i] == 'A' && m_buffer[1 + i] == 'D' && m_buffer[2 + i] == 'I' && m_buffer[3 + i] == 'F'))
                {
                    FillBuffer();
                    break;
                }
                else
                {
                    i++;
                    m_bytesConsumed += 1;
                    m_bytesIntoBuffer -= 1;
                }
            }
            else
            {
                FillBuffer();
                i = 0;
            }
        }
    }
}
// namespace rePlayer