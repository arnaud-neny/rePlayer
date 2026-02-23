// Some minor changes are done in pxtone:
// pxtone/pxtnPulse_Oggv.cpp
// pxtone/pxtnService.h
// pxtone/pxtnService_moo.h
#include "ReplayPxTone.h"

#include <Audio/Audiotypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::PxTone,
        .name = "PxTone",
        .extensions = "ptcop;pttune",
        .about = "PxTone 22/09/10a\nCopyright (c) Daisuke \"Pixel\" Amaya",
        .settings = "PxTone 22/09/10a",
        .init = ReplayPxTone::Init,
        .load = ReplayPxTone::Load,
        .displaySettings = ReplayPxTone::DisplaySettings,
        .editMetadata = ReplayPxTone::Settings::Edit
    };

    bool ReplayPxTone::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_globals.stereoSeparation, "ReplayPxToneStereoSeparation");
        window.RegisterSerializedData(ms_globals.surround, "ReplayPxToneSurround");

        return false;
    }

    Replay* ReplayPxTone::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        struct
        {
            static bool _pxtn_r(void* user, void* p_dst, int size, int num)
            {
                auto* s = (io::Stream*)user;
                auto i = s->Read(p_dst, size * num);
                if (i < size * num)
                    return false;
                return true;
            }

/*
            static bool _pxtn_w(void* user, const void* p_dst, int size, int num)
            {
                int i = fwrite(p_dst, size, num, (FILE*)user);
                if (i < num) return false;
                return true;
            }
*/

            static bool _pxtn_s(void* user, int mode, int size)
            {
                auto* s = (io::Stream*)user;
                if (s->Seek(size, io::Stream::SeekWhence(mode)) != Status::kOk)
                    return false;
                return true;
            }

            static bool _pxtn_p(void* user, int32_t* p_pos)
            {
                auto* s = (io::Stream*)user;
                *p_pos = int32_t(s->GetPosition());
                return true;
            }
        } cb;

        auto pxtn = core::Scope([&]()
        {
            return new pxtnService(cb._pxtn_r, /*cb._pxtn_w*/nullptr, cb._pxtn_s, cb._pxtn_p);
        }, [](auto data) { if (data) delete data; });

        if (pxtn->init() != pxtnOK)
            return nullptr;
        if (!pxtn->set_destination_quality(2, kSampleRate))
            return nullptr;
        if (pxtn->read(stream) != pxtnOK)
            return nullptr;
        if (pxtn->tones_ready() != pxtnOK)
            return nullptr;

        pxtnVOMITPREPARATION prep = {
            .flags = pxtnVOMITPREPFLAG_loop,
            .master_volume = 1.0f
        };
        if (!pxtn->moo_preparation(&prep))
            return nullptr;

        char pttune[6];
        stream->Seek(0, io::Stream::kSeekBegin);
        stream->Read(pttune, 6);
        return new ReplayPxTone(pxtn.Detach(), memcmp(pttune, "PTTUNE", 6) == 0);
    }

    bool ReplayPxTone::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_globals.stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_globals.surround, surround, NumItemsOf(surround));
        return changed;
    }

    void ReplayPxTone::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_globals.stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_globals.surround, "Output: Stereo", "Output: Surround");

        context.metadata.Update(entry, entry->value == 0);
    }

    ReplayPxTone::Globals ReplayPxTone::ms_globals = {
        .stereoSeparation = 100,
        .surround = 0
    };

    ReplayPxTone::~ReplayPxTone()
    {
        delete m_pxtn;
    }

    ReplayPxTone::ReplayPxTone(pxtnService* pxtn, bool isTune)
        : Replay(isTune ? eExtension::_pttune : eExtension::_ptcop, eReplay::PxTone)
        , m_surround(kSampleRate)
        , m_pxtn(pxtn)
    {}

    uint32_t ReplayPxTone::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_pxtn->_moo_b_looped)
        {
            m_pxtn->_moo_b_looped = false;
            return 0;
        }

        auto buf = reinterpret_cast<int16_t*>(output + numSamples) - numSamples * 2;
        auto numRendered = uint32_t(m_pxtn->Moo(buf, numSamples * sizeof(int16_t) * 2));
        output->Convert(m_surround, buf, numRendered, m_stereoSeparation);
        if (m_pxtn->_moo_b_looped)
            m_pxtn->_moo_b_looped = numRendered != 0;

        return numRendered;
    }

    void ReplayPxTone::ResetPlayback()
    {
        m_surround.Reset();

        pxtnVOMITPREPARATION prep = {
            .flags = pxtnVOMITPREPFLAG_loop,
            .master_volume = 1.0f
        };
        m_pxtn->moo_preparation(&prep);
    }

    void ReplayPxTone::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_globals.stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_globals.surround);
    }

    void ReplayPxTone::SetSubsong(uint32_t subsongIndex)
    {
        (void)subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayPxTone::GetDurationMs() const
    {
        return uint32_t((m_pxtn->moo_get_total_sample() * 1000ull) / kSampleRate);
    }

    uint32_t ReplayPxTone::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayPxTone::GetExtraInfo() const
    {
        std::string metadata;
        if (m_pxtn->text->is_name_buf())
            metadata = m_pxtn->text->get_name_buf(nullptr);
        if (m_pxtn->text->is_comment_buf())
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += m_pxtn->text->get_comment_buf(nullptr);
        }
        return metadata;
    }

    std::string ReplayPxTone::GetInfo() const
    {
        std::string info;

        char buf[8];
        sprintf(buf, "%d", m_pxtn->Woice_Num());
        info = buf;
        info += " channels\nPiston Collage";
        info += m_mediaType.ext == eExtension::_pttune ? " Protected\n" : "\n";
        info += g_replayPlugin.settings;
        return info;
    }
}
// namespace rePlayer