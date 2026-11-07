// SPDX-License-Identifier: MIT
// Synth Dream player: the song. Every tick runs the four voices in turn. A voice that has no note left reads its next
// command from the track (a note's duration, pitch, volume cut and instrument, with modifiers that switch the volume
// or pitch table, slide the pitch or hold the sample); every tick then steps the note's volume, arpeggio, pitch and
// waveform tables and writes the voice's Paula registers.

#include "sdr_internal.h"
#include <stdlib.h>
#include <string.h>

#define MAX_MODIFIERS 4096   // modifiers after one note
#define MAX_FETCH_STEPS 4096 // track entries and repeats one voice may pass before its first note of a tick
#define STREAM_END 0xFF      // first byte of a command stream's last word, and of a table entry that restarts it
#define TRACK_LOOP 0xFE
#define DEFAULT_VOLUME 0x40
#define RATIO_SHIFT 13
#define RATIO_ONE 0x2000
#define GLIDE_LONGER 0xFD00 // a glide that lengthens the period; any other value shortens it
#define NOTE_C1 60          // the table index shown as C-1

// Command words. A note command's word selects which parameters follow; modifiers follow a note and may repeat.

typedef enum SdrModifier {
    SdrModifier_VolumeSwitch = 0x24, // after a delay the volume table continues at an offset
    SdrModifier_HoldOff = 0x26,
    SdrModifier_HoldOn = 0x28,      // the sample keeps playing after the note
    SdrModifier_PitchUp = 0x2A,     // a slide that shortens the period
    SdrModifier_PitchDown = 0x2C,   // a slide that lengthens it
    SdrModifier_PitchSwitch = 0x2E, // after a delay the pitch table continues at an offset
} SdrModifier;

#define NOTE_COMMANDS 18
#define FIRST_MODIFIER 0x24
#define OPCODE_LIMIT 0x30 // the table of handlers ends here

// The parameters of a note command, in stream order. A command without a duration repeats the previous one.
typedef struct SdrCommandForm {
    bool duration;
    bool note;
    bool note_cut;
    bool glide;      // a slide that spreads over the note: kind, amount, delay
    bool instrument; // loads an instrument record
    bool rest;       // DMA off for the note
} SdrCommandForm;

static const SdrCommandForm command_forms[NOTE_COMMANDS] = {
    { 0 },
    { .duration = true },
    { .note = true },
    { .note_cut = true },
    { .instrument = true },
    { .duration = true, .note = true },
    { .duration = true, .note_cut = true },
    { .duration = true, .instrument = true },
    { .note = true, .note_cut = true },
    { .note = true, .instrument = true },
    { .duration = true, .note = true, .note_cut = true },
    { .duration = true, .note = true, .instrument = true },
    { .duration = true, .note = true, .glide = true },
    { .note = true, .note_cut = true, .instrument = true },
    { .duration = true, .note = true, .note_cut = true, .instrument = true },
    { .duration = true, .rest = true },
    { .duration = true, .note = true, .glide = true, .instrument = true },
    { .duration = true, .note = true, .glide = true, .instrument = true },
};

// SongEnd: each voice that reaches the end of its track clears its flag; with all flags clear the song has ended and
// the flags are set again as they were at the start.

static void voice_reached_end(SdrPlayer* p, int index) {
    p->voice_pending[index] = false;
    for (int i = 0; i < SDR_VOICES; i++) {
        if (p->voice_pending[i]) {
            return;
        }
    }
    memcpy(p->voice_pending, p->voice_pending_reload, sizeof(p->voice_pending));
    p->song_ended = true;
}

static uint16_t add_to_low_byte(uint16_t word, uint8_t value) {
    return (uint16_t)((word & 0xFF00) | ((word + value) & 0xFF));
}

// A word read as a signed byte distance, added to a table offset.

static uint32_t table_entry(uint32_t table, uint16_t distance) {
    return table + (uint32_t)(int32_t)(int16_t)distance;
}

static uint16_t scale_period(uint16_t period, uint16_t ratio) {
    return (uint16_t)(((uint32_t)period * ratio) >> RATIO_SHIFT);
}

// Table entries are [length][repeats] and `length` elements of `element_size` bytes. Each call returns the next
// element; after the repeats the next entry follows, and an entry starting with 0xFF restarts at the table's start.

static uint32_t cursor_step(SdrMemory* module, SdrCursor* cursor, uint32_t table, uint32_t element_size) {
    if (cursor->left == 0) {
        if (cursor->repeats != 0) {
            cursor->at -= cursor->length * element_size;
            cursor->left = cursor->length;
            cursor->repeats--;
        } else {
            if (sdr_u8(module, cursor->at) == STREAM_END) {
                cursor->at = table;
            }
            cursor->length = sdr_u8(module, cursor->at);
            cursor->left = cursor->length;
            cursor->repeats = sdr_u8(module, cursor->at + 1);
            cursor->at += 2;
        }
    }
    const uint32_t element = cursor->at;
    cursor->at += element_size;
    cursor->left--;
    return element;
}

// A new note starts all four tables over.

static void restart_tables(SdrVoice* v) {
    v->volume_cursor.at = v->instrument.volume_table;
    v->pitch_cursor.at = v->instrument.pitch_table;
    v->arpeggio_cursor.at = v->instrument.arpeggio;
    v->waveform_cursor.at = v->instrument.waveform;
}

static void clear_cursor(SdrCursor* cursor) {
    cursor->left = 0;
    cursor->repeats = 0;
}

static void clear_note_state(SdrVoice* v) {
    v->slide = 0;
    v->arpeggio_offset = 0;
    v->rest = false;
    clear_cursor(&v->volume_cursor);
    clear_cursor(&v->pitch_cursor);
    clear_cursor(&v->arpeggio_cursor);
    clear_cursor(&v->waveform_cursor);
}

static uint16_t next_word(SdrPlayer* p, SdrVoice* v) {
    const uint16_t word = sdr_u16(&p->memory, v->commands);
    v->commands += 2;
    return word;
}

// The slide of a note: its ratio per tick, from a table, and the ticks to wait.

static void start_slide(SdrPlayer* p, SdrVoice* v, SdrRomTable table, uint16_t delay, uint16_t distance) {
    v->slide_delay = delay;
    v->slide_ratio = sdr_rom_word(table, (int16_t)distance, &p->memory.stray);
    v->slide = RATIO_ONE;
}

// A glide spreads a pitch change over the note: the table is chosen by the first word, the ratio index is the
// amount divided by the ticks of the note after the delay. False when that division is by zero.

static bool read_glide(SdrPlayer* p, SdrVoice* v) {
    const uint16_t kind = next_word(p, v);
    const uint16_t amount = next_word(p, v);
    const uint16_t delay = next_word(p, v);
    const uint16_t ticks = (uint16_t)(v->duration - delay);
    if (ticks == 0) {
        return false;
    }
    const uint16_t step = (uint16_t)((uint32_t)amount / ticks * 2);
    start_slide(p, v, kind == GLIDE_LONGER ? SdrRomTable_PeriodUp : SdrRomTable_PeriodDown, delay, step);
    return true;
}

static void load_instrument(SdrPlayer* p, SdrVoice* v) {
    uint16_t number = next_word(p, v);
    if ((uint8_t)number == v->substitute_of) {
        number = (uint16_t)((number & 0xFF00) | v->substitute_by);
    }
    v->instrument = sdr_read_instrument(&p->memory, p->header.instruments, number);
    v->display_instrument = number & 0xFF;
}

// The note's table index with the track's transposition and the arpeggio added to its low byte.

static uint16_t note_index(const SdrVoice* v) {
    uint16_t index = v->note;
    if (!v->instrument.fixed_pitch) {
        index = add_to_low_byte(index, v->transpose);
    }
    return add_to_low_byte(index, v->arpeggio_offset);
}

// A note command. False when the stream asks for something the replayer cannot do (a glide over no ticks, or a note
// without an instrument after a junk one).

static bool run_note_command(SdrPlayer* p, SdrVoice* v, const SdrCommandForm* form) {
    if (form->rest) {
        v->rest = true;
    }
    if (form->duration) {
        v->duration = next_word(p, v);
    }
    v->left = v->duration;
    if (form->note) {
        v->note = next_word(p, v);
    }
    if (form->glide && !read_glide(p, v)) {
        return false;
    }
    if (form->note_cut) {
        v->note_cut = next_word(p, v);
    }
    if (form->instrument) {
        load_instrument(p, v);
    } else if (v->instrument.muted && !form->rest) {
        return false; // the junk record's own fields would drive this note
    }
    restart_tables(v);
    if (!form->rest) {
        v->display_note = (int)(note_index(v) & 0xFF) - NOTE_C1;
        if (v->display_note < 0) {
            v->display_note = 0;
        }
    }
    return true;
}

// A modifier at the stream position: skips its word, then reads its parameters. A modifier that comes first in a stream
// entry has had its word read as the command word, so one more word is skipped.

static void run_modifier(SdrPlayer* p, SdrVoice* v, uint16_t word) {
    p->work++;
    v->commands += 2;
    switch (word) {
        case SdrModifier_VolumeSwitch: {
            v->volume_switch.delay = next_word(p, v);
            v->volume_switch.target = sdr_u32(&p->memory, table_entry(p->header.volume_switches, next_word(p, v)));
            break;
        }
        case SdrModifier_HoldOff:
            v->hold = false;
            break;
        case SdrModifier_HoldOn:
            v->hold = true;
            break;
        case SdrModifier_PitchUp:
        case SdrModifier_PitchDown: {
            const uint16_t delay = next_word(p, v);
            const uint16_t distance = next_word(p, v);
            start_slide(p, v, word == SdrModifier_PitchUp ? SdrRomTable_PeriodDown : SdrRomTable_PeriodUp, delay,
                        distance);
            break;
        }
        case SdrModifier_PitchSwitch: {
            v->pitch_switch.delay = next_word(p, v);
            v->pitch_switch.target = sdr_u32(&p->memory, table_entry(p->header.pitch_switches, next_word(p, v)));
            break;
        }
        default:
            break;
    }
}

// The first word of a stream entry is a note command, or a modifier that then skips one more word. The handler table
// holds the words 0 to 0x2E; the replayer would read other words as table offsets elsewhere in its data.

static bool is_command_word(uint16_t word) {
    return word % 2 == 0 && word < OPCODE_LIMIT;
}

// The modifiers continue while the next word is in 0x24..0x8023.

static bool is_modifier_word(uint16_t word) {
    return word >= FIRST_MODIFIER && (uint16_t)(word - FIRST_MODIFIER) < 0x8000;
}

// The commands of one stream entry: a note and its modifiers. False for a word the replayer cannot handle.

static bool run_commands(SdrPlayer* p, SdrVoice* v) {
    const uint16_t first = next_word(p, v);
    if (!is_command_word(first)) {
        return false;
    }
    if (first < FIRST_MODIFIER) {
        if (!run_note_command(p, v, &command_forms[first / 2])) {
            return false;
        }
    } else {
        run_modifier(p, v, first);
    }
    int modifiers = 0;
    for (uint16_t word = sdr_u16(&p->memory, v->commands); is_modifier_word(word);
         word = sdr_u16(&p->memory, v->commands)) {
        if (!is_command_word(word) || ++modifiers > MAX_MODIFIERS) {
            return false;
        }
        run_modifier(p, v, word);
    }
    return true;
}

static void load_track_entry(SdrPlayer* p, SdrVoice* v) {
    const SdrTrackEntry entry = sdr_read_track_entry(&p->memory, v->track_next);
    v->commands_start = entry.commands;
    v->commands = entry.commands;
    v->transpose = entry.transpose;
    v->repeat = entry.repeat;
    v->substitute_of = entry.substitute_of;
    v->substitute_by = entry.substitute_by;
    v->volume_cut = entry.volume_cut;
    v->track_next += SDR_TRACK_ENTRY_BYTES;
}

// Read the voice's next command: from its stream, else from the stream's repeats, else from the next track entry.
// False when the voice has ended.

static bool fetch_commands(SdrPlayer* p, SdrVoice* v, int index) {
    for (int step = 0; step < MAX_FETCH_STEPS; step++) {
        p->work++;
        clear_note_state(v);
        if (sdr_u8(&p->memory, v->commands) != STREAM_END) {
            if (!run_commands(p, v)) {
                break;
            }
            return true;
        }
        if (v->repeat != 0) {
            v->repeat--;
            v->commands = v->commands_start;
            continue;
        }
        v->volume_switch.target = 0;
        v->pitch_switch.target = 0;
        const uint8_t first = sdr_track_first_byte(&p->memory, v->track_next);
        if (first > TRACK_LOOP) {
            v->ended = true;
            voice_reached_end(p, index);
            return false;
        }
        if (first == TRACK_LOOP) {
            v->track_next = v->track_start;
            v->loops++;
            voice_reached_end(p, index);
        }
        load_track_entry(p, v);
    }
    p->unmodelled = true;
    v->ended = true;
    return false;
}

// A table switch whose delay has run out moves the cursor to its target and restarts the cursor's entry.

static void apply_switch(SdrSwitch* pending, SdrCursor* cursor) {
    if (pending->target == 0) {
        return;
    }
    if (pending->delay == 0) {
        cursor->at = pending->target;
        pending->target &= 0xFFFF; // only the upper word is cleared
        clear_cursor(cursor);
    }
    pending->delay--;
}

// The tick's volume: the table's byte (or the default) less the note's and the track's cuts, 0 if negative.

static uint8_t note_volume(SdrPlayer* p, SdrVoice* v) {
    uint8_t volume = DEFAULT_VOLUME;
    if (v->instrument.volume_table != 0) {
        volume = sdr_u8(&p->memory, cursor_step(&p->memory, &v->volume_cursor, v->instrument.volume_table, 1));
    }
    volume = (uint8_t)(volume - (uint8_t)v->note_cut);
    volume = (uint8_t)(volume - v->volume_cut);
    return (volume & 0x80) != 0 ? 0 : volume;
}

static int16_t pitch_table_value(SdrPlayer* p, SdrVoice* v) {
    const uint32_t at = cursor_step(&p->memory, &v->pitch_cursor, v->instrument.pitch_table, 2);
    return (int16_t)sdr_u16(&p->memory, at);
}

// The pitch table's word moves the period by a ratio: from the period_up table for 0 and above, else from the
// period_down table.

static uint16_t note_period(SdrPlayer* p, SdrVoice* v) {
    const int16_t offset = (int16_t)(uint16_t)(note_index(v) * 2);
    const uint16_t period = sdr_rom_word(SdrRomTable_Period, offset, &p->memory.stray);
    if (v->instrument.pitch_table == 0) {
        return period;
    }
    const int16_t bend = pitch_table_value(p, v);
    if (bend >= 0) {
        return scale_period(period,
                            sdr_rom_word(SdrRomTable_PeriodUp, (int16_t)(uint16_t)(bend * 2), &p->memory.stray));
    }
    return scale_period(period, sdr_rom_word(SdrRomTable_PeriodDown, (int16_t)(uint16_t)(-bend * 2), &p->memory.stray));
}

// A slide changes the period by a ratio that itself changes by `slide_ratio` every tick, after its delay.

static uint16_t slide_period(SdrVoice* v, uint16_t period) {
    if (v->slide == 0) {
        return period;
    }
    if (v->slide_delay != 0) {
        v->slide_delay--;
        return period;
    }
    v->slide = (uint16_t)(((uint32_t)v->slide * v->slide_ratio) >> RATIO_SHIFT);
    return scale_period(period, v->slide);
}

// The instrument's fine detune: a byte offset into the ratio tables, the period_up table for a positive one.

static uint16_t detuned_period(SdrPlayer* p, const SdrVoice* v, uint16_t period) {
    const int16_t detune = (int16_t)v->instrument.detune;
    if (detune == 0) {
        return period;
    }
    if (detune > 0) {
        return scale_period(period, sdr_rom_word(SdrRomTable_PeriodUp, detune, &p->memory.stray));
    }
    return scale_period(period, sdr_rom_word(SdrRomTable_PeriodDown, (int16_t)(uint16_t)-detune, &p->memory.stray));
}

// The waveform of a synthesised note: a pulse preset with one byte lowered. The table entry's upper byte is both the
// preset and the byte; the lower byte is what is subtracted.

static void build_waveform(SdrPlayer* p, SdrVoice* v, int index) {
    const uint16_t step = sdr_u16(&p->memory, cursor_step(&p->memory, &v->waveform_cursor, v->instrument.waveform, 2));
    const uint32_t preset = step >> 8;
    if (preset >= SDR_WAVE_PRESETS) {
        p->memory.stray = true; // the original reads past its presets and writes past the voice's buffer
        return;
    }
    int8_t* buffer = &p->waveforms[index * SDR_WAVE_BYTES];
    for (uint32_t i = 0; i < SDR_WAVE_BYTES; i++) {
        buffer[i] = sdr_wave_preset_byte(preset, i, &p->memory.stray);
    }
    buffer[preset] = (int8_t)(buffer[preset] - (int8_t)(step & 0xFF));
}

static void set_sound(SdrPlayer* p, SdrVoice* v, int index, bool loop) {
    NuadePaulaChannel* paula = &v->paula;
    if (v->instrument.waveform != 0) {
        paula->lc = &p->waveforms[index * SDR_WAVE_BYTES];
        paula->len_words = SDR_WAVE_WORDS;
        return;
    }
    const SdrSampleRef* sample = &v->instrument.sample;
    paula->len_words = loop ? sample->loop_length_words : sample->length_words;
    paula->lc = sdr_sample_pointer(&p->memory, loop ? sample->loop_start : sample->start, paula->len_words);
}

// The note's tick: the period and volume from the tables, and the registers. The sound is written on the note's first
// tick (and starts the DMA) and its repeat on the second; the DMA stops at the note's last tick unless it is held.

static void play_note(SdrPlayer* p, SdrVoice* v, int index) {
    NuadePaulaChannel* paula = &v->paula;
    if (v->rest) {
        nuade_paula_channel_dma_off(paula);
        v->left--;
        return;
    }
    apply_switch(&v->volume_switch, &v->volume_cursor);
    apply_switch(&v->pitch_switch, &v->pitch_cursor);
    if (v->instrument.muted) {
        if (v->hold && v->left == 1) {
            p->unmodelled = true; // the junk sound would keep playing into the next note
        }
        nuade_paula_channel_dma_off(paula);
        v->left--;
        return;
    }

    v->volume = note_volume(p, v);
    if (v->instrument.arpeggio != 0) {
        v->arpeggio_offset
            = sdr_u8(&p->memory, cursor_step(&p->memory, &v->arpeggio_cursor, v->instrument.arpeggio, 1));
    }
    v->period = note_period(p, v);
    v->period = slide_period(v, v->period);
    if (v->instrument.waveform != 0) {
        build_waveform(p, v, index);
    }
    v->period = detuned_period(p, v, v->period);

    const uint16_t first_tick = (uint16_t)(v->duration - 1);
    paula->period = v->period;
    paula->volume = v->volume;
    if (first_tick < v->left) {
        set_sound(p, v, index, false);
        nuade_paula_channel_dma_on(paula);
    } else if (first_tick == v->left) {
        set_sound(p, v, index, true);
        nuade_paula_channel_dma_on(paula);
    } else if (v->left == 1) {
        if (!v->hold) {
            nuade_paula_channel_dma_off(paula);
        }
    } else {
        nuade_paula_channel_dma_on(paula);
    }
    v->left--;
}

void sdr_voice_tick(SdrPlayer* p, int index) {
    SdrVoice* v = &p->voices[index];
    p->work++;
    if (v->left == 0 && !fetch_commands(p, v, index)) {
        return;
    }
    play_note(p, v, index);
}

SdrTickResult sdr_tick(SdrPlayer* p, NuadeVoices* voices) {
    p->song_ended = false;
    for (int i = 0; i < SDR_VOICES; i++) {
        if (!p->voices[i].ended) {
            sdr_voice_tick(p, i);
        }
    }
    for (int i = 0; i < SDR_VOICES; i++) {
        nuade_paula_channel_sync(&p->voices[i].paula, (uint32_t)i, voices);
    }
    return (SdrTickResult) { .looped = p->song_ended };
}

// InitSound: each voice starts at the first entry of its track. A track that is empty (starts with 0xFF) ends at once
// and does not count for SongEnd.

static void start_voice(SdrPlayer* p, int index, uint32_t subsong) {
    SdrVoice* v = &p->voices[index];
    memset(v, 0, sizeof(*v));
    v->display_note = -1;
    v->display_instrument = -1;
    v->track_start = sdr_read_track_offset(&p->memory, subsong, (uint32_t)index);
    v->track_next = v->track_start;
    load_track_entry(p, v);
    p->voice_pending[index] = true;
    if (sdr_track_first_byte(&p->memory, v->track_start) == STREAM_END) {
        v->ended = true;
        p->voice_pending[index] = false;
    }
}

bool sdr_start(SdrPlayer* p, int subsong) {
    if (subsong < 0 || subsong >= p->subsong_count) {
        return false;
    }
    memset(p->waveforms, 0, sizeof(p->waveforms));
    for (int i = 0; i < SDR_VOICES; i++) {
        start_voice(p, i, (uint32_t)subsong);
    }
    memcpy(p->voice_pending_reload, p->voice_pending, sizeof(p->voice_pending));
    p->song_ended = false;
    return true;
}

static SdrPlayer* new_player(const uint8_t* data, size_t size) {
    if (size > UINT32_MAX) {
        return nullptr;
    }
    SdrPlayer* p = calloc(1, sizeof(SdrPlayer));
    uint8_t* copy = malloc(size);
    if (p == nullptr || copy == nullptr) {
        free(p);
        free(copy);
        return nullptr;
    }
    memcpy(copy, data, size);
    p->memory.module = copy;
    p->memory.module_size = (uint32_t)size;
    p->memory.sample_base = sdr_sample_base((uint32_t)size);
    p->header = sdr_read_header(&p->memory);
    p->subsong_count = sdr_module_subsong_count(&p->memory);
    return p;
}

bool sdr_probe(const uint8_t* data, size_t size) {
    SdrPlayer* p = new_player(data, size);
    if (p == nullptr) {
        return false;
    }
    p->memory.samples_unknown = true;
    const bool ok = sdr_check_module(&p->memory) && !p->memory.stray && sdr_prove_module(p);
    sdr_destroy(p);
    return ok;
}

SdrCreateResult sdr_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    SdrCreateResult result = { .status = SdrStatus_InvalidData };
    SdrPlayer* p = new_player(data, size);
    if (p == nullptr) {
        result.status = size > UINT32_MAX ? SdrStatus_InvalidData : SdrStatus_OutOfMemory;
        return result;
    }
    if (!sdr_check_module(&p->memory) || p->memory.stray) {
        sdr_destroy(p);
        return result;
    }
    if (!sdr_load_samples(&p->memory, path, io)) {
        sdr_destroy(p);
        result.status = SdrStatus_MissingSamples;
        return result;
    }
    if (!sdr_prove_module(p)) {
        sdr_destroy(p);
        result.status = SdrStatus_Unmodelled;
        return result;
    }
    result.player = p;
    result.status = SdrStatus_Ok;
    return result;
}

void sdr_destroy(SdrPlayer* p) {
    if (p == nullptr) {
        return;
    }
    free(p->memory.module);
    free(p->memory.samples);
    free(p);
}

int sdr_subsong_count(const SdrPlayer* p) {
    return p->subsong_count;
}

SdrVoiceState sdr_voice_state(const SdrPlayer* p, int voice) {
    if (voice < 0 || voice >= SDR_VOICES) {
        return (SdrVoiceState) { .note = -1, .instrument = -1 };
    }
    const SdrVoice* v = &p->voices[voice];
    return (SdrVoiceState) { .note = v->display_note, .instrument = v->display_instrument };
}
