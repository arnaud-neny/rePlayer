// SPDX-License-Identifier: MIT
// Paul Shields player: the sequencer of the short and long module layouts. A voice reads its pattern bytes until a
// note or a rest gives it a duration; every tick then runs the volume slide, the volume envelope, the period slide and
// the pitch envelope.

#include "ps_internal.h"

// The period a note or slide byte stands for: the byte in 1/8 semitones, converted at once when the voice takes direct
// periods.

static uint16_t period_of(const PshVoice* v, uint8_t note) {
    const uint16_t units = (uint16_t)(note << 3);
    return v->direct_period ? psh_note_period(v, units, false) : units;
}

// The envelope the byte selects from a table of offsets (the byte is the table position in bytes).

static int64_t envelope_offset(const PshModule* m, int32_t table, uint8_t position) {
    return (int16_t)psh_u16(m, (int64_t)table + position);
}

// A slide command: two note bytes and a duration, then the note starts.

static void read_slide(PshPlayer* p, uint32_t channel) {
    const PshModule* m = &p->module;
    PshVoice* v = &p->voices[channel];
    const uint16_t start = period_of(v, psh_u8(m, v->stream++));
    const uint16_t target = period_of(v, psh_u8(m, v->stream++));
    psh_read_duration(m, v);
    psh_start_slide(v, start, target, v->duration);
}

// The pattern bytes up to the next note or rest. False when the voice stopped.

static bool read_commands(PshPlayer* p, uint32_t channel) {
    const PshModule* m = &p->module;
    PshVoice* v = &p->voices[channel];
    for (int guard = 0; guard < PSH_COMMAND_LIMIT; guard++) {
        switch ((PshPatternCommand)psh_u8(m, v->stream++)) {
            case PshPatternCommand_Note: {
                const uint8_t note = psh_u8(m, v->stream++);
                v->period = period_of(v, note);
                psh_read_duration(m, v);
                v->period_slide = 0;
                v->display_note = psh_display_note(note);
                psh_start_note(p, channel);
                return true;
            }
            case PshPatternCommand_Sample: {
                const uint8_t number = psh_u8(m, v->stream++);
                const PshSample* sample = psh_sample_number(m, number);
                if (sample == nullptr) {
                    break; // the static walk refuses a song with such a number
                }
                psh_select_sample(v, sample);
                v->display_instrument = number;
                break;
            }
            case PshPatternCommand_VolumeOverride:
                v->volume_override = psh_u8(m, v->stream++);
                break;
            case PshPatternCommand_NextPattern:
            case PshPatternCommand_NextPatternAlias:
                if (!psh_next_pattern(p, channel)) {
                    return false;
                }
                break;
            case PshPatternCommand_Rest:
                v->releasing = true;
                psh_read_duration(m, v);
                return true;
            case PshPatternCommand_DirectPitchEnvelope:
                v->direct_period = true;
                psh_select_pitch_envelope(m, v, envelope_offset(m, m->pitch_envelopes, psh_u8(m, v->stream++)));
                break;
            case PshPatternCommand_NotePitchEnvelope:
                v->direct_period = false;
                psh_select_pitch_envelope(m, v, envelope_offset(m, m->pitch_envelopes, psh_u8(m, v->stream++)));
                break;
            case PshPatternCommand_VolumeEnvelope:
                psh_select_volume_envelope(m, v, envelope_offset(m, m->volume_envelopes, psh_u8(m, v->stream++)));
                break;
            case PshPatternCommand_Slide:
                read_slide(p, channel);
                psh_start_note(p, channel);
                return true;
            case PshPatternCommand_RestartOrderList:
                if (!psh_restart_order_list(p, channel)) {
                    return false;
                }
                break;
            case PshPatternCommand_Stop:
                return psh_stop_song(p, channel);
            default:
                psh_stop_voice(v);
                return false;
        }
    }
    psh_stop_voice(v);
    return false;
}

// One tick of a voice. The volume slide runs first, then the pattern when the note has run out, then the level and the
// period move on and the registers are written.

void psh_step_voice(PshPlayer* p, uint32_t channel) {
    const PshModule* m = &p->module;
    PshVoice* v = &p->voices[channel];
    if (!v->active) {
        return;
    }
    psh_volume_slide(v);
    v->duration--;
    if (v->duration == 0 && !read_commands(p, channel)) {
        return;
    }
    if (v->releasing) {
        psh_release_level(v);
    } else {
        psh_volume_envelope(m, v);
    }
    psh_add_period_slide(v);
    psh_pitch_envelope(m, v);
    psh_write_registers(p, channel, v->direct_period ? v->period : psh_note_period(v, v->period, false));
}
