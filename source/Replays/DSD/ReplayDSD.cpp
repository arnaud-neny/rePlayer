#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ReplayDll.h>

#include "ReplayDSD.h"

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::DSD,
        .name = "Direct Stream Digital",
        .extensions = "dsf;dff",
        .about = "Direct Stream Digital\nCopyright (c) 2015-2019 Robert Tari\nCopyright (c) 2012 Vladislav Goncharov\nCopyright (c) 2011-2016 Maxim V.Anisiutkin",
        .load = ReplayDSD::Load
    };

#pragma pack(1)
    struct ID
    {
        uint32_t id;
        bool operator==(const char* check) const
        {
            return memcmp(&id, check, 4) == 0;
        }
        bool Read(io::Stream* stream, const char* check)
        {
            if (stream->Read(this, sizeof(ID)) != sizeof(ID))
            {
                id = 0;
                return false;
            }
            return *this == check;
        }
    };

    struct Chunk : public ID
    {
        uint64_t size;
        bool Read(io::Stream* stream, const char* check)
        {
            if (stream->Read(this, sizeof(Chunk)) != sizeof(Chunk))
            {
                id = 0;
                return false;
            }
            return *this == check;
        }
        uint64_t GetSize() const { return _byteswap_uint64(size); }
    };

    struct FmtDSFChunk
    {
        uint32_t formatVersion;
        uint32_t formatId;
        uint32_t channelType;
        uint32_t channelCount;
        uint32_t samplerate;
        uint32_t bitsPerSample;
        uint64_t sampleCount;
        uint32_t blockSize;
        uint32_t reserved;
    };

    enum MarkType : uint16_t
    {
        TrackStart = 0,
        TrackStop = 1,
        ProgramStart = 2,
        Index = 4
    };

    struct Marker
    {
        uint16_t hours;
        uint8_t minutes;
        uint8_t seconds;
        uint32_t samples;
        int32_t offset;
        MarkType markType;
        uint16_t markChannel;
        uint16_t TrackFlags;
        uint32_t count;
        uint64_t GetTime(uint32_t sampleRate) const
        {
            return sampleRate * (hours * 60ull * 60ull + minutes * 60ull + seconds) + samples + offset;
        }
    };
#pragma pack()

    Replay* ReplayDSD::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        Chunk chunk;
        if (chunk.Read(stream,"DSD "))
        {
            // try to load  a dsf
            if (chunk.size != 28)
                return nullptr;
            DSF dsf;
            if (stream->Read(&dsf.fileSize, sizeof(dsf.fileSize)) != sizeof(dsf.fileSize))
                return nullptr;
            if (stream->Read(&dsf.id3Offset, sizeof(dsf.id3Offset)) != sizeof(dsf.id3Offset))
                return nullptr;

            auto pos = stream->GetPosition();

            if (!chunk.Read(stream, "fmt "))
                return nullptr;

            FmtDSFChunk fmt;
            if (stream->Read(&fmt, sizeof(fmt)) != sizeof(fmt))
                return nullptr;
            if (fmt.formatId != 0)
                return nullptr;
            if (fmt.channelCount < 1 || fmt.channelCount > 6)
                return nullptr;
            dsf.numChannels = fmt.channelCount;
            dsf.sampleRate = fmt.samplerate;
            if (fmt.bitsPerSample == 1)
            {
                dsf.isLSB = true;
                for (int i = 0; i < 256; i++)
                {
                    dsf.swapBits[i] = 0;

                    for (int j = 0; j < 8; j++)
                    {
                        dsf.swapBits[i] |= ((i >> j) & 1) << (7 - j);
                    }
                }
            }
            else if (fmt.bitsPerSample == 8)
                dsf.isLSB = false;
            else
                return nullptr;

            dsf.numSamples = fmt.sampleCount;
            dsf.blockOffset = dsf.blockSize = fmt.blockSize;
            dsf.blockEnd = 0;

            if (stream->Seek(pos + chunk.size, io::Stream::kSeekBegin) != Status::kOk)
                return nullptr;

            if (!chunk.Read(stream, "data"))
                return nullptr;

            dsf.blockData = core::Alloc<uint8_t>(dsf.numChannels * dsf.blockSize);
            dsf.dataOffset = stream->GetPosition();
            dsf.dataEnd = dsf.dataOffset + (((dsf.numSamples + 7) / 8) * dsf.numChannels);
            dsf.dataSize = chunk.size - sizeof(chunk);

            return new ReplayDSD(stream, std::move(dsf));
        }
        else if (chunk == "FRM8")
        {
            // try to load  a dff
            ID id;
            if (!id.Read(stream, "DSD "))
                return nullptr;
            auto frm8Size = chunk.GetSize() + sizeof(chunk);
            DFF dff = {};
//             uint32_t numStartMarks = 0;
            while (stream->GetPosition() < frm8Size)
            {
                if (chunk.Read(stream, "FVER"))
                {
                    if (chunk.GetSize() == 4)
                    {
                        if (stream->Read(&dff.version, sizeof(dff.version)) != sizeof(dff.version))
                            return nullptr;
                        dff.version = _byteswap_ulong(dff.version);
                    }
                }
                else if (chunk == "PROP")
                {
                    if (!id.Read(stream, "SND "))
                        return nullptr;
                    auto idPropEnd = stream->GetPosition() - sizeof(chunk) + chunk.GetSize();
                    while (stream->GetPosition() < idPropEnd)
                    {
                        if (chunk.Read(stream, "FS  "))
                        {
                            if (chunk.GetSize() == 4)
                            {
                                if (stream->Read(&dff.sampleRate, sizeof(dff.sampleRate)) != sizeof(dff.sampleRate))
                                    return nullptr;
                                dff.sampleRate = _byteswap_ulong(dff.sampleRate);
                            }
                        }
                        else if (chunk == "CHNL")
                        {
                            if (stream->Read(&dff.numChannels, sizeof(dff.numChannels)) != sizeof(dff.numChannels))
                                return nullptr;
                            dff.numChannels = _byteswap_ushort(dff.numChannels);
                            stream->Seek(chunk.GetSize() - sizeof(dff.numChannels), io::Stream::kSeekCurrent);
                        }
                        else if (chunk == "CMPR")
                        {
                            if (id.Read(stream, "DST "))
                                dff.isDstEncoded = true;
                            stream->Seek(chunk.GetSize() - sizeof(id), io::Stream::kSeekCurrent);
                        }
                        else if (chunk == "LSCO")
                        {
                            if (stream->Read(&dff.loudspeakerConfig, sizeof(dff.loudspeakerConfig)) != sizeof(dff.loudspeakerConfig))
                                return nullptr;
                            dff.loudspeakerConfig = _byteswap_ushort(dff.loudspeakerConfig);
                            stream->Seek(chunk.GetSize() - sizeof(dff.loudspeakerConfig), io::Stream::kSeekCurrent);
                        }
                        else if (chunk == "ID3 ")
                        {
//                             t_old.index = 0;
//                             t_old.offset = stream->GetPosition();
//                             t_old.id3_value.resize((uint32_t)ck.get_size());
//                             m_file->read(t_old.id3_value.data(), t_old.id3_value.size());
                            stream->Seek(chunk.GetSize(), io::Stream::kSeekCurrent);
                        }
                        else
                        {
                            stream->Seek(chunk.GetSize(), io::Stream::kSeekCurrent);
                        }

                        stream->Seek(stream->GetPosition() & 1, io::Stream::kSeekCurrent);
                    }
                }
                else if (chunk == "DSD ")
                {
                    dff.dataOffset = stream->GetPosition();
                    dff.dataSize = chunk.GetSize();
                    dff.frameRate = 75;
                    dff.frameSize = dff.sampleRate / 8 * dff.numChannels / dff.frameRate;
                    dff.numFrames = uint32_t(dff.dataSize / dff.frameSize);
                    stream->Seek(chunk.GetSize(), io::Stream::kSeekCurrent);
//                     dff.subsongs.Add({ 0, dff.numFrames });
                }
                else if (chunk == "DST ")
                {
                    dff.dataOffset = stream->GetPosition();
                    dff.dataSize = chunk.GetSize();

                    if (!chunk.Read(stream, "FRTE") || chunk.GetSize() != 6)
                        return nullptr;

                    dff.dataOffset += sizeof(chunk) + chunk.GetSize();
                    dff.dataSize -= sizeof(chunk) + chunk.GetSize();
                    dff.currentOffset = dff.dataOffset;
                    dff.currentSize = dff.dataSize;
                    if (stream->Read(&dff.numFrames, sizeof(dff.numFrames)) != sizeof(dff.numFrames))
                        return nullptr;
                    dff.numFrames = _byteswap_ulong(dff.numFrames);
                    if (stream->Read(&dff.frameRate, sizeof(dff.frameRate)) != sizeof(dff.frameRate))
                        return nullptr;
                    dff.frameRate = _byteswap_ushort(dff.frameRate);
                    dff.frameSize = dff.sampleRate / 8 * dff.numChannels / dff.frameRate;
                    stream->Seek(dff.dataOffset + dff.dataSize, io::Stream::kSeekBegin);
//                     dff.subsongs.Add({ 0, dff.numFrames });
                }
                else if (chunk == "DSTI")
                {
                    dff.dstiOffset = stream->GetPosition();
                    dff.dstiSize = chunk.GetSize();
                    stream->Seek(chunk.GetSize(), io::Stream::kSeekCurrent);
                }
                else if (chunk == "DIIN")
                {
                    uint64_t idDiinEnd = stream->GetPosition() + chunk.GetSize();

                    while (stream->GetPosition() < idDiinEnd)
                    {
                        if (chunk.Read(stream, "MARK") && chunk.GetSize() >= sizeof(Marker))
                        {
                            Marker marker;
                            if (stream->Read(&marker, sizeof(marker)) == sizeof(marker))
                            {
/*
                                marker.hours = _byteswap_ushort(marker.hours);
                                marker.samples = _byteswap_ulong(marker.samples);
                                marker.offset = _byteswap_ulong(marker.offset);
                                marker.markType = MarkType(_byteswap_ushort(marker.markType));
                                marker.markChannel = _byteswap_ushort(marker.markChannel);
                                marker.TrackFlags = _byteswap_ushort(marker.TrackFlags);
                                marker.count = _byteswap_ulong(marker.count);

                                switch (marker.markType)
                                {
                                case TrackStart:
                                    if (numStartMarks > 0)
                                        dff.subsongs.Push();

                                    numStartMarks++;

                                    if (dff.subsongs.NumItems() > 0)
                                    {
                                    dff.sampleRate / 8 * dff.numChannels / dff.frameRate
                                        dff.subsongs.Last().start = marker.GetTime(dff.sampleRate) / (dff.sampleRate / 8 / dff.frameRate);
                                        dff.subsongs.Last().stop = dff.numFrames;

                                        if (dff.subsongs.NumItems() > 1)
                                        {
                                            if (dff.subsongs[dff.subsongs.NumItems() - 2].stop > dff.subsongs.Last().start)
                                                dff.subsongs[dff.subsongs.NumItems() - 2].stop = dff.subsongs.Last().start;
                                        }
                                    }
                                    break;
                                case TrackStop:
                                    if (dff.subsongs.NumItems() > 0)
                                        dff.subsongs.Last().stop = marker.GetTime(dff.sampleRate) / (dff.sampleRate / 8 / dff.frameRate);
                                    break;
                                }
*/
                            }

                            stream->Seek(chunk.GetSize() - sizeof(Marker), io::Stream::kSeekCurrent);
                        }
                        else
                            stream->Seek(chunk.GetSize(), io::Stream::kSeekCurrent);

                        stream->Seek(stream->GetPosition() & 1, io::Stream::kSeekCurrent);
                    }
                }
                else if (chunk == "ID3 ")
                {
//                     id3tags_t t;
//                     t.index = m_id3tags.size();
//                     t.offset = m_file->get_position();
//                     t.id3_value.resize((uint32_t)ck.get_size());
//                     m_file->read(t.id3_value.data(), t.id3_value.size());
//                     m_id3tags.push_back(t);
                    stream->Seek(chunk.GetSize(), io::Stream::kSeekCurrent);
                }
                else
                    stream->Seek(chunk.GetSize(), io::Stream::kSeekCurrent);

                stream->Seek(stream->GetPosition() & 1, io::Stream::kSeekCurrent);
            }
            stream->Seek(dff.dataOffset, io::Stream::kSeekBegin);

            // track 0
            dff.currentOffset = dff.dataOffset;
            dff.currentSize = dff.dataSize;

            return new ReplayDSD(stream, std::move(dff));
        }
        return nullptr;
    }

    ReplayDSD::~ReplayDSD()
    {
        core::Free(m_dsf.blockData);
        core::Free(m_arrDsdBuf);
        core::Free(m_arrDstBuf);
        core::Free(m_arrPcmBuf);
        if (m_dff.isDstEncoded)
            m_decoder.close();
    }

    ReplayDSD::ReplayDSD(io::Stream* stream, DSF&& dsf)
        : Replay(eExtension::_dsf, eReplay::DSD)
        , m_stream(stream)
        , m_dsf(std::move(dsf))
        , m_arrDsdBuf(nullptr)
    {
        m_converter.init(int(dsf.numChannels), int(dsf.sampleRate), int(kSampleRate));
        m_dstSize = dsf.sampleRate / 8 / 75 * dsf.numChannels;
        m_numPcmOutSamples = kSampleRate / 75;
        m_arrDstBuf = core::Alloc<uint8_t>(m_dstSize);
        m_arrPcmBuf = core::Alloc<float>(dsf.numChannels * m_numPcmOutSamples * sizeof(float));

        float fPcmOutDelay = m_converter.get_delay();
        m_numPcmOutDelta = (int)(fPcmOutDelay - 0.5f);//  + 0.5f originally
        if (m_numPcmOutDelta > m_numPcmOutSamples - 1)
        {
            m_numPcmOutDelta = m_numPcmOutSamples - 1;
        }
    }

    ReplayDSD::ReplayDSD(io::Stream* stream, DFF&& dff)
        : Replay(eExtension::_dff, eReplay::DSD)
        , m_stream(stream)
        , m_dff(std::move(dff))
    {
        m_converter.init(int(dff.numChannels), int(dff.sampleRate), int(kSampleRate));
        m_dstSize = dff.sampleRate / 8 / dff.frameRate * dff.numChannels;
        m_numPcmOutSamples = kSampleRate / dff.frameRate;
        m_arrDsdBuf = core::Alloc<uint8_t>(m_dstSize);
        m_arrDstBuf = core::Alloc<uint8_t>(m_dstSize);
        m_arrPcmBuf = core::Alloc<float>(dff.numChannels * m_numPcmOutSamples * sizeof(float));

        float fPcmOutDelay = m_converter.get_delay();
        m_numPcmOutDelta = (int)(fPcmOutDelay - 0.5f);//  + 0.5f originally
        if (m_numPcmOutDelta > m_numPcmOutSamples - 1)
        {
            m_numPcmOutDelta = m_numPcmOutSamples - 1;
        }

        if (dff.isDstEncoded)
            m_decoder.init(dff.numChannels, (dff.sampleRate / 44100) / (dff.frameRate / 75));
    }

    uint32_t ReplayDSD::Render(StereoSample* output, uint32_t numSamples)
    {
        auto numChannels = m_arrDsdBuf ? m_dff.numChannels : m_dsf.numChannels;
        auto remainingSamples = numSamples;
        while (remainingSamples)
        {
            if (m_remainingSamples == 0)
            {
                auto* dstBuf = m_arrDstBuf;
                uint32_t frameSize;
                if (m_arrDsdBuf)
                {
                    auto dffState = ReadDFF();
                    if (dffState.first == 0)
                        return numSamples - remainingSamples;
                    if (dffState.second > 0) //dst
                    {
                        m_decoder.decode(m_arrDstBuf, dffState.first * 8, m_arrDsdBuf);
                        frameSize = m_dff.frameSize;
                        dstBuf = m_arrDsdBuf;
                    }
                    else
                        frameSize = dffState.first;
                }
                else
                {
                    frameSize = ReadDSF();
                    if (frameSize == 0)
                        return numSamples - remainingSamples;
                }

                auto numAvailableSamples = m_converter.convert(dstBuf, frameSize, m_arrPcmBuf) / numChannels;

                int nRemoveSamples = 0;
                if (!m_converter.is_convert_called() && m_numPcmOutDelta > 0)
                {
                    nRemoveSamples = m_numPcmOutDelta;
                    auto numPcmSamples = m_numPcmOutSamples - nRemoveSamples;
                    if (numPcmSamples > 1)
                    {
                        auto* arrPcmBuf = m_arrPcmBuf + numChannels * nRemoveSamples;
                        for (uint32_t ch = 0; ch < numChannels; ch++)
                        {
                            arrPcmBuf[0 * numChannels + ch] = arrPcmBuf[1 * numChannels + ch];
                        }
                    }
                }

                m_numSamples = numAvailableSamples;
                m_remainingSamples = numAvailableSamples - nRemoveSamples;
            }
            else
            {
                auto numSamplesToCopy = Min(remainingSamples, m_remainingSamples);
                if (numChannels == 1)
                {
                    for (uint32_t i = 0; i < numSamplesToCopy; i++)
                        output[i].left = output[i].right = m_arrPcmBuf[i + m_numSamples - m_remainingSamples];
                }
                else if (numChannels == 2)
                {
                    memcpy(output, m_arrPcmBuf + (m_numSamples - m_remainingSamples) * 2, sizeof(StereoSample) * numSamplesToCopy);
                }
                else
                {
                    for (uint32_t i = 0; i < numSamplesToCopy; i++)
                    {
                        output[i].left = m_arrPcmBuf[(i + m_numSamples - m_remainingSamples) * numChannels];
                        output[i].right = m_arrPcmBuf[(i + m_numSamples - m_remainingSamples) * numChannels + 1];
                    }
                }
                output += numSamplesToCopy;
                remainingSamples -= numSamplesToCopy;
                m_remainingSamples -= numSamplesToCopy;
            }
        }

        return numSamples;
    }

    uint32_t ReplayDSD::ReadDSF()
    {
        uint32_t samplesRead = 0;
        uint32_t numSamples = m_dsf.sampleRate / 8 / 75;

        for (uint32_t i = 0; i < numSamples; i++)
        {
            if (m_dsf.blockOffset * m_dsf.numChannels >= m_dsf.blockEnd)
            {
                m_dsf.blockEnd = uint32_t(Min(m_dsf.dataEnd - m_stream->GetPosition(), m_dsf.numChannels * uint64_t(m_dsf.blockSize)));

                if (m_dsf.blockEnd > 0)
                {
                    m_dsf.blockEnd = uint32_t(m_stream->Read(m_dsf.blockData, m_dsf.blockEnd));
                }

                if (m_dsf.blockEnd > 0)
                {
                    m_dsf.blockOffset = 0;
                }
                else
                {
                    break;
                }
            }

            for (uint32_t ch = 0; ch < m_dsf.numChannels; ch++)
            {
                uint8_t b = m_dsf.blockData[ch * m_dsf.blockSize + m_dsf.blockOffset];
                m_arrDstBuf[i * m_dsf.numChannels + ch] = m_dsf.isLSB ? m_dsf.swapBits[b] : b;
            }

            m_dsf.blockOffset++;
            samplesRead++;
        }

        return samplesRead * m_dsf.numChannels;
    }

    std::pair<uint32_t, int32_t> ReplayDSD::ReadDFF()
    {
        if (m_dff.isDstEncoded)
        {
            Chunk chunk;

            while (m_stream->GetPosition() < m_dff.currentOffset + m_dff.currentSize)
            {
                if (chunk.Read(m_stream, "DSTF") && chunk.GetSize() <= uint64_t(m_dstSize))
                {
                    if (m_stream->Read(m_arrDstBuf, chunk.GetSize()) == chunk.GetSize())
                    {
                        m_stream->Seek(chunk.GetSize() & 1, io::Stream::kSeekCurrent);
                        return { uint32_t(chunk.GetSize()), 1 };
                    }
                    break;
                }
                else if (chunk == "DSTC" && chunk.GetSize() == sizeof(uint32_t))
                {
//                     uint32_t crc;
//                     if (m_stream->Read(&crc, sizeof(crc)) != sizeof(crc))
//                         break;
                    m_stream->Seek(sizeof(uint32_t), io::Stream::kSeekCurrent);
                }
                else
                {
                    m_stream->Seek(1 - int64_t(sizeof(chunk)), io::Stream::kSeekCurrent);
                }
            }
        }
        else
        {
            auto position = m_stream->GetPosition();
            auto frameSize = Min(m_dff.frameSize, Max(0, m_dff.currentOffset + m_dff.currentSize - position));

            if (frameSize > 0)
            {
                frameSize = m_stream->Read(m_arrDstBuf, frameSize);
                frameSize -= frameSize % m_dff.numChannels;

                if (frameSize > 0)
                    return { uint32_t(frameSize), -1 };
            }
        }

        return { 0, 0 };
    }

    uint32_t ReplayDSD::Seek(uint32_t timeInMs)
    {
        if (m_arrDsdBuf)
        {
            auto numSamples = (uint64_t(m_dff.sampleRate) * timeInMs) / 8000ull;
            numSamples /= m_dff.frameSize;

            m_stream->Seek(m_dff.dataOffset + numSamples * m_dff.frameSize * m_dff.numChannels, io::Stream::kSeekBegin);
            m_remainingSamples = 0;

            return uint32_t((numSamples * m_dff.frameSize * 8000ull) / uint64_t(m_dff.sampleRate));
        }
        auto numSamples = (uint64_t(m_dsf.sampleRate) * timeInMs) / 8000ull;
        numSamples /= m_dsf.blockSize;

        m_stream->Seek(m_dsf.dataOffset + numSamples * m_dsf.blockSize * m_dsf.numChannels, io::Stream::kSeekBegin);
        m_dsf.blockEnd = 0;
        m_dsf.blockOffset = m_dsf.blockSize;
        m_remainingSamples = 0;

        return uint32_t((numSamples * m_dsf.blockSize * 8000ull) / uint64_t(m_dsf.sampleRate));
    }

    void ReplayDSD::ResetPlayback()
    {
        Seek(0);
    }

    void ReplayDSD::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayDSD::SetSubsong(uint16_t)
    {}

    uint32_t ReplayDSD::GetDurationMs() const
    {
        if (m_arrDsdBuf)
            return uint32_t((m_dff.numFrames * 1000ull) / m_dff.frameRate);
        return uint32_t((m_dsf.numSamples * 1000ull) / m_dsf.sampleRate);
    }

    uint32_t ReplayDSD::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayDSD::GetExtraInfo() const
    {
        std::string info;
        return info;
    }

    std::string ReplayDSD::GetInfo() const
    {
        std::string info;
        char buf[128];
        auto numChannels = m_arrDsdBuf ? m_dff.numChannels : m_dsf.numChannels;
        if (numChannels == 1)
            info = "1 channel\n";
        else
        {
            sprintf(buf, "%u channels\n", numChannels);
            info = buf;
        }
        if (m_arrDsdBuf)
            sprintf(buf, "DSD Interchange File Format %.1f Mhz\n", m_dff.sampleRate / 1000000.0f);
        else
            sprintf(buf, "DSD Stream File %.1f Mhz\n", m_dsf.sampleRate / 1000000.0f);
        info += buf;
        info += "Direct Stream Digital";
        return info;
    }
}
// namespace rePlayer