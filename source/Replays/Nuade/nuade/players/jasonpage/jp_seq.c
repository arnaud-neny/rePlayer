// SPDX-License-Identifier: MIT
// Jason Page player: the sequencer. Every speed + 1 ticks each voice reads its track list, the patterns it names, and
// queues the instruments and notes in them.

#include "jp_internal.h"

#include <string.h>

// Pattern bytes
#define PATTERN_END 0xFF
#define PATTERN_REST 0xF9
#define PATTERN_SLIDE 0xFE       // two bytes: the slide
#define PATTERN_HOLD_VOLUME 0xFC // Virocop: one byte for the hold volume
#define PATTERN_INSTRUMENT_BIT 0x80
#define PATTERN_STEP_BIT 0x40
#define PATTERN_INSTRUMENT_MASK 0x7F
#define PATTERN_STEP_MASK 0x3F
#define PATTERN_FIRST_COMMAND 0x40 // Virocop: below this a byte is a note

#define LEGATO_INSTRUMENT 0x78     // Graftgold, Realms: notes of this instrument change the pitch only
#define PORTAMENTO_INSTRUMENT 0x77 // Virocop: notes of this instrument slide to the pitch
#define MAX_NOTE 0x54
#define SLIDE_DOWN_TARGET 0
#define SLIDE_UP_TARGET 0xFFFF
#define HOLD_VOLUME_SHIFT 8

// A well-formed song reaches a pattern byte that ends a voice's step within a few track words.
#define MAX_TRACK_WORDS_PER_STEP 256
#define MAX_PATTERN_BYTES_PER_STEP 1024

#define STEP_COUNTER_LAST 0 // the step counter runs down from reload through 0 to -1

typedef enum StepResult {
    StepResult_VoiceDone, // the voice has played its step
    StepResult_NextTrackWord,
} StepResult;

static uint32_t pattern_address(const JpPlayer* player, uint8_t code) {
    return jp_table_pointer(player, player->layout.pattern_table, player->layout.pattern_base, code);
}

// The loop marker of a voice's track list. The song has looped once every voice has passed its marker.

static void mark_loop(JpPlayer* player, const JpVoice* voice) {
    player->voice_looped[voice->number] = true;
    for (int i = 0; i < JP_VOICES; i++) {
        if (!player->voice_looped[i]) {
            return;
        }
    }
    player->looped = true;
    for (int i = 0; i < JP_VOICES; i++) {
        player->voice_looped[i] = false;
    }
}

// A note: transposed, capped, and looked up. A note of the instrument that the replayer calls "no instrument" is
// ignored without a queued instrument as well.

static uint8_t transposed_note(const JpVoice* voice, uint8_t note) {
    const uint8_t sum = (uint8_t)(note + voice->transpose);
    return (int8_t)sum < MAX_NOTE ? sum : MAX_NOTE;
}

// Graftgold and Realms patterns. A byte is an instrument (bit 7), a new speed for this voice (bit 6) or a note; 0xF9
// rests, 0xFE sets a slide and 0xFF ends the pattern.

static StepResult graftgold_pattern(JpPlayer* player, JpVoice* voice, uint32_t pattern) {
    for (int count = 0; count < MAX_PATTERN_BYTES_PER_STEP; count++) {
        const uint8_t byte = jp_u8(&player->mem, pattern + voice->pattern_pos);
        if (byte == PATTERN_END) {
            voice->pattern_pos = 0;
            voice->track_pos += 2;
            return StepResult_NextTrackWord;
        }
        if (byte == PATTERN_REST) {
            voice->pattern_pos++;
            return StepResult_VoiceDone;
        }
        if (byte == PATTERN_SLIDE) {
            const uint32_t at = pattern + voice->pattern_pos;
            voice->slide = (int16_t)(jp_u8(&player->mem, at + 1) << 8 | jp_u8(&player->mem, at + 2));
            voice->pattern_pos = (uint8_t)(voice->pattern_pos + 3);
            return StepResult_VoiceDone;
        }
        if ((byte & PATTERN_INSTRUMENT_BIT) != 0) {
            voice->instrument = byte & PATTERN_INSTRUMENT_MASK;
        } else if ((byte & PATTERN_STEP_BIT) != 0) {
            voice->step_reload = byte & PATTERN_STEP_MASK;
            voice->step_count = voice->step_reload;
        } else {
            if (voice->instrument == LEGATO_INSTRUMENT || jp_voice_queue(player, voice, voice->instrument)) {
                voice->last_note = transposed_note(voice, byte);
                voice->period = jp_voice_pattern_period(player, (uint8_t)voice->last_note);
            }
            voice->pattern_pos++;
            return StepResult_VoiceDone;
        }
        voice->pattern_pos++;
    }
    player->failed = true;
    return StepResult_VoiceDone;
}

// Virocop patterns: a byte below 0x40 is a note, 0x40..0x7F a new speed, 0x80..0xF8 an instrument. 0xF9 (and 0xFA,
// 0xFB, 0xFD) rest, 0xFC sets the hold volume, 0xFE a slide that runs to the end of the period range, 0xFF ends the
// pattern. The position is kept in a 16-bit register whose low byte wraps.

static uint16_t advance_byte(uint16_t position, uint16_t n) {
    return (uint16_t)((position & 0xFF00) | ((position + n) & JP_BYTE_MASK));
}

static void virocop_note(JpPlayer* player, JpVoice* voice, uint32_t pattern, uint16_t* position, uint8_t byte) {
    const uint16_t instrument = voice->instrument;
    if (instrument < PORTAMENTO_INSTRUMENT && !jp_voice_queue(player, voice, instrument)) {
        return;
    }
    voice->last_note = transposed_note(voice, byte);
    const uint16_t period = jp_voice_pattern_period(player, (uint8_t)voice->last_note);
    if (instrument != PORTAMENTO_INSTRUMENT) {
        voice->period = period;
        return;
    }
    voice->target_period = period;
    *position = (uint16_t)(*position + 1);
    const int16_t speed = jp_u8(&player->mem, pattern + *position);
    voice->slide = (int16_t)(period - voice->period) >= 0 ? speed : (int16_t)-speed;
}

static StepResult virocop_pattern(JpPlayer* player, JpVoice* voice, uint32_t pattern) {
    uint16_t position = voice->pattern_pos;
    bool done = false;
    for (int count = 0; count < MAX_PATTERN_BYTES_PER_STEP && !done; count++) {
        const uint8_t byte = jp_u8(&player->mem, pattern + position);
        if (byte < PATTERN_FIRST_COMMAND) {
            virocop_note(player, voice, pattern, &position, byte);
            position = advance_byte(position, 1);
            done = true;
        } else if (byte < PATTERN_INSTRUMENT_BIT) {
            voice->step_reload = byte & PATTERN_STEP_MASK;
            voice->step_count = voice->step_reload;
            position = advance_byte(position, 1);
        } else if (byte < PATTERN_REST) {
            voice->instrument = byte & PATTERN_INSTRUMENT_MASK;
            position = advance_byte(position, 1);
        } else if (byte == PATTERN_END) {
            voice->pattern_pos = 0;
            voice->track_pos += 2;
            return StepResult_NextTrackWord;
        } else if (byte == PATTERN_HOLD_VOLUME) {
            voice->hold_volume_next = (uint16_t)(jp_u8(&player->mem, pattern + position + 1) << HOLD_VOLUME_SHIFT);
            position = advance_byte(position, 2);
            done = true;
        } else if (byte == PATTERN_SLIDE) {
            voice->slide = (int16_t)(jp_u8(&player->mem, pattern + position + 1) << 8
                                     | jp_u8(&player->mem, pattern + position + 2));
            voice->target_period = voice->slide < 0 ? SLIDE_DOWN_TARGET : SLIDE_UP_TARGET;
            position = advance_byte(position, 3);
            done = true;
        } else {
            position = advance_byte(position, 1);
            done = true;
        }
    }
    player->failed |= !done;
    voice->pattern_pos = (uint8_t)position;
    return StepResult_VoiceDone;
}

// One sequencer step of a voice. Returns false when the track list ended the song.

static bool voice_step(JpPlayer* player, JpVoice* voice) {
    voice->step_count = (uint8_t)(voice->step_count - 1);
    if ((int8_t)voice->step_count >= STEP_COUNTER_LAST) {
        return true;
    }
    voice->step_count = voice->step_reload;
    for (int count = 0; count < MAX_TRACK_WORDS_PER_STEP; count++) {
        const uint32_t list = player->track_list[voice->number];
        const uint16_t word = jp_u16(&player->mem, list + (uint32_t)(int32_t)(int16_t)voice->track_pos);
        const uint8_t code = (uint8_t)(word >> 8);
        voice->transpose = (uint8_t)word;
        if (code == JP_TRACK_END) {
            player->active = false;
            return false;
        }
        if (code == JP_TRACK_LOOP || code == JP_TRACK_LOOP_ALT) {
            mark_loop(player, voice);
            voice->track_pos = (uint16_t)(voice->transpose * 2u);
            continue;
        }
        if (code == JP_TRACK_SKIP) {
            voice->track_pos += 2;
            continue;
        }
        const uint32_t pattern = pattern_address(player, code);
        const StepResult result = player->format == JpFormat_Virocop ? virocop_pattern(player, voice, pattern)
                                                                     : graftgold_pattern(player, voice, pattern);
        if (result == StepResult_VoiceDone) {
            return true;
        }
    }
    player->failed = true;
    return true;
}

void jp_seq_step(JpPlayer* player) {
    if (!player->active) {
        return;
    }
    player->speed_count--;
    if (player->speed_count >= 0) {
        return;
    }
    player->speed_count = (int16_t)player->speed_reload;
    for (int i = 0; i < JP_VOICES; i++) {
        if (!voice_step(player, &player->voices[i])) {
            return;
        }
    }
}

// Start a subsong: its speed, the track list of each voice, and every voice back at the head of its track list with the
// idle script. Fields the song does not name keep what the previous run left in them.

void jp_seq_reset(JpPlayer* player, int subsong) {
    const JpLayout* layout = &player->layout;
    const uint32_t index = (uint32_t)subsong & JP_SUBSONG_MASK;
    player->subsong = subsong;
    player->speed_reload = jp_table_value(player, layout->speed_table, index);
    player->speed_count = 0;
    // InitSound sets the eagleplayer's SongEnd marks back, so no voice has passed its loop word yet.
    memset(player->voice_looped, 0, sizeof(player->voice_looped));
    for (int i = 0; i < JP_VOICES; i++) {
        JpVoice* voice = &player->voices[i];
        player->track_list[i] = jp_table_pointer(player, layout->track_index[i], layout->track_base[i], index);
        voice->script = player->idle_script;
        voice->track_pos = 0;
        voice->pattern_pos = 0;
        voice->transpose = 0;
        voice->last_note = 0;
        voice->step_count = 0;
        voice->step_reload = 0;
        voice->priority = 0;
        voice->has_pending = false;
        if (player->format == JpFormat_Virocop) {
            voice->hold_volume_next = JP_HOLD_VOLUME_NONE;
        }
    }
    player->active = true;
}
