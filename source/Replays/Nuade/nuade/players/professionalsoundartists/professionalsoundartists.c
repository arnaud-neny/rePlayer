// SPDX-License-Identifier: MIT
// Professional Sound Artists player: the replayer of the UADE eagleplayer, one function per routine (the label of the
// original in the comment). A tick counts the speed down; on the tick it passes zero every voice reads its next note,
// and on every tick the voices run their effect, the pulse-width LFO and the volume envelope. The interrupt then
// switches on the DMA of the voices that started a note and writes the repeat registers of the sample notes (the
// eagleplayer waits for the DMA start before each of the two).
//
// The replayer keeps its pulse-width waves in a 516-byte chip buffer of its own (the eagleplayer's second hunk: 128
// bytes of -128 and of 127 around offset 0x100, then four zero bytes). The port keeps that buffer and the module in
// one chip image, apart from each other, and refuses a song whose Paula buffer leaves either of the two, whose module
// reads leave the file or whose word accesses are odd. Opening a song plays every subsong through (until it has
// wrapped three times) without sound to find that out: the port does not model what lies around the module and the
// buffer in UADE's memory.

#include <nuade/players/professionalsoundartists.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define PSA_MAGIC 0x50534100u // "PSA\0"
#define PSA_TITLE_OFFSET 4
#define PSA_HEADER_INSTRUMENTS 0x28 // module offsets of the instrument table, the pattern number table, the patterns
#define PSA_HEADER_ORDERS 0x2C
#define PSA_HEADER_PATTERNS 0x30
#define PSA_SUBSONG_TABLE 0x38
#define PSA_SUBSONG_BYTES 8
#define PSA_INSTRUMENT_SHIFT 6 // an instrument is 64 bytes
#define PSA_INSTRUMENT_READ_BYTES 0x22
#define PSA_PATTERN_SHIFT 10 // a pattern is 64 rows of 4 voices x 4 bytes
#define PSA_NOTE_BYTES 4u
#define PSA_DEFAULT_SPEED 6
#define PSA_ROW_LAST 0x3F
#define PSA_NO_LOOP 0xFFFF
#define PSA_ARPEGGIO_STEPS 3
#define PSA_WRAP_HISTORY 64          // wraps of a subsong the dry run compares before it gives up
#define PSA_DRY_RUN_TICKS (1u << 23) // ticks the dry runs of all subsongs of a song share
#define PSA_SUBSONGS_MAX 4096        // the replayer indexes the subsong table with a word (subsong * 8)
#define PSA_SYNTH_WORDS_MAX 128      // longest pulse-width wave: every window then stays in the wave buffer
#define PSA_PAULA_MAX_WORDS 0x10000u // AUDxLEN 0

// The chip image: the replayer's wave buffer and the module, each with a gap of unmapped memory in front of it.
#define PSA_WAVE_BASE 0x400u
#define PSA_WAVE_BYTES 0x204u
#define PSA_WAVE_CENTER 0x100u // the pulse-width waves are built around this offset
#define PSA_WAVE_ZERO 0x200u   // four zero bytes behind the waves: the repeat of a sample that plays once
#define PSA_MODULE_BASE 0x1000u

typedef enum PsaEffect {
    PsaEffect_Arpeggio = 0,
    PsaEffect_SlideUp = 1,
    PsaEffect_SlideDown = 2,
    PsaEffect_TonePortamento = 3,
    PsaEffect_Vibrato = 4,
    PsaEffect_Release = 5,
    PsaEffect_VolumeSlide = 0xA,
    PsaEffect_SetVolume = 0xC,
    PsaEffect_PatternBreak = 0xD,
    PsaEffect_Speed = 0xF,
} PsaEffect;

typedef enum PsaEnvelopeState {
    PsaEnvelope_Off = 0,
    PsaEnvelope_Attack = 1,
    PsaEnvelope_Decay = 2,
    PsaEnvelope_Sustain = 3,
    // every other value releases
} PsaEnvelopeState;

// The period table: 37 periods, then the word behind the table (the last word of the eagleplayer's code hunk), which
// notes 38 reads.
static const uint16_t psa_periods[38] = {
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320, 302,
    285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113, 107, 22356,
};

// The module layout.

typedef struct PsaInstrument {
    uint32_t sample; // module offset
    uint16_t length_words;
    uint32_t loop_start; // module offset, 0: the repeat is silent
    uint16_t loop_words;
    uint16_t volume;
    PsaPwm pwm;
    PsaEnvelope envelope;
} PsaInstrument;

static const uint8_t* module_bytes(const PsaPlayer* player) {
    return player->chip + PSA_MODULE_BASE;
}

static uint8_t module_u8(PsaPlayer* player, int64_t offset) {
    if (offset < 0 || (uint64_t)offset >= player->size) {
        player->fault = true;
        return 0;
    }
    return module_bytes(player)[offset];
}

// A word access of the replayer: an odd address is an address error of the 68000.

static uint16_t module_u16(PsaPlayer* player, int64_t offset) {
    if ((offset & 1) != 0) {
        player->fault = true;
    }
    return (uint16_t)(((unsigned)module_u8(player, offset) << 8) | module_u8(player, offset + 1));
}

static uint32_t module_u32(PsaPlayer* player, int64_t offset) {
    return ((uint32_t)module_u16(player, offset) << 16) | module_u16(player, offset + 2);
}

static PsaSubsong read_subsong(PsaPlayer* player, uint32_t index) {
    const int64_t at = PSA_SUBSONG_TABLE + (int64_t)index * PSA_SUBSONG_BYTES;
    return (PsaSubsong) { .start = module_u16(player, at),
                          .end = module_u16(player, at + 2),
                          .loop = module_u16(player, at + 4) };
}

static PsaNote read_note(PsaPlayer* player, int64_t offset) {
    const uint8_t effect = module_u8(player, offset + 2);
    return (PsaNote) { .note = module_u8(player, offset),
                       .instrument = module_u8(player, offset + 1),
                       .period_only = (effect & 0x80) != 0,
                       .effect = effect & 0x7F,
                       .param = module_u8(player, offset + 3) };
}

// The instrument record: the fields from +0 to +0x21 (the envelope is read as a long at +0x1E).

static PsaInstrument read_instrument(PsaPlayer* player, int64_t at) {
    PsaInstrument ins = { 0 };
    if (at < 0 || (uint64_t)at > player->size || player->size - (uint64_t)at < PSA_INSTRUMENT_READ_BYTES) {
        player->fault = true;
        return ins;
    }
    ins.sample = module_u32(player, at);
    ins.length_words = module_u16(player, at + 4);
    ins.loop_start = module_u32(player, at + 6);
    ins.loop_words = module_u16(player, at + 0xA);
    ins.volume = module_u16(player, at + 0xC);
    ins.pwm = (PsaPwm) { .enabled = module_u8(player, at + 0xE) != 0,
                         .start = module_u8(player, at + 0xF),
                         .up_step = module_u8(player, at + 0x10),
                         .up_delay = module_u8(player, at + 0x11),
                         .up_count = module_u8(player, at + 0x12),
                         .down_step = module_u8(player, at + 0x13),
                         .down_delay = module_u8(player, at + 0x14),
                         .down_count = module_u8(player, at + 0x15) };
    ins.envelope = (PsaEnvelope) { .state = module_u8(player, at + 0x16),
                                   .attack_step = module_u8(player, at + 0x17),
                                   .attack_delay = module_u8(player, at + 0x18),
                                   .attack_count = module_u8(player, at + 0x19),
                                   .decay_step = module_u8(player, at + 0x1A),
                                   .decay_delay = module_u8(player, at + 0x1B),
                                   .decay_count = module_u8(player, at + 0x1C),
                                   .sustain_count = module_u8(player, at + 0x1D),
                                   .release_step = module_u8(player, at + 0x1E),
                                   .release_delay = module_u8(player, at + 0x1F),
                                   .release_count = module_u8(player, at + 0x20) };
    return ins;
}

// The period table; the index the replayer computes from the note and the arpeggio.

static uint16_t period_at(PsaPlayer* player, uint32_t index) {
    if (index >= nuade_sizeof_array(psa_periods)) {
        player->fault = true;
        return 0;
    }
    return psa_periods[index];
}

// Chip memory: an address is an offset into the image. Paula ignores bit 0 of AUDxLC.

static void write_lc(PsaPlayer* player, PsaVoice* voice, int64_t address) {
    if (address < 0 || (uint64_t)address >= PSA_MODULE_BASE + player->size) {
        player->fault = true;
        voice->paula.lc = nullptr;
        return;
    }
    voice->paula.lc = (const int8_t*)player->chip + (address & ~(int64_t)1);
}

// A Paula block must lie wholly in the module or wholly in the wave buffer (AUDxLEN 0 plays 65536 words).

static void check_block(PsaPlayer* player, const int8_t* lc, uint16_t len_words) {
    if (lc == nullptr) {
        player->fault = true;
        return;
    }
    const int64_t at = (const uint8_t*)lc - player->chip;
    const int64_t bytes = (int64_t)(len_words == 0 ? PSA_PAULA_MAX_WORDS : len_words) * 2;
    const bool in_wave = at >= PSA_WAVE_BASE && at + bytes <= PSA_WAVE_BASE + PSA_WAVE_BYTES;
    const bool in_module = at >= PSA_MODULE_BASE && at + bytes <= (int64_t)(PSA_MODULE_BASE + player->size);
    player->fault = player->fault || !(in_wave || in_module);
}

// L342: AUDxVOL. The eagleplayer scales the volume by its master volume and balance, the identity at the defaults.

static void write_volume(PsaVoice* voice, uint16_t volume) {
    voice->paula.volume = (uint8_t)(volume & 0x7F);
}

// L73a: the replayer's SongEnd call.

static void song_end(PsaPlayer* player) {
    player->looped = true;
}

// L892: the song stops: DMA and volume of all four channels off.

static void stop_song(PsaPlayer* player) {
    player->playing = false;
    for (uint32_t i = 0; i < PSA_VOICES; i++) {
        nuade_paula_channel_dma_off(&player->voices[i].paula);
        player->voices[i].paula.volume = 0;
    }
}

// L9aa: the first row of the pattern the pattern number table gives at `position`.

static void load_pattern(PsaPlayer* player) {
    const int16_t index = (int16_t)player->position; // the replayer indexes with a word
    const uint32_t number = module_u16(player, (int64_t)player->layout.orders + index);
    player->next_row = (int64_t)player->layout.patterns + ((int64_t)number << PSA_PATTERN_SHIFT);
}

// L976: counts the row; behind the last row of a pattern the next entry of the pattern number table is read, and the
// end of the table wraps to the loop entry or stops the song.

static void count_row(PsaPlayer* player) {
    player->row_counter = (uint8_t)((player->row_counter + 1) & PSA_ROW_LAST);
    if (player->row_counter == 0) {
        const uint16_t next = (uint16_t)(player->position + 2);
        if (next == player->subsong.end) {
            song_end(player); // with no loop the original calls SongEnd a second time here: the same event
            if (player->subsong.loop == PSA_NO_LOOP) {
                stop_song(player);
                player->speed_counter = player->speed;
                return;
            }
            player->position = player->subsong.loop;
        } else {
            player->position = next;
        }
        load_pattern(player);
    }
    player->speed_counter = player->speed;
}

// L9ce, a note without an instrument start: the period only (L9a14 and L9ac2 end here as well).

static void set_note_period(PsaPlayer* player, PsaVoice* voice, uint8_t note) {
    voice->note = (uint16_t)(note - 1);
    voice->period = period_at(player, voice->note);
    voice->paula.period = voice->period;
    voice->last_period = voice->period;
}

// L9ce, effect 3 on a note: the target period and the direction to slide.

static void set_portamento(PsaPlayer* player, PsaVoice* voice, const PsaNote* n) {
    voice->porta_target = period_at(player, (uint32_t)n->note - 1);
    // cmp.w / bpl: the slide goes down in period (up in pitch) when the target is at or above the period
    const bool above = (int16_t)(voice->porta_target - voice->period) >= 0;
    voice->porta_speed = above ? (int16_t)(uint16_t)(-(int)n->param) : (int16_t)n->param;
}

// L9a14: a note starts its instrument. DMA is switched off now and on at the end of the tick; the registers are
// written for the new sound. A pulse-width instrument plays a window of the wave buffer, a sample the module.

static void start_instrument(PsaPlayer* player, PsaVoice* voice, const PsaNote* n) {
    nuade_paula_channel_dma_off(&voice->paula);
    voice->dma_start = true;
    const int64_t at = (int64_t)player->layout.instruments + ((int64_t)n->instrument << PSA_INSTRUMENT_SHIFT);
    const PsaInstrument ins = read_instrument(player, at);
    voice->last_instrument = n->instrument;
    voice->envelope = ins.envelope;
    voice->envelope_timer = 1;
    voice->pwm = ins.pwm;
    voice->pwm_position = ins.pwm.start;
    const uint16_t volume = n->effect == PsaEffect_SetVolume ? n->param : ins.volume;
    if (ins.pwm.enabled) {
        // Lengths up to 128 words keep every window (0x100 - 2 * len + position, position 0..255) in the wave buffer
        // whatever the LFO does; longer ones read memory the port does not model.
        if (ins.length_words == 0 || ins.length_words > PSA_SYNTH_WORDS_MAX) {
            player->fault = true;
        }
        voice->pwm_down = false;
        voice->pwm_delay = ins.pwm.up_delay;
        voice->pwm_count = ins.pwm.up_count;
        voice->paula.len_words = ins.length_words;
        // suba.w: the window starts below the center by the (word) length in bytes
        voice->wave = (int64_t)PSA_WAVE_BASE + PSA_WAVE_CENTER - (int16_t)(uint16_t)(ins.length_words * 2u);
        write_lc(player, voice, voice->wave + ins.pwm.start);
    } else {
        voice->repeat_pending = true;
        write_lc(player, voice, (int64_t)PSA_MODULE_BASE + ins.sample);
        voice->paula.len_words = ins.length_words;
        voice->wave
            = ins.loop_start != 0 ? (int64_t)PSA_MODULE_BASE + ins.loop_start : (int64_t)PSA_WAVE_BASE + PSA_WAVE_ZERO;
    }
    write_volume(voice, volume);
    voice->volume = volume;
    voice->loop_words = ins.loop_words;
}

// L9ce: one voice reads its note of the row.

static void voice_row(PsaPlayer* player, PsaVoice* voice, const PsaNote* n) {
    voice->row = *n;
    voice->dma_start = false;
    voice->repeat_pending = false;
    if (n->note != 0) {
        if (n->period_only) {
            set_note_period(player, voice, n->note);
        } else if (n->effect == PsaEffect_TonePortamento) {
            set_portamento(player, voice, n);
            return;
        } else {
            start_instrument(player, voice, n);
            set_note_period(player, voice, n->note);
        }
    }
    // L9b4a: a period-only note has no row effect
    if (n->period_only) {
        return;
    }
    switch (n->effect) {
        case PsaEffect_Release:
            voice->envelope.sustain_count = 1;
            break;
        case PsaEffect_SetVolume:
            write_volume(voice, n->param);
            voice->volume = n->param;
            break;
        case PsaEffect_PatternBreak:
            player->row_counter = PSA_ROW_LAST;
            break;
        case PsaEffect_Speed:
            player->speed = n->param;
            break;
        default:
            break;
    }
}

// L92c: the tick that reads the next row.

static void next_row(PsaPlayer* player) {
    int64_t at = player->next_row;
    for (uint32_t i = 0; i < PSA_VOICES; i++) {
        const PsaNote n = read_note(player, at);
        at += PSA_NOTE_BYTES;
        voice_row(player, &player->voices[i], &n);
    }
    player->next_row = at;
    count_row(player);
}

// Lb8c, the arpeggio: the voice cycles through the note, the note plus the high nibble and the note plus the low
// nibble of the parameter, one per tick.

static void arpeggio(PsaPlayer* player, PsaVoice* voice, uint8_t param) {
    uint16_t period = voice->period;
    if (player->arpeggio_tick == 1) {
        period = period_at(player, voice->note + (param >> 4));
    } else if (player->arpeggio_tick == 2) {
        period = period_at(player, voice->note + (param & 0xF));
    }
    voice->paula.period = period;
}

// Lbf0, effect 4. The depth and length come from the parameter on the tick the row was read.

static void vibrato(PsaPlayer* player, PsaVoice* voice, uint8_t param) {
    if (player->speed_counter == player->speed && param != 0) {
        voice->vibrato_speed = param >> 4;
        voice->vibrato_count = param & 0xF;
        voice->vibrato_length = (uint8_t)(voice->vibrato_count * 2);
        voice->vibrato_offset = 0;
    }
    voice->vibrato_offset = (int16_t)(uint16_t)(voice->vibrato_offset + voice->vibrato_speed);
    voice->paula.period = (uint16_t)(voice->vibrato_offset + voice->period);
    voice->vibrato_count--;
    if (voice->vibrato_count == 0) {
        voice->vibrato_count = voice->vibrato_length;
        voice->vibrato_speed = (int16_t)-voice->vibrato_speed;
    }
}

// Lc52, effect A: the volume rises by the parameter, or falls by its low seven bits when bit 7 is set.

static void volume_slide(PsaVoice* voice, uint8_t param) {
    uint16_t volume = voice->volume;
    if ((param & 0x80) == 0) {
        volume = (uint16_t)(volume + param);
        // the clamp tests the low byte only
        if ((int8_t)(uint8_t)(volume - 0x40) >= 0) {
            volume = (uint16_t)((volume & 0xFF00) | 0x40);
        }
    } else {
        volume = (uint16_t)(volume - (param & 0x7F));
        if ((int16_t)volume < 0) {
            volume = 0;
        }
    }
    write_volume(voice, volume);
    voice->volume = volume;
}

// Lce4, effect 3: the period slides to the target and stops there.

static void tone_portamento(PsaVoice* voice) {
    const int16_t speed = voice->porta_speed;
    if (speed == 0) {
        return;
    }
    const uint16_t next = (uint16_t)(voice->period - speed);
    const int16_t diff = (int16_t)(next - voice->porta_target);
    const bool reached = speed > 0 ? diff < 0 : diff >= 0;
    if (reached) {
        voice->paula.period = voice->porta_target;
        voice->period = voice->porta_target;
        voice->porta_speed = 0;
    } else {
        voice->paula.period = next;
        voice->period = next;
    }
}

// Lb8c: the effect of the row, on every tick.

static void row_effect(PsaPlayer* player, PsaVoice* voice) {
    const uint8_t effect = voice->row.effect;
    const uint8_t param = voice->row.param;
    if (param != 0 && effect == PsaEffect_Arpeggio) {
        arpeggio(player, voice, param);
        return;
    }
    if (effect == PsaEffect_Vibrato) {
        vibrato(player, voice, param);
        return;
    }
    voice->paula.period = voice->period;
    switch (effect) {
        case PsaEffect_VolumeSlide:
            volume_slide(voice, param);
            break;
        case PsaEffect_SlideUp:
            voice->period = (uint16_t)(voice->period - param);
            voice->paula.period = voice->period;
            break;
        case PsaEffect_SlideDown:
            voice->period = (uint16_t)(voice->period + param);
            voice->paula.period = voice->period;
            break;
        case PsaEffect_TonePortamento:
            tone_portamento(voice);
            break;
        default:
            break;
    }
}

// Ld42: the pulse-width LFO moves the window of the wave buffer up and down.

static void pulse_width(PsaPlayer* player, PsaVoice* voice) {
    if (!voice->pwm.enabled) {
        return;
    }
    voice->pwm_delay--;
    if (voice->pwm_delay != 0) {
        return;
    }
    uint8_t position = voice->pwm_position;
    if (!voice->pwm_down) {
        voice->pwm_delay = voice->pwm.up_delay;
        position = (uint8_t)(position + voice->pwm.up_step);
        voice->pwm_count--;
        if (voice->pwm_count == 0) {
            voice->pwm_count = voice->pwm.down_count;
            voice->pwm_down = true;
        }
    } else {
        voice->pwm_delay = voice->pwm.down_delay;
        position = (uint8_t)(position - voice->pwm.down_step);
        voice->pwm_count--;
        if (voice->pwm_count == 0) {
            voice->pwm_count = voice->pwm.up_count;
            voice->pwm_down = false;
        }
    }
    voice->pwm_position = position;
    write_lc(player, voice, voice->wave + position);
}

// Lda4: the volume envelope. Attack and decay add to / subtract from the volume byte, the sustain waits for the
// release effect (or counts down), every other state releases.

static void volume_envelope(PsaVoice* voice) {
    PsaEnvelope* env = &voice->envelope;
    if (env->state == PsaEnvelope_Off) {
        return;
    }
    voice->envelope_timer--;
    if (voice->envelope_timer != 0) {
        return;
    }
    uint8_t low = (uint8_t)voice->volume;
    switch (env->state) {
        case PsaEnvelope_Attack:
            voice->envelope_timer = env->attack_delay;
            low = (uint8_t)(low + env->attack_step);
            env->attack_count--;
            if (env->attack_count == 0) {
                env->state++;
            }
            break;
        case PsaEnvelope_Decay:
            voice->envelope_timer = env->decay_delay;
            low = (uint8_t)(low - env->decay_step);
            env->decay_count--;
            if (env->decay_count == 0) {
                env->state++;
            }
            break;
        case PsaEnvelope_Sustain:
            voice->envelope_timer = 1;
            if (env->sustain_count != 0) {
                env->sustain_count--;
                if (env->sustain_count == 0) {
                    env->state++;
                }
            }
            return;
        default:
            voice->envelope_timer = env->release_delay;
            low = (uint8_t)(low - env->release_step);
            env->release_count--;
            if (env->release_count == 0) {
                env->state = PsaEnvelope_Off;
            }
            break;
    }
    // a negative byte clears the whole word; otherwise only the low byte changed
    voice->volume = (int8_t)low < 0 ? 0 : (uint16_t)((voice->volume & 0xFF00) | low);
    write_volume(voice, voice->volume);
}

// L8d2: one tick of the replayer.

static void play_tick(PsaPlayer* player) {
    player->speed_counter--;
    if ((int8_t)player->speed_counter < 0) {
        next_row(player);
    }
    player->arpeggio_tick = (uint8_t)(player->arpeggio_tick + 1);
    if (player->arpeggio_tick == PSA_ARPEGGIO_STEPS) {
        player->arpeggio_tick = 0;
    }
    for (uint32_t i = 0; i < PSA_VOICES; i++) {
        PsaVoice* voice = &player->voices[i];
        row_effect(player, voice);
        pulse_width(player, voice);
        volume_envelope(voice);
    }
}

// L80a: DMA on for the voices that started a note, then the repeat registers of the sample notes.

static void start_dma(PsaPlayer* player) {
    for (uint32_t i = 0; i < PSA_VOICES; i++) {
        if (player->voices[i].dma_start) {
            nuade_paula_channel_dma_on(&player->voices[i].paula);
            player->voices[i].dma_start = false;
        }
    }
    for (uint32_t i = 0; i < PSA_VOICES; i++) {
        PsaVoice* voice = &player->voices[i];
        if (voice->repeat_pending) {
            voice->repeat_pending = false;
            write_lc(player, voice, voice->wave);
            voice->paula.len_words = voice->loop_words;
        }
    }
}

// What Paula would play must be inside the module or the wave buffer.

static void check_voices(PsaPlayer* player) {
    for (uint32_t i = 0; i < PSA_VOICES; i++) {
        const NuadePaulaChannel* c = &player->voices[i].paula;
        if (c->dma || c->dma_started) {
            check_block(player, c->lc, c->len_words);
        }
        if (c->dma_started) {
            check_block(player, c->start_lc, c->start_len_words);
        }
    }
}

// L6fe: the interrupt.

static void interrupt(PsaPlayer* player) {
    player->looped = false;
    if (player->playing) {
        play_tick(player);
    }
    start_dma(player);
    check_voices(player);
}

// L76c: InitSound.

bool psa_start(PsaPlayer* player, uint32_t subsong, NuadeVoices* voices) {
    if (player == nullptr || subsong >= player->layout.subsong_count) {
        return false;
    }
    player->fault = false;
    player->looped = false;
    player->subsong = read_subsong(player, subsong);
    player->playing = true;
    player->speed = PSA_DEFAULT_SPEED;
    player->speed_counter = 0;
    player->row_counter = 0;
    player->arpeggio_tick = 0;
    player->position = player->subsong.start;
    load_pattern(player);
    for (uint32_t i = 0; i < PSA_VOICES; i++) {
        player->voices[i] = (PsaVoice) { .last_instrument = -1 };
        if (voices != nullptr) {
            nuade_voice_stop(voices, i);
        }
    }
    return !player->fault;
}

PsaTickResult psa_tick(PsaPlayer* player, NuadeVoices* voices) {
    interrupt(player);
    if (player->fault) {
        // not reachable after the dry run; never hand a buffer the port does not model to the mixer
        stop_song(player);
        for (uint32_t i = 0; i < PSA_VOICES; i++) {
            nuade_voice_stop(voices, i);
        }
        return (PsaTickResult) { .looped = false, .ended = true };
    }
    bool silent = !player->playing;
    for (uint32_t i = 0; i < PSA_VOICES; i++) {
        silent = silent && !player->voices[i].paula.dma;
        nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
    }
    return (PsaTickResult) { .looped = player->looped, .ended = silent };
}

// What a wrap of the sequence carries into the next pass and decides whether a later pass reads outside what the port
// models: the speed and its counter, the arpeggio phase, and per voice the last note's table index and the row's
// effect (an arpeggio keeps running on the last row's parameter). Positions and row counter are the loop entry and 0 at
// every wrap, instruments and pulse-width state are fixed by the static rules (start_instrument), and the rest (period,
// volume, vibrato, envelope) never selects a memory address.

typedef struct PsaWrapState {
    uint8_t speed;
    uint8_t speed_counter;
    uint8_t arpeggio_tick;
    uint16_t note[PSA_VOICES];
    uint8_t effect[PSA_VOICES];
    uint8_t param[PSA_VOICES];
    bool period_only[PSA_VOICES];
} PsaWrapState;

static PsaWrapState wrap_state(const PsaPlayer* player) {
    PsaWrapState state
        = { .speed = player->speed, .speed_counter = player->speed_counter, .arpeggio_tick = player->arpeggio_tick };
    for (uint32_t i = 0; i < PSA_VOICES; i++) {
        const PsaVoice* v = &player->voices[i];
        state.note[i] = v->note;
        state.effect[i] = v->row.effect;
        state.param[i] = v->row.param;
        state.period_only[i] = v->row.period_only;
    }
    return state;
}

static bool same_wrap_state(const PsaWrapState* a, const PsaWrapState* b) {
    if (a->speed != b->speed || a->speed_counter != b->speed_counter || a->arpeggio_tick != b->arpeggio_tick) {
        return false;
    }
    for (uint32_t i = 0; i < PSA_VOICES; i++) {
        if (a->note[i] != b->note[i] || a->effect[i] != b->effect[i] || a->param[i] != b->param[i]
            || a->period_only[i] != b->period_only[i]) {
            return false;
        }
    }
    return true;
}

// Plays `subsong` through without sound. Every module read, word access and Paula buffer of the run goes through the
// checks that set `fault`. The run is a proof only when it stops on a repeat: from the loop entry the sequence is the
// same every pass, so once the state of a wrap (PsaWrapState) equals that of an earlier wrap all later passes repeat
// the earlier ones. A song that stops is covered by its single pass. A subsong whose state does not repeat within
// PSA_WRAP_HISTORY wraps, or that exhausts the ticks left in `budget` (shared by all subsongs of a song), is refused.

static bool dry_run_is_clean(PsaPlayer* player, uint32_t subsong, uint32_t* budget) {
    if (!psa_start(player, subsong, nullptr)) {
        return false;
    }
    PsaWrapState history[PSA_WRAP_HISTORY];
    uint32_t wraps = 0;
    while (*budget > 0) {
        (*budget)--;
        interrupt(player);
        if (player->fault) {
            return false;
        }
        for (uint32_t i = 0; i < PSA_VOICES; i++) {
            player->voices[i].paula.dma_started = false;
        }
        if (!player->playing) {
            return true;
        }
        if (!player->looped) {
            continue;
        }
        const PsaWrapState state = wrap_state(player);
        for (uint32_t k = 0; k < wraps; k++) {
            if (same_wrap_state(&history[k], &state)) {
                return true;
            }
        }
        if (wraps == PSA_WRAP_HISTORY) {
            return false;
        }
        history[wraps++] = state;
    }
    return false;
}

// Check2 and the part of InitPlayer that counts the subsongs: the pattern number table, the patterns and the subsong
// table must be inside the module.

static bool parse_layout(PsaPlayer* player) {
    if (player->size < PSA_SUBSONG_TABLE + PSA_SUBSONG_BYTES || module_u32(player, 0) != PSA_MAGIC) {
        return false;
    }
    player->layout.instruments = module_u32(player, PSA_HEADER_INSTRUMENTS);
    player->layout.orders = module_u32(player, PSA_HEADER_ORDERS);
    player->layout.patterns = module_u32(player, PSA_HEADER_PATTERNS);
    const uint32_t table = player->layout.instruments;
    // InitPlayer walks the instrument table with long reads: an odd offset is an address error
    if (table < PSA_SUBSONG_TABLE + PSA_SUBSONG_BYTES || table > player->size || (table & 1) != 0) {
        return false;
    }
    player->layout.subsong_count = (table - PSA_SUBSONG_TABLE) / PSA_SUBSONG_BYTES;
    if (player->layout.subsong_count > PSA_SUBSONGS_MAX) {
        return false;
    }
    return !player->fault;
}

PsaPlayer* psa_create(const uint8_t* data, size_t size) {
    if (data == nullptr || size < PSA_SUBSONG_TABLE + PSA_SUBSONG_BYTES || size > 0x7FFFFFFF) {
        return nullptr;
    }
    PsaPlayer* player = (PsaPlayer*)calloc(1, sizeof(*player));
    if (player == nullptr) {
        return nullptr;
    }
    // [wave buffer gap][wave buffer][gap][module], then Paula's padding
    uint8_t* image = (uint8_t*)calloc(1, PSA_MODULE_BASE + size);
    if (image == nullptr) {
        free(player);
        return nullptr;
    }
    memset(image + PSA_WAVE_BASE, 0x80, PSA_WAVE_CENTER);
    memset(image + PSA_WAVE_BASE + PSA_WAVE_CENTER, 0x7F, PSA_WAVE_CENTER);
    memcpy(image + PSA_MODULE_BASE, data, size);
    player->chip = nuade_paula_padded_copy(image, PSA_MODULE_BASE + size);
    free(image);
    player->size = size;
    if (player->chip == nullptr || !parse_layout(player)) {
        psa_destroy(player);
        return nullptr;
    }
    for (uint32_t i = 0; i < PSA_TITLE_BYTES; i++) {
        const uint8_t c = module_bytes(player)[PSA_TITLE_OFFSET + i];
        player->title[i] = c >= 0x20 && c < 0x7F ? (char)c : ' ';
    }
    for (size_t n = PSA_TITLE_BYTES; n > 0 && player->title[n - 1] == ' '; n--) {
        player->title[n - 1] = '\0';
    }
    uint32_t budget = PSA_DRY_RUN_TICKS;
    for (uint32_t i = 0; i < player->layout.subsong_count; i++) {
        // psa_start resets all state: a subsong with the entry of an earlier one plays the same
        bool seen = false;
        const PsaSubsong entry = read_subsong(player, i);
        for (uint32_t k = 0; k < i && !seen; k++) {
            const PsaSubsong earlier = read_subsong(player, k);
            seen = earlier.start == entry.start && earlier.end == entry.end && earlier.loop == entry.loop;
        }
        if (!seen && !dry_run_is_clean(player, i, &budget)) {
            psa_destroy(player);
            return nullptr;
        }
    }
    psa_start(player, 0, nullptr);
    return player;
}

void psa_destroy(PsaPlayer* player) {
    if (player != nullptr) {
        free(player->chip);
        free(player);
    }
}

bool psa_probe(const uint8_t* data, size_t size) {
    PsaPlayer* player = psa_create(data, size);
    const bool playable = player != nullptr;
    psa_destroy(player);
    return playable;
}

uint32_t psa_subsong_count(const PsaPlayer* player) {
    return player->layout.subsong_count;
}

const char* psa_title(const PsaPlayer* player) {
    return player->title;
}

uint32_t psa_grid_ticks(const PsaPlayer* player) {
    return (uint32_t)player->speed + 1;
}

PsaVoiceState psa_voice_state(const PsaPlayer* player, uint32_t voice) {
    const PsaVoice* v = &player->voices[voice];
    return (PsaVoiceState) { .period = v->last_period, .instrument = v->last_instrument };
}
