#pragma once

#include <Core/Core.h>

namespace SoundMon
{
    static constexpr uint32_t kSongDataSize     = 32;
    static constexpr uint32_t kInstrSize        = 32;
    static constexpr uint32_t kNumInstr         = 15;
    static constexpr uint32_t kHeaderSize       = (kNumInstr * kInstrSize) + kSongDataSize;
    static constexpr uint32_t kVoices           = 4;
    static constexpr uint32_t kNotesPerPattern  = 16;
    static constexpr uint32_t kNoteSize         = 3;
    static constexpr uint32_t kPatternSize      = kNotesPerPattern * kNoteSize;
    static constexpr uint32_t kStepVoiceSize    = 4;
    static constexpr uint32_t kStepSize         = kVoices * kStepVoiceSize;
    static constexpr uint32_t kTableSize        = 64;
    static constexpr uint32_t kSynthFxSize      = 32;

    //static constexpr uint32_t kPerToFreqConst   = 0x179d8 * 10;
    //static constexpr uint32_t kPerToFreqConst = 8192 * 428;
    static constexpr uint32_t kPerToFreqConst = 3546895;//pal - 3579545 ntsc
}
//namespace SoundMon