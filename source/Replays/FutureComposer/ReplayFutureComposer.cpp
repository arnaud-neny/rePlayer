// some "minor" changes to make it compatible (tag scan and precise loop detection):
// - Chris/SamplesFile.cpp
// - Chris/TFMXDecoder.cpp
// - Chris/TFMXDecoder.h
// - Jochen/HippelDecoder.h
// - Config.h
// - Decoder.cpp
// - Decoder.h
// - DecoderProxy.cpp
// - DecoderProxy.h
// - LamePaulaMixer.cpp
// - LamePaulaMixer.h
// - MyTypes.h
#include "ReplayFutureComposer.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

#include "libtfmxaudiodecoder/src/Chris/TFMXDecoder.h"
#include "libtfmxaudiodecoder/src/Jochen/HippelDecoder.h"

#define VERSION "1.0.1"

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::FutureComposer,
        .name = "Future Composer",
        .extensions = "fc;fc13;fc14;fc3;fc4;smod;hip;hip7;hipc;mcmd;tfmx;tfx;tfm;mdat",
        .about = "libtfmxaudiodecoder " VERSION "\nCopyright(c) Michael Schwendt",
        .settings = "libtfmxaudiodecoder " VERSION,
        .init = ReplayFutureComposer::Init,
        .load = ReplayFutureComposer::Load,
        .displaySettings = ReplayFutureComposer::DisplaySettings,
        .editMetadata = ReplayFutureComposer::Settings::Edit
    };

    bool ReplayFutureComposer::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayFutureComposerStereoSeparation");
        window.RegisterSerializedData(ms_isNtsc, "ReplayFutureComposerNtsc");
        window.RegisterSerializedData(ms_surround, "ReplayFutureComposerSurround");
        window.RegisterSerializedData(ms_filter, "ReplayFutureComposerFilter");

        return false;
    }

    Replay* ReplayFutureComposer::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        if (stream->GetSize() > 1024 * 1024)
            return nullptr;
        auto decoder = Scope([](){ return new DecoderProxy(); }, [](auto decoder) { delete decoder; });
        struct
        {
            static std::pair<uint8_t*, uint32_t> Loader(io::Stream* stream, const char* filename)
            {
                std::pair<uint8_t*, uint32_t> res = {};
                auto newStream = stream->Open(filename);
                if (newStream.IsValid())
                {
                    auto data = newStream->Read();
                    auto buf = new uint8_t[data.NumItems()];
                    memcpy(buf, data.Items(), data.NumItems());
                    res = { buf, data.NumItems() };
                }
                return res;
            }
        } cb;
        decoder->setPath(stream->GetName().c_str(), LoaderCallback(cb.Loader), stream);
        auto data = stream->Read();
        if (decoder->maybeOurs(const_cast<uint8_t*>(data.Items()), static_cast<unsigned long int>(data.Size())) == 0)
            return nullptr;

        if (decoder->init(const_cast<uint8_t*>(data.Items()), static_cast<unsigned long int>(data.Size()), 0) == 0)
            return nullptr;

        return new ReplayFutureComposer(stream, decoder.Detach());
    }

    bool ReplayFutureComposer::DisplaySettings()
    {
        bool changed = false;
        const char* const clocks[] = { "PAL", "NTSC" };
        changed |= ImGui::Combo("Amiga Clock###FCAmigaClock", &ms_isNtsc, clocks, NumItemsOf(clocks));
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, NumItemsOf(surround));
        const char* const filter[] = { "Off", "On" };
        changed |= ImGui::Combo("Lowpass Filter", &ms_filter, filter, NumItemsOf(filter));
        return changed;
    }

    void ReplayFutureComposer::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find<Settings>(&dummy);

        ComboOverride("AmigaClock", GETSET(entry, overrideNtsc), GETSET(entry, isNtsc),
            ms_isNtsc, "Amiga Clock PAL", "Amiga Clock NTSC");
        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");
        ComboOverride("LowpassFilter", GETSET(entry, overrideFilter), GETSET(entry, filter),
            ms_filter, "No filter", "Lowpass filter");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplayFutureComposer::ms_stereoSeparation = 100;
    int32_t ReplayFutureComposer::ms_isNtsc = 0;
    int32_t ReplayFutureComposer::ms_surround = 1;
    int32_t ReplayFutureComposer::ms_filter = 0;

    ReplayFutureComposer::~ReplayFutureComposer()
    {
        delete m_decoder;
    }

    static eExtension GetExtension(DecoderProxy* decoder)
    {
        auto* tag = decoder->getFormatID();
        if (tag == HippelDecoder::FC14_TAG)
            return eExtension::_fc;
        else if(tag == HippelDecoder::SMOD_TAG)
            return eExtension::_smod;
        else if (tag == HippelDecoder::TFMX_TAG && decoder->getFormatName() == HippelDecoder::TFMX_FORMAT_NAME)
            return eExtension::_hip;
        else if (tag == HippelDecoder::TFMX_7V_ID)
            return eExtension::_hip7;
        else if (tag == HippelDecoder::COSO_TAG)
            return eExtension::_hipc;
        else if (tag == HippelDecoder::MCMD_TAG)
            return eExtension::_mcmd;
        else if (tag == TFMXDecoder::TAG
            || tag == TFMXDecoder::TAG_TFMXSONG
            || tag == TFMXDecoder::TAG_TFMXSONG_LC)
            return eExtension::_mdat;
        else if (tag == TFMXDecoder::TAG_TFMXPAK
            || tag == TFMXDecoder::TAG_TFHD
            || tag == TFMXDecoder::TAG_TFMXMOD)
            return eExtension::_tfm;
        return eExtension::Unknown;
    }

    ReplayFutureComposer::ReplayFutureComposer(io::Stream* stream, DecoderProxy* decoder)
        : Replay(GetExtension(decoder), eReplay::FutureComposer)
        , m_stream(stream)
        , m_decoder(decoder)
        , m_surround(kSampleRate)
    {
        m_mixer.init(kSampleRate, 16, 2, 0, 100);
        decoder->setMixer(&m_mixer);
    }

    uint32_t ReplayFutureComposer::Render(StereoSample* output, uint32_t numSamples)
    {
        auto samples = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;

        auto usedSize = m_decoder->mixerFillBuffer(samples, static_cast<unsigned long>(numSamples * sizeof(int16_t) * 2));
        numSamples = usedSize / sizeof(int16_t) / 2;

        output->Convert(m_surround, samples, numSamples, m_stereoSeparation);

        return numSamples;
    }

    uint32_t ReplayFutureComposer::Seek(uint32_t timeInMs)
    {
        m_decoder->seek(timeInMs);
        m_surround.Reset();
        return timeInMs;
    }


    void ReplayFutureComposer::ResetPlayback()
    {
        m_decoder->reinit(m_subsongIndex);
        m_surround.Reset();
    }

    void ReplayFutureComposer::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_mixer.enableNtsc((settings && settings->overrideNtsc) ? settings->isNtsc : ms_isNtsc);
        m_mixer.setFiltering((settings && settings->overrideFilter) ? settings->filter : ms_filter);
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
    }

    void ReplayFutureComposer::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayFutureComposer::GetDurationMs() const
    {
        return m_decoder->getDuration();
    }

    uint32_t ReplayFutureComposer::GetNumSubsongs() const
    {
        return m_decoder->getSongs();
    }

    std::string ReplayFutureComposer::GetExtraInfo() const
    {
        std::string metadata;
        metadata = "Artist: ";
        if (auto* str = m_decoder->getInfoString("artist"))
            metadata += str;
        metadata += "\nTitle : ";
        if (auto* str = m_decoder->getInfoString("title"))
            metadata += str;
        metadata += "\nGame  : ";
        if (auto* str = m_decoder->getInfoString("game"))
            metadata += str;
        return metadata;
    }

    std::string ReplayFutureComposer::GetInfo() const
    {
        std::string info;

        info += m_decoder->getFormatName() == HippelDecoder::TFMX_7V_FORMAT_NAME || m_decoder->getFormatName() == TFMXDecoder::FORMAT_NAME_7V ? "7 channels\n" : "4 channels\n";
        info += m_decoder->getFormatName();
        info += "\nlibtfmxaudiodecoder " VERSION;

        return info;
    }
}
// namespace rePlayer