#pragma once

#include <Audio/AudioTypes.h>
#include <Containers/Array.h>
#include <Thread/Semaphore.h>
#include <Database/Types/MusicID.h>

namespace rePlayer
{
    using namespace core;
    class Replay;
    struct ReplayMetadataContext;

    class SongEndEditor
    {
    public:
        static SongEndEditor* Create(ReplayMetadataContext& context, MusicID musicId);
        SongEndEditor(MusicID musicId, Replay* replay, uint32_t duration);
        ~SongEndEditor();

        bool Update(ReplayMetadataContext& context);

    private:
        // frame is set of samples info per pixel
        struct Frame
        {
            uint8_t min;
            uint8_t max;
            uint8_t rms;
        };

        struct Wave;

    private:
        void Render();
        uint32_t ReadCurrentSample();
        void OpenAudio();

        uint32_t WaveformUI();
        void PlaybackUI();
        void ScaleUI(uint32_t width);
        void DurationUI();
        void RenderUI();

    private:
        MusicID m_musicId;
        Replay* m_replay;

        Array<StereoSample> m_samples;
        Array<Frame> m_frames;
        Array<uint32_t> m_loops;
        uint64_t m_numMillisecondsPerPixel = 250;
        uint32_t m_duration;
        uint32_t m_currentSample = 0;
        uint32_t m_currentFrameSample = 0;
        uint32_t m_numSamples = 0;
        bool m_isPlaying = false;
        bool m_isRunning = true;
        bool m_isJobDone = false;
        bool m_isWaveScrolling = false;
        float m_waveScroll = 0.0f;
        uint32_t m_waveStartPosition = 0;
        Wave* m_wave = nullptr;
        thread::Semaphore m_semaphore;
    };
}
// namespace rePlayer