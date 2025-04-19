// minor changes for rePlayer and MSVC:
// - lib_flac\libFLAC\flac_config.h
// - lib_sundog\sound\sound.cpp
// - lib_sundog\sundog.h
// - lib_sundog\time\time.cpp
// - lib_sunvox\sunvox_engine.h
// - lib_sunvox\sunvox_engine_load_proj.cpp
// - lib_sunvox\xm\xm_song.cpp
// - lib_vorbis\tremor\codebook.c
// - lib_vorbis\tremor\tremor\os_types.h

#include "ReplaySunVox.h"

#include <Audio/AudioTypes.inl.h>
#include <ReplayDll.h>

#pragma comment(lib, "winmm.lib")

const char* g_app_config[] = { nullptr };
const char* g_app_log = nullptr;

int app_global_init()
{
    int rv = 0;
    if (sunvox_global_init()) rv = -1;
    return rv;
}

int app_global_deinit()
{
    int rv = 0;
    if (sunvox_global_deinit()) rv = -1;
    return rv;
}

int render_piece_of_sound(sundog_sound* ss, int slot_num)
{
    int handled = 0;

    if (!ss) return 0;

    sundog_sound_slot* slot = &ss->slots[slot_num];
    sunvox_engine* s = (sunvox_engine*)slot->user_data;

    if (!s) return 0;
    if (!s->initialized) return 0;

    sunvox_render_data rdata;
    smem_clear_struct(rdata);
    rdata.buffer_type = ss->out_type;
    rdata.buffer = slot->buffer;
    rdata.frames = slot->frames;
    rdata.channels = ss->out_channels;
    rdata.out_latency = ss->out_latency;
    rdata.out_latency2 = ss->out_latency2;
    rdata.out_time = slot->time;
    rdata.in_buffer = slot->in_buffer;
    rdata.in_type = ss->in_type;
    rdata.in_channels = ss->in_channels;

    handled = sunvox_render_piece_of_sound(&rdata, s);

    if (handled && rdata.silence)
        handled = 2;

    return handled;
}

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::SunVox,
        .name = "SunVox",
        .extensions = "sunvox",
        .about = "SunVox " SUNVOX_ENGINE_VERSION_STR "\nCopyright(c) 2012-2024 Alexander Zolotov <nightradio@gmail.com>",
        .init = ReplaySunVox::Init,
        .release = ReplaySunVox::Release,
        .load = ReplaySunVox::Load,
        .editMetadata = ReplaySunVox::Settings::Edit
    };

    bool ReplaySunVox::Init(SharedContexts* ctx, Window& window)
    {
        UnusedArg(window);

        ctx->Init();

        return sundog_global_init();
    }

    void ReplaySunVox::Release()
    {
        sundog_global_deinit();
    }

    Replay* ReplaySunVox::Load(io::Stream* stream, CommandBuffer metadata)
    {
        UnusedArg(metadata);

        auto* engine = new sunvox_engine;
        sunvox_engine_init(SUNVOX_FLAG_CREATE_PATTERN | SUNVOX_FLAG_CREATE_MODULES | SUNVOX_FLAG_MAIN | SUNVOX_FLAG_ONE_THREAD | SUNVOX_FLAG_PLAYER_ONLY, kSampleRate, nullptr, nullptr, nullptr, nullptr, engine);

        auto data = stream->Read();
        sfs_file f = sfs_open_in_memory((void*)data.Items(), data.Size());
        auto rv = sunvox_load_proj_from_fd(f, 0, engine);
        sfs_close(f);
        if (rv)
        {
            sunvox_engine_close(engine);
            delete engine;

            return nullptr;
        }

        return new ReplaySunVox(engine);
    }

    void ReplaySunVox::Settings::Edit(ReplayMetadataContext& context)
    {
        Settings dummy;
        auto* entry = context.metadata.Find(&dummy);

        int tmpExtraTime = entry->extraTime;
        ImGui::SliderInt("##Slider", &tmpExtraTime, 0, 60000, "Extra Time %dms", ImGuiSliderFlags_ClampOnInput);
        if (ImGui::IsItemHovered())
            ImGui::Tooltip("Time to add at the end of the song (can't loop)");
        entry->extraTime = uint32_t(tmpExtraTime);

        context.metadata.Update(entry, entry->value == 0);
    }

    ReplaySunVox::~ReplaySunVox()
    {
        sunvox_engine_close(m_engine);
        sundog_sound_deinit(&m_sound);

        delete m_engine;
    }

    ReplaySunVox::ReplaySunVox(sunvox_engine* engine)
        : Replay(eExtension::_sunvox, eReplay::SunVox)
        , m_engine(engine)
        , m_numFrames(sunvox_get_proj_frames(engine))
    {
        sundog_sound_init(&m_sound, nullptr, sound_buffer_float32, kSampleRate, 2, SUNDOG_SOUND_FLAG_ONE_THREAD | SUNDOG_SOUND_FLAG_USER_CONTROLLED);

        sundog_sound_set_slot_callback(&m_sound, 0, &render_piece_of_sound, engine);
        sundog_sound_play(&m_sound, 0);
        sundog_sound_input(&m_sound, true);

        engine->net->global_volume = 256;
    }

    void ReplaySunVox::SetPosition(int position)
    {
        // first stop to pause (put in not playing)
        if (sunvox_get_playing_status(m_engine))
            sunvox_stop(m_engine);
        // as it's not playing now, this stop will clear the modules
        sunvox_stop(m_engine);
        // restart from the beginning
        sunvox_play(position, true, -1, m_engine);
    }

    uint32_t ReplaySunVox::Render(StereoSample* output, uint32_t numSamples)
    {
        auto extraTime = m_extraTime;
        auto numFrames = m_numFrames + extraTime;
        auto currentFrame = m_currentFrame;
        auto nextFrame = currentFrame + numSamples;
        if (nextFrame > numFrames)
        {
            numSamples = numFrames - currentFrame;
            nextFrame = numFrames;
        }
        if (numSamples == 0)
            m_currentFrame = 0;
        else
        {
            m_currentFrame = nextFrame;
            m_engine->stop_at_the_end_of_proj = extraTime != 0;
            user_controlled_sound_callback(&m_sound, output, numSamples, 0, 0);
        }
        return numSamples;
    }

    uint32_t ReplaySunVox::Seek(uint32_t timeInMs)
    {
        uint64_t currentFrame = timeInMs;
        currentFrame *= kSampleRate;
        currentFrame /= 1000;

        uint64_t frameToCheck = currentFrame % m_numFrames;
        currentFrame -= frameToCheck;

        int numLines = sunvox_get_proj_lines(m_engine);
        sunvox_time_map_item* map = (sunvox_time_map_item*)smem_new(sizeof(sunvox_time_map_item) * numLines);
        uint32_t* frameMap = (uint32_t*)smem_new(sizeof(uint32_t) * (numLines + 1));
        sunvox_get_time_map(map, frameMap, 0, numLines, m_engine);
        smem_free(map);
        frameMap[numLines] = ~uint32_t(0);

        bool isFound = false;
        for (int i = 0; i <= numLines; ++i)
        {
            if (frameMap[i] >= frameToCheck)
            {
                int pos = frameMap[i] == frameToCheck ? i : i - 1;
                frameToCheck = frameMap[pos];
                SetPosition(pos);
                isFound = true;
                break;
            }
        }
        smem_free(frameMap);
        m_currentFrame = uint32_t(frameToCheck);

        return uint32_t(((currentFrame + frameToCheck) * 1000ull) / kSampleRate);
    }

    void ReplaySunVox::ResetPlayback()
    {
        SetPosition(0);
        m_currentFrame = 0;
    }

    void ReplaySunVox::ApplySettings(const CommandBuffer metadata)
    {
        auto* settings = metadata.Find<Settings>();
        m_extraTime = settings ? settings->extraTime : 0;
    }

    void ReplaySunVox::SetSubsong(uint32_t subsongIndex)
    {
        UnusedArg(subsongIndex);
        ResetPlayback();
    }

    uint32_t ReplaySunVox::GetDurationMs() const
    {
        return uint32_t((m_numFrames * 1000ull) / kSampleRate);
    }

    uint32_t ReplaySunVox::GetNumSubsongs() const
    {
        return 1;
    }

    std::string ReplaySunVox::GetExtraInfo() const
    {
        std::string metadata;
        if (m_engine->proj_name && m_engine->proj_name[0])
            metadata = m_engine->proj_name;
        return metadata;
    }

    std::string ReplaySunVox::GetInfo() const
    {
        std::string info;
        info = "2 channels\nSunVox ";
        char buf[64];
        sprintf(buf, "%u.%u.%u.%u", m_engine->base_version >> 24
            , (m_engine->base_version >> 16) & 0xff
            , (m_engine->base_version >> 8) & 0xff
            , m_engine->base_version & 0xff);
        info += buf;
        info += "\nSunVox " SUNVOX_ENGINE_VERSION_STR;
        return info;
    }
}
// namespace rePlayer