#include "ReplayMDX.h"
#include "mdxmini/src/version.h"

#include <Audio/AudioTypes.inl.h>
#include <Containers/Array.inl.h>
#include <Core/Log.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::MDX,
        .name = "MDX X68000 Chiptunes",
        .extensions = "mdx",
        .about = "mdxmini",
        .settings = "mdxmini " VERSION_ID,
        .init = ReplayMDX::Init,
        .load = ReplayMDX::Load,
        .displaySettings = ReplayMDX::DisplaySettings,
        .editMetadata = ReplayMDX::Settings::Edit
    };

    bool ReplayMDX::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayMDXStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayMDXSurround");
        window.RegisterSerializedData(ms_loops, "ReplayMDXLoop");

        return false;
    }

    size_t ReplayMDX::LoadCB(const char* filename, void** buf, void* user_data)
    {
        auto stream = reinterpret_cast<io::Stream*>(user_data)->Open(filename);
        if (stream.IsValid())
        {
            *buf = malloc(stream->GetSize());
            stream->Read(*buf, stream->GetSize());
            return stream->GetSize();
        }
        Log::Error("[MDX] can't open %s\n", filename);
        return 0;
    }

    Replay* ReplayMDX::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        SmartPtr<io::Stream> mdxStream = stream;
        uint32_t fileIndex = 0;
        do
        {
            auto data = stream->Read();

            t_mdxmini mdx = {};
            auto res = mdx_open(&mdx, data.Items(), data.Size(), kSampleRate, LoadCB, stream);
            if (res == 0)
            {
                return new ReplayMDX(std::move(mdx), fileIndex, stream, mdxStream);
            }
            mdx_close(&mdx);
        } while (mdxStream = mdxStream->Next());

        return nullptr;
    }

    bool ReplayMDX::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, _countof(surround));
        changed |= ImGui::SliderInt("Loops", &ms_loops, 0, 15, "%d", ImGuiSliderFlags_NoInput);
        return changed;
    }

    void ReplayMDX::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");
        SliderOverride("Loops", GETSET(entry, overrideLoops), GETSET(entry, loops),
            ms_loops, 0, 15, "Loop x%d");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplayMDX::ms_stereoSeparation = 100;
    int32_t ReplayMDX::ms_surround = 1;
    int32_t ReplayMDX::ms_loops = 0;

    ReplayMDX::~ReplayMDX()
    {
        mdx_close(&m_mdx);
    }

    ReplayMDX::ReplayMDX(t_mdxmini&& mdx, uint32_t fileIndex, io::Stream* stream, SmartPtr<io::Stream> mdxStream)
        : Replay(eExtension::_mdx, eReplay::MDX)
        , m_stream(stream)
        , m_mdx(std::move(mdx))
        , m_surround(kSampleRate)
    {
        m_subsongs.Add(fileIndex++);

        while (mdxStream = mdxStream->Next())
        {
            auto data = mdxStream->Read();

            t_mdxmini nextMdx = {};
            auto res = mdx_open(&nextMdx, data.Items(), data.Size(), kSampleRate, LoadCB, stream);
            if (res == 0)
                m_subsongs.Add(fileIndex);
            fileIndex++;
            mdx_close(&nextMdx);
        }
    }

    uint32_t ReplayMDX::Render(StereoSample* output, uint32_t numSamples)
    {
        auto buf = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;
        if (!mdx_calc_sample(&m_mdx, buf, numSamples))
            return 0;

        output->Convert(m_surround, buf, numSamples, m_stereoSeparation);

        return numSamples;
    }

    void ReplayMDX::ResetPlayback()
    {
        m_surround.Reset();

        mdx_close(&m_mdx);
        auto mdxStream = m_stream;
        for (uint32_t i = 0; mdxStream; i++)
        {
            if (i == m_subsongs[m_subsongIndex])
            {
                auto data = mdxStream->Read();
                mdx_open(&m_mdx, data.Items(), data.Size(), kSampleRate, LoadCB, m_stream.Get());
                m_filename = mdxStream->GetName();
                break;
            }
            mdxStream = mdxStream->Next();
        }

        mdx_set_max_loop(&m_mdx, 1 + m_loops);
    }

    void ReplayMDX::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
        m_loops = (settings && settings->overrideLoops) ? settings->loops : ms_loops;
        mdx_set_max_loop(&m_mdx, 1 + m_loops);
    }

    void ReplayMDX::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayMDX::GetDurationMs() const
    {
        return uint32_t(mdx_get_length(const_cast<t_mdxmini*>(&m_mdx)) * 1000ull);
    }

    uint32_t ReplayMDX::GetNumSubsongs() const
    {
        return m_subsongs.NumItems();
    }

    std::string ReplayMDX::GetSubsongTitle() const
    {
        char title[MDX_MAX_TITLE_LENGTH];
        mdx_get_title(const_cast<t_mdxmini*>(&m_mdx), title);
        if (title[0] == 0)
            return m_filename;
        return title;
    }

    std::string ReplayMDX::GetExtraInfo() const
    {
        std::string metadata;
        char title[MDX_MAX_TITLE_LENGTH];
        mdx_get_title(const_cast<t_mdxmini*>(&m_mdx), title);
        metadata = title;
        return metadata;
    }

    std::string ReplayMDX::GetInfo() const
    {
        std::string info;
        char buf[64];
        sprintf(buf, "%d channels\nMDX X68000 Chiptune\nmdxmini " VERSION_ID, mdx_get_tracks(const_cast<t_mdxmini*>(&m_mdx)));
        info = buf;
        return info;
    }
}
// namespace rePlayer