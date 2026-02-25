#include "ReplayXMP.h"

#include <Audio/Audiotypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

extern "C" {
#   include "libxmp/src/common.h"
    struct HIO_HANDLE* hio_open_callbacks(void*, struct xmp_callbacks);

    // wrap stdio

    typedef struct {
        char* buf;
        int pos;
        int size;
    } mem_out;

    FILE* replayer_fopen(const char* filename, const char* mode)
    {
        core::UnusedArg(filename, mode);
        assert(0);
        return nullptr;
    }
    int replayer_fread(void* buffer, size_t size, size_t count, mem_out* out)
    {
        core::UnusedArg(buffer, size, count, out);
        assert(0);
        return 0;
    }
    int replayer_fwrite(const void* buf, size_t l, size_t n, mem_out* out)
    {
        int s = out->pos + l * n;
        if (s > out->size) {
            out->size = (s * 3 + 1) / 2;
            char* b = (char*)malloc(out->size);
            if (out->buf)
            {
                memcpy(b, out->buf, out->pos);
                free(out->buf);
            }
            out->buf = b;
        }
        memcpy(out->buf + out->pos, buf, l * n);
        out->pos = s;
        return n;
    }
    int replayer_fseek(FILE* stream, long offset, int origin)
    {
        core::UnusedArg(stream, offset, origin);
        assert(0);
        return 0;
    }
    int replayer_fclose(mem_out* out)
    {
        if (out->buf)
        {
            free(out->buf);
            out->buf = NULL;
            out->pos = 0;
            out->size = 0;
        }
        return 0;
    }
    int replayer_ftell(mem_out* stream)
    {
        core::UnusedArg(stream);
        assert(0);
        return 0;
    }
    int replayer_fputc(uint8_t c, mem_out* stream)
    {
        replayer_fwrite(&c, 1, 1, stream);
        return c;
    }
    int replayer_fprintf(void* const, char const* const, ...) { return 0; }
    int replayer_fgetc(mem_out* stream)
    {
        int8_t c;
        replayer_fread(&c, 1, 1, stream);
        return c;
    }
}

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::XMP,
        .name = "Extended Module Player",
        .extensions = "669;abk;adsc;amf;arch;coco;dbm;digi;dsym;dtm;emod;far;fnk;gdm;imf;ims;it;j2b;kris;liq;mdl;med;mfp;mgt;mod;mtm;okt;psm;ptm;riff;rtm;s3m;sfx;stk;stm;stx;ult;umx;xm;xmf;" // xmp
            "gmc;ksm;ntp", // prowizard
        .about = "libxmp " XMP_VERSION "\nCopyright (c) 1996-2026 Claudio Matsuoka & Hipolito Carraro Jr",
        .settings = "Extended Module Player " XMP_VERSION,
        .init = ReplayXMP::Init,
        .load = ReplayXMP::Load,
        .displaySettings = ReplayXMP::DisplaySettings,
        .editMetadata = ReplayXMP::Settings::Edit
    };

    bool ReplayXMP::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_stereoSeparation, "ReplayXMPStereoSeparation");
        window.RegisterSerializedData(ms_surround, "ReplayXMPSurround");
        window.RegisterSerializedData(ms_a500, "ReplayXMPA500");
        window.RegisterSerializedData(ms_interpolation, "ReplayXMPInterpolation");
        window.RegisterSerializedData(ms_filter, "ReplayXMPFilter");
        window.RegisterSerializedData(ms_vblank, "ReplayXMPVblank");
        window.RegisterSerializedData(ms_fx9bug, "ReplayXMPFX9bug");
        window.RegisterSerializedData(ms_fixLoop, "ReplayXMPFixLoop");
        window.RegisterSerializedData(ms_mode, "ReplayXMPMode");
        window.RegisterSerializedData(ms_patterns, "ReplayXMPPatterns");

        return false;
    }

    static unsigned long xmpReadFunc(void* dest, unsigned long len, unsigned long nmemb, void* priv);
    static int xmpSeekFunc(void* priv, long offset, int whence);
    static long xmpTellFunc(void* priv);
    static int xmpCloseFunc(void* priv);
    static HIO_HANDLE* xmpOpenFunc(const char* filename, void* priv);
    static const char* xmpGetName(void* priv);
    static xmp_callbacks xmpCallbacks = {
        .read_func = xmpReadFunc,
        .seek_func = xmpSeekFunc,
        .tell_func = xmpTellFunc,
        .close_func = xmpCloseFunc,
        .open_func = xmpOpenFunc,
        .get_name = xmpGetName
    };

    static unsigned long xmpReadFunc(void* dest, unsigned long len, unsigned long nmemb, void* priv)
    {
        return (unsigned long)(reinterpret_cast<io::Stream*>(priv)->Read(dest, len * nmemb) / len);
    }
    static int xmpSeekFunc(void* priv, long offset, int whence)
    {
        return (int)reinterpret_cast<io::Stream*>(priv)->Seek(offset, io::Stream::SeekWhence(whence));
    }
    static long xmpTellFunc(void* priv)
    {
        return (long)reinterpret_cast<io::Stream*>(priv)->GetPosition();
    }
    static int xmpCloseFunc(void* priv)
    {
        reinterpret_cast<io::Stream*>(priv)->Release();
        return 0;
    }
    static HIO_HANDLE* xmpOpenFunc(const char* filename, void* priv)
    {
        auto stream = reinterpret_cast<io::Stream*>(priv)->Open(filename);
        if (stream.IsValid())
        {
            stream.Get()->AddRef();
            return hio_open_callbacks(stream.Get(), xmpCallbacks);
        }
        return nullptr;
    }
    static const char* xmpGetName(void* priv)
    {
        return reinterpret_cast<io::Stream*>(priv)->GetName().c_str();
    }

    Replay* ReplayXMP::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        auto data = stream->Read();
        stream->AddRef();

        xmp_context contextPlayback = xmp_create_context();
        if (xmp_load_module_from_callbacks(contextPlayback, stream, xmpCallbacks) >= 0)
        {
            stream->Rewind();
            stream->AddRef();

            xmp_context contextVisuals = xmp_create_context();
            xmp_set_player(contextVisuals, XMP_PLAYER_SMPCTL, XMP_SMPCTL_SKIP);
            xmp_load_module_from_callbacks(contextVisuals, stream, xmpCallbacks);

            return new ReplayXMP(contextPlayback, contextVisuals);
        }
        xmp_free_context(contextPlayback);
        return nullptr;
    }

    bool ReplayXMP::DisplaySettings()
    {
        bool changed = false;
        changed |= ImGui::SliderInt("Stereo", &ms_stereoSeparation, 0, 100, "%d%%", ImGuiSliderFlags_NoInput);
        const char* const surround[] = { "Stereo", "Surround" };
        changed |= ImGui::Combo("Output", &ms_surround, surround, NumItemsOf(surround));
        const char* const interp[] = { "None", "Linear", "Cubic" };
        changed |= ImGui::Combo("Interpolation", &ms_interpolation, interp, NumItemsOf(interp));
        const char* const filter[] = { "None", "Low Pass" };
        changed |= ImGui::Combo("Filter", &ms_filter, filter, NumItemsOf(filter));
        const char* const timing[] = { "Auto/CIA", "VBlank" };
        changed |= ImGui::Combo("Timing", &ms_vblank, timing, NumItemsOf(timing));
        const char* const modes[] = { "Autodetect", "Generic MOD", "Noisetracker", "Protracker", "Generic S3M", "ST3 bug emulation", "ST3+GUS", "generic XM", "FT2", "IT", "IT sample mode" };
        changed |= ImGui::Combo("Mode", &ms_mode, modes, NumItemsOf(modes));
        const char* const onOff[] = { "Off", "On" };
        changed |= ImGui::Combo("A500 Mixer", &ms_a500, onOff, NumItemsOf(onOff));
        changed |= ImGui::Combo("Emulate FX9 Bug", &ms_fx9bug, onOff, NumItemsOf(onOff));
        changed |= ImGui::Combo("Emulate Sample Loop Bug", &ms_fixLoop, onOff, NumItemsOf(onOff));
        changed |= ImGui::Combo("Show Patterns", &ms_patterns, onOff, NumItemsOf(onOff));
        return changed;
    }

    void ReplayXMP::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find<Settings>(&dummy);

        SliderOverride("StereoSeparation", GETSET(entry, overrideStereoSeparation), GETSET(entry, stereoSeparation),
            ms_stereoSeparation, 0, 100, "Stereo Separation %d%%");
        ComboOverride("Surround", GETSET(entry, overrideSurround), GETSET(entry, surround),
            ms_surround, "Output: Stereo", "Output: Surround");
        ComboOverride("Interpolation", GETSET(entry, overrideInterpolation), GETSET(entry, interpolation),
            ms_interpolation, "Interpolation: None", "Interpolation: Linear", "Interpolation: Cubic");
        ComboOverride("Filter", GETSET(entry, overrideFilter), GETSET(entry, filter),
            ms_filter, "Filter: None", "Filter: Low Pass");
        ComboOverride("Timing", GETSET(entry, overrideVblank), GETSET(entry, vblank),
            ms_filter, "Timing: Auto/CIA", "Timing: VBlank");
        ComboOverride("Mode", GETSET(entry, overrideMode), GETSET(entry, mode),
            ms_mode, "Mode: Autodetect", "Mode: Generic MOD", "Mode: Noisetracker", "Mode: Protracker", "Mode: Generic S3M", "Mode: ST3 bug emulation", "Mode: ST3+GUS", "Mode: generic XM", "Mode: FT2", "Mode: IT", "Mode: IT sample mode");
        ComboOverride("A500 Mixer", GETSET(entry, overrideA500), GETSET(entry, a500),
            ms_a500, "A500 Mixer: Off", "A500 Mixer: On");
        ComboOverride("Fx9bug", GETSET(entry, overrideFx9bug), GETSET(entry, fx9bug),
            ms_fx9bug, "Emulate FX9 bug: Off", "Emulate FX9 bug: On");
        ComboOverride("FixLoop", GETSET(entry, overrideFixLoop), GETSET(entry, fixLoop),
            ms_fixLoop, "Emulate Sample Loop Bug: Off", "Emulate Sample Loop Bug: On");

        context.metadata.Update(entry, entry->value == 0);
    }

    int32_t ReplayXMP::ms_stereoSeparation = 100;
    int32_t ReplayXMP::ms_surround = 1;
    int32_t ReplayXMP::ms_a500 = 0;
    int32_t ReplayXMP::ms_interpolation = XMP_INTERP_LINEAR;
    int32_t ReplayXMP::ms_filter = XMP_DSP_LOWPASS;
    int32_t ReplayXMP::ms_vblank = false;
    int32_t ReplayXMP::ms_fx9bug = false;
    int32_t ReplayXMP::ms_fixLoop = false;
    int32_t ReplayXMP::ms_mode = XMP_MODE_AUTO;
    int32_t ReplayXMP::ms_patterns = 1;

    ReplayXMP::~ReplayXMP()
    {
        xmp_free_context(m_contextPlayback);
        xmp_free_context(m_contextVisuals);
    }

    ReplayXMP::ReplayXMP(xmp_context contextPlayback, xmp_context contextVisuals)
        : Replay(eExtension(reinterpret_cast<context_data*>(contextPlayback)->m.mod.ext), eReplay::XMP)
        , m_contextPlayback(contextPlayback)
        , m_contextVisuals(contextVisuals)
        , m_surround(kSampleRate)
    {
        m_states.interpolation = XMP_INTERP_LINEAR;
        m_states.filter = XMP_DSP_LOWPASS;
        xmp_get_module_info(contextPlayback, &m_moduleInfo);
        xmp_start_player(contextPlayback, kSampleRate, 0);
        xmp_start_player(contextVisuals, kSampleRate, 0);
    }

    void ReplayXMP::SetStates(xmp_context context)
    {
        auto states = m_states;

        auto flags = xmp_get_player(context, XMP_PLAYER_CFLAGS);
        auto oldFlags = flags;
        states.vblank ? flags |= XMP_FLAGS_VBLANK : flags &= ~XMP_FLAGS_VBLANK;
        states.fx9bug ? flags |= XMP_FLAGS_FX9BUG : flags &= ~XMP_FLAGS_FX9BUG;
        states.fixLoop ? flags |= XMP_FLAGS_FIXLOOP : flags &= ~XMP_FLAGS_FIXLOOP;
        states.a500 ? flags |= XMP_FLAGS_A500 : flags &= ~XMP_FLAGS_A500;
        if (flags != oldFlags)
            xmp_set_player(context, XMP_PLAYER_CFLAGS, flags);

        auto mode = xmp_get_player(context, XMP_PLAYER_MODE);
        if (mode != int(states.mode))
            xmp_set_player(context, XMP_PLAYER_MODE, states.mode);

        auto interpolation = xmp_get_player(context, XMP_PLAYER_INTERP);
        if (interpolation != int(states.interpolation))
            xmp_set_player(context, XMP_PLAYER_INTERP, states.interpolation);

        auto filter = xmp_get_player(context, XMP_PLAYER_DSP);
        if (filter != int(states.filter))
            xmp_set_player(context, XMP_PLAYER_DSP, states.filter);
    }

    uint32_t ReplayXMP::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_hasLooped)
        {
            m_hasLooped = false;
            return 0;
        }

        auto numSamplesToRender = numSamples;
        auto remainingSamples = m_remainingSamples;
        while (numSamplesToRender)
        {
            if (remainingSamples == 0)
            {
                auto* context = m_contextPlayback;
                SetStates(context);
                if (xmp_play_frame(context) != 0)
                {
                    m_remainingSamples = remainingSamples;
                    return numSamples - numSamplesToRender;
                }
                auto oldLoopCount = m_frameInfo.loop_count;
                xmp_get_frame_info(context, &m_frameInfo);
                remainingSamples = uint32_t(m_frameInfo.buffer_size >> 2);
                if (oldLoopCount != m_frameInfo.loop_count)
                {
                    m_remainingSamples = remainingSamples;
                    m_hasLooped = numSamplesToRender != numSamples;
                    return numSamples - numSamplesToRender;
                }
            }
            auto numSamplesToCopy = Min(numSamplesToRender, remainingSamples);
            output = output->Convert(m_surround, reinterpret_cast<int16_t*>(m_frameInfo.buffer) + (m_frameInfo.buffer_size >> 1) - remainingSamples * 2, numSamplesToCopy, m_stereoSeparation);
            numSamplesToRender -= numSamplesToCopy;
            remainingSamples -= numSamplesToCopy;
        }
        m_remainingSamples = remainingSamples;

        return numSamples;
    }

    uint32_t ReplayXMP::Seek(uint32_t timeInMs)
    {
        m_surround.Reset();
        auto pos = xmp_seek_time(m_contextPlayback, timeInMs);
        xmp_seek_time(m_contextVisuals, timeInMs);
        // xmp_seek_time goes only from pattern to pattern, so now tick until we reach the time
        auto time = 1000ull * reinterpret_cast<context_data*>(m_contextPlayback)->m.xxo_info[pos].time;
        auto timeInMicroSeconds = 1000ull * timeInMs;
        while (time < timeInMicroSeconds)
        {
            xmp_play_frame(m_contextPlayback);
            xmp_play_frame(m_contextVisuals);
            xmp_get_frame_info(m_contextPlayback, &m_frameInfo);
            time += m_frameInfo.frame_time;
        }
        m_visualsSamples = 0;
        m_remainingSamples = 0;
        return uint32_t(timeInMicroSeconds / 1000ull);
    }

    void ReplayXMP::ResetPlayback()
    {
        m_surround.Reset();
        auto* context = m_contextPlayback;
        xmp_start_player(context, kSampleRate, 0);
        SetStates(context);
        xmp_set_position(context, m_moduleInfo.seq_data[m_subsongIndex].entry_point);

        context = m_contextVisuals;
        xmp_start_player(context, kSampleRate, 0);
        xmp_set_position(context, m_moduleInfo.seq_data[m_subsongIndex].entry_point);
        m_remainingSamples = 0;
        m_visualsSamples = 0;
    }

    void ReplayXMP::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        m_stereoSeparation = (settings && settings->overrideStereoSeparation) ? settings->stereoSeparation : ms_stereoSeparation;
        m_surround.Enable((settings && settings->overrideSurround) ? settings->surround : ms_surround);
        auto states = m_states;
        states.a500 = (settings && settings->overrideA500) ? settings->a500 : ms_a500;
        states.interpolation = (settings && settings->overrideInterpolation) ? settings->interpolation : ms_interpolation;
        states.filter = (settings && settings->overrideFilter) ? settings->filter : ms_filter;
        states.vblank = (settings && settings->overrideVblank) ? settings->vblank : ms_vblank;
        states.fx9bug = (settings && settings->overrideFx9bug) ? settings->fx9bug : ms_fx9bug;
        states.fixLoop = (settings && settings->overrideFixLoop) ? settings->fixLoop : ms_fixLoop;
        m_states = states;
    }

    void ReplayXMP::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayXMP::GetDurationMs() const
    {
        return uint32_t(m_moduleInfo.seq_data[m_subsongIndex].duration);
    }

    uint32_t ReplayXMP::GetNumSubsongs() const
    {
        return m_moduleInfo.num_sequences;
    }

    std::string ReplayXMP::GetExtraInfo() const
    {
        std::string metadata;
        if (m_moduleInfo.comment && m_moduleInfo.comment[0])
        {
            metadata = "Comment: ";
            metadata += m_moduleInfo.comment;
        }
        if (m_moduleInfo.mod->name[0])
        {
            if (!metadata.empty())
                metadata += "\n";
            metadata += "Name: ";
            metadata += m_moduleInfo.mod->name;
        }
        char fmtInstrument[] = "Instrument %00d: ";
        if (m_moduleInfo.mod->ins < 10)
        {
            fmtInstrument[12] = 'd';
            fmtInstrument[13] = ':';
            fmtInstrument[14] = ' ';
            fmtInstrument[15] = 0;
        }
        else if(m_moduleInfo.mod->ins < 100)
            fmtInstrument[13] = '2';
        else
            fmtInstrument[13] = '3';
        for (int i = 0, e = m_moduleInfo.mod->ins; i < e; i++)
        {
            if (m_moduleInfo.mod->xxi[i].name[0])
            {
                if (!metadata.empty())
                    metadata += "\n";
                char buf[64];
                sprintf(buf, fmtInstrument, i);
                metadata += buf;
                metadata += m_moduleInfo.mod->xxi[i].name;
            }
        }
        char fmtSample[] = "Sample %00d: ";
        if (m_moduleInfo.mod->smp < 10)
        {
            fmtSample[8] = 'd';
            fmtSample[9] = ':';
            fmtSample[10] = ' ';
            fmtSample[11] = 0;
        }
        else if (m_moduleInfo.mod->smp < 100)
            fmtSample[9] = '2';
        else
            fmtSample[9] = '3';
        for (int i = 0, e = m_moduleInfo.mod->smp; i < e; i++)
        {
            if (m_moduleInfo.mod->xxs[i].name[0])
            {
                if (!metadata.empty())
                    metadata += "\n";
                char buf[64];
                sprintf(buf, fmtSample, i);
                metadata += buf;
                metadata += m_moduleInfo.mod->xxs[i].name;
            }
        }
        return metadata;
    }

    std::string ReplayXMP::GetInfo() const
    {
        std::string info;
        char buf[128];
        sprintf(buf, "%d", m_moduleInfo.mod->chn);
        info = buf;
        info += " channels\n";
        info += m_moduleInfo.mod->type;
        info += "\nlibxmp " XMP_VERSION;
        return info;
    }

    Replay::Patterns ReplayXMP::UpdatePatterns(uint32_t numSamples, uint32_t numLines, uint32_t charWidth, uint32_t spaceWidth, Patterns::Flags flags)
    {
        auto context = m_contextVisuals;
        if (numSamples > 0)
        {
            auto availableSamples = Min(numSamples, m_visualsSamples);
            m_visualsSamples -= availableSamples;
            numSamples -= availableSamples;
            while (numSamples)
            {
                if (xmp_play_frame(context) != 0)
                    break;
                xmp_frame_info frameInfo;
                xmp_get_frame_info(context, &frameInfo);
                m_visualsSamples = frameInfo.buffer_size >> 2;
                availableSamples = Min(numSamples, m_visualsSamples);
                m_visualsSamples -= availableSamples;
                numSamples -= availableSamples;
            }
        }

        if (!ms_patterns && !(flags & Patterns::kEnablePatterns)) // kEnablePatterns has priority over internal flag
            return {};

        xmp_frame_info frameInfo;
        xmp_get_frame_info(context, &frameInfo);

        auto numChannels = m_moduleInfo.mod->chn;

        Patterns patterns;

        patterns.lines.Reserve(65536);
        patterns.sizes.Resize(numLines);
        patterns.currentLine = frameInfo.row;
        if (flags & (Patterns::kEnableInstruments | Patterns::kEnableVolume | Patterns::kEnableEffects))
            patterns.width = (numChannels * 4 - 1) * charWidth;
        else
            patterns.width = (numChannels * 3) * charWidth + (numChannels - 1) * spaceWidth;
        uint16_t size = uint16_t(numChannels * 4 - 1);
        if (flags & Patterns::kEnableRowNumbers)
        {
            patterns.width += 4 * charWidth;
            size += uint16_t(4);
        }
        if (flags & Patterns::kEnableInstruments)
        {
            patterns.width += numChannels * 2 * charWidth + spaceWidth * numChannels;
            size += uint16_t(numChannels * 3);
        }
        if (flags & Patterns::kEnableVolume)
        {
            patterns.width += numChannels * 2 * charWidth + spaceWidth * numChannels;
            size += uint16_t(numChannels * 3);
        }
        if (flags & Patterns::kEnableEffects)
        {
            patterns.width += (numChannels * 4 * charWidth + spaceWidth * numChannels) * 2;
            size += uint16_t(numChannels * 5 * 2);
        }

        auto numRows = frameInfo.num_rows;
        auto currentRow = patterns.currentLine - int32_t(numLines) / 2;

        for (uint32_t line = 0; line < numLines; currentRow++, line++)
        {
            if (currentRow >= 0 && currentRow < numRows)
            {
                char colorBit = -128; // 0x80
                if (flags & Patterns::kEnableRowNumbers)
                {
                    if (flags & Patterns::kEnableColors)
                        patterns.lines.Add(colorBit | Patterns::kColorDefault);
                    patterns.lines.Add('0' + ((currentRow / 100) % 10));
                    patterns.lines.Add('0' + ((currentRow / 10) % 10));
                    patterns.lines.Add('0' + (currentRow % 10));
                    patterns.lines.Add('\x7f');
                }

                for (int32_t channel = 0;;)
                {
                    int track = m_moduleInfo.mod->xxp[frameInfo.pattern]->index[channel];
                    struct xmp_event* event = &m_moduleInfo.mod->xxt[track]->event[currentRow];

                    static const char* notenames[] = { "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };
                    static const char toHex[] = "0123456789ABCDEF";
                    if (flags & Patterns::kEnableColors)
                    {
                        patterns.lines.Add(colorBit | ((event->note > 0 && event->note < 0x80) ? Patterns::kColorText : Patterns::kColorEmpty));
                    }
                    int note = event->note;
                    if (note == 0)
                        patterns.lines.Add("...", 3);
                    else if (note < 0x80)
                    {
                        note--;
                        patterns.lines.Add(notenames[note % 12], 2);
                        patterns.lines.Add('0' + char(note / 12));
                    }
                    else
                        patterns.lines.Add("===", 3);
                    if (flags & Patterns::kEnableInstruments)
                    {
                        patterns.lines.Add(' ');
                        if (event->ins)
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorInstrument);
                            patterns.lines.Add(toHex[event->ins >> 4]);
                            patterns.lines.Add(toHex[event->ins & 0xf]);
                        }
                        else
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                            patterns.lines.Add("..", 2);
                        }
                    }
                    if (flags & Patterns::kEnableVolume)
                    {
                        patterns.lines.Add(' ');
                        auto vol = event->vol;
                        if (vol)
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorVolume);
                            vol--;
                            patterns.lines.Add(toHex[vol >> 4]);
                            patterns.lines.Add(toHex[vol & 0xf]);
                        }
                        else
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                            patterns.lines.Add("..", 2);
                        }
                    }
                    if (flags & Patterns::kEnableEffects)
                    {
                        char buf[6] = " ....";
                        if (event->fxp || event->fxp)
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorText);
                            buf[1] = toHex[event->fxt >> 4];
                            buf[2] = toHex[event->fxt & 0xf];
                            buf[3] = toHex[event->fxp >> 4];
                            buf[4] = toHex[event->fxp & 0xf];
                            patterns.lines.Add(buf, 5);
                        }
                        else
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                            patterns.lines.Add(" ....", 5);
                        }
                        if (event->f2p || event->f2p)
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorText);
                            buf[1] = toHex[event->f2t >> 4];
                            buf[2] = toHex[event->f2t & 0xf];
                            buf[3] = toHex[event->f2p >> 4];
                            buf[4] = toHex[event->f2p & 0xf];
                            patterns.lines.Add(buf, 5);
                        }
                        else
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                            patterns.lines.Add(" ....", 5);
                        }
                    }
                    if (++channel == numChannels)
                        break;
                    if (flags & (Patterns::kEnableInstruments | Patterns::kEnableVolume | Patterns::kEnableEffects))
                        patterns.lines.Add('\x7f');
                    else
                        patterns.lines.Add(' ');
                }

                patterns.sizes[line] = size;
            }
            else
                patterns.sizes[line] = 0;
            patterns.lines.Add('\0');
        }
        return patterns;
    }
}
// namespace rePlayer