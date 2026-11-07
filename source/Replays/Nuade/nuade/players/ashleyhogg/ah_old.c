// SPDX-License-Identifier: MIT
// Ashley Hogg player, old family (CJ in the USA, Nobby the Aardvark, Spikey in Transylvania, Stuntman Seymour).
//
// The driver is a single routine that the CIA calls every tick; the eagleplayer calls it at 50 Hz. Four voice records
// of 0x4A bytes: the first 0x2C are the copy of the instrument that the voice plays, the rest is the sequencer.
//
// A song is four tracks, one per voice. A track is a list of entries (transposition, pattern number, repeat count when
// the pattern number has bit 7 set) ended by a restart (0x81) or stop (0x80) entry. A pattern is a stream of events:
// note + instrument (0x3F = keep the instrument) followed by optional envelope (0x65) and slide (0x60) bytes, wait
// (0x63), speed (0x64), release (0x62) and a negative byte at the end.
//
// A tick that reads an event does nothing else for that voice. The next tick starts the DMA of the sample the event
// loaded, and the one after writes the repeat registers; from then on every tick runs the envelope, slide, vibrato,
// arpeggio and writes volume and period.
//
// Not translated, because the eagleplayer never calls them: the sound effect entry (a note on one of the voices
// while a game is running), the fade-out entry and the second init routine. In the oldest version only voice 3 plays
// the song; the other voices belong to the game's sound effects, and a voice of them that a song gave a track would
// run the effect routine, which is not translated.
//
// Not modelled: the wave base of the wave-table voice is the driver's own 7 byte event buffer, whose first bytes are
// variables while the driver runs (the port reads the file's bytes; no corpus song uses the wave-table voice), and a
// pattern event that reads past those 7 bytes would read the variables behind the buffer (the corpus maximum is 7).

#include "ah_internal.h"
#include "nuade_tracker.h"

#define NO_PATTERN_BYTE 0x80 // a negative pattern byte ends the pattern
#define EVENT_RELEASE 0x62   // key off
#define EVENT_WAIT 0x63      // wait n steps (the event after it is read without a note)
#define EVENT_SPEED 0x64     // ticks per step
#define EVENT_ENVELOPE 0x65  // sustain level
#define EVENT_SLIDE 0x60     // slide per tick, big endian
#define NO_INSTRUMENT 0x3F
#define TRACK_RESTART 0x81
#define TRACK_STOP 0x80
#define TRACK_PATTERN_REPEAT 0x80 // flag of the pattern number: a repeat count follows
#define TRACK_PATTERN_MASK 0x7F
#define MAX_EVENTS 4096      // per voice and tick: an iteration guard against streams that never reach a note
#define MAX_TRACK_ENTRIES 64 // the same for restart entries that lead back to a restart entry
#define VOLUME_SHIFT 6
#define WAVE_POSITION_MASK 0xFFF
#define WAVE_STRIDE_SHIFT 7
#define WAVE_INDEX_SHIFT 5
#define WAVE_LENGTH_WORDS 0x40
#define ARP_LOOP 0x80
#define BYTES_PER_LONG 4
#define TRACK_ENTRY_SIZE 2   // transposition and pattern number; with a repeat count a third byte follows
#define SILENT_WORD_OFFSET 4 // the silent word follows the long that holds its address

// An entry of a track list: a command, or a transposition and a pattern number.

typedef struct TrackEntry {
    uint8_t first;  // TRACK_RESTART, TRACK_STOP, the fade command, or the transposition
    uint8_t second; // the restart offset, the fade step, or the pattern number
} TrackEntry;

static TrackEntry read_track_entry(const AhMemory* m, uint32_t offset) {
    return (TrackEntry) { .first = ah_u8(m, offset), .second = ah_u8(m, offset + 1) };
}

// The period of a note (index into the period table; a negative or large index reads the neighbouring data).

static uint16_t period_of(const AhOld* old, const AhMemory* m, int note_index) {
    return ah_u16(m, old->tables.periods + (uint32_t)(int32_t)(note_index * 2));
}

// The track list's restart command ends the song when every voice that has a track has reached one: SongEnd, then the
// voices are armed again.

static void song_end_check(AhOldSong* s, AhOldVoice* v) {
    v->alive = false;
    for (int i = 0; i < AH_VOICES; i++) {
        if (s->voices[i].alive) {
            return;
        }
    }
    s->looped = true;
    for (int i = 0; i < AH_VOICES; i++) {
        s->voices[i].alive = s->voices[i].alive_initial;
    }
}

// $41E: load the next track entry into the voice.

static void load_track(AhOld* old, const AhMemory* m, int index) {
    AhOldSong* s = &old->song;
    AhOldVoice* v = &s->voices[index];
    const AhOldTables* t = &old->tables;
    if (v->track == t->track_base) {
        return;
    }
    if (v->pattern_repeats != 0) {
        v->pattern_repeats--;
        if (v->pattern_repeats != 0) {
            v->pattern = v->pattern_start;
            return;
        }
    }
    for (int guard = 0; guard < MAX_TRACK_ENTRIES; guard++) {
        const TrackEntry entry = read_track_entry(m, v->track);
        if (entry.first == TRACK_RESTART) {
            song_end_check(s, v);
            v->track = v->track_restart + (old->version.restart_offset ? entry.second : 0u);
        } else if (old->version.fade_command != 0 && entry.first == old->version.fade_command) {
            if (s->fade_ticks == 0) {
                s->fade_ticks = entry.second;
                s->fade_count = entry.second;
            }
            v->track += TRACK_ENTRY_SIZE;
        } else if (entry.first == TRACK_STOP) {
            return;
        } else {
            s->track_loaded = true;
            v->transpose_base = (int8_t)entry.first;
            v->track += TRACK_ENTRY_SIZE;
            const uint32_t pattern
                = t->pattern_base
                  + ah_u32(m, t->pattern_offsets + (entry.second & TRACK_PATTERN_MASK) * BYTES_PER_LONG);
            v->pattern = pattern;
            v->pattern_start = pattern;
            if ((entry.second & TRACK_PATTERN_REPEAT) != 0) {
                v->pattern_repeats = ah_u8(m, v->track);
                v->track++;
            }
            return;
        }
    }
}

// A note names an instrument: its record replaces the voice's, the sample is stopped and its registers are written
// (the DMA starts on the next tick).

static void start_instrument(AhOld* old, const AhMemory* m, int index, uint8_t number) {
    AhOldVoice* v = &old->song.voices[index];
    NuadePaulaChannel* paula = &old->song.paula[index];
    v->instrument = ah_old_read_instrument(m, old->tables.instruments + number * OLD_INSTRUMENT_SIZE);
    v->instrument.vibrato_count >>= 1;
    v->env_count = 0;
    v->display_instrument = number;
    nuade_paula_channel_dma_off(paula);
    if (!v->instrument.wave_table) {
        ah_set_sample(m, paula, old->tables.samples + v->instrument.sample, v->instrument.length_words);
    }
}

// $12A: the events at the voice's pattern position, until one takes the tick.

static void read_events(AhOld* old, const AhMemory* m, int index) {
    AhOldSong* s = &old->song;
    AhOldVoice* v = &s->voices[index];
    AhOldInstrument* in = &v->instrument;
    bool note = false;
    for (int guard = 0; guard < MAX_EVENTS && !note; guard++) {
        const uint8_t event = ah_u8(m, v->pattern);
        if ((event & NO_PATTERN_BYTE) != 0) {
            s->master = 0;
            return;
        } else if (event == EVENT_RELEASE) {
            v->pattern++;
            in->attack_rate = 0;
            in->sustain_level = 0;
            v->env_count = 0;
        } else if (event == EVENT_SPEED) {
            v->step_count = ah_u8(m, v->pattern + 1);
            v->step_ticks = v->step_count;
            v->pattern += 2;
        } else if (event == EVENT_WAIT) {
            v->wait = ah_u8(m, v->pattern + 1);
            v->pattern += 2;
            note = true;
        } else {
            v->note = event;
            const uint8_t instrument = ah_u8(m, v->pattern + 1);
            v->pattern += 2;
            if (instrument != NO_INSTRUMENT) {
                start_instrument(old, m, index, instrument);
            }
            in->slide = 0;
            v->pitch = 0;
            note = true;
        }
    }
    if (ah_u8(m, v->pattern) == EVENT_ENVELOPE) {
        in->sustain_level = ah_u8(m, v->pattern + 1);
        v->pattern += 2;
    }
    if (ah_u8(m, v->pattern) == EVENT_SLIDE) {
        in->slide = (int16_t)(-(int16_t)(ah_u8(m, v->pattern + 1) << 8 | ah_u8(m, v->pattern + 2)));
        v->pattern += 3;
    }
    v->transposition = v->transpose_base;
    v->display_note = nuade_period_to_note(period_of(old, m, v->note + v->transpose_base));
}

// $3A8: the volume envelope.

static void run_envelope(AhOldVoice* v) {
    AhOldInstrument* in = &v->instrument;
    if (in->attack_rate != 0) {
        if (v->env_count == 0) {
            v->env_count = in->attack_rate;
            in->level++;
            if (in->level == in->sustain_level) {
                in->attack_rate = 0;
                v->env_count = 0;
                return;
            }
        }
        v->env_count--;
        return;
    }
    if (in->level == 0) {
        if (in->sustain_level == 0) {
            return;
        }
        in->level = in->sustain_level;
    }
    if (in->sustain_level != 0 || in->level == 0) {
        return;
    }
    if (v->env_count == 0) {
        v->env_count = in->release_rate;
        if (v->env_count == 0) {
            in->level = 0;
            return;
        }
        in->level--;
        if (in->level == 0) {
            return;
        }
    }
    v->env_count--;
}

// The sample registers of a voice that plays: the DMA starts once, then every tick writes the repeat.

static void run_sample(AhOld* old, const AhMemory* m, int index) {
    AhOldInstrument* in = &old->song.voices[index].instrument;
    NuadePaulaChannel* paula = &old->song.paula[index];
    const AhOldTables* t = &old->tables;
    if (in->wave_table) {
        return;
    }
    if (!in->dma_started) {
        nuade_paula_channel_dma_on(paula);
        in->dma_started = true;
    } else if (in->loop != 0) {
        ah_set_sample(m, paula, t->samples + in->loop, in->length_words);
    } else {
        ah_set_sample(m, paula, t->silent + SILENT_WORD_OFFSET, 1);
    }
}

// $2A8: the vibrato adds its step to the pitch; the step flips every half period and grows up to its maximum.

static void run_vibrato(AhOldVoice* v) {
    AhOldInstrument* in = &v->instrument;
    if (in->vibrato_delay != 0) {
        in->vibrato_delay--;
        return;
    }
    if (in->vibrato_step == 0) {
        return;
    }
    v->pitch = (int16_t)(v->pitch + in->vibrato_step);
    in->vibrato_count--;
    if (in->vibrato_count != 0) {
        return;
    }
    in->vibrato_count = in->vibrato_period;
    in->vibrato_step = (int16_t)-in->vibrato_step;
    if (in->vibrato_step >= 0 && in->vibrato_max > in->vibrato_step) {
        in->vibrato_step = (int16_t)(in->vibrato_step + in->vibrato_growth);
    }
}

// $2E2: the period is the table entry of note + arpeggio step (+ transposition) plus the pitch; the arpeggio moves on
// or, before an ARP_LOOP marker, jumps to the position after it.

static void write_period(AhOld* old, const AhMemory* m, int index) {
    const AhOldTables* t = &old->tables;
    AhOldVoice* v = &old->song.voices[index];
    AhOldInstrument* in = &v->instrument;
    const uint32_t table = t->arp_base + ah_u32(m, t->arp_offsets + (uint16_t)(in->arp_table << 2));
    const uint32_t at = table + (uint32_t)(int32_t)(int16_t)in->arp_pos;
    int note_index = (int8_t)ah_u8(m, at) + v->note;
    if (in->transpose_note) {
        note_index += v->transposition;
    }
    old->song.paula[index].period = (uint16_t)(period_of(old, m, (int16_t)note_index) + v->pitch);
    if (ah_u8(m, at + 1) == ARP_LOOP) {
        in->arp_pos = ah_u8(m, at + 2);
    } else {
        in->arp_pos = (uint16_t)((in->arp_pos & 0xFF00u) | (uint8_t)(in->arp_pos + 1));
    }
}

// $33A: the wave-table voice cycles through the waveforms at a bouncing position and restarts its DMA every tick.

static void run_wave(AhOld* old, const AhMemory* m, int index) {
    AhOldInstrument* in = &old->song.voices[index].instrument;
    NuadePaulaChannel* paula = &old->song.paula[index];
    if (!in->wave_table) {
        return;
    }
    in->wave_pos = (uint16_t)(in->wave_pos + in->wave_step);
    const uint8_t high = (uint8_t)(in->wave_pos >> 8);
    if (in->wave_limit_lo == high || in->wave_limit_hi == high) {
        in->wave_step = (int16_t)-in->wave_step;
    }
    const uint32_t waveform = (uint32_t)((in->wave_pos & WAVE_POSITION_MASK) >> WAVE_INDEX_SHIFT) << WAVE_STRIDE_SHIFT;
    ah_set_sample(m, paula, old->tables.wave_base + waveform, WAVE_LENGTH_WORDS);
    nuade_paula_channel_dma_on(paula);
}

// $236: what a voice does on every tick that does not read an event.

static void run_sound(AhOld* old, const AhMemory* m, int index) {
    AhOldSong* s = &old->song;
    AhOldVoice* v = &s->voices[index];
    AhOldInstrument* in = &v->instrument;
    if (in->length_ticks != 0) {
        in->length_ticks--;
        if (in->length_ticks == 0) {
            in->attack_rate = 0;
            in->sustain_level = 0;
            v->env_count = 0;
        }
    }
    run_envelope(v);
    run_sample(old, m, index);
    s->paula[index].volume = ah_paula_volume((s->master * in->level & 0xFFFFu) >> VOLUME_SHIFT);
    v->pitch = (int16_t)(v->pitch + in->slide);
    run_vibrato(v);
    write_period(old, m, index);
    run_wave(old, m, index);
    // No voice read a track entry in this tick and the pattern is over: the next entry.
    if (!s->track_loaded && (ah_u8(m, v->pattern) & NO_PATTERN_BYTE) != 0) {
        load_track(old, m, index);
    }
}

// $96: one voice's tick.

static void run_voice(AhOld* old, const AhMemory* m, int index) {
    AhOldVoice* v = &old->song.voices[index];
    if (v->track == old->tables.track_base) {
        return;
    }
    if (!old->version.music_on_all_voices && index != AH_LAST_VOICE) {
        return;
    }
    v->step_count--;
    if (v->step_count == 0) {
        v->step_count = v->step_ticks;
        if (v->wait == 0 || --v->wait == 0) {
            read_events(old, m, index);
            return;
        }
    }
    run_sound(old, m, index);
}

// Stopped: the volumes are 0 and the DMA is off.

static void silence(AhOldSong* s) {
    for (int i = 0; i < AH_VOICES; i++) {
        s->paula[i].volume = 0;
        nuade_paula_channel_dma_off(&s->paula[i]);
    }
    s->ended = true;
}

void ah_old_tick(AhOld* old, const AhMemory* module) {
    AhOldSong* s = &old->song;
    s->looped = false;
    if (s->master == 0) {
        silence(s);
        return;
    }
    if (s->fade_count != 0) {
        s->fade_count--;
        if (s->fade_count == 0) {
            s->fade_count = s->fade_ticks;
            s->master--;
        }
    }
    if (!s->started) {
        s->started = true;
        for (int i = old->version.music_on_all_voices ? 0 : AH_LAST_VOICE; i < AH_VOICES; i++) {
            load_track(old, module, i);
        }
        return;
    }
    s->track_loaded = false;
    for (int i = 0; i < AH_VOICES; i++) {
        run_voice(old, module, i);
    }
}

// The init routine: the voices' tracks and speed from the subsong's entry; the first tick loads the first patterns.

void ah_old_start(AhOld* old, const AhMemory* module, int subsong) {
    AhOldSong* s = &old->song;
    const AhOldTables* t = &old->tables;
    *s = (AhOldSong) { .master = OLD_DEFAULT_MASTER, .speed = ah_u8(module, t->speeds + (uint32_t)subsong) };
    for (int i = 0; i < AH_VOICES; i++) {
        AhOldVoice* v = &s->voices[i];
        const uint32_t entry = ah_u32(module, t->songs + (uint32_t)subsong * OLD_SONG_ENTRY_SIZE + i * BYTES_PER_LONG);
        v->instrument.transpose_note = true;
        v->instrument.dma_started = true; // a record with sample offset 0: the voice starts out with the silent repeat
        v->step_count = 1;
        v->step_ticks = s->speed;
        v->track = t->track_base + entry;
        v->track_restart = v->track;
        v->alive_initial = entry != 0;
        v->alive = v->alive_initial;
        v->display_note = -1;
        v->display_instrument = -1;
        nuade_paula_channel_dma_off(&s->paula[i]);
    }
}
