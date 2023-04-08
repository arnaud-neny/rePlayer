#pragma once

#include "SoundMonTypes.h"

namespace SoundMon
{
    class Module;
    class Mixer;

    class Player
    {
    public:
        Player(Module* modptr, uint32_t samplerate);
        ~Player();
        uint32_t Render(float* data, uint32_t numsamples);
        uint32_t Seek(uint32_t timeInMs);
        void Stop();
        void SetStep(int32_t step);
        void DecStep();
        void IncStep();
        void SetStereoSeparation(float ratio);

        void SetSubsong(uint8_t subsong);
        uint8_t GetNumSubsongs() const { return m_numsubsongs; }
        uint32_t GetDuration() const;

    private:
        struct BPCURRENT
        {
            uint8_t note;
            uint8_t instrument;
            uint16_t period;
            uint8_t option;
            uint8_t optiondata;
            uint8_t autoslide;
            uint8_t autoarpeggio;
            uint16_t volume;
            uint16_t adsrptr;
            uint16_t egptr;
            uint16_t modptr;
            uint16_t lfoptr;
            uint8_t vibrato;
            uint8_t adsrcount;
            uint8_t egcount;
            uint8_t modcount;
            uint8_t lfocount;
            uint8_t fxcount;
            uint8_t adsrcontrol;
            uint8_t lfocontrol;
            uint8_t egcontrol;
            uint8_t modcontrol;
            uint8_t fxcontrol;
            bool restart;
            bool newnote;
        };

    private:
        void BpPlay();
        void BpNext();
        void BpFx();
        void BpProcess();
        void BpArpeggio(int16_t voice, uint8_t arpeggio);
        void BpAutoSlide(int16_t voice, uint8_t slide);
        void BpVibrato(int16_t voice);
        void BpSynthFx(int16_t voice);
        void BpAveraging(int16_t voice);
        void BpTransform(int16_t voice, uint8_t table, uint8_t delta, bool invert);
        void BpBackInversion(int16_t voice, uint8_t delta);
        void BpTransform(int16_t voice, uint8_t table, uint8_t delta);

        template <typename T1, typename T2>
        static auto CalcVolume(T1 x, T2 y) { return static_cast<uint8_t>((static_cast<uint16_t>(x) * static_cast<uint16_t>(y)) >> 8); }
        template <typename T>
        static auto GetFreq(T x) { return ms_bpper[static_cast<char>(x) + 36 - 1]; }

        void BuildSubsongs();

    private:
        Module* m_modptr;
        Mixer* m_mixer;

        int32_t m_patptr{ 0 };
        int32_t m_stepptr{ 0 };
        int32_t m_nextstep{ -1 };
        int16_t m_speed{ 6 };
        int16_t m_count{ 0 };
        int16_t m_arpptr{ 0 };
        int16_t m_vibrptr{ 0 };
        BPCURRENT m_bpcurrent[kVoices]{};
        uint8_t m_synthbuf[kVoices * kSynthFxSize];
        bool m_loop{ false };
        uint8_t m_numsubsongs{ 0 };
        uint8_t m_subsongptr{ 0 };
        uint16_t* m_subsongs{ nullptr };

        static int16_t		ms_bpper[256];
        static int16_t		ms_vibtable[8];
    };
}
//namespace SoundMon