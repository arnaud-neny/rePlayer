#include "ReplayV2.h"

#include <Core/String.h>
#include <Core/Window.inl.h>
#include <Imgui.h>
#include <ReplayDll.h>

#include <bit>
#include <pmmintrin.h>
#include <xmmintrin.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::V2,
        .name = "V2 Redux",
        .extensions = "v2m",
        .about = "V2 Redux\nCopyright (c) 2026 Martin Schrodt\n"
            "Farbrausch ViruZ II\nCopyright (c) 2000-2008 Tammo \"kb\" Hinrichs",
        .settings = "V2 Redux",
        .init = ReplayV2::Init,
        .load = ReplayV2::Load,
        .displaySettings = ReplayV2::DisplaySettings,
        .editMetadata = ReplayV2::Settings::Edit
    };

    bool ReplayV2::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        window.RegisterSerializedData(ms_patterns, "ReplayArtOfNoisePatterns");

        return false;
    }

    Replay* ReplayV2::Load(io::Stream* stream, CommandBuffer metadata)
    {
        if (stream->GetSize() < 4 || stream->GetSize() > 1024 * 1024 * 256)
            return nullptr;
        auto data = stream->Read();
        if (data[2] != 0 || data[3] != 0)
            return nullptr;

        auto era = v2redux::Era::Auto();
        uint64_t seed = 0;
        if (auto settings = metadata.Find<Settings>())
        {
            if (settings->version)
                era.base = settings->version - 1;
            if (settings->overriddenDeltas)
            {
                era.overridden = settings->overriddenDeltas;
                era.forcedNew = settings->deltas;
            }
            seed = settings->seed[0] | (uint64_t(settings->seed[1]) << 32);
        }

        auto oldFP = _mm_getcsr();
        _mm_setcsr(oldFP & ~(_MM_FLUSH_ZERO_MASK | _MM_DENORMALS_ZERO_MASK));
        auto* replay = new ReplayV2();
        if (replay->m_player.open(data.Items(), data.NumItems(), era) != v2redux::Result::OK)
        {
            delete replay;
            replay = nullptr;
        }
        else
        {
            replay->m_player.setSeed(seed);
            v2mpat::extract(data.Items(), data.NumItems(), kSampleRate, &replay->m_pattern);
        }
        _mm_setcsr(oldFP);
        return replay;
    }

    bool ReplayV2::DisplaySettings()
    {
        bool changed = false;
        const char* const patterns[] = { "Disable", "Enable" };
        changed |= ImGui::Combo("Show Patterns", &ms_patterns, patterns, NumItemsOf(patterns));
        return changed;
    }

    void ReplayV2::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find<Settings>(&dummy);

        static v2redux::Era eras[] = { v2redux::eras::fr08, v2redux::eras::flybye,
            v2redux::eras::fr014, v2redux::eras::fr019,
            v2redux::eras::candytron, v2redux::eras::kkrieger2004,
            v2redux::eras::release2004
        };

        int index = 0;
        if (entry->value[0] != 0 || entry->value[1] != 0)
        {
            v2redux::Era era = {
                .base = int(entry->version) - 1,
                .overridden = entry->overriddenDeltas,
                .forcedNew = entry->deltas
            };
            index = NumItemsOf(eras) + 1;
            for (int i = 0; i < NumItemsOf<int>(eras); ++i)
            {
                if (era.base == eras[i].base && era.overridden == eras[i].overridden && era.forcedNew == eras[i].forcedNew)
                {
                    index = i + 1;
                    break;
                }
            }
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        static const char* presets[] = { "Preset: Default", "Preset: fr08", "Preset: flybye", "Preset: fr014",
            "Preset: fr019", "Preset: candytron", "Preset: kkrieger2004", "Preset: release2004", "Preset: custom" };
        if (ImGui::Combo("##Presets", &index, presets, NumItemsOf(presets)))
        {
            if (index == 0)
                entry->value[0] = entry->value[1] = 0;
            else if (index != NumItemsOf<int>(presets) - 1)
            {
                entry->version = eras[index - 1].base + 1;
                entry->overriddenDeltas = eras[index - 1].overridden;
                entry->deltas = eras[index - 1].forcedNew;
            }
        }

        ImGui::SetNextItemWidth(-FLT_MIN);
        index = entry->version;
        static const char* versions[] = { "Version: Auto", "Version: 0 (fr08)", "Version: 1 (flybye)", "Version: 2 (?)", "Version: 3 (fr014)", "Version: 4 (fr019)", "Version: 5 (candytron)", "Version: 6 (2004 synth.asm)" };
        ImGui::Combo("##Versions", &index, versions, NumItemsOf(versions));
        entry->version = uint32_t(index);

        for (int i = 0; i < v2redux::DELTA_COUNT; ++i)
        {
            static const char* deltaEnums[] = { "DELTA_ENV_CURVES", "DELTA_FRAME256", "DELTA_ENV_CLAMP_SUSREL", "DELTA_OSC_BOXFILTER",
                "DELTA_NOISE_LCG_MSVC", "DELTA_NATIVE_FSIN", "DELTA_NATIVE_FPATAN", "DELTA_OSC_FREQ_CONST",
                "DELTA_NO_DCOFFSET", "DELTA_CRUSHER_SPLIT_GAIN1", "DELTA_PGMCHANGE_V0", "DELTA_TICK_BEFORE_SET",
                "DELTA_SUBFRAME_RENDER", "DELTA_NO_VOICE_DCF", "DELTA_NO_MASTER_DCF", "DELTA_NO_MOOG",
                "DELTA_KEYSYNC_OSC_ONLY", "DELTA_RVB_E_FULLPREC", "DELTA_NO_COMP_BOOST", "DELTA_NO_AUX_BUSSES",
                "DELTA_NO_RVB_LOWCUT", "DELTA_CC6_HICUT", "DELTA_RDTSC_SEED", "DELTA_NO_FM_OSC",
                "DELTA_NO_CHAN_DCF", "DELTA_POLY_16", "DELTA_POLY_32", "DELTA_CHANMOD_NO_VOICE_SRC",
                "DELTA_NO_RONAN_CHANNEL" };
            static const char* deltaTooltips[] = { "env attackmul -11/128 + dec/rel via calcfreq x10\n"
                "(calcfreq2/x11 did not exist in 2000)\n",
                "control frame 256 samples (2004: 128); implies\n"
                "volramp coeff 1/256 and env/LFO tick rate; LFO\n"
                "freq without the modern 0.5x compensation",
                "the val<=2^-13 ->OFF clamp only in SUSTAIN/\n"
                "RELEASE (2004 added the DECAY runout check);\n"
                "-> v0 envs decay into subnormals while gated",
                "tri/saw/pulse = 4x-oversampled numeric box\n"
                "filter (2004: analytic OSM convolution)",
                "noise LCG x214013+2531011 (2004: x196314165+\n"
                "907633515) + the 2000 16-bit float recurrence",
                "osc/LFO sine = native fsin (2004: fastsin poly)",
                "overdrive waveshaper = native atan (2004:\n"
                "fastatan rational poly)",
                "osc base freq = baked 3185015.0 (2004: computed\n"
                "from fcoscbase at runtime; SR-flexible)",
                "fcdcoffset (2^-18 denormal bias) absent\n"
                "everywhere (SVF, voice mix, chorus in, delay/\n"
                "reverb in)",
                "bitcrusher renders (in*gain1)*(32768/x) as two\n"
                "muls; 2004 folds gain1 into crush1 at set",
                "PGM change: no same-program early-out + resets\n"
                "ctl7 (chan volume) to 127",
                "frame control order TICK then SET: sub-objects\n"
                "step with the PREVIOUS frame's params",
                "render in sub-frame chunks with trailing-edge\n"
                "control tick (mid-frame note-on voice/chorus\n"
                "phase + control-edge facets)",
                "per-voice DC filter absent (voice = osc->flt->\n"
                "dist->volramp only)",
                "master DC filter absent(mix->lc / hc EQ direct)",
                "VCF modes 6/7 (MoogL/H) alias to passthrough",
                "noteOn keysync has no full-resync path: ANY\n"
                "keysync != 0 only zeros the osc phase counters\n"
                "(SYNC_FULL collapses to SYNC_OSC; noise/LFO/\n"
                "filter state RESUMES on re-trigger)",
                "reverb set: e = sqr(64/(revtime+1)) with the\n"
                "quotient kept full-precision through the square\n"
                "(the 2000 set path ran at x87 PC=64) and no\n"
                "SRfclinfreq factor (2004 SR-flexibility, =1.0\n"
                "at 44100)\n"
                "--- feature-introduction deltas (anchored by the param tables) ----------",
                "channel dcf1/comp/boost/dcf2 + global sum comp\n"
                "absent (no code in the v0 binary)",
                "AuxA/B busses absent (default-handled: rcv/send\n"
                "gains canonicalize to 0; lab port verified\n"
                "bit-exact without an explicit gate)",
                "reverb low-cut stage absent (ENGINE-GATED, not\n"
                "default-handled: the canonical global defaults\n"
                "give pre-v4 files a NONZERO lowcut param, so the\n"
                "2004 hpf stage would be a real extra filter --\n"
                "proven by fr08's reverb tail, DELTA.md)",
                "\"FAKE 2: Lowcut!\" -- a ch15(speech)-only control\n"
                "hack: MIDI CC6 on channel 15 ALSO sets the master\n"
                "high-cut freq hcfreq = sqr((val+1)/128), on top of\n"
                "storing the controller. PROVEN present at v0 (fr08\n"
                "ProcessControlChange @0x40bded) AND v5 (candytron/\n"
                "RG2 _viruz2a.asm \".FAKE 2 : Lowcut!\"); REMOVED in\n"
                "2004 (synth.asm just stores the CC). Drives the\n"
                "whole-mix master EQ in josie (ch15 sweeps CC6).",
                "osc-noise + LFO-S&H seed from rdtsc (== 0 under the\n"
                "deterministic pinned-rdtsc convention) -- NOT the\n"
                "2004 fixed osc-seed table {0xdeadbeef,0xbaadf00d,\n"
                "0xd3adc0de} / libc-rand LFO sequence. PROVEN old at\n"
                "v0 (fr08 syOscInit/syLFOInit rdtsc @0x40a..) AND v5\n"
                "(candytron syOscInit @0x41dc99 + syLFOInit rdtsc);\n"
                "the fixed table is a v6 addition (oscseeds in\n"
                "synth.asm _OSC_). Decoupled from DELTA_NOISE_LCG_MSVC\n"
                "(which only picks the LCG constants): at v5 the LCG\n"
                "is modern but the SEED is still rdtsc->0.",
                "osc mode 5 (FM sine) absent: the 2000 oscjtab\n"
                "(@0x40a565 in the fr08 image) maps modes 5/6/7\n"
                "to OFF -- no FM renderer exists in that binary.\n"
                "Present by v5 (candytron .mode4). Coupled to\n"
                "DELTA_NATIVE_FSIN for the implementation choice\n"
                "once present (see v2core renderFMSin_v5).",
                "channel dcf1/dcf2 DC filters absent: the v5\n"
                "syChanProcess (genthree _viruz2a.asm == candytron\n"
                "binary) is comp -> boost -> dist/chorus -> sends,\n"
                "with NO DC filter stage; the dcf1 (pre-comp) and\n"
                "dcf2 (post-dist) one-poles are 2004 additions.\n"
                "Previously mis-bundled into DELTA_NO_COMP_BOOST\n"
                "(whose v1 param anchor only covers comp/boost).\n"
                "Root cause of the josie whole-mix DC-decay\n"
                "residual (every channel, 2nd sample of first\n"
                "note).",
                "voice pool is 16, not bigger. The pool size is\n"
                "the first VALUE-typed era difference (16/32/64\n"
                "across builds), expressed as two composed bool\n"
                "rows -- see voicePoolSize() below. When a dense\n"
                "song saturates the period pool, the old engine\n"
                "STEALS the oldest gate-off voice where a bigger\n"
                "pool opens a fresh one: allocation choices (and\n"
                "thus steal-cut release tails + reused-voice\n"
                "state) diverge. Root cause of the flybye 66.42s\n"
                "divergence (first >16-voice moment of the song;\n"
                "alloc trace: 2001 steals slot 10, the 64-voice\n"
                "port opened voice 16).",
                "voice pool is 32, not 64. Both v5 binaries agree\n"
                "(unlike the osc trio) so the format proxy is\n"
                "clean at this flip; 64 is the 2004 value.",
                "the CHANNEL mod matrix applies ONLY sources < 8\n"
                "-- velocity (0) + ctl1..7 (1..7). Sources >= 8\n"
                "(aenv/env2, lfo1/lfo2, note) are voice-PRIVATE and\n"
                "have no channel-level meaning, so the old store\n"
                "skips them for channel params. PROVEN old at v3\n"
                "(fr014 syChanSet @0x40fe39 `cmp al,8; jae`) AND v4\n"
                "(fr019 @0x429900, same `cmp al,8; jae` after the\n"
                "dest-range cmp al,0x39/0x52). The modern-core store\n"
                "(v5 brullwurfel: no such range/source check) applies\n"
                "ALL sources -- proven by the bit-exact v5/v6 corpus\n"
                "(pzero/zeitmaschine/debris/kkrieger6 carry source>=8\n"
                "channel mods and only stay bit-exact if applied).\n"
                "Build-date proxy at v5 like the osc/player rows\n"
                "(early-v5 old-core fr-022 would still skip). Root\n"
                "cause of the fr-014 v3 ch8 ~5x divergence: aenv ->\n"
                "comp.outgain pushed makeup 98->128 (3.7x extra) plus\n"
                "lfo1 -> chorus.amount, both wrongly applied.",
                "the render-time channel-15 -> Ronan (speech\n"
                "vocal-tract) routing. OLD (early eras): ch15 is an\n"
                "ordinary music channel, never passed through Ronan.\n"
                "The year-2000 fr08 core (v0) inits Ronan but its\n"
                "render makes NO ronan calls (C1 harness), and a\n"
                "Ronan-OFF build is bit-exact to the C1 oracle --\n"
                "fr08's ch15 carries music (an F3 layer entering at\n"
                "1:58) that the unconditional routing silenced by\n"
                "running it through the idle vocal tract. NEW (v5+):\n"
                "ch15 -> ronanCBProcess, so candytron/kkrieger\n"
                "speech articulates."
            };

            bool isChecked = entry->deltas & (1 << i);
            if (!(entry->overriddenDeltas & (1 << i)))
            {
                ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
                if (ImGui::Checkbox(deltaEnums[i], &isChecked))
                    entry->overriddenDeltas |= 1 << i;
                ImGui::PopItemFlag();
            }
            else
            {
                if (ImGui::Checkbox(deltaEnums[i], &isChecked))
                {
                    if (!isChecked)
                    {
                        entry->deltas &= ~(1 << i);
                        entry->overriddenDeltas &= ~(1 << i);
                    }
                    else
                        entry->deltas |= 1 << i;
                }
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::Tooltip(deltaTooltips[i]);
        }

        uint64_t seed = entry->seed[0] | (uint64_t(entry->seed[1]) << 32);
        char buf[32] = "";
        if (seed)
            sprintf(buf, "%llX", seed);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputTextWithHint("##Seed", "Seed", buf, NumItemsOf(buf), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase))
        {
            seed = 0;
            for (int i = 0; i < 16; ++i)
            {
                if (buf[i] == 0)
                    break;
                seed = (seed << 4) + (buf[i] >= '0' && buf[i] <= '9') ? (buf[i] - '0') : (buf[i] - 'A' + 10);
            }
            entry->seed[0] = uint32_t(seed & 0xffFFffFF);
            entry->seed[1] = uint32_t(seed >> 32);
        }

        context.metadata.Update(entry, entry->value[0] == 0 && entry->value[1] == 0 && entry->value[2] == 0 && entry->value[3] == 0);
    }

    int32_t ReplayV2::ms_patterns = 1;

    ReplayV2::~ReplayV2()
    {}

    ReplayV2::ReplayV2()
        : Replay(eExtension::_v2m, eReplay::V2)
    {}

    uint32_t ReplayV2::Render(StereoSample* output, uint32_t numSamples)
    {
        if (!m_player.isPlaying())
        {
            if (!m_hasEnded)
            {
                m_hasEnded = true;
                return 0;
            }
        }
        auto oldFP = _mm_getcsr();
        _mm_setcsr(oldFP & ~(_MM_FLUSH_ZERO_MASK | _MM_DENORMALS_ZERO_MASK));
        m_player.render(pCast<float>(output), numSamples);
        _mm_setcsr(oldFP);
        return numSamples;
    }

    uint32_t ReplayV2::Seek(uint32_t timeInMs)
    {
        m_hasEnded = false;
        auto oldFP = _mm_getcsr();
        _mm_setcsr(oldFP & ~(_MM_FLUSH_ZERO_MASK | _MM_DENORMALS_ZERO_MASK));
        m_player.play(timeInMs);
        m_numPatSamples = uint32_t((timeInMs * kSampleRate) / 1000ull);
        _mm_setcsr(oldFP);
        return timeInMs;
    }

    void ReplayV2::ResetPlayback()
    {
        m_hasEnded = false;
        m_numPatSamples = 0;
        auto oldFP = _mm_getcsr();
        _mm_setcsr(oldFP & ~(_MM_FLUSH_ZERO_MASK | _MM_DENORMALS_ZERO_MASK));
        m_player.play();
        _mm_setcsr(oldFP);
    }

    void ReplayV2::ApplySettings(const CommandBuffer metadata)
    {
        if (auto settings = metadata.Find<Settings>())
        {
            uint64_t seed = settings->seed[0] | (uint64_t(settings->seed[1]) << 32);
            m_player.setSeed(seed);
        }
    }

    void ReplayV2::SetSubsong(uint32_t /*subsongIndex*/)
    {
        ResetPlayback();
    }

    uint32_t ReplayV2::GetDurationMs() const
    {
        return uint32_t(m_player.lengthMs());
    }

    uint32_t ReplayV2::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplayV2::GetExtraInfo() const
    {
        return {};
    }

    std::string ReplayV2::GetInfo() const
    {
        char txt[32];
        sprintf(txt, "%d channels\n", std::popcount(m_pattern.used_channels));
        std::string info = txt;
        sprintf(txt, "%d", m_player.fileVersion());
        info += "v2m version ";
        info += txt;
        info +="\nV2 Redux";
        return info;
    }

    Replay::Patterns ReplayV2::UpdatePatterns(uint32_t numSamples, uint32_t numLines, uint32_t charWidth, uint32_t spaceWidth, Patterns::Flags flags)
    {
        m_numPatSamples += numSamples;

        uint32_t numVisChannels = std::popcount(m_pattern.used_channels);
        if (!ms_patterns && !(flags & Patterns::kEnablePatterns) && numVisChannels) // kEnablePatterns has priority over internal flag
            return {};

        int32_t numChannels = numVisChannels;

        // Rows are timed, not counted: find the last row that has already started.
        const std::vector<uint64_t>& times = m_pattern.row_sample;
        uint32_t lo = 0;
        uint32_t hi = m_pattern.rows - 1;
        while (lo < hi) {
            uint32_t mid = (lo + hi + 1) / 2;
            if (times[mid] <= m_numPatSamples) {
                lo = mid;
            }
            else {
                hi = mid - 1;
            }
        }

        Patterns patterns;
        patterns.lines.Reserve(65536);
        patterns.sizes.Resize(numLines);
        patterns.currentLine = lo;
        if (flags & (Patterns::kEnableInstruments | Patterns::kEnableEffects))
            patterns.width = (numChannels * 4 - 1) * charWidth;
        else
            patterns.width = (numChannels * 3) * charWidth + (numChannels - 1) * spaceWidth;
        uint16_t size = uint16_t(numChannels * 4 - 1);
        if (flags & Patterns::kEnableInstruments)
        {
            patterns.width += numChannels * 2 * charWidth + spaceWidth * numChannels;
            size += uint16_t(numChannels * 3);
        }
        if (flags & Patterns::kEnableEffects)
        {
            patterns.width += numChannels * 2 * charWidth + spaceWidth * numChannels;
            size += uint16_t(numChannels * 3);
        }

        int32_t numRows = m_pattern.rows;
        auto currentRow = patterns.currentLine - int32_t(numLines) / 2;

        for (uint32_t line = 0; line < numLines; currentRow++, line++)
        {
            if (currentRow >= 0 && currentRow < numRows)
            {
                char colorBit = -128; // 0x80

                for (auto channels = m_pattern.used_channels;;)
                {
                    auto channel = std::countr_zero(channels);

                    const auto& source = m_pattern.cells[currentRow * v2mpat::CHANNELS + channel];

                    char notename[4] = "...";
                    if (source.note)
                    {
                        static const char* names[12] = { "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };
                        if (source.vel)
                            core::sprintf(notename, "===");
                        else
                            sprintf(notename, "%s%d", names[source.note % 12], source.note / 12 + 1);
                    }

                    if (flags & Patterns::kEnableColors)
                    {
                        patterns.lines.Add(colorBit | (source.note ? Patterns::kColorText : Patterns::kColorEmpty));
                    }
                    patterns.lines.Add(notename, 3);
                    if (flags & Patterns::kEnableInstruments)
                    {
                        if (source.pgm)
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorInstrument);
                            char buf[4];
                            sprintf(buf, " %02X", source.pgm);
                            patterns.lines.Add(buf, 3);
                        }
                        else
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                            patterns.lines.Add(" ..", 3);
                        }
                    }
                    if (flags & Patterns::kEnableEffects)
                    {
                        if (source.note && source.vel)
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorPitch);
                            char buf[4];
                            sprintf(buf, " %02X", source.vel);
                            patterns.lines.Add(buf, 3);
                        }
                        else
                        {
                            if (flags & Patterns::kEnableColors)
                                patterns.lines.Add(colorBit | Patterns::kColorEmpty);
                            patterns.lines.Add(" ..", 3);
                        }
                    }
                    channels &= ~(1 << channel);
                    if (channels == 0)
                        break;
                    if (flags & (Patterns::kEnableInstruments | Patterns::kEnableEffects))
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