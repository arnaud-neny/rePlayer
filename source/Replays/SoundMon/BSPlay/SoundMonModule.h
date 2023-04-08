#pragma once

#include "SoundMonInstrument.h"

namespace SoundMon
{
    class Module
    {
    public:
        static Module* Load(const uint8_t* data, size_t size);
        void Release();

        uint16_t GetPattern(uint16_t step, uint16_t voice) const;

        uint8_t GetST(uint16_t step, uint16_t voice) const { return *(m_startsteps + kStepVoiceSize * voice + step * kStepSize + 2); };
        uint8_t GetTR(uint16_t step, uint16_t voice) const { return *(m_startsteps + kStepVoiceSize * voice + step * kStepSize + 3); };
        uint8_t GetNote(uint16_t pattern, uint16_t position) const { return *(m_startpatterns + kPatternSize * (pattern - 1) + position * kNoteSize); };
        uint8_t GetFX(uint16_t pattern, uint16_t position) const { return (*(m_startpatterns + kPatternSize * (pattern - 1) + position * kNoteSize + 1)) & 0x0F; };
        uint8_t GetInstrument(uint16_t pattern, uint16_t position) const { return ((*(m_startpatterns + kPatternSize * (pattern - 1) + position * kNoteSize + 1)) & 0xF0) >> 4; };
        uint8_t GetFXByte(uint16_t pattern, uint16_t position) const { return (*(m_startpatterns + kPatternSize * (pattern - 1) + position * kNoteSize + 2)); };
        const char* GetSongName() const { return m_songname; };
        uint16_t GetNumTables() const { return m_numtables; };
        uint16_t GetNumPatterns() const { return m_numpatterns; };
        uint16_t GetNumSteps() const { return m_numsteps; };
        uint16_t GetInstrumentVolume(int16_t instr) const { return (instr > kNumInstr || instr < 1) ? 0 : m_instr[instr - 1].GetInstrVolume(); }
        bool IsSynthInstrument(int16_t instr) const { return (instr > kNumInstr || instr < 1) ? false : m_instr[instr - 1].IsSynthInstr(); }
        uint8_t GetTableValue(uint8_t table, uint16_t offset) const { return *(m_starttables + int32_t(table) * kTableSize + offset); }
        uint8_t GetVersion() const { return m_version; }

        bool GetSampledInstrument(int16_t instr, const char*& name, uint16_t& size, uint16_t& repeat, uint16_t& replen, uint16_t& volume) const;
        bool GetSampledInstrument(int16_t instr, uint16_t& size, uint16_t& repeat, uint16_t& replen, uint16_t& volume, uint8_t*& start) const;
        bool GetSynthInstrument(int16_t instr, uint16_t& size, uint8_t*& start) const;
        bool GetSynthDelays(int16_t instr, uint8_t& egdelay, uint8_t& lfodelay, uint8_t& fxdelay, uint8_t& moddelay) const;
        bool GetSynthEGData(int16_t instr, uint8_t& table, uint16_t& size, uint8_t& speed) const;
        bool GetSynthLFOData(int16_t instr, uint8_t& table, uint16_t& size, uint8_t& speed, uint8_t& depth) const;
        bool GetSynthFXData(int16_t instr, uint8_t& table, uint8_t& speed) const;
        bool GetSynthMODData(int16_t instr, uint8_t table, uint16_t& size, uint8_t& speed) const;
        bool GetSynthADSRData(int16_t instr, uint8_t& table, uint16_t& size, uint8_t& speed) const;
        bool GetSynthControls(int16_t instr, uint8_t& adsr, uint8_t& lfo, uint8_t& eg, uint8_t& fx, uint8_t& mod) const;

        const char* GetInstrumentName(int16_t instr) const;

    private:
        Module() = default;
        ~Module() = default;

        void Init(uint8_t * modptr);

        static void ConvertAmigaSample(uint8_t* ptr, size_t size);
        static void ConvertHeader(uint8_t* modptr);

    private:
        uint8_t* m_startsteps;
        uint8_t* m_startpatterns;
        uint8_t* m_starttables;
        uint8_t* m_startsamples;

        char m_songname[27]{ 0 };
        uint8_t m_version{ 3 };

        uint16_t m_numsteps;
        uint16_t m_numpatterns;
        uint16_t m_numtables;
        Instrument m_instr[kNumInstr];
    };
}
//namespace SoundMon