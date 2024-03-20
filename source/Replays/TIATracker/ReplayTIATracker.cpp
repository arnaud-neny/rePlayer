#include "ReplayTIATracker.h"

#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::TIATracker,
        .name = "TIATracker",
        .extensions = "ttt",
        .about = "TIATracker 1.3\nCopyright (c) 2016-2017 Andre \"Kylearan\" Wichmann",
        .settings = "TIATracker 1.3",
        .init = ReplayTIATracker::Init,
        .load = ReplayTIATracker::Load,
        .displaySettings = ReplayTIATracker::DisplaySettings,
        .editMetadata = ReplayTIATracker::Settings::Edit
    };

    bool ReplayTIATracker::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayTIATrackerStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayTIATrackerSurround");

        return false;
    }

    Replay* ReplayTIATracker::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        if (stream->GetSize() > 1024 * 1024 * 128)
            return nullptr;
        auto data = stream->Read();

        if (QJsonObject::accept(data.begin(), data.end()))
        {
            auto json = QJsonObject::parse(data.begin(), data.end());
            Track::Track track;
            if (track.fromJson(json))
                return new ReplayTIATracker(std::move(track));
        }
        return nullptr;
    }

    bool ReplayTIATracker::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, _countof(surround));
        return changed;
    }

    void ReplayTIATracker::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplayTIATracker::ms_stereoSeparation = 100;
    int32_t ReplayTIATracker::ms_surround = 1;

    ReplayTIATracker::~ReplayTIATracker()
    {
        delete[] m_samples;
    }

    ReplayTIATracker::ReplayTIATracker(Track::Track&& track)
        : Replay(eExtension::_ttt, eReplay::TIATracker)
        , m_track(std::move(track))
        , m_player(&m_track)
        , m_numSamplesPerFrame(m_track.getTvMode() == TiaSound::TvStandard::PAL ? kSampleRate / 50 : kSampleRate / 60)
        , m_samples(new int16_t[m_numSamplesPerFrame * 2])
        , m_surround(kSampleRate)
    {
        m_player.playTrack(0, 0);
    }

    uint32_t ReplayTIATracker::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_isLoopîng)
        {
            m_isLoopîng = false;
            return 0;
        }
        const auto totalSamples = numSamples;
        const auto numSamplesPerFrame = m_numSamplesPerFrame;
        const float stereo = 0.5f - 0.5f * m_stereoSeparation / 100.0f;
        auto numRemainingSamples = m_numRemainingSamples;
        auto surround = m_surround.Begin();
        while (numSamples > 0)
        {
            if (numRemainingSamples > 0)
            {
                auto* source = m_samples + (numSamplesPerFrame - numRemainingSamples) * 2;
                auto numSamplesToCopy = Min(numSamples, numRemainingSamples);
                numRemainingSamples -= numSamplesToCopy;
                numSamples -= numSamplesToCopy;
                while (numSamplesToCopy--)
                {
                    float l = *source++ / 32767.0f;
                    float r = *source++ / 32767.0f;
                    *output++ = surround({ l + (r - l) * stereo, r + (l - r) * stereo });
                }
            }
            else
            {
                m_player.timerFired();
                if (m_player.mode == Emulation::Player::PlayMode::None)
                {
                    // force loop
                    m_player.playTrack(0, 0);
                    m_player.timerFired();
                }

                m_player.sdlSound.processFragment(m_samples, numSamplesPerFrame);
                numRemainingSamples = numSamplesPerFrame;

                // loop detection
                if (m_player.trackCurNoteIndex[0] < m_trackCurNoteIndex[0] || m_player.trackCurNoteIndex[1] < m_trackCurNoteIndex[1])
                {
                    Order order = { m_player.trackCurEntryIndex[0], m_player.trackCurEntryIndex[1] };
                    if (!m_sequences.AddOnce(order).second)
                    {
                        m_sequences.Clear();
                        m_sequences.Add(order);
                        m_isLoopîng = numSamples != totalSamples;
                        m_trackCurNoteIndex[0] = m_player.trackCurNoteIndex[0];
                        m_trackCurNoteIndex[1] = m_player.trackCurNoteIndex[1];
                        break;
                    }
                }
                m_trackCurNoteIndex[0] = m_player.trackCurNoteIndex[0];
                m_trackCurNoteIndex[1] = m_player.trackCurNoteIndex[1];
            }
        }
        m_surround.End(surround);
        m_numRemainingSamples = numRemainingSamples;

        return totalSamples - numSamples;
    }

    void ReplayTIATracker::ResetPlayback()
    {
        m_surround.Reset();
        m_sequences.Clear();
        m_trackCurNoteIndex[0] = m_trackCurNoteIndex[1] = INT_MAX;
        m_isLoopîng = false;
        m_numRemainingSamples = 0;
        m_player.playTrack(0, 0);
    }

    void ReplayTIATracker::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
    }

    void ReplayTIATracker::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
    }

    uint32_t ReplayTIATracker::GetDurationMs() const
    {
        // it can be wrong if the user plays with the loop and make the channels async, but it will be rare, and I don't care
        // the render will take care of it anyway
        uint32_t numRows = m_track.getTrackNumRows();
        uint32_t numOddTicks = ((numRows + 1) / 2) * m_track.oddSpeed;
        uint32_t numEvenTicks = (numRows / 2) * m_track.evenSpeed;
        uint32_t numTicks = numOddTicks + numEvenTicks;
        uint32_t ticksPerSecond = m_track.getTvMode() == TiaSound::TvStandard::PAL ? 50 : 60;
        return (numTicks * 1000) / ticksPerSecond;
    }

    uint32_t ReplayTIATracker::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayTIATracker::GetExtraInfo() const
    {
        std::string metadata;
        metadata = m_track.metaName;
        if (!m_track.metaAuthor.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += m_track.metaAuthor;
        }
        if (!m_track.metaComment.empty())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += m_track.metaComment;
        }
        return metadata;
    }

    std::string ReplayTIATracker::GetInfo() const
    {
        std::string info;

        info += "2 channels\n";
        info += m_track.getTvMode() == TiaSound::TvStandard::PAL ? "PAL" : "NTSC";
        info += "\nTIATracker 1.3";

        return info;
    }
}
// namespace rePlayer