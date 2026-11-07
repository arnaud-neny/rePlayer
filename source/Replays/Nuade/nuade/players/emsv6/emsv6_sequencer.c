// SPDX-License-Identifier: MIT
// EMS v6 player: the sequencer of the eagleplayer. 32 tracks read a track list (pattern numbers, repeats, transposes)
// and the patterns (notes, instrument changes, glides, envelopes, vibrato, arpeggios, wavetable sweeps) and drive the
// mixer voice of the same number. It runs once per sequencer tick, 50 times per second by default.

#include "emsv6_internal.h"
#include "nuade_base.h"

#define DEFAULT_SPEED 6
#define TEMPO_MIN 0x20 // a speed command below this sets ticks per row, from here on beats per minute
// A track that reads more list words and pattern commands than this in one go is cut (the replayer would hang).
#define MAX_COMMANDS 4096
#define INSTRUMENT_MASK 0x3F
#define FINETUNE_MASK 0x0F

// Words of the track list: a non-negative word is a pattern number, these are commands (the high byte for the
// parameterised ones).
typedef enum ListWord {
    ListWord_End = 0xFFFE,
    ListWord_Loop = 0xFFFF, // followed by the list word to continue at
} ListWord;

typedef enum ListCommand {
    ListCommand_Transpose = 0xFD,
    ListCommand_InstrumentShift = 0xFC,
    ListCommand_Repeat = 0xFB,
    ListCommand_MasterFade = 0xFA,
} ListCommand;

// Bytes of a pattern: below 0xEF a note, from there on commands.
typedef enum PatternCode {
    PatternCode_Finetune = 0xEF,
    PatternCode_SampleStartOff = 0xF0, // notes do not restart the sample
    PatternCode_SampleStartOn = 0xF1,
    PatternCode_Wavetable = 0xF2,
    PatternCode_Retrigger = 0xF3,
    PatternCode_Skip = 0xF4,
    PatternCode_Cut = 0xF5,
    PatternCode_Tie = 0xF6,
    PatternCode_Envelope = 0xF7,
    PatternCode_Vibrato = 0xF8,
    PatternCode_Arpeggio = 0xF9,
    PatternCode_GlideHold = 0xFA, // a glide that does not restart the sample
    PatternCode_Glide = 0xFB,
    PatternCode_Tempo = 0xFC,
    PatternCode_Speed = 0xFD,
    PatternCode_Instrument = 0xFE,
    PatternCode_End = 0xFF,
} PatternCode;

typedef struct Context {
    Ems6Sequencer* seq;
    Ems6Mixer* mx;
    const Ems6Module* m;
} Context;

typedef enum NoteKind {
    NoteKind_Plain,     // starts the sample (a glide too)
    NoteKind_GlideHold, // glides without starting the sample
} NoteKind;

static void update_track(Context* c, int index);

// The envelope and the master fade compare their levels as signed bytes.

static bool signed_less(uint8_t a, uint8_t b) {
    return (int8_t)a < (int8_t)b;
}

// Ticks a note sounds: the track's speed times the global speed, less the cut.

static uint16_t note_length(const Ems6Track* t, const Ems6Sequencer* seq) {
    const uint32_t ticks = (uint16_t)((t->speed == 0 ? 0x100 : t->speed) * seq->speed);
    const int16_t left = (int16_t)(uint16_t)(ticks - t->cut_ticks);
    return left < 0 ? 0 : (uint16_t)left;
}

// The master volume moves one step towards its target every `speed + 1` ticks.

static void fade_master(Context* c) {
    Ems6Sequencer* s = c->seq;
    if (s->master_volume != s->master_target) {
        if (s->master_timer != 0) {
            s->master_timer--;
        } else {
            s->master_timer = s->master_speed;
            if (signed_less(s->master_volume, s->master_target)) {
                s->master_volume++;
            } else {
                s->master_volume--;
            }
        }
    }
    c->mx->master_volume = s->master_volume;
}

// Start the instrument's sample on the mixer voice, then run the track's per-tick update.

static void start_sample(Context* c, int index) {
    Ems6Track* t = &c->seq->tracks[index];
    Ems6MixVoice* v = &c->mx->voices[index];
    if (!t->retrigger_sample) {
        update_track(c, index);
        return;
    }
    v->stopped = false;
    const E6Sample sample = e6_module_sample(c->m, t->instrument);
    t->wavetable = sample.kind == E6SampleKind_Wavetable;
    v->pointer = sample.data;
    v->remaining = (int32_t)sample.length;
    v->position = 0;
    v->looping = sample.kind != E6SampleKind_Raw;
    if (sample.kind != E6SampleKind_Raw) {
        v->loop_pointer = sample.data + sample.loop_start;
        v->loop_length = sample.loop_length;
    }
    if (t->wavetable) {
        t->wave_size = (uint8_t)sample.loop_length;
        t->wave_down = false;
        t->wave_pos = t->wave_start;
    }
    update_track(c, index);
}

// The retrigger command: every few ticks the note's sample starts again.

static void run_retrigger(Context* c, int index) {
    Ems6Track* t = &c->seq->tracks[index];
    if (t->retrigger_count == 0) {
        return;
    }
    if (t->retrigger_timer != 0) {
        t->retrigger_timer--;
        return;
    }
    t->retrigger_timer = t->retrigger_reload;
    t->retrigger_count--;
    start_sample(c, index);
}

// The wavetable sweep: the voice's loop is the wave at the current position, which bounces between two limits.

static void sweep_wavetable(Context* c, Ems6Track* t, Ems6MixVoice* v) {
    v->loop_pointer = e6_module_wave_data(c->m, t->instrument) + (uint32_t)t->wave_size * t->wave_pos;
    if (!t->wave_down) {
        int pos = t->wave_pos + t->wave_step;
        t->wave_pos = (uint8_t)pos;
        if (pos >= t->wave_max) {
            t->wave_down = true;
            t->wave_pos = (uint8_t)(pos - t->wave_step);
        }
        return;
    }
    const int pos = t->wave_pos - t->wave_step;
    if (pos < 0) {
        t->wave_down = false;
        t->wave_pos = (uint8_t)(pos + t->wave_step);
        return;
    }
    t->wave_pos = (uint8_t)pos;
    if (pos <= t->wave_min) {
        t->wave_down = false;
        t->wave_pos = (uint8_t)(pos + t->wave_step);
    }
}

// The glide: the period moves by 1 / divisor of the distance between the periods of the start and the target note per
// tick, for `glide_steps` ticks, then lands on the target.

static void glide_tick(Ems6Track* t, Ems6MixVoice* v) {
    const uint16_t start = e6_period_table(t->finetune, t->note_work);
    const uint16_t target = e6_period_table(t->finetune, t->glide_target);
    if (start != target) {
        const uint16_t distance = (uint16_t)(start > target ? start - target : target - start);
        const uint16_t step = (uint16_t)(distance / (t->glide_divisor == 0 ? 1 : t->glide_divisor));
        v->period = (uint16_t)(start > target ? v->period - step : v->period + step);
        t->glide_steps--;
        if (t->glide_steps != 0) {
            return;
        }
    }
    t->note = t->glide_target;
    v->period = target;
    t->glide_steps = 0;
}

// The arpeggio: a list of note offsets, 0xFF jumps to the list index that follows it, 0xFE ends it. Returns false at
// the end, where the vibrato takes over.

static bool arpeggio_tick(Context* c, Ems6Track* t, Ems6MixVoice* v) {
    uint8_t pos = t->arpeggio_pos;
    uint8_t offset = e6_module_arpeggio_entry(c->m, t->arpeggio_table, pos);
    if (offset == 0xFF) {
        pos = e6_module_arpeggio_entry(c->m, t->arpeggio_table, (uint32_t)pos + 1);
        offset = e6_module_arpeggio_entry(c->m, t->arpeggio_table, pos);
    }
    if (offset == 0xFE) {
        return false;
    }
    v->period = e6_period_table(t->finetune, (uint8_t)(offset + t->note));
    t->arpeggio_pos = (uint8_t)(pos + 1);
    return true;
}

// The vibrato: the period steps up for `depth` ticks, down twice as far for `depth` ticks, up again for `depth` ticks
// and starts over.

static void vibrato_tick(Ems6Track* t, Ems6MixVoice* v) {
    if (t->vibrato_depth == 0) {
        return;
    }
    if (t->vibrato_delay != 0) {
        t->vibrato_delay--;
        return;
    }
    const int index = t->note & E6_NOTE_MASK;
    const uint16_t here = e6_period_table(t->finetune, index);
    const uint16_t below = e6_period_table(t->finetune, index - 1);
    const uint16_t step = (uint16_t)((uint16_t)(below - here) / (t->vibrato_divisor == 0 ? 1 : t->vibrato_divisor));
    const int8_t depth = t->vibrato_depth;
    const int8_t depth2 = (int8_t)(depth + depth);
    const int8_t depth3 = (int8_t)(depth2 + depth);
    if (depth > t->vibrato_phase) {
        v->period = (uint16_t)(v->period + step);
    } else if (depth2 > t->vibrato_phase) {
        v->period = (uint16_t)(v->period - step - step);
    } else if (depth3 > t->vibrato_phase) {
        v->period = (uint16_t)(v->period + step);
    } else {
        t->vibrato_phase = 0;
        v->period = here;
        return;
    }
    t->vibrato_phase++;
}

// The volume envelope moves the level towards its target by `envelope_step` every `envelope_reload + 1` ticks.

static void envelope_tick(Ems6Track* t) {
    if (t->envelope_timer != 0) {
        t->envelope_timer--;
        return;
    }
    t->envelope_timer = t->envelope_reload;
    const uint8_t target = t->volume_target;
    if (target == t->volume) {
        return;
    }
    if (target < t->volume) {
        const uint8_t level = (uint8_t)(t->volume - t->envelope_step);
        if (!signed_less(level, 0) && signed_less(target, level)) {
            t->volume = level;
            return;
        }
    } else {
        const uint8_t level = (uint8_t)(t->volume + t->envelope_step);
        if (signed_less(level, target)) {
            t->volume = level;
            return;
        }
    }
    t->volume = target;
}

// The per-tick work of a track: the wavetable sweep, the pitch (glide, arpeggio or vibrato), the envelope, the
// retrigger timer and the release at the end of the note. Writes the mixer voice's period and volume.

static void update_track(Context* c, int index) {
    Ems6Track* t = &c->seq->tracks[index];
    Ems6MixVoice* v = &c->mx->voices[index];
    if (t->wavetable) {
        sweep_wavetable(c, t, v);
    }
    if (t->glide_steps != 0) {
        if (t->glide_delay != 0) {
            t->glide_delay--;
        } else {
            glide_tick(t, v);
        }
    } else if (!(t->arpeggio_on && arpeggio_tick(c, t, v))) {
        vibrato_tick(t, v);
    }
    envelope_tick(t);
    run_retrigger(c, index);
    if (t->note_length != 0) {
        t->note_length--;
        v->volume = t->volume;
        return;
    }
    // After the note the volume falls by the release rate; a byte that goes negative silences the voice.
    const uint8_t released = (uint8_t)(v->volume - t->release_rate);
    v->volume = (int8_t)released < 0 ? 0 : released;
}

// A note: the pattern position advances (unless the skip count holds it), the note sets the period and the volume, the
// vibrato restarts, and the sample starts unless the note only continues a glide.

static void play_note(Context* c, int index, uint8_t note, uint32_t next_pos, NoteKind kind) {
    Ems6Track* t = &c->seq->tracks[index];
    Ems6MixVoice* v = &c->mx->voices[index];
    t->row_countdown = t->speed;
    if (t->skip != 0) {
        t->skip--;
    } else {
        next_pos++;
    }
    t->pattern_pos = (uint16_t)next_pos;
    note = (uint8_t)(note + t->transpose);
    t->note_work = note;
    t->note = note;
    v->period = e6_period_table(t->finetune, note & E6_NOTE_MASK);
    v->volume = t->volume;
    t->note_length = note_length(t, c->seq);
    t->vibrato_depth = t->vibrato_depth_set;
    t->vibrato_divisor = t->vibrato_divisor_set;
    t->vibrato_delay = t->vibrato_delay_set;
    t->arpeggio_pos = 0;
    t->vibrato_phase = 0;
    t->note_starts++;
    if (kind == NoteKind_GlideHold) {
        update_track(c, index);
    } else {
        start_sample(c, index);
    }
}

// The speed command: below TEMPO_MIN the ticks per row, else beats per minute (the length of a tick in samples).

static void set_speed(Context* c, uint8_t value) {
    if (value < TEMPO_MIN) {
        c->seq->speed = value;
        c->seq->speed_countdown = value;
    } else {
        c->mx->seq_tick_samples = e6_mixer_ticks_for_bpm(value);
    }
}

// Read the pattern commands of a track up to its next note, tie or the end of the pattern; each command spends one of
// `budget`. Returns false at the end of the pattern (or when the budget is spent), which makes the track read its
// list again.

static bool read_pattern(Context* c, int index, uint32_t pattern, int* budget) {
    Ems6Track* t = &c->seq->tracks[index];
    Ems6MixVoice* v = &c->mx->voices[index];
    uint32_t pos = t->pattern_pos;
    while ((*budget)-- > 0) {
        const PatternCommand cmd = e6_module_pattern_command(c->m, pattern + pos);
        const uint8_t arg1 = cmd.args[0];
        const uint8_t arg2 = cmd.args[1];
        const uint8_t arg3 = cmd.args[2];
        const uint8_t arg4 = cmd.args[3];
        switch (cmd.code) {
            case PatternCode_End:
                t->pattern_pos = 0;
                if (t->repeat == 0) {
                    t->list_pos = (uint16_t)(t->list_pos + 2);
                } else {
                    t->repeat--;
                }
                return false;
            case PatternCode_Instrument:
                t->instrument = (uint8_t)(arg1 + t->instrument_shift) & INSTRUMENT_MASK;
                pos += 2;
                continue;
            case PatternCode_Speed:
                t->speed = arg1;
                pos += 2;
                continue;
            case PatternCode_Glide:
            case PatternCode_GlideHold:
                t->glide_divisor = arg2;
                t->glide_steps = arg2;
                t->glide_delay = arg3;
                t->glide_target = (uint8_t)(arg4 + t->transpose);
                play_note(c, index, arg1, pos + 4, cmd.code == PatternCode_Glide ? NoteKind_Plain : NoteKind_GlideHold);
                return true;
            case PatternCode_Envelope:
                t->volume = arg1;
                t->volume_target = arg2;
                t->envelope_step = arg3;
                t->envelope_reload = arg4;
                t->envelope_timer = arg4;
                pos += 5;
                continue;
            case PatternCode_Tempo:
                set_speed(c, arg1);
                pos += 2;
                continue;
            case PatternCode_Arpeggio:
                t->arpeggio_table = arg1;
                t->arpeggio_on = true;
                pos += 2;
                continue;
            case PatternCode_Vibrato:
                t->vibrato_divisor_set = arg1;
                t->vibrato_depth_set = (int8_t)arg2;
                t->vibrato_delay_set = arg3;
                t->arpeggio_on = false;
                pos += 4;
                continue;
            case PatternCode_Tie:
                t->row_countdown = t->speed;
                t->pattern_pos = (uint16_t)(pos + 1);
                v->stopped = false;
                t->note_length = note_length(t, c->seq);
                update_track(c, index);
                return true;
            case PatternCode_Cut:
                t->cut_ticks = arg1;
                t->release_rate = arg2;
                pos += 3;
                continue;
            case PatternCode_Skip:
                t->skip = (uint8_t)(arg1 - 1);
                pos += 2;
                continue;
            case PatternCode_Retrigger:
                t->retrigger_timer = arg1;
                t->retrigger_reload = arg1;
                t->retrigger_count = (uint8_t)(arg2 - 1);
                pos += 3;
                continue;
            case PatternCode_Wavetable:
                t->wave_min = arg1;
                t->wave_max = arg2;
                t->wave_start = arg3;
                t->wave_step = arg4;
                pos += 5;
                continue;
            case PatternCode_SampleStartOff:
            case PatternCode_SampleStartOn:
                t->retrigger_sample = cmd.code == PatternCode_SampleStartOn;
                pos += 1;
                continue;
            case PatternCode_Finetune:
                t->finetune = arg1 & FINETUNE_MASK;
                pos += 2;
                continue;
            default:
                play_note(c, index, cmd.code, pos, NoteKind_Plain);
                return true;
        }
    }
    return false;
}

// A track's next event: the commands of its list up to the next pattern number, then that pattern's commands up to a
// note. The list loops (0xFFFF + position / 2), ends (0xFFFE) or changes transpose, instrument shift, repeat count and
// the master fade on the way. List words and pattern commands share one budget.

static void read_track(Context* c, int index) {
    Ems6Track* t = &c->seq->tracks[index];
    Ems6MixVoice* v = &c->mx->voices[index];
    const Ems6Module* m = c->m;
    int budget = MAX_COMMANDS;
    while (budget > 0) {
        uint32_t list_pos = t->list_pos;
        uint16_t word = e6_module_u16(m, t->track_list + list_pos);
        while ((int16_t)word < 0 && budget-- > 0) {
            const uint8_t arg = (uint8_t)word;
            if (word == ListWord_Loop) {
                t->looped = true;
                list_pos = (uint16_t)(e6_module_u16(m, t->track_list + list_pos + 2) * 2);
            } else if (word == ListWord_End) {
                t->ended = true;
                v->volume = 0;
                v->stopped = true;
                return;
            } else if ((word >> 8) == ListCommand_Transpose) {
                t->transpose = arg;
                list_pos += 2;
            } else if ((word >> 8) == ListCommand_InstrumentShift) {
                t->instrument_shift = arg;
                list_pos += 2;
            } else if ((word >> 8) == ListCommand_Repeat) {
                t->repeat = (uint8_t)(arg - 1);
                list_pos += 2;
            } else if ((word >> 8) == ListCommand_MasterFade) {
                Ems6Sequencer* s = c->seq;
                s->master_volume = arg;
                c->mx->master_volume = arg;
                const uint16_t fade = e6_module_u16(m, t->track_list + list_pos + 2);
                s->master_target = (uint8_t)(fade >> 8);
                s->master_speed = (uint8_t)fade;
                s->master_timer = s->master_speed;
                list_pos += 4;
            } else {
                break; // any other negative word is taken as a (huge) pattern number
            }
            word = e6_module_u16(m, t->track_list + list_pos);
        }
        if (budget <= 0) {
            return;
        }
        t->list_pos = (uint16_t)list_pos;
        t->glide_steps = 0;
        t->retrigger_count = 0;
        t->retrigger_reload = 0;
        t->retrigger_timer = 0;
        if (read_pattern(c, index, e6_module_pattern(m, word), &budget)) {
            return;
        }
    }
}

// The sequencer tick. Once per `speed` ticks every active track counts its row down and reads its next event; on the
// other ticks the tracks only run their per-tick update.

void e6_sequencer_tick(Ems6Sequencer* s, Ems6Mixer* mx, const Ems6Module* m) {
    Context c = { .seq = s, .mx = mx, .m = m };
    fade_master(&c);
    s->speed_countdown--;
    if (s->speed_countdown != 0) {
        for (int i = 0; i < E6_TRACKS; i++) {
            update_track(&c, i);
        }
        return;
    }
    s->speed_countdown = s->speed;
    for (int i = 0; i < E6_TRACKS; i++) {
        Ems6Track* t = &s->tracks[i];
        if (t->ended) {
            continue;
        }
        t->row_countdown--;
        if (t->row_countdown == 0) {
            read_track(&c, i);
        } else {
            update_track(&c, i);
        }
    }
}

// InitSound: every track starts at the beginning of its list, every note restarts its sample.

void e6_sequencer_init(Ems6Sequencer* s, const Ems6Subsong* subsong) {
    *s = (Ems6Sequencer) {
        .master_volume = E6_MASTER_FULL, .master_target = E6_MASTER_FULL, .speed = DEFAULT_SPEED, .speed_countdown = 1
    };
    for (int i = 0; i < E6_TRACKS; i++) {
        s->tracks[i] = (Ems6Track) {
            .speed = 1, .row_countdown = 1, .retrigger_sample = true, .track_list = subsong->track_lists[i]
        };
    }
}

// The song is over when every track has jumped back or ended: the eagleplayer's SongEnd.

bool e6_sequencer_finished(const Ems6Sequencer* s) {
    for (int i = 0; i < E6_TRACKS; i++) {
        if (!s->tracks[i].looped && !s->tracks[i].ended) {
            return false;
        }
    }
    return true;
}

uint32_t e6_sequencer_used_tracks(const Ems6Module* m, const Ems6Subsong* subsong) {
    uint32_t used = 1;
    for (uint32_t i = 0; i < E6_TRACKS; i++) {
        if (e6_module_u16(m, subsong->track_lists[i]) != ListWord_End) {
            used = i + 1;
        }
    }
    return used;
}
