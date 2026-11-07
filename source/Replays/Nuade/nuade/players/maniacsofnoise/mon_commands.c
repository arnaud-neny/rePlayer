// SPDX-License-Identifier: MIT
// Maniacs of Noise player: which command a sequence word or a pattern byte is. The replayers and the walk of the songs
// (mon_walk.c) use the same tables.

#include "mon_internal.h"

#define SEQUENCE_COMMAND_BASE 0xF000
#define SEQUENCE_PARAMETER_MASK 0xFF
#define SEQUENCE_COMMAND_SHIFT 8
#define SEQUENCE_COMMAND_MASK 0x0F
#define PATTERN_COMMAND_BIT 0x80 // a pattern byte with this bit is a command (not MonVersion_Gyroscope)
#define PATTERN_COMMAND_MASK 0x0F

bool mon_sequence_is_pattern(uint16_t word) {
    return word < SEQUENCE_COMMAND_BASE;
}

uint8_t mon_sequence_parameter(uint16_t word) {
    return word & SEQUENCE_PARAMETER_MASK;
}

MonSequenceCommand mon_sequence_command(uint16_t word) {
    static const MonSequenceCommand k_commands[] = {
        MonSequenceCommand_Transpose, MonSequenceCommand_Repeat, MonSequenceCommand_FadeIn, MonSequenceCommand_FadeOut,
        MonSequenceCommand_Tempo,     MonSequenceCommand_Jump,   MonSequenceCommand_Stop,
    };
    const unsigned index = (word >> SEQUENCE_COMMAND_SHIFT) & SEQUENCE_COMMAND_MASK;
    return index < nuade_sizeof_array(k_commands) ? k_commands[index] : MonSequenceCommand_Unknown;
}

// MonVersion_Gyroscope marks its five commands with the bytes 0xFB to 0xFF; the other versions index a table of
// handlers with the low bits of a byte with bit 7 set, the older of them with six entries, the newer with ten.

MonPatternCommand mon_pattern_command(MonVersion version, uint8_t byte) {
    if (version == MonVersion_Gyroscope) {
        switch (byte) {
            case 0xFF:
                return MonPatternCommand_End;
            case 0xFE:
                return MonPatternCommand_Instrument;
            case 0xFD:
                return MonPatternCommand_Volume;
            case 0xFC:
                return MonPatternCommand_Slide;
            case 0xFB:
                return MonPatternCommand_SlideNote;
            default:
                return MonPatternCommand_Note;
        }
    }
    if ((byte & PATTERN_COMMAND_BIT) == 0) {
        return MonPatternCommand_Note;
    }
    static const MonPatternCommand k_old[] = {
        MonPatternCommand_End,   MonPatternCommand_Instrument, MonPatternCommand_Volume,
        MonPatternCommand_Slide, MonPatternCommand_SlideNote,  MonPatternCommand_Release,
    };
    static const MonPatternCommand k_new[] = {
        MonPatternCommand_End,       MonPatternCommand_Instrument, MonPatternCommand_Volume,   MonPatternCommand_Slide,
        MonPatternCommand_SlideNote, MonPatternCommand_Release,    MonPatternCommand_Arpeggio, MonPatternCommand_Wait,
        MonPatternCommand_Swing,     MonPatternCommand_Rest,
    };
    const unsigned index = byte & PATTERN_COMMAND_MASK;
    if (mon_is_old_version(version)) {
        return index < nuade_sizeof_array(k_old) ? k_old[index] : MonPatternCommand_Unknown;
    }
    return index < nuade_sizeof_array(k_new) ? k_new[index] : MonPatternCommand_Unknown;
}

unsigned mon_pattern_arguments(MonPatternCommand command) {
    switch (command) {
        case MonPatternCommand_End:
        case MonPatternCommand_Unknown:
            return 0;
        case MonPatternCommand_Note:
        case MonPatternCommand_Instrument:
        case MonPatternCommand_Volume:
        case MonPatternCommand_Release:
        case MonPatternCommand_Wait:
        case MonPatternCommand_Swing:
        case MonPatternCommand_Rest:
            return 1;
        case MonPatternCommand_Slide:
        case MonPatternCommand_Arpeggio:
            return 2;
        case MonPatternCommand_SlideNote:
            return 3;
    }
    return 0;
}
