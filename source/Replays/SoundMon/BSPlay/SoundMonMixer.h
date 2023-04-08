#pragma once

#include "SoundMonTypes.h"

namespace SoundMon
{
    class Module;

    class Mixer
    {
    public:
        Mixer(Module* modptr);
        ~Mixer();
        void CleanUp();
        bool Initialize(int16_t nrofvoices, uint32_t samplerate);
        void Stop();
        void SetStereoSeparation(float ratio) { m_stereoseparation = 0.5f + ratio * 0.5f; }
        bool Play(int16_t voicenr, uint8_t instrument, int32_t period, int32_t volume);
        bool Stop(int16_t voicenr);
        bool SetPan(int16_t voicenr, bool left);
        bool SetPeriod(int16_t voicenr, int32_t period);
        bool SetVolume(int16_t voicenr, int32_t volume);
        bool Update(int16_t voicenr, uint8_t* memptr, int32_t offset, int32_t numofbytes);
        uint32_t Fill(float* data, uint32_t numsamples);
        void Mix();

    private:
        class Channel
        {
        public:
            void SetPeriod(int32_t period, uint32_t samplerate) { m_rate = period * samplerate; }
            void SetLeftRight(bool left) { m_leftchannel = left; }
            void SetVolume(int32_t volume) { m_volume = volume; }
            void StopPlay() { m_playing = false; }
            void StartPlay() { m_playing = true; }
            void SetPlayBuffer(uint8_t* memptr, uint16_t size, uint16_t repeat, uint16_t replen);
            uint8_t GetNextBytes();
            void Update(uint8_t* memptr, uint16_t offset, uint16_t numofbytes);
            bool IsLeft() const { return m_leftchannel; }

        private:
            uint8_t* m_memptr{ nullptr };
            uint8_t* m_altmemptr{ nullptr };
            uint32_t m_count{ 0 };
            uint32_t m_rate{ 0 };
            uint16_t m_offset{ 0 };
            uint16_t m_numofbytes{ 0 };
            uint16_t m_size{ 0 };
            uint16_t m_repeat{ 0 };
            int32_t m_volume{ 0 };
            uint16_t m_replen{ 0 };
            uint16_t m_playcursor{ 0 };
            bool m_leftchannel{ true };
            bool m_playing{ false };
        };

        uint32_t GetBufferSize() const { return m_samplerate / 50; }

    private:
        Module* m_modptr;
        Channel* m_channels{ nullptr };
        int16_t* m_playbuffer{ nullptr };
        int16_t m_nrofvoices{ 0 };
        uint32_t m_bufferpos{ 0 };
        uint32_t m_remainingsize{ 0 };
        uint32_t m_samplerate{ 0 };
        float m_stereoseparation{ 1.0f };
    };
}
//namespace SoundMon