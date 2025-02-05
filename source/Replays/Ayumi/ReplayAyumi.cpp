#include "ReplayAyumi.h"

#include <Audio/Audiotypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <IO/StreamMemory.h>
#include <Imgui.h>
#include <ReplayDll.h>

extern "C" {
    int fxm_init(const uint8_t* fxm, uint32_t len);
    void fxm_loop();
    int* fxm_get_registers();
    void fxm_get_songinfo(const char** songName, const char** songAuthor);
    extern int isAmad;
}

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::Ayumi, .isThreadSafe = false,
        .name = "Ayumi",
        .extensions = "amad;fxm",
        .about = "Ayumi/webAyumi\nCopyright (c) Peter Sovietov\nCopyright (c) 2019 Juergen Wothke",
        .settings = "Ayumi",
        .init = ReplayAyumi::Init,
        .load = ReplayAyumi::Load,
        .displaySettings = ReplayAyumi::DisplaySettings,
        .editMetadata = ReplayAyumi::Settings::Edit,
        .globals = &ReplayAyumi::ms_globals
    };

    bool ReplayAyumi::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        if (&window != nullptr)
        {
            window.RegisterSerializedData(ms_globals.stereoSeparation, "ReplayAyumiStereoSeparation");
            window.RegisterSerializedData(ms_globals.surround, "ReplayAyumiSurround");
        }

        return false;
    }

    Replay* ReplayAyumi::Load(io::Stream* stream, CommandBuffer metadata)
    {
        auto data = stream->Read();

        text_data textData = {};
        textData.sample_rate = kSampleRate;
        textData.is_ym = 0;
        textData.pan[0] = 0.8;
        textData.pan[1] = 0.2;
        textData.pan[2] = 0.5;
        textData.volume = 0.7;
        textData.eqp_stereo_on = 1;
        textData.dc_filter_on = 1;
        textData.clock_rate = 2000000;
        textData.frame_rate = 50.0;

        if (fxm_init(data.Items(), data.NumItems()))
        {
            textData.clock_rate = 1773400;
            //textData.frame_count = 30000;
            //textData.frame_rate = 50.0;
        }
        else
            return nullptr;

        ayumi ay;
        if (!ayumi_configure(&ay, textData.is_ym, textData.clock_rate, textData.sample_rate))
            return nullptr;
        if (textData.pan[0] >= 0)
            ayumi_set_pan(&ay, 0, textData.pan[0], textData.eqp_stereo_on);
        if (textData.pan[1] >= 0)
            ayumi_set_pan(&ay, 1, textData.pan[1], textData.eqp_stereo_on);
        if (textData.pan[2] >= 0)
            ayumi_set_pan(&ay, 2, textData.pan[2], textData.eqp_stereo_on);
        return new ReplayAyumi(stream, metadata, textData, ay);
    }

    bool ReplayAyumi::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_globals.stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_globals.surround, surround, NumItemsOf(surround));
        return changed;
    }

    void ReplayAyumi::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_globals.stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_globals.surround, "Output: Stereo", "Output: Surround");
        Durations(context, &entry->duration, 1, "Duration");

        context.metadata.Update(entry, entry->value == 0 && entry->duration == 0);
    }

    ReplayAyumi::Globals ReplayAyumi::ms_globals = {
        .stereoSeparation = 100,
        .surround = 1
    };

    ReplayAyumi::~ReplayAyumi()
    {
    }

    ReplayAyumi::ReplayAyumi(SmartPtr<io::Stream> stream, CommandBuffer metadata, const text_data& textData, const ayumi& ay)
        : Replay(isAmad ? eExtension::_amad : eExtension::_fxm, eReplay::Ayumi)
        , m_surround(uint32_t(textData.sample_rate))
        , m_stream(stream)
        , m_textData(textData)
        , m_ay(ay)
        , m_isrStep(textData.frame_rate / textData.sample_rate)
    {
        if (auto* settings = metadata.Find<Settings>())
            m_currentDuration = (uint64_t(settings->duration) * GetSampleRate()) / 1000;
    }

    uint32_t ReplayAyumi::Render(StereoSample* output, uint32_t numSamples)
    {
        auto currentPosition = m_currentPosition;
        auto currentDuration = m_currentDuration;
        if (currentDuration != 0 && (currentPosition + numSamples) >= currentDuration)
        {
            numSamples = currentPosition < currentDuration ? uint32_t(currentDuration - currentPosition) : 0;
            if (numSamples == 0)
            {
                m_currentPosition = 0;
                return 0;
            }
        }
        m_currentPosition = currentPosition + numSamples;
        m_globalPosition += numSamples;

        auto surround = m_surround.Begin();
        float stereo = 0.5f - 0.5f * m_stereoSeparation / 100.0f;

        auto remainingSamples = numSamples;
        while (remainingSamples--)
        {
            m_isrCounter += m_isrStep;
            if (m_isrCounter >= 1)
            {
                m_isrCounter -= 1;

                int* regs;
                if (1)
                {
                    fxm_loop();
                    regs = fxm_get_registers();
                }
                else
                {
//                     regs = &textData.frame_data[m_frame * 16]; // original Ayumi
                }

                auto updateAyumiState = [](struct ayumi* ay, int* r)
                {
                    ayumi_set_tone(ay, 0, (r[1] << 8) | r[0]);
                    ayumi_set_tone(ay, 1, (r[3] << 8) | r[2]);
                    ayumi_set_tone(ay, 2, (r[5] << 8) | r[4]);
                    ayumi_set_noise(ay, r[6]);
                    ayumi_set_mixer(ay, 0, r[7] & 1, (r[7] >> 3) & 1, r[8] >> 4);
                    ayumi_set_mixer(ay, 1, (r[7] >> 1) & 1, (r[7] >> 4) & 1, r[9] >> 4);
                    ayumi_set_mixer(ay, 2, (r[7] >> 2) & 1, (r[7] >> 5) & 1, r[10] >> 4);
                    ayumi_set_volume(ay, 0, r[8] & 0xf);
                    ayumi_set_volume(ay, 1, r[9] & 0xf);
                    ayumi_set_volume(ay, 2, r[10] & 0xf);
                    ayumi_set_envelope(ay, (r[12] << 8) | r[11]);
                    if (r[13] != 255)
                    {
                        ayumi_set_envelope_shape(ay, r[13]);
                    }
                };
                updateAyumiState(&m_ay, regs);
//                 m_frame += 1;
            }
            ayumi_process(&m_ay);
            if (m_textData.dc_filter_on)
                ayumi_remove_dc(&m_ay);

            float left = float(m_ay.left * m_textData.volume);
            float right = float(m_ay.right * m_textData.volume);
            StereoSample s;
            s.left = left + (right - left) * stereo;
            s.right = right + (left - right) * stereo;
            *output++ = surround(s);
        }
        m_surround.End(surround);

        return numSamples;
    }

    uint32_t ReplayAyumi::Seek(uint32_t timeInMs)
    {
        auto seekPosition = (uint64_t(timeInMs) * kSampleRate) / 1000;
        auto seekSamples = seekPosition;
        if (seekPosition < m_globalPosition)
            ResetPlayback();
        else
            seekSamples -= m_globalPosition;
        m_globalPosition = seekPosition;

        while (seekSamples--)
        {
            m_isrCounter += m_isrStep;
            if (m_isrCounter >= 1)
            {
                m_isrCounter -= 1;

                int* regs;
                if (1)
                {
                    fxm_loop();
                    regs = fxm_get_registers();
                }
                else
                {
//                     regs = &textData.frame_data[m_frame * 16]; // original Ayumi
                }

                auto updateAyumiState = [](struct ayumi* ay, int* r)
                {
                    ayumi_set_tone(ay, 0, (r[1] << 8) | r[0]);
                    ayumi_set_tone(ay, 1, (r[3] << 8) | r[2]);
                    ayumi_set_tone(ay, 2, (r[5] << 8) | r[4]);
                    ayumi_set_noise(ay, r[6]);
                    ayumi_set_mixer(ay, 0, r[7] & 1, (r[7] >> 3) & 1, r[8] >> 4);
                    ayumi_set_mixer(ay, 1, (r[7] >> 1) & 1, (r[7] >> 4) & 1, r[9] >> 4);
                    ayumi_set_mixer(ay, 2, (r[7] >> 2) & 1, (r[7] >> 5) & 1, r[10] >> 4);
                    ayumi_set_volume(ay, 0, r[8] & 0xf);
                    ayumi_set_volume(ay, 1, r[9] & 0xf);
                    ayumi_set_volume(ay, 2, r[10] & 0xf);
                    ayumi_set_envelope(ay, (r[12] << 8) | r[11]);
                    if (r[13] != 255)
                    {
                        ayumi_set_envelope_shape(ay, r[13]);
                    }
                };
                updateAyumiState(&m_ay, regs);
//                 m_frame += 1;
            }
            ayumi_process(&m_ay);
            if (m_textData.dc_filter_on)
                ayumi_remove_dc(&m_ay);
        }

        return timeInMs;
    }

    void ReplayAyumi::ResetPlayback()
    {
        auto data = m_stream->Read();
        fxm_init(data.Items(), data.NumItems());
        ayumi_configure(&m_ay, m_textData.is_ym, m_textData.clock_rate, m_textData.sample_rate);
        if (m_textData.pan[0] >= 0)
            ayumi_set_pan(&m_ay, 0, m_textData.pan[0], m_textData.eqp_stereo_on);
        if (m_textData.pan[1] >= 0)
            ayumi_set_pan(&m_ay, 1, m_textData.pan[1], m_textData.eqp_stereo_on);
        if (m_textData.pan[2] >= 0)
            ayumi_set_pan(&m_ay, 2, m_textData.pan[2], m_textData.eqp_stereo_on);
        m_currentPosition = 0;
        m_globalPosition = 0;
        m_isrCounter = 1.0;
    }

    void ReplayAyumi::ApplySettings(const CommandBuffer metadata)
    {
        auto* globals = static_cast<Globals*>(g_replayPlugin.globals);
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : globals->stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : globals->surround);
        if (settings)
            m_currentDuration = (uint64_t(settings->duration) * GetSampleRate()) / 1000;
    }

    void ReplayAyumi::SetSubsong(uint32_t subsongIndex)
    {
        (void)subsongIndex;
    }

    uint32_t ReplayAyumi::GetDurationMs() const
    {
        return uint32_t(m_currentDuration * 1000ull / GetSampleRate());
    }

    uint32_t ReplayAyumi::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayAyumi::GetExtraInfo() const
    {
        std::string metadata;

        const char* songName;
        const char* songAuthor;
        fxm_get_songinfo(&songName, &songAuthor);
        if (songName && songName[0])
        {
            metadata = "Title : ";
            metadata += songName;
        }
        if (songAuthor && songAuthor[0])
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Author: ";
            metadata += songAuthor;
        }

        return metadata;
    }

    std::string ReplayAyumi::GetInfo() const
    {
        std::string info;
        info = "3 channels\n";
        if (m_mediaType.ext == eExtension::_fxm)
            info += "Fuxoft AY Language";
        else if (m_mediaType.ext == eExtension::_amad)
            info += "AY Amadeus";
        info += "\nAyumi";
        return info;
    }
}
// namespace rePlayer