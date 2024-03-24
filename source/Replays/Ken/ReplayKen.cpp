#include "ReplayKen.h"

#include <Audio/Audiotypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <IO/StreamMemory.h>
#include <Imgui.h>
#include <ReplayDll.h>

#include "ken/ken.h"

// ---- interfacing with Ken's code
extern "C" {
    //SMSND variables & code
    extern long smsndsamplerate, kind;
    long smsndload(Loader*);
    void smsndseek(long);
    void smsndmusicon();
    void smsndmusicoff();
    long smsndrendersound(void*, long);

    //KSM variables & code
    extern unsigned int speed;
    long ksmsamplerate;
    long ksmload(Loader*);
    void ksmseek(long);
    void ksmmusicon();
    void ksmmusicoff();
    long ksmrendersound(void*, long);

    //KDM variables & code
    extern int kdmsamplerate;
    int kdmload(Loader*);
    void kdmseek(long);
    void kdmmusicon();
    void kdmmusicoff();
    long kdmrendersound(char* dasnd, int numbytes);
    void freekdmeng();
}

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::Ken, .isThreadSafe = false,
        .name = "Ken Silverman",
        .extensions = "kdm;ksm;sm;snd",
        .about = "Ken Silverman\nBased on Ken's SND/SM/KSM/KDM plugin (x86)\nProgrammed by Ken Silverman (04/19/2001)",
        .settings = "Ken Silverman",
        .init = ReplayKen::Init,
        .load = ReplayKen::Load,
        .displaySettings = ReplayKen::DisplaySettings,
        .editMetadata = ReplayKen::Settings::Edit,
        .globals = &ReplayKen::ms_globals
    };

    bool ReplayKen::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        if (&window != nullptr)
        {
            window.RegisterSerializedData(ms_globals.stereoSeparation, "ReplayKenStereoSeparation");
            window.RegisterSerializedData(ms_globals.surround, "ReplayKenSurround");
        }

        return false;
    }

    Replay* ReplayKen::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        struct StreamLoader : public Loader
        {
            StreamLoader()
            {
                Loader::Open = Open;
                Loader::Close = Close;
                Loader::Read = Read;
                Loader::GetName = GetName;
            }
            static Loader* Open(Loader* loader, const char* filename)
            {
                if (reinterpret_cast<StreamLoader*>(loader)->stream2.IsValid())
                {
                    auto* otherLoader = new StreamLoader;
                    otherLoader->stream = reinterpret_cast<StreamLoader*>(loader)->stream2;
                    return otherLoader;
                }
                else
                {
                    auto s = reinterpret_cast<StreamLoader*>(loader)->stream->Open(filename);
                    if (s.IsValid())
                    {
                        auto* otherLoader = new StreamLoader;
                        otherLoader->stream = s;
                        return otherLoader;
                    }
                }
                return nullptr;
            }

            static void Close(Loader* loader)
            {
                delete reinterpret_cast<StreamLoader*>(loader);
            }

            static int Read(Loader* loader, void* buf, size_t size)
            {
                if (reinterpret_cast<StreamLoader*>(loader)->stream->Read(buf, size) == size)
                    return 0;
                return -1;
            }

            static const char* GetName(Loader* loader)
            {
                return reinterpret_cast<StreamLoader*>(loader)->stream->GetName().c_str();
            }

            SmartPtr<io::Stream> stream;
            SmartPtr<io::Stream> stream2;
        };

        StreamLoader loader;
        loader.stream = stream;
        smsndsamplerate = ksmsamplerate = kdmsamplerate = kSampleRate;

        uint64_t header = 0;
        stream->Read(&header, sizeof(header));
        if (memcmp(&header, "KENS-ADB", 8) == 0)
        {
            auto data = stream->Read();
            auto offset = *data.Items<const uint32_t>(8);
            loader.stream = io::StreamMemory::Create(stream->GetName(), data.Items(12), offset - 12, true);
            auto duration = ksmload(&loader);
            if (duration > 0)
            {
                speed = 240;
                return new ReplayKen(eExtension::_ksm, uint32_t(duration));
            }
            return nullptr;
        }
        else if (memcmp(&header, "KENS-KDM", 8) == 0)
        {
            auto data = stream->Read();
            auto offset = *data.Items<const uint32_t>(8);
            loader.stream = io::StreamMemory::Create(stream->GetName(), data.Items(12), offset - 12, true);
            loader.stream2 = io::StreamMemory::Create("waves.kwv", data.Items(offset), data.NumItems() - offset, true);
            auto duration = kdmload(&loader);
            if (duration > 0)
                return new ReplayKen(eExtension::_kdm, uint32_t(duration));
            freekdmeng();
            return nullptr;
        }
        stream->Seek(0, io::Stream::kSeekBegin);
        auto duration = kdmload(&loader);
        if (duration > 0)
            return new ReplayKen(eExtension::_kdm, uint32_t(duration));
        stream->Seek(0, io::Stream::kSeekBegin);
        duration = ksmload(&loader);
        if (duration > 0)
        {
            speed = 240;
            return new ReplayKen(eExtension::_ksm, uint32_t(duration));
        }
        freekdmeng();
        stream->Seek(0, io::Stream::kSeekBegin);
        duration = smsndload(&loader);
        if (duration > 0)
            return new ReplayKen(kind == 1 ? eExtension::_sm : eExtension::_snd, uint32_t(duration));
        return nullptr;
    }

    bool ReplayKen::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_globals.stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_globals.surround, surround, _countof(surround));
        return changed;
    }

    void ReplayKen::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_globals.stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_globals.surround, "Output: Stereo", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    ReplayKen::Globals ReplayKen::ms_globals = {
        .stereoSeparation = 100,
        .surround = 1
    };

    ReplayKen::~ReplayKen()
    {
        freekdmeng();
    }

    ReplayKen::ReplayKen(eExtension ext, uint32_t duration)
        : Replay(ext, eReplay::Ken)
        , m_surround(kSampleRate)
        , m_duration(duration)
    {
        if (ext == eExtension::_kdm)
            kdmmusicon();
        else if (ext == eExtension::_ksm)
            ksmmusicon();
        else
            smsndmusicon();
    }

    uint32_t ReplayKen::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_hasLooped)
        {
            m_hasLooped = false;
            return 0;
        }

        auto ext = m_mediaType.ext;
        uint32_t sampleSize = kSampleSize;
        if (ext == eExtension::_kdm)
            sampleSize = kSampleRate / 120;


        auto numSamplesToRender = numSamples;
        auto remainingSamples = m_remainingSamples;
        while (numSamplesToRender)
        {
            if (remainingSamples == 0)
            {
                uint32_t loopcnt;
                if (ext == eExtension::_kdm)
                    loopcnt = uint32_t(kdmrendersound((char*)m_samples, sizeof(int16_t) * 2 * sampleSize));
                else if (ext == eExtension::_ksm)
                    loopcnt = uint32_t(ksmrendersound((char*)m_samples, sizeof(int16_t) * 2 * kSampleSize));
                else
                    loopcnt = uint32_t(smsndrendersound((char*)m_samples, sizeof(int16_t) * 2 * kSampleSize));
                if (loopcnt != m_numLoops)
                {
                    m_numLoops = loopcnt;
                    m_remainingSamples = sampleSize;
                    m_hasLooped = numSamplesToRender != numSamples;
                    return numSamples - numSamplesToRender;
                }
                m_numLoops = loopcnt;
                remainingSamples = sampleSize;
            }
            auto numSamplesToCopy = Min(numSamplesToRender, remainingSamples);
            output = output->Convert(m_surround, m_samples + 2 * (sampleSize - remainingSamples), numSamplesToCopy, m_stereoSeparation);
            numSamplesToRender -= numSamplesToCopy;
            remainingSamples -= numSamplesToCopy;
        }
        m_remainingSamples = remainingSamples;
        return numSamples;
    }

    uint32_t ReplayKen::Seek(uint32_t timeInMs)
    {
        return timeInMs;
    }

    void ReplayKen::ResetPlayback()
    {
        if (m_mediaType.ext == eExtension::_kdm)
        {
            kdmmusicoff();
            kdmmusicon();
        }
        else if (m_mediaType.ext == eExtension::_ksm)
        {
            ksmmusicoff();
            ksmmusicon();
        }
        else
        {
            smsndmusicoff();
            smsndmusicon();
        }
        m_hasLooped = 0;
        m_remainingSamples = 0;
    }

    void ReplayKen::ApplySettings(const CommandBuffer metadata)
    {
        auto* globals = static_cast<Globals*>(g_replayPlugin.globals);
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : globals->stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : globals->surround);
    }

    void ReplayKen::SetSubsong(uint32_t subsongIndex)
    {
        (void)subsongIndex;
    }

    uint32_t ReplayKen::GetDurationMs() const
    {
        return m_duration;
    }

    uint32_t ReplayKen::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayKen::GetExtraInfo() const
    {
        std::string metadata;
        return metadata;
    }

    std::string ReplayKen::GetInfo() const
    {
        std::string info;
        info = "2 channels\n";
        if (m_mediaType.ext == eExtension::_kdm)
            info += "Ken's Digitial Music";
        else if (m_mediaType.ext == eExtension::_ksm)
            info += "Ken's Adlib Music";
        else if (m_mediaType.ext == eExtension::_sm)
            info += "Ken's CT-640 Music";
        else
            info += "Ken's 4-note Music";
        info += "\nKen Silverman";
        return info;
    }
}
// namespace rePlayer