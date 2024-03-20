#include "ReplayOpus.h"

#include <Core/String.h>
#include <Core/Window.inl.h>
#include <ReplayDll.h>

#include "../3rdParty/opus/version.h"
#include "opusfile/src/internal.h"

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::Opus,
        .name = "Opus",
        .extensions = "opus",
        .about = "Opus " PACKAGE_VERSION "\nCopyright (c) 2001-2023 Xiph.Org, Skype Limited, Octasic\n"
                    "Jean - Marc Valin, Timothy B.Terriberry\n"
                    "CSIRO, Gregory Maxwell, Mark Borgerding\n"
                    "Erik de Castro Lopo",
        .load = ReplayOpus::Load
    };

    Replay* ReplayOpus::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        OpusFileCallbacks cb = { op_read_func(OnRead), op_seek_func(OnSeek), op_tell_func(OnTell), nullptr };
        auto* opus = op_open_callbacks(stream, &cb, nullptr, 0, nullptr);
        if (opus)
            return new ReplayOpus(opus, stream);
        return nullptr;
    }

    ReplayOpus::~ReplayOpus()
    {
        op_free(m_opus);
    }

    ReplayOpus::ReplayOpus(OggOpusFile* opus, io::Stream* stream)
        : Replay(eExtension::_opus, eReplay::Opus)
        , m_opus(opus)
        , m_stream(stream)
    {}

    int32_t ReplayOpus::OnRead(io::Stream* stream, uint8_t* ptr, int32_t nbytes)
    {
        return int32_t(stream->Read(ptr, nbytes));
    }

    opus_int64 ReplayOpus::OnSeek(io::Stream* stream, opus_int64 offset, int32_t whence)
    {
        if (stream->Seek(offset, whence == SEEK_SET ? io::Stream::kSeekBegin : whence == SEEK_CUR ? io::Stream::kSeekCurrent : io::Stream::kSeekEnd) == Status::kOk)
            return 0;
        return -1;
    }

    opus_int64 ReplayOpus::OnTell(io::Stream* stream)
    {
        return stream->GetPosition();
    }


    bool ReplayOpus::IsSeekable() const
    {
        return op_seekable(m_opus) != 0;
    }

    bool ReplayOpus::IsStreaming() const
    {
        return op_seekable(m_opus) == 0;
    }

    uint32_t ReplayOpus::Render(StereoSample* output, uint32_t numSamples)
    {
        auto remainingSamples = numSamples;
        while (remainingSamples)
        {
            if (m_remainingSamples == 0)
            {
                int32_t numReadSamples;
                do
                {
                    numReadSamples = op_read_float_stereo(m_opus, reinterpret_cast<float*>(m_samples), 120 * 48 * 2);
                } while (numReadSamples == OP_HOLE);
                if (numReadSamples <= 0)
                    return numSamples - remainingSamples;
                m_numSamples = m_remainingSamples = uint32_t(numReadSamples);

                auto link = op_current_link(m_opus);
                if (link != m_previousLink)
                {
                    auto* head = op_head(m_opus, link);
                    m_numChannels = uint8_t(head->channel_count);
                    m_originalSampleRate = head->input_sample_rate;

                    std::string metadata;
                    std::string artists;
                    std::string title;
                    if (auto* tags = op_tags(m_opus, link))
                    {
                        auto numArtists = opus_tags_query_count(tags, "artist");
                        for (int32_t i = 0; i < numArtists; i++)
                        {
                            if (i != 0)
                                artists += " & ";
                            artists += opus_tags_query(tags, "artist", i);
                        }
                        if (auto* tag = opus_tags_query(tags, "title", 0))
                            title = tag;
                        for (int32_t i = 0; i < tags->comments; i++)
                        {
                            auto comments = tags->user_comments[i];
                            if (opus_tagncompare("METADATA_BLOCK_PICTURE", 22, comments) == 0)
                                continue;
                            if (!metadata.empty())
                                metadata += "\n";
                            metadata += comments;
                        }
                    }
                    m_metadata = std::move(metadata);
                    m_artists = std::move(artists);
                    m_title = std::move(title);

                    m_previousLink = link;
                }

                if (m_opus->samples_tracked >= kSampleRate)
                {
                    auto bitRate = op_bitrate_instant(m_opus);
                    if (bitRate > 0)
                        m_bitRate = uint32_t((bitRate + 999) / 1000);
                }
            }
            else
            {
                auto numSamplesToCopy = Min(remainingSamples, m_remainingSamples);
                memcpy(output, m_samples + m_numSamples - m_remainingSamples, sizeof(StereoSample) * numSamplesToCopy);
                output += numSamplesToCopy;
                remainingSamples -= numSamplesToCopy;
                m_remainingSamples -= numSamplesToCopy;
            }
        }

        return numSamples;
    }

    uint32_t ReplayOpus::Seek(uint32_t timeInMs)
    {
        ogg_int64_t index = timeInMs;
        index *= GetSampleRate();
        index /= 1000;
        op_pcm_seek(m_opus, index);
        return timeInMs;
    }

    void ReplayOpus::ResetPlayback()
    {
        if (op_seekable(m_opus))
            op_raw_seek(m_opus, 0);
        m_stream->Seek(0, io::Stream::kSeekBegin);
        op_free(m_opus);
        OpusFileCallbacks cb = { op_read_func(OnRead), op_seek_func(OnSeek), op_tell_func(OnTell), nullptr };
        m_opus = op_open_callbacks(m_stream.Get(), &cb, nullptr, 0, nullptr);
    }

    void ReplayOpus::ApplySettings(const CommandBuffer /*metadata*/)
    {}

    void ReplayOpus::SetSubsong(uint32_t)
    {}

    uint32_t ReplayOpus::GetDurationMs() const
    {
        auto pcm = op_pcm_total(m_opus, -1);
        if (pcm == OP_EINVAL)
            return 0;
        return uint32_t((pcm * 1000) / kSampleRate);
    }

    uint32_t ReplayOpus::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayOpus::GetStreamingTitle() const
    {
        std::string title;
        if (IsStreaming())
        {
            auto tags = op_tags(m_opus, -1);
            int index = 0;
            for (auto* tag = opus_tags_query(tags, "title", index); tag; tag = opus_tags_query(tags, "title", ++index))
            {
                if (index)
                    title += '-';
                title += tag;
            }
        }
        return title;
    }

    std::string ReplayOpus::GetStreamingArtist() const
    {
        std::string artist;
        if (IsStreaming())
        {
            auto tags = op_tags(m_opus, -1);
            int index = 0;
            for (auto* tag = opus_tags_query(tags, "artist", index); tag; tag = opus_tags_query(tags, "artist", ++index))
            {
                if (index)
                    artist += " & ";
                artist += tag;
            }
        }
        return artist;
    }

    std::string ReplayOpus::GetExtraInfo() const
    {
        std::string info;
        auto tags = op_tags(m_opus, -1);
        for (int32_t i = 0; i < tags->comments; i++)
        {
            if (!info.empty())
                info += "\n";
            info.append(tags->user_comments[i], tags->comment_lengths[i]);
        }
        auto streamInfo = m_stream->GetInfo();
        if (!streamInfo.empty())
        {
            if (!info.empty())
                info += "\n";
            info += streamInfo;
        }
        return info;
    }

    std::string ReplayOpus::GetInfo() const
    {
        std::string info;
        char buf[128];
        sprintf(buf, "%u channel%s%u kb/s - %u hz\nOpus " PACKAGE_VERSION, m_numChannels, m_numChannels > 1 ? "s\n" : "\n", m_bitRate, m_originalSampleRate);
        info = buf;
        return info;
    }
}
// namespace rePlayer