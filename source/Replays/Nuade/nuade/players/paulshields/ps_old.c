// SPDX-License-Identifier: MIT
// Paul Shields player: the sequencer of the oldest module layout. Its pattern bytes are notes (below 0x80) and commands
// from 0x80 up, its sample, volume envelope and pitch envelope come together in an instrument record, and every period
// is converted on each tick.

#include "ps_internal.h"

#define INSTRUMENT_RECORD_SIZE 6

typedef enum StepKind {
    StepKind_Note, // a new note has started
    StepKind_Rest,
    StepKind_Stopped,
} StepKind;

// The instrument command: a record of three offsets picks the sample table entry and the two envelopes. False when the
// sample table has no such entry.

static bool select_instrument(PshPlayer* p, uint32_t channel) {
    const PshModule* m = &p->module;
    PshVoice* v = &p->voices[channel];
    const uint8_t number = psh_u8(m, v->stream++);
    const int64_t record = (int64_t)m->instruments + number;
    const PshSample* sample = psh_instrument_sample(m, record);
    if (sample == nullptr) {
        return false;
    }
    psh_select_sample(v, sample);
    psh_select_volume_envelope(m, v, (int16_t)psh_u16(m, record + 2));
    psh_select_pitch_envelope(m, v, (int16_t)psh_u16(m, record + 4));
    v->display_instrument = number / INSTRUMENT_RECORD_SIZE;
    return true;
}

// The replayer moves a byte into the low half of the level and leaves the high half.

static uint16_t low_byte_set(uint16_t level, uint8_t low) {
    return (uint16_t)((level & 0xFF00u) | low);
}

// The pattern bytes up to the next note or rest.

static StepKind read_commands(PshPlayer* p, uint32_t channel) {
    const PshModule* m = &p->module;
    PshVoice* v = &p->voices[channel];
    for (int guard = 0; guard < PSH_COMMAND_LIMIT; guard++) {
        const uint8_t byte = psh_u8(m, v->stream++);
        if (byte < PshOldPatternCommand_Rest) {
            v->period = (uint16_t)(byte << 3);
            psh_read_duration(m, v);
            v->period_slide = 0;
            v->display_note = psh_display_note(byte);
            psh_start_note(p, channel);
            return StepKind_Note;
        }
        switch ((PshOldPatternCommand)byte) {
            case PshOldPatternCommand_Rest:
                v->releasing = true;
                psh_read_duration(m, v);
                return StepKind_Rest;
            case PshOldPatternCommand_Slide: {
                const uint16_t start = (uint16_t)(psh_u8(m, v->stream++) << 3);
                const uint16_t target = (uint16_t)(psh_u8(m, v->stream++) << 3);
                psh_read_duration(m, v);
                psh_start_slide(v, start, target, v->duration);
                psh_start_note(p, channel);
                return StepKind_Note;
            }
            case PshOldPatternCommand_VolumeOverride:
            case PshOldPatternCommand_VolumeOverrideAlias: {
                const uint8_t level = psh_u8(m, v->stream++);
                v->volume_override = level;
                v->level = low_byte_set(v->level, level);
                break;
            }
            case PshOldPatternCommand_LevelOnly:
                v->level = low_byte_set(v->level, psh_u8(m, v->stream++));
                break;
            case PshOldPatternCommand_Instrument:
                if (!select_instrument(p, channel)) {
                    psh_stop_voice(v);
                    return StepKind_Stopped;
                }
                break;
            case PshOldPatternCommand_NextPattern:
                if (!psh_next_pattern(p, channel)) {
                    return StepKind_Stopped;
                }
                break;
            case PshOldPatternCommand_FilterOn:
            case PshOldPatternCommand_FilterOff:
            case PshOldPatternCommand_FilterOffAlias:
            case PshOldPatternCommand_NoOperation:
                break;
            default:
                psh_stop_voice(v);
                return StepKind_Stopped;
        }
    }
    psh_stop_voice(v);
    return StepKind_Stopped;
}

// One tick of a voice. The envelope steps in the tick of a note start and while the note goes on, not in the tick of a
// rest.

void psh_step_voice_old(PshPlayer* p, uint32_t channel) {
    const PshModule* m = &p->module;
    PshVoice* v = &p->voices[channel];
    if (!v->active) {
        return;
    }
    psh_volume_slide(v);
    v->duration--;
    if (v->duration == 0) {
        const StepKind step = read_commands(p, channel);
        if (step == StepKind_Stopped) {
            return;
        }
        if (step == StepKind_Note) {
            psh_volume_envelope(m, v);
        }
    } else if (v->releasing) {
        psh_release_level(v);
    } else {
        psh_volume_envelope(m, v);
    }
    psh_add_period_slide(v);
    psh_pitch_envelope(m, v);
    psh_write_registers(p, channel, psh_note_period(v, v->period, true));
}
