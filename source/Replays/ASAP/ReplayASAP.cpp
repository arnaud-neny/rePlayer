#include "ReplayASAP.h"
#include "ASAP/asap.h"

#include <Audio/Audiotypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

extern "C"
{
    const char* ASAPInfo_GetModuleExt(const ASAPInfo* self);
}

typedef struct {
    int (*load)(const ASAPFileLoader* self, const char* filename, uint8_t* buffer, int length);
} ASAPFileLoaderVtbl;
struct ASAPFileLoader {
    const ASAPFileLoaderVtbl* vtbl;
    core::io::Stream* stream;

    static int load(const ASAPFileLoader* self, const char* filename, uint8_t* buffer, int length)
    {
        auto s = self->stream->Open(filename);
        if (s)
        {
            if (s->GetSize() == 0 || s->GetSize() > length)
                return -1;
            return int(s->Read(buffer, length));
        }
        return -1;
    }
};

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::ASAP,
        .name = "Another Slight Atari Player",
        .extensions = "sap;cmc;cm3;cmr;cms;dmc;dlt;fc;mpt;mpd;rmt;cm3;tmc;tm8;tm2",
        .about = "Another Slight Atari Player " ASAPInfo_VERSION "\n" ASAPInfo_CREDITS,
        .settings = "Another Slight Atari Player " ASAPInfo_VERSION,
        .init = ReplayASAP::Init,
        .load = ReplayASAP::Load,
        .displaySettings = ReplayASAP::DisplaySettings,
        .editMetadata = ReplayASAP::Settings::Edit
    };

    bool ReplayASAP::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayASAPStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayASAPSurround");

        return false;
    }

    Replay* ReplayASAP::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        if (stream->GetSize() > 1024 * 1024)
            return nullptr;
        auto song = ASAP_New();
        auto data = stream->Read();

        const ASAPFileLoaderVtbl ASAPFileLoader_vtbl = { ASAPFileLoader::load };
        ASAPFileLoader loader = {
            .vtbl = &ASAPFileLoader_vtbl,
            .stream = stream
        };

        if (!ASAP_LoadFiles(song, stream->GetName().c_str(), &loader))
        {
            ASAP_Delete(song);
            return nullptr;
        }  

        auto songInfo = ASAP_GetInfo(song);
        if (ASAPInfo_GetChannels(songInfo) == 1)
        {
            auto dupSong = ASAP_New();
            ASAP_LoadFiles(dupSong, stream->GetName().c_str(), &loader);
            return new ReplayASAP(song, dupSong);
        }
        return new ReplayASAP(song, nullptr);
    }

    bool ReplayASAP::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, NumItemsOf(surround));
        return changed;
    }

    void ReplayASAP::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplayASAP::ms_stereoSeparation = 100;
    int32_t ReplayASAP::ms_surround = 1;

    ReplayASAP::~ReplayASAP()
    {
        ASAP_Delete(m_song[0]);
        ASAP_Delete(m_song[1]);
    }

    static MediaType BuildMediaType(const char* ext)
    {
        if (ext)
            return MediaType(ext, eReplay::ASAP);
        return MediaType(eExtension::_sap, eReplay::ASAP);
    }

    ReplayASAP::ReplayASAP(ASAP* song, ASAP* dupSong)
        : Replay(BuildMediaType(ASAPInfo_GetModuleExt(ASAP_GetInfo(song))))
        , m_song{ song, dupSong }
        , m_surround(ASAP_SAMPLE_RATE)
    {}

    uint32_t ReplayASAP::GetSampleRate() const
    {
        return ASAP_SAMPLE_RATE;
    }

    uint32_t ReplayASAP::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_hasEnded)
        {
            m_hasEnded = false;
            return 0;
        }

        auto position = m_position + numSamples;
        if (position > m_duration)
        {
            numSamples = uint32_t(m_duration - m_position);
            m_position = 0;
            if (numSamples == 0)
                return 0;
            m_hasEnded = true;
        }
        else
            m_position = position;

        if (m_song[1] != nullptr)
        {
            // asap mute is not deterministic (song0 != song1 when unmuting)... 
            if (m_surround.IsEnabled())
            {
                auto* buf = pCast<int16_t>(output);

                // generate dual asap
                auto count = ASAP_Generate(m_song[0], pCast<uint8_t>(buf), numSamples * sizeof(int16_t), ASAPSampleFormat_S16_L_E);
                ASAP_Generate(m_song[1], pCast<uint8_t>(buf + numSamples), numSamples * sizeof(int16_t), ASAPSampleFormat_S16_L_E);
                // interleaved
                for (uint32_t i = 0; i < count / sizeof(int16_t); ++i)
                {
                    buf[(i + numSamples) * 2] = buf[i];
                    buf[(i + numSamples) * 2 + 1] = buf[numSamples + i];
                }
                buf += numSamples * 2;
                numSamples = count / 2;

                output->Convert(m_surround, buf, numSamples, m_stereoSeparation);
            }
            else
            {
                auto* buf = pCast<int16_t>(output + numSamples) - numSamples;

                // generate dual asap
                auto count = ASAP_Generate(m_song[0], pCast<uint8_t>(buf), numSamples * sizeof(int16_t), ASAPSampleFormat_S16_L_E);
                ASAP_Generate(m_song[1], pCast<uint8_t>(buf), numSamples * sizeof(int16_t), ASAPSampleFormat_S16_L_E);
                numSamples = count / 2;

                output->ConvertMono(buf, numSamples);
            }
        }
        else
        {
            auto* buf = pCast<int16_t>(output + numSamples) - numSamples * 2;
            auto count = ASAP_Generate(m_song[0], pCast<uint8_t>(buf), numSamples * sizeof(int16_t) * 2, ASAPSampleFormat_S16_L_E);
            numSamples = count / 4;

            output->Convert(m_surround, buf, numSamples, m_stereoSeparation);
        }

        return numSamples;
    }

    uint32_t ReplayASAP::Seek(uint32_t timeInMs)
    {
        ASAP_Seek(m_song[0], timeInMs);
        if (m_song[1])
            ASAP_Seek(m_song[1], timeInMs);
        m_hasEnded = false;
        m_position = uint64_t(timeInMs) * ASAP_SAMPLE_RATE / 1000;
        m_surround.Reset();
        return timeInMs;
    }

    void ReplayASAP::ResetPlayback()
    {
        ASAP_PlaySong(m_song[0], m_subsongIndex, -1);
        if (m_song[1])
            ASAP_PlaySong(m_song[1], m_subsongIndex, -1);
        m_position = 0;
        m_hasEnded = false;
        m_surround.Reset();
    }

    void ReplayASAP::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
        if (m_song[1])
        {
            ASAP_MutePokeyChannels(m_song[0], m_surround.IsEnabled() ? 0b10101010 : 0);
            ASAP_MutePokeyChannels(m_song[1], m_surround.IsEnabled() ? 0b01010101 : 0);
        }
    }

    void ReplayASAP::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
        auto duration = ASAPInfo_GetDuration(ASAP_GetInfo(m_song[0]), m_subsongIndex);
        if (duration < 0)
            duration = 60 * 4 * 1000;
        m_duration = uint64_t(duration) * ASAP_SAMPLE_RATE / 1000;
    }

    uint32_t ReplayASAP::GetDurationMs() const
    {
        auto duration = ASAPInfo_GetDuration(ASAP_GetInfo(m_song[0]), m_subsongIndex);
        if (duration < 0)
            return 60 * 4 * 1000;
        return uint32_t(duration);
    }

    uint32_t ReplayASAP::GetNumSubsongs() const
    {
        return uint32_t(ASAPInfo_GetSongs(ASAP_GetInfo(m_song[0])));
    }

    std::string ReplayASAP::GetExtraInfo() const
    {
        std::string metadata;
        auto songInfo = ASAP_GetInfo(m_song[0]);
        metadata  = "Title  : ";
        metadata += ASAPInfo_GetTitle(songInfo);
        metadata += "\nArtist : ";
        metadata += ASAPInfo_GetAuthor(songInfo);
        metadata += "\nDate   : ";
        metadata += ASAPInfo_GetDate(songInfo);
        char str[64];
        sprintf(str, ASAPInfo_IsNtsc(songInfo) ? "\nRate   : %dHz NTSC" : "\nRate   : %dHz PAL", ASAPInfo_GetPlayerRateHz(songInfo));
        metadata += str;
        return metadata;
    }

    std::string ReplayASAP::GetInfo() const
    {
        std::string info;
        auto songInfo = ASAP_GetInfo(m_song[0]);
        info = ASAPInfo_GetChannels(songInfo) == 1 ? "1 channel\n" : "2 channels\n";
        auto* ext = ASAPInfo_GetOriginalModuleExt(songInfo);
        info += ASAPInfo_GetExtDescription(ext ? ext : "sap");
        info += "\nASAP " ASAPInfo_VERSION;
        return info;
    }
}
// namespace rePlayer