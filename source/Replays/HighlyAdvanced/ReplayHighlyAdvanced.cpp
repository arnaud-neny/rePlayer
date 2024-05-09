#include "ReplayHighlyAdvanced.h"
#include <mgba/core/blip_buf.h>
#include <mgba-util/vfs.h>

#include <Audio/AudioTypes.inl.h>
#include <Containers/Array.inl.h>
#include <Core/String.h>
#include <Imgui.h>
#include <ReplayDll.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::HighlyAdvanced,
        .name = "Highly Advanced",
        .extensions = "gsf;minigsf",
        .about = "Highly Advanced 2023-09-30\nChristopher Snowhill",
        .init = ReplayHighlyAdvanced::Init,
        .load = ReplayHighlyAdvanced::Load,
        .editMetadata = ReplayHighlyAdvanced::Settings::Edit
    };

    bool ReplayHighlyAdvanced::Init(SharedContexts* ctx, Window& /*window*/)
    {
        ctx->Init();

        return false;
    }

    Replay* ReplayHighlyAdvanced::Load(io::Stream* stream, CommandBuffer metadata)
    {
        auto replay = new ReplayHighlyAdvanced(stream);
        return replay->Load(metadata);
    }

    void ReplayHighlyAdvanced::Settings::Edit(ReplayMetadataContext& context)
    {
        auto* entry = context.metadata.Find<Settings>();
        if (entry == nullptr)
        {
            // ok, we are here because we never played this song in this player
            entry = context.metadata.Create<Settings>(sizeof(Settings) + sizeof(uint32_t));
            entry->GetDurations()[0] = 0;
        }

        const float buttonSize = ImGui::GetFrameHeight();
        auto durations = entry->GetDurations();
        for (uint16_t i = 0; i <= entry->numSongsMinusOne; i++)
        {
            ImGui::PushID(i);
            bool isEnabled = durations[i] != 0;
            uint32_t duration = isEnabled ? durations[i] : kDefaultSongDuration;
            auto pos = ImGui::GetCursorPosX();
            if (ImGui::Checkbox("##Checkbox", &isEnabled))
                duration = kDefaultSongDuration;
            ImGui::SameLine();
            ImGui::BeginDisabled(!isEnabled);
            char txt[64];
            sprintf(txt, "Subsong #%d Duration", i + 1);
            auto width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 4 - buttonSize;
            ImGui::SetNextItemWidth(2.0f * width / 3.0f - ImGui::GetCursorPosX() + pos);
            ImGui::DragUint("##Duration", &duration, 1000.0f, 1, 0xffFFffFF, txt, ImGuiSliderFlags_NoInput, ImVec2(0.0f, 0.5f));
            int32_t milliseconds = duration % 1000;
            int32_t seconds = (duration / 1000) % 60;
            int32_t minutes = duration / 60000;
            ImGui::SameLine();
            width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3 - buttonSize;
            ImGui::SetNextItemWidth(width / 3.0f);
            ImGui::DragInt("##Minutes", &minutes, 0.1f, 0, 65535, "%d m", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2 - buttonSize;
            ImGui::SetNextItemWidth(width / 2.0f);
            ImGui::DragInt("##Seconds", &seconds, 0.1f, 0, 59, "%d s", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x - buttonSize;
            ImGui::SetNextItemWidth(width);
            ImGui::DragInt("##Milliseconds", &milliseconds, 1.0f, 0, 999, "%d ms", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            if (ImGui::Button("E", ImVec2(buttonSize, 0.0f)))
            {
                context.duration = duration;
                context.subsongIndex = i;
                context.isSongEndEditorEnabled = true;
            }
            else if (context.isSongEndEditorEnabled == false && context.duration != 0 && context.subsongIndex == i)
            {
                milliseconds = context.duration % 1000;
                seconds = (context.duration / 1000) % 60;
                minutes = context.duration / 60000;
                context.duration = 0;
            }
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Open Waveform Viewer");
            ImGui::EndDisabled();
            durations[i] = isEnabled ? uint32_t(minutes) * 60000 + uint32_t(seconds) * 1000 + uint32_t(milliseconds) : 0;
            ImGui::PopID();
        }
    }

    void* ReplayHighlyAdvanced::OpenPSF(void* context, const char* uri)
    {
        auto This = reinterpret_cast<ReplayHighlyAdvanced*>(context);
        if (This->m_stream->GetName() == uri)
            return This->m_stream->Clone().Detach();
        return This->m_stream->Open(uri).Detach();
    }

    size_t ReplayHighlyAdvanced::ReadPSF(void* buffer, size_t size, size_t count, void* handle)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return size_t(stream->Read(buffer, size * count) / size);
    }

    int ReplayHighlyAdvanced::SeekPSF(void* handle, int64_t offset, int whence)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return stream->Seek(offset, io::Stream::SeekWhence(whence)) == Status::kOk ? 0 : -1;
    }

    int ReplayHighlyAdvanced::ClosePSF(void* handle)
    {
        if (handle)
            reinterpret_cast<io::Stream*>(handle)->Release();
        return 0;
    }

    long ReplayHighlyAdvanced::TellPSF(void* handle)
    {
        auto stream = reinterpret_cast<io::Stream*>(handle);
        return long(stream->GetPosition());
    }

    int ReplayHighlyAdvanced::InfoMetaPSF(void* context, const char* name, const char* value)
    {
        auto This = reinterpret_cast<ReplayHighlyAdvanced*>(context);
        auto tag = name;
        if (!_strnicmp(name, "replaygain_", sizeof("replaygain_") - 1))
        {}
        else if (!_stricmp(tag, "length"))
        {
            auto getDigit = [](const char*& value)
            {
                bool isDigit = false;
                int32_t digit = 0;
                while (*value && (*value < '0' || *value > '9'))
                    ++value;
                while (*value && *value >= '0' && *value <= '9')
                {
                    isDigit = true;
                    digit = digit * 10 + *value - '0';
                    ++value;
                }
                if (isDigit)
                    return digit;
                else
                    return -1;
            };
            auto length = getDigit(value);
            if (length >= 0)
            {
                while (*value && *value != ':' && *value != '.')
                    ++value;
                if (*value)
                {
                    if (*value == ':')
                    {
                        auto d = getDigit(++value);
                        if (d >= 0)
                            length = length * 60 + Clamp(d, 0, 59);
                    }
                    length *= 1000;
                    while (*value && *value != '.')
                        ++value;
                    if (*value == '.')
                    {
                        auto d = getDigit(++value);
                        if (d >= 0)
                            length += Clamp(d, 0, 999);
                    }
                }
                else
                    length *= 1000;

                if (length > 0)
                    This->m_length = length;
            }
        }
        else if (!_stricmp(tag, "fade"))
        {}
        else if (!_stricmp(tag, "utf8"))
        {}
        else if (!_stricmp(tag, "_lib"))
        {
            This->m_hasLib = true;
        }
        else if (tag[0] == '_')
        {
            return -1;
        }
        else
        {
            This->m_tags.Add(tag);
            This->m_tags.Add(value);
        }
        return 0;
    }

    int ReplayHighlyAdvanced::GsfLoad(void* context, const uint8_t* exe, size_t exe_size, const uint8_t* /*reserved*/, size_t /*reserved_size*/)
    {
        if (exe_size < 12) return -1;

        auto* state = reinterpret_cast<gsf_loader_state*>(context);

        auto get_le32 = [](void const* p)
        {
            return  (unsigned)((unsigned char const*)p)[3] << 24 |
                (unsigned)((unsigned char const*)p)[2] << 16 |
                (unsigned)((unsigned char const*)p)[1] << 8 |
                (unsigned)((unsigned char const*)p)[0];
        };

        unsigned char* iptr;
        unsigned isize;
        unsigned char* xptr;
        unsigned xentry = get_le32(exe + 0);
        unsigned xsize = get_le32(exe + 8);
        unsigned xofs = get_le32(exe + 4) & 0x1ffffff;
        if (xsize < exe_size - 12) return -1;
        if (!state->entry_set)
        {
            state->entry = xentry;
            state->entry_set = 1;
        }
        {
            iptr = state->data;
            isize = uint32_t(state->data_size);
            state->data = 0;
            state->data_size = 0;
        }
        if (!iptr)
        {
            unsigned rsize = xofs + xsize;
            {
                rsize -= 1;
                rsize |= rsize >> 1;
                rsize |= rsize >> 2;
                rsize |= rsize >> 4;
                rsize |= rsize >> 8;
                rsize |= rsize >> 16;
                rsize += 1;
            }
            iptr = (unsigned char*)malloc(rsize + 10);
            if (!iptr)
                return -1;
            memset(iptr, 0, rsize + 10);
            isize = rsize;
        }
        else if (isize < xofs + xsize)
        {
            unsigned rsize = xofs + xsize;
            {
                rsize -= 1;
                rsize |= rsize >> 1;
                rsize |= rsize >> 2;
                rsize |= rsize >> 4;
                rsize |= rsize >> 8;
                rsize |= rsize >> 16;
                rsize += 1;
            }
            xptr = (unsigned char*)realloc(iptr, xofs + rsize + 10);
            if (!xptr)
            {
                free(iptr);
                return -1;
            }
            iptr = xptr;
            isize = rsize;
        }
        memcpy(iptr + xofs, exe + 12, xsize);
        {
            state->data = iptr;
            state->data_size = isize;
        }

        return 0;
    }

    ReplayHighlyAdvanced::~ReplayHighlyAdvanced()
    {
        if (m_gbaCore)
        {
            mCoreConfigDeinit(&m_gbaCore->config);
            m_gbaCore->deinit(m_gbaCore);
        }
    }

    ReplayHighlyAdvanced::ReplayHighlyAdvanced(io::Stream* stream)
        : Replay(eExtension::Unknown, eReplay::HighlyAdvanced)
        , m_stream(stream)
    {
        m_gbaStream = {};
        m_gbaStream.postAudioBuffer = [](mAVStream* stream, blip_t* left, blip_t* right)
        {
            auto state = reinterpret_cast<GbaStream*>(stream);
            blip_read_samples(left, state->samples, 2048, true);
            blip_read_samples(right, state->samples + 1, 2048, true);
            state->numSamples = 2048;
        };
    }

    ReplayHighlyAdvanced* ReplayHighlyAdvanced::Load(CommandBuffer metadata)
    {
        auto stream = m_stream;
        uint32_t fileIndex = 0;
        do
        {
            m_length = kDefaultSongDuration;
            if (psf_load(stream->GetName().c_str(), &m_psfFileSystem, 0x22, nullptr, nullptr, InfoMetaPSF, this, 0) >= 0)
            {
                auto extPos = stream->GetName().find_last_of('.');
                if (extPos == std::string::npos || _stricmp(stream->GetName().c_str() + extPos + 1, "gsflib") != 0)
                {
                    if (psf_load(stream->GetName().c_str(), &m_psfFileSystem, 0x22, GsfLoad, &m_gbaRom, nullptr, nullptr, 0) >= 0)
                    {
                        m_mediaType.ext = m_hasLib ? eExtension::_minigsf : eExtension::_gsf;
                        m_subsongs.Add({ fileIndex, uint32_t(m_length) });
                    }
                }
            }
            m_tags.Clear();

            fileIndex++;
        } while (stream = stream->Next());

        if (m_subsongs.IsEmpty())
        {
            delete this;
            return nullptr;
        }

        SetupMetadata(metadata);

        return this;
    }

    uint32_t ReplayHighlyAdvanced::Render(StereoSample* output, uint32_t numSamples)
    {
        if (m_gbaCore == nullptr)
            return 0;

        uint64_t currentPosition = m_currentPosition;
        auto currentDuration = m_currentDuration;
        if (currentPosition == currentDuration)
        {
            m_currentPosition = 0;
            return 0;
        }

        uint64_t nextPosition = currentPosition + numSamples;
        if (nextPosition > currentDuration)
            numSamples = uint32_t(currentDuration - currentPosition);

        auto remainingSamples = numSamples;
        while (remainingSamples)
        {
            if (m_gbaStream.numSamples != 0)
            {
                auto numSamplesToConvert = Min(remainingSamples, m_gbaStream.numSamples);
                output = output->Convert(m_gbaStream.samples + (2048 - m_gbaStream.numSamples) * 2, numSamplesToConvert);

                remainingSamples -= numSamplesToConvert;
                m_gbaStream.numSamples -= numSamplesToConvert;
            }
            else while (m_gbaStream.numSamples == 0)
                m_gbaCore->runFrame(m_gbaCore);
        }

        m_currentPosition = currentPosition + numSamples;
        m_globalPosition += numSamples;

        return numSamples;
    }

    uint32_t ReplayHighlyAdvanced::Seek(uint32_t timeInMs)
    {
        uint64_t seekPosition = timeInMs;
        seekPosition *= kSampleRate;
        seekPosition /= 1000;
        uint64_t globalPosition = m_globalPosition;

        if (seekPosition < globalPosition)
        {
            ResetPlayback();
            globalPosition = 0;
        }
        auto numSamples = seekPosition - globalPosition;
        while (numSamples)
        {
            if (m_gbaStream.numSamples != 0)
            {
                auto numSamplesToConvert = Min(numSamples, m_gbaStream.numSamples);
                numSamples -= numSamplesToConvert;
                m_gbaStream.numSamples -= uint32_t(numSamplesToConvert);
                globalPosition += numSamplesToConvert;
            }
            else while (m_gbaStream.numSamples == 0)
                m_gbaCore->runFrame(m_gbaCore);
        }

        m_globalPosition = globalPosition;
        seekPosition = globalPosition;
        m_currentPosition = globalPosition % m_currentDuration;
        return uint32_t((seekPosition * 1000) / kSampleRate);
    }

    void ReplayHighlyAdvanced::ResetPlayback()
    {
        auto subsongIndex = m_subsongIndex;
        if (m_currentSubsongIndex != subsongIndex)
        {
            m_gbaRom = {};
            m_tags.Clear();
            m_title.clear();

            auto stream = m_stream;
            for (uint32_t fileIndex = 0; stream; fileIndex++)
            {
                if (fileIndex == m_subsongs[m_subsongIndex].index)
                {
                    m_title = stream->GetName();
                    psf_load(stream->GetName().c_str(), &m_psfFileSystem, 0x22, GsfLoad, &m_gbaRom, InfoMetaPSF, this, 0);
                    break;
                }
                stream = stream->Next();
            }
        }

        m_currentPosition = 0;
        m_globalPosition = 0;
        m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
        m_currentSubsongIndex = subsongIndex;
        m_gbaStream.numSamples = 0;

        if (m_gbaCore)
        {
            mCoreConfigDeinit(&m_gbaCore->config);
            m_gbaCore->deinit(m_gbaCore);
            m_gbaCore = nullptr;
        }

        auto* rom = VFileFromConstMemory(m_gbaRom.data, m_gbaRom.data_size);
        auto* core = mCoreFindVF(rom);
        if (!core)
        {
            rom->close(rom);
            return;
        }
        core->init(core);
        core->setAVStream(core, &m_gbaStream);
        mCoreInitConfig(core, nullptr);

        core->setAudioBufferSize(core, 2048);

        blip_set_rates(core->getAudioChannel(core, 0), core->frequency(core), kSampleRate);
        blip_set_rates(core->getAudioChannel(core, 1), core->frequency(core), kSampleRate);

        struct mCoreOptions opts = {};
        opts.useBios = false;
        opts.skipBios = true;
        opts.volume = 0x100;
        opts.sampleRate = kSampleRate;
        mCoreConfigLoadDefaults(&core->config, &opts);

        core->loadROM(core, rom);
        core->reset(core);
        m_gbaCore = core;
    }

    void ReplayHighlyAdvanced::ApplySettings(const CommandBuffer metadata)
    {
        auto settings = metadata.Find<Settings>();
        if (settings)
        {
            auto durations = settings->GetDurations();
            for (uint16_t i = 0; i <= settings->numSongsMinusOne; i++)
                m_subsongs[i].overriddenDuration = durations[i];
            m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
        }
    }

    void ReplayHighlyAdvanced::SetSubsong(uint32_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        ResetPlayback();
    }

    uint32_t ReplayHighlyAdvanced::GetDurationMs() const
    {
        return m_subsongs[m_subsongIndex].overriddenDuration > 0 ? m_subsongs[m_subsongIndex].overriddenDuration : m_subsongs[m_subsongIndex].duration;
    }

    uint32_t ReplayHighlyAdvanced::GetNumSubsongs() const
    {
        return Max(1ul, m_subsongs.NumItems());
    }

    std::string ReplayHighlyAdvanced::GetSubsongTitle() const
    {
        for (uint32_t i = 0, e = m_tags.NumItems(); i < e; i += 2)
        {
            if (_stricmp(m_tags[i].c_str(), "title") == 0)
                return m_tags[i + 1];
        }
        auto offset = m_title.find_last_of('.');
        if (offset != m_title.npos)
            return std::string(m_title.c_str(), offset);
        return m_title;
    }

    std::string ReplayHighlyAdvanced::GetExtraInfo() const
    {
        std::string metadata;
        for (uint32_t i = 0, e = m_tags.NumItems(); i < e; i += 2)
        {
            if (i != 0)
                metadata += "\n";
            metadata += m_tags[i];
            metadata += ": ";
            metadata += m_tags[i + 1];
        }
        return metadata;
    }

    std::string ReplayHighlyAdvanced::GetInfo() const
    {
        return "6 channels\nGame Boy Advance Sound Format\nHighly Advanced 2023-09-30";
    }

    void ReplayHighlyAdvanced::SetupMetadata(CommandBuffer metadata)
    {
        uint32_t numSongsMinusOne = m_subsongs.NumItems() - 1;
        auto settings = metadata.Find<Settings>();
        if (settings && settings->numSongsMinusOne == numSongsMinusOne)
        {
            auto durations = settings->GetDurations();
            for (uint32_t i = 0; i <= numSongsMinusOne; i++)
                m_subsongs[i].overriddenDuration = durations[i];
        }
        else
        {
            auto value = settings ? settings->value : 0;
            settings = metadata.Create<Settings>(sizeof(Settings) + (numSongsMinusOne + 1) * sizeof(int32_t));
            settings->value = value;
            settings->numSongsMinusOne = numSongsMinusOne;
            auto durations = settings->GetDurations();
            for (uint16_t i = 0; i <= numSongsMinusOne; i++)
                durations[i] = 0;
        }
        m_currentDuration = (uint64_t(GetDurationMs()) * kSampleRate) / 1000;
    }
}
// namespace rePlayer