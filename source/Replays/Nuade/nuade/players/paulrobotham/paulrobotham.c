// SPDX-License-Identifier: MIT
// Paul Robotham: the replayer of the eagleplayer "PaulRobotham", one tick per call.
//
// What the eagleplayer's glue does that matters here: the interrupt runs the replayer, then compares the first voice's
// stream position with the end of its stream (SongEnd) and starts the song again. Its volume patch scales by 64/64 and
// all four voices are enabled, so the volume registers get the replayer's value unchanged.

#include "pr_internal.h"
#include <string.h>

#define INITIAL_TEMPO 1000
#define TEMPO_DIVISOR 10800 // note durations are (value * tempo) / 10800 ticks, the remainder carried on
#define INITIAL_DURATION 10 // ticks before the first note
#define INITIAL_ENVELOPE_POS 3
#define MAX_VOLUME 63
#define VIBRATO_SCALE 10000
#define NOTE_UNIT_MASK 0x1F // a duration byte: bits 0..4 are the length, bits 5..7 the shift
#define NOTE_SHIFT 5
#define SILENT_LOOP_WORDS 8
#define SILENT_LOOP_PERIOD 0x100
#define MAX_EVENTS_PER_TICK 0x30000 // a stream of commands only would loop for ever
#define MAX_ENVELOPE_JUMPS 256      // an envelope table of jumps that never reaches a level
#define ENVELOPE_STEP 4
#define SONG_START_MASTER 0x3F

// The loop of an instrument without one: 16 bytes of the replayer's own zeroed data (UADE: chip RAM just behind the
// replayer's hunk), so it plays silence.
static const int8_t silent_loop[SILENT_LOOP_WORDS * 2] = { 0 };

static const uint16_t period_table[PR_PERIOD_COUNT] = {
    0x358, 0x328, 0x2FA, 0x2D0, 0x2A6, 0x280, 0x25C, 0x23A, 0x21A, 0x1FC, 0x1E0, 0x1C4, 0x1AC, 0x194, 0x17D,
    0x168, 0x153, 0x140, 0x12E, 0x11D, 0x10D, 0x0FE, 0x0F0, 0x0E2, 0x0D6, 0x0CA, 0x0BE, 0x0B4, 0x0A9, 0x0A0,
    0x097, 0x08E, 0x086, 0x07F, 0x078, 0x071, 0x06B, 0x065, 0x05F, 0x05A, 0x054, 0x050, 0x04B, 0x047, 0x043,
    0x03F, 0x03C, 0x038, 0x035, 0x032, 0x02F, 0x02D, 0x02A, 0x028, 0x025, 0x023, 0x021, 0x01F, 0x01E, 0x01C,
};

// InitSound's per-voice part: the fields it writes. The others (periods, the repeat, the fade) keep their values when
// the song starts again.

static void voice_init(const PrSong* song, PrVoice* v, uint32_t index) {
    v->stream = pr_song_stream(song, index);
    v->duration = INITIAL_DURATION;
    v->duration_rest = 0;
    v->vibrato_pos = 0;
    v->envelope_pos = INITIAL_ENVELOPE_POS;
    v->instrument = 0;
    v->vibrato_table = pr_song_vibrato(song, 0);
    v->envelope_table = pr_song_envelope(song, 0);
    v->volume = 0;
    v->vibrato_speed = 1;
    v->vibrato_divisor = VIBRATO_SCALE;
    v->portamento_step = 0;
    v->portamento = false;
    v->vibrato = false;
    v->fade = false;
    v->tie = false;
    v->ended = false;
    v->legato = false;
    v->keep_envelope = false;
    v->keep_vibrato = false;
    v->instrument_changed = false;
    v->loop_depth = 0;
}

static void song_restart(PrPlayer* player) {
    for (uint32_t i = 0; i < player->song.voice_count; i++) {
        voice_init(&player->song, &player->voices[i], i);
    }
    player->master_volume = SONG_START_MASTER;
    player->master_target = SONG_START_MASTER;
}

static void player_reset(PrPlayer* player) {
    memset(player->voices, 0, sizeof(player->voices));
    player->tempo = INITIAL_TEMPO;
    player->master_step = 0;
    player->master_counter = 0;
    player->master_period = 1;
    song_restart(player);
}

bool pr_start(PrPlayer* player, NuadeVoices* voices) {
    (void)voices;
    player_reset(player);
    return true;
}

// The silent loop stands in for an instrument with a loop length of 0.

static void voice_set_repeat(const PrSong* song, PrVoice* v, const PrInstrument* in, uint16_t period) {
    if (in->loop_length_words == 0) {
        v->loop_lc = silent_loop;
        v->loop_len_words = SILENT_LOOP_WORDS;
    } else {
        v->loop_lc = pr_song_sample(song, in->loop);
        v->loop_len_words = in->loop_length_words;
    }
    v->loop_period = period;
}

// A note on a voice that is not in legato mode: restart the channel with the attack of the instrument. DMA stays off
// until the next tick, then the repeat is written one tick after that.

static void note_start(const PrSong* song, PrVoice* v, uint8_t note) {
    const PrInstrument in = pr_song_instrument(song, v->instrument);
    const uint16_t period = period_table[note];
    v->instrument_changed = false;
    nuade_paula_channel_dma_off(&v->paula);
    v->dma_running = false;
    v->fade_volume = 0;
    v->fade = false;
    v->paula.lc = pr_song_sample(song, in.sample);
    v->paula.len_words = in.length_words;
    v->paula.period = period;
    v->target_period = period;
    voice_set_repeat(song, v, &in, period);
}

// A legato note after an instrument command: the new instrument starts at its repeat, with the volume at 0.

static void note_start_at_repeat(const PrSong* song, PrVoice* v, uint8_t note) {
    const PrInstrument in = pr_song_instrument(song, v->instrument);
    const uint16_t period = in.loop_length_words == 0 ? SILENT_LOOP_PERIOD : period_table[note];
    voice_set_repeat(song, v, &in, period);
    v->paula.volume = 0;
    nuade_paula_channel_dma_off(&v->paula);
    v->dma_running = false;
    v->paula.lc = v->loop_lc;
    v->paula.len_words = v->loop_len_words;
    v->paula.period = period;
    v->target_period = period;
    v->instrument_changed = false;
}

// A duration byte: length << shift in units of the tempo, as a number of ticks plus a remainder.

static void duration_set(const PrPlayer* player, PrVoice* v, uint8_t byte) {
    const uint32_t units = (uint32_t)(byte & NOTE_UNIT_MASK) << (byte >> NOTE_SHIFT);
    const uint32_t total = units * player->tempo + v->duration_rest;
    v->duration = (uint16_t)(total / TEMPO_DIVISOR);
    v->duration_rest = (uint16_t)(total % TEMPO_DIVISOR);
}

typedef enum PrFlow {
    PrFlow_Ended,   // the stream ended: nothing else happens on this voice this tick
    PrFlow_Pitch,   // go on with portamento, vibrato and the volume
    PrFlow_Staging, // the note goes on: finish the DMA start of the earlier note first
} PrFlow;

static PrFlow note_event(const PrPlayer* player, PrVoice* v, uint8_t note, uint32_t pos) {
    const PrSong* song = &player->song;
    bool retrigger = true;
    v->tie = note == PR_STREAM_TIE;
    if (!v->tie) {
        if (!v->legato) {
            note_start(song, v, note);
        } else if (v->instrument_changed) {
            note_start_at_repeat(song, v, note);
        } else {
            // Legato: only the pitch changes, the sample plays on.
            v->target_period = period_table[note];
            v->paula.period = v->target_period;
            retrigger = false;
        }
    }
    duration_set(player, v, pr_song_u8(song, pos++));
    if (!v->tie) {
        if (!v->keep_vibrato) {
            v->vibrato_pos = 0;
        }
        if (!v->keep_envelope) {
            v->envelope_pos = INITIAL_ENVELOPE_POS;
        }
    }
    v->stream = pos;
    if (v->tie) {
        return PrFlow_Staging;
    }
    v->start_pending = v->start_pending || retrigger;
    return PrFlow_Pitch;
}

static void voice_end(PrVoice* v) {
    v->ended = true;
    nuade_paula_channel_dma_off(&v->paula);
    v->dma_running = false;
}

// The master fade command: a target volume and the ticks per step (0 jumps to the target).

static void master_fade(PrPlayer* player, uint8_t target, uint8_t period) {
    player->master_target = target;
    player->master_step = target > player->master_volume ? 1 : -1;
    if (period == 0) {
        player->master_step = 0;
        player->master_volume = player->master_target;
    }
    player->master_period = period;
}

// Runs a command; `pos` is behind the command byte and ends up behind its arguments. False when the voice has to stop.

static bool command_run(PrPlayer* player, PrVoice* v, uint32_t command, uint32_t* pos) {
    const PrSong* song = &player->song;
    uint8_t arg[2] = { 0, 0 };
    const uint32_t arg_count = pr_command_arg_count(command);
    for (uint32_t i = 0; i < arg_count; i++) {
        arg[i] = pr_song_u8(song, (*pos)++);
    }
    switch (command) {
        case PrCommand_LoopStart:
            if (v->loop_depth >= PR_LOOP_DEPTH) {
                return false;
            }
            v->loops[v->loop_depth++] = (PrLoop) { .start = *pos, .count = arg[0] };
            break;
        case PrCommand_LoopEnd: {
            if (v->loop_depth == 0) {
                return false;
            }
            PrLoop* loop = &v->loops[v->loop_depth - 1];
            if (--loop->count != 0) {
                *pos = loop->start;
            } else {
                v->loop_depth--;
            }
            break;
        }
        case PrCommand_VibratoDepth:
            v->vibrato = arg[0] != 0;
            if (v->vibrato) {
                v->vibrato_divisor = (uint16_t)(VIBRATO_SCALE / arg[0]);
            }
            break;
        case PrCommand_VibratoSpeed:
            v->vibrato_speed = arg[0];
            break;
        case PrCommand_Portamento:
            v->portamento = arg[0] != 0;
            if (v->portamento) {
                v->portamento_step = (uint16_t)(arg[0] * 4);
            }
            break;
        case PrCommand_VibratoTable:
            v->vibrato_table = pr_song_vibrato(song, arg[0]);
            break;
        case PrCommand_EnvelopeTable:
            v->envelope_table = pr_song_envelope(song, arg[0]);
            break;
        case PrCommand_Volume:
            v->volume = arg[0];
            break;
        case PrCommand_Tempo:
            player->tempo = (uint16_t)(arg[0] << 4);
            break;
        case PrCommand_Instrument:
            v->instrument = arg[0];
            v->legato = false;
            v->instrument_changed = true;
            break;
        case PrCommand_Options:
            v->legato = (arg[0] & 1) != 0;
            v->keep_envelope = (arg[0] & 2) != 0;
            v->keep_vibrato = (arg[0] & 4) != 0;
            break;
        case PrCommand_MasterFade:
            master_fade(player, arg[0], arg[1]);
            break;
        case PrCommand_FadeStart:
            v->fade_volume = v->volume;
            v->fade = true;
            break;
        case PrCommand_FadeStep:
            v->fade_step = arg[0];
            break;
        default: // arguments that nothing reads, the LED, and the numbers that only flash the screen
            break;
    }
    return true;
}

// Reads stream bytes until a note or the end of the stream.

static PrFlow events_read(PrPlayer* player, PrVoice* v) {
    const PrSong* song = &player->song;
    uint32_t pos = v->stream;
    for (uint32_t events = 0; events < MAX_EVENTS_PER_TICK; events++) {
        const uint8_t b = pr_song_u8(song, pos++);
        if (b == PR_STREAM_END) {
            break;
        }
        if (b < PR_STREAM_COMMAND) {
            if (!pr_stream_note_ok(b)) {
                break; // not a stream the open check lets through
            }
            return note_event(player, v, b, pos);
        }
        if (!command_run(player, v, b & PR_COMMAND_MASK, &pos)) {
            break;
        }
        v->stream = pos;
    }
    voice_end(v);
    return PrFlow_Ended;
}

// The tick after a note start switches the DMA on, the tick after that writes the repeat.

static void dma_stage(PrVoice* v) {
    if (v->start_pending) {
        v->start_pending = false;
        v->loop_pending = true;
        nuade_paula_channel_dma_on(&v->paula);
        v->dma_running = true;
    } else if (v->loop_pending) {
        v->loop_pending = false;
        v->paula.lc = v->loop_lc;
        v->paula.len_words = v->loop_len_words;
        v->paula.period = v->loop_period;
    }
}

// Portamento towards the target period: a step of the voice's size, or the target itself when it is within one step.

static uint16_t portamento_period(const PrVoice* v) {
    if (!v->portamento) {
        return v->target_period;
    }
    int16_t distance = (int16_t)(v->target_period - v->period);
    int16_t step = (int16_t)v->portamento_step;
    if (distance < 0) {
        step = (int16_t)-step;
    }
    const uint16_t moved = (uint16_t)(v->period + step);
    if (step < 0) {
        distance = (int16_t)-distance;
        step = (int16_t)-step;
    }
    return step >= distance ? v->target_period : moved;
}

// The period register: the portamento period plus the vibrato (a signed byte of the table as a fraction of the period).

static void pitch_update(const PrSong* song, PrVoice* v) {
    v->period = portamento_period(v);
    uint16_t out = v->period;
    if (v->vibrato) {
        const int32_t wave = (int8_t)pr_song_u8(song, v->vibrato_table + v->vibrato_pos);
        out = (uint16_t)(out + (int16_t)((int32_t)v->period * wave / v->vibrato_divisor));
    }
    v->paula.period = out;
    v->duration--;
}

// The envelope table holds levels (0..63); a negative byte jumps back by (-byte + 3) & ~3 positions.

static bool envelope_level(const PrSong* song, PrVoice* v, uint8_t* level) {
    for (uint32_t jumps = 0; jumps < MAX_ENVELOPE_JUMPS; jumps++) {
        const int8_t b = (int8_t)pr_song_u8(song, v->envelope_table + v->envelope_pos);
        if (b >= 0) {
            *level = (uint8_t)b & MAX_VOLUME;
            return true;
        }
        v->envelope_pos = (uint8_t)(v->envelope_pos - (((-b) + 3) & ~3));
    }
    return false;
}

// The volume register of a voice whose DMA runs: the voice volume (or the fade-out volume) times the envelope level,
// then the master fade.

static void volume_update(const PrPlayer* player, PrVoice* v) {
    uint32_t volume = 0;
    uint8_t level = 0;
    if (v->volume != 0 && envelope_level(&player->song, v, &level)) {
        uint32_t base = v->volume;
        if (v->fade) {
            if (v->fade_volume != 0) {
                v->fade_volume = (uint8_t)(v->fade_volume - v->fade_step);
                if ((int8_t)v->fade_volume < 0) {
                    v->fade_volume = 0;
                }
            }
            base = v->fade_volume;
        }
        volume = level * (base & MAX_VOLUME) / MAX_VOLUME;
        if (player->master_volume != MAX_VOLUME) {
            volume = (volume * player->master_volume / MAX_VOLUME) & MAX_VOLUME;
        }
    }
    v->paula.volume = (uint8_t)volume;
}

static void voice_tick(PrPlayer* player, PrVoice* v) {
    if (v->ended) {
        return;
    }
    v->vibrato_pos = (uint8_t)(v->vibrato_pos + v->vibrato_speed);
    v->envelope_pos = (uint8_t)(v->envelope_pos + ENVELOPE_STEP);
    PrFlow flow = PrFlow_Staging;
    if (v->duration == 0) {
        flow = events_read(player, v);
        if (flow == PrFlow_Ended) {
            return;
        }
    }
    if (flow == PrFlow_Staging) {
        dma_stage(v);
    }
    pitch_update(&player->song, v);
    if (v->dma_running) {
        volume_update(player, v);
    }
}

// Every master_period ticks the master volume takes one step towards its target.

static void master_fade_tick(PrPlayer* player) {
    const uint16_t counter = (uint16_t)(player->master_counter + 1);
    if (counter < player->master_period) {
        player->master_counter = counter;
        return;
    }
    player->master_counter = 0;
    if (player->master_step == 0) {
        return;
    }
    if (player->master_volume == player->master_target) {
        player->master_step = 0;
    } else {
        player->master_volume = (uint16_t)(player->master_volume + player->master_step);
    }
}

PrTickResult pr_tick(PrPlayer* player, NuadeVoices* voices) {
    for (uint32_t i = 0; i < player->song.voice_count; i++) {
        voice_tick(player, &player->voices[i]);
    }
    master_fade_tick(player);
    PrTickResult result = { 0 };
    if (player->voices[0].stream == pr_song_end_marker(&player->song)) {
        result.looped = true;
        song_restart(player);
    }
    for (uint32_t i = 0; i < PR_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
    }
    return result;
}

uint32_t pr_voice_count(const PrPlayer* player) {
    return player->song.voice_count;
}

PrVoiceState pr_voice_state(const PrPlayer* player, uint32_t voice) {
    if (voice >= PR_VOICES) {
        return (PrVoiceState) { .period = 0, .instrument = -1 };
    }
    const PrVoice* v = &player->voices[voice];
    return (PrVoiceState) { .period = v->target_period, .instrument = v->instrument };
}
