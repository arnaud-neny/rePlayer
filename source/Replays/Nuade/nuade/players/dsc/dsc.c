// SPDX-License-Identifier: MIT
// Digital Sonix & Chrome player, translated from the UADE eagleplayer "Digital Sonix & Chrome player module V1.0"
// (Wanted Team; the replayer itself is compiled C).
//
// DscModule: a header (tempo, sound and song-entry counts, sample data size, song length in lines), the song entries
// (a line offset, a repeat count and a line count; an entry with no repeats ends a subsong), the song as four columns
// of sound numbers, one per voice, the sound table and the sample data. A sound byte starts that sound on its voice.
// A sound plays its sample once, then its loop as often as its repeat count says, then a silent block: the audio
// interrupt of the channel chains the blocks.

#include <nuade/players/dsc.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>

#define HEADER_SIZE 0x0C
#define ENTRY_SIZE 6
#define SOUND_SIZE 0x12
#define NO_SOUND 0xFF

// Ticks per line: (header tempo / 2 + TEMPO_SCALE) / header tempo
#define TEMPO_SCALE 1500

// The silent block the interrupt handler chains after the last loop: 2 words (UADE reads zeros there).
#define SILENCE_WORDS 2

static const int8_t k_silence[SILENCE_WORDS * 2] = { 0 };

typedef struct Sound {
    uint16_t period;
    uint32_t length;     // bytes
    uint32_t loop_start; // bytes
    uint16_t repeats;    // loop repeats after the first play, 0 = none
    uint32_t offset;     // into the sample data
    int8_t volume;
} Sound;

typedef struct Entry {
    uint32_t line; // into the columns
    uint8_t repeats;
    uint8_t lines;
} Entry;

static uint8_t mod_byte(const DscModule* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

static uint16_t mod_word(const DscModule* m, uint32_t offset) {
    return (uint16_t)(mod_byte(m, offset) << 8 | mod_byte(m, offset + 1));
}

static uint32_t mod_long(const DscModule* m, uint32_t offset) {
    return (uint32_t)mod_word(m, offset) << 16 | mod_word(m, offset + 2);
}

static Entry entry_at(const DscModule* m, uint32_t index) {
    const uint32_t at = m->entries + index * ENTRY_SIZE;
    return (Entry) { .line = mod_long(m, at), .repeats = mod_byte(m, at + 4), .lines = mod_byte(m, at + 5) };
}

static Sound sound_at(const DscModule* m, uint8_t index) {
    const uint32_t at = m->sounds + (uint32_t)index * SOUND_SIZE;
    return (Sound) { .period = mod_word(m, at),
                     .length = mod_long(m, at + 2),
                     .loop_start = mod_long(m, at + 6),
                     .repeats = mod_word(m, at + 0x0A),
                     .offset = mod_long(m, at + 0x0C),
                     .volume = (int8_t)mod_byte(m, at + 0x10) };
}

// AUDxLC for a byte offset into the sample data of sound `index`. InitPlayer turns the sample offsets of the sound
// table into addresses, so a sound number past the table plays an unrelocated offset (low memory, not the module): it
// gets a null AUDxLC, as does an address past the module. Any AUDxLEN can follow an AUDxLC inside the module: what runs
// past its end plays the zeros behind the padded copy.
static const int8_t* sample_pointer(const DscModule* m, uint8_t index, uint64_t offset) {
    const uint64_t at = m->samples + offset;
    return index < m->sound_count && at <= m->size ? (const int8_t*)m->data + at : nullptr;
}

// The channel's audio interrupt, at every block start: chain the loop again, or the silence after the last one, then
// switch itself off.
static void audio_interrupt(DscVoice* v) {
    if (!v->irq_enabled) {
        return;
    }
    if (v->blocks_left == 0) {
        v->irq_enabled = false;
        return;
    }
    v->blocks_left--;
    if (v->blocks_left == 0) {
        v->paula.lc = k_silence;
        v->paula.len_words = SILENCE_WORDS;
    } else {
        v->paula.lc = v->loop;
        v->paula.len_words = v->loop_words;
    }
}

// Block starts the mixer played since the last tick are the interrupts that fired in between.

static void answer_interrupts(DscVoice* v, uint32_t index, const NuadeVoices* voices) {
    NuadeVoiceState state;
    nuade_voice_get_state(voices, index, &state);
    for (uint32_t n = state.wraps - v->wraps_seen; n > 0; n--) {
        audio_interrupt(v);
    }
    v->wraps_seen = state.wraps;
}

static void start_sound(const DscModule* m, DscVoice* v, uint8_t index) {
    const Sound s = sound_at(m, index);
    v->sound = index;
    nuade_paula_channel_dma_off(&v->paula);
    v->paula.volume = (uint8_t)s.volume;
    v->paula.lc = sample_pointer(m, index, s.offset);
    v->paula.len_words = (uint16_t)(s.length >> 1);
    v->blocks_left = (uint16_t)(s.repeats + 1);
    if (s.repeats != 0) {
        // 32-bit arithmetic as in the replayer: a loop start past the length gives the low word of a negative length
        v->loop_words = (uint16_t)((s.length - s.loop_start) >> 1);
        v->loop = sample_pointer(m, index, (uint64_t)s.offset + s.loop_start);
    }
    v->paula.period = s.period;
    nuade_paula_channel_dma_on(&v->paula);
    v->irq_enabled = true;
    audio_interrupt(v); // DMA start is a block start: its interrupt writes the repeat
}

static void play_line(DscPlayer* p) {
    const DscModule* m = &p->module;
    DscSong* s = &p->song;

    uint8_t bytes[DSC_VOICES];
    for (uint32_t i = 0; i < DSC_VOICES; i++) {
        bytes[i] = mod_byte(m, m->lines + s->line + i * m->rows);
    }
    s->line++;
    for (uint32_t i = 0; i < DSC_VOICES; i++) {
        if (bytes[i] != NO_SOUND) {
            start_sound(m, &s->voices[i], bytes[i]);
        }
    }

    s->row++;
    if (s->row != entry_at(m, s->entry).lines) {
        return;
    }
    s->row = 0;
    s->repeats--;
    if (s->repeats != 0) {
        s->line = entry_at(m, s->entry).line;
        return;
    }
    s->entry++;
    if (entry_at(m, s->entry).repeats == 0) {
        s->entry = s->first_entry;
        s->looped = true;
    }
    const Entry e = entry_at(m, s->entry);
    s->line = e.line;
    s->repeats = e.repeats;
}

bool dsc_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size < HEADER_SIZE) {
        return false;
    }
    const DscModule m = { .data = data, .size = (uint32_t)size };
    const uint8_t sounds = data[2];
    const uint8_t entries = data[3];
    const uint32_t sample_size = mod_long(&m, 4);
    const uint32_t rows = mod_long(&m, 8);
    if (mod_word(&m, 0) == 0 || sounds == 0 || entries < 2 || sample_size == 0 || (sample_size & 1)
        || sample_size > 0x80000 || sample_size >= size || rows == 0 || rows > 0x20000) {
        return false;
    }
    uint32_t at = HEADER_SIZE;
    for (uint32_t i = 0; i + 1 < entries; i++, at += ENTRY_SIZE) {
        const uint32_t line = mod_long(&m, at);
        if ((line & 0x80000000u) || (line & 1) || line > 0x20000) {
            return false;
        }
    }
    if (mod_long(&m, at) != 0 || mod_word(&m, at + 4) != 0) {
        return false; // the last entry is all zero
    }
    at += ENTRY_SIZE + rows * 4;
    const uint32_t end = at + (uint32_t)sounds * SOUND_SIZE;
    if (end >= size) {
        return false;
    }
    uint32_t offset = 0;
    for (; at < end; at += SOUND_SIZE) {
        const uint32_t length = mod_long(&m, at + 2);
        offset = mod_long(&m, at + 0x0C);
        if (length == 0 || (length & 0x80000000u) || length > sample_size || offset > sample_size) {
            return false;
        }
    }
    return offset + mod_long(&m, at - SOUND_SIZE + 2) == sample_size;
}

DscPlayer* dsc_create(const uint8_t* data, size_t size) {
    if (!dsc_probe(data, size)) {
        return nullptr;
    }
    DscPlayer* p = calloc(1, sizeof(DscPlayer));
    uint8_t* copy = nuade_paula_padded_copy(data, size);
    if (p == nullptr || copy == nullptr) {
        free(p);
        free(copy);
        return nullptr;
    }
    p->data = copy;
    DscModule* m = &p->module;
    m->data = copy;
    m->size = (uint32_t)size;
    m->tempo = (int16_t)mod_word(m, 0);
    m->sound_count = copy[2];
    m->entry_count = copy[3];
    m->sample_size = mod_long(m, 4);
    m->rows = mod_long(m, 8);
    m->entries = HEADER_SIZE;
    m->lines = m->entries + (uint32_t)m->entry_count * ENTRY_SIZE;
    m->sounds = m->lines + m->rows * 4;
    m->samples = m->sounds + (uint32_t)m->sound_count * SOUND_SIZE;
    // Every entry without repeats ends a subsong
    for (uint32_t i = 0; i < m->entry_count; i++) {
        if (entry_at(m, i).repeats == 0) {
            m->subsong_count++;
        }
    }
    if (m->subsong_count < 1) {
        dsc_destroy(p);
        return nullptr;
    }
    return p;
}

void dsc_destroy(DscPlayer* player) {
    if (player != nullptr) {
        free(player->data);
        free(player);
    }
}

int dsc_subsong_count(const DscPlayer* player) {
    return player->module.subsong_count;
}

bool dsc_start(DscPlayer* player, int subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong < 0 || subsong >= player->module.subsong_count) {
        return false;
    }
    const DscModule* m = &player->module;
    DscSong* s = &player->song;
    *s = (DscSong) { 0 };

    // Skip `subsong` subsongs: each ends with its entry without repeats
    uint32_t entry = 0;
    for (int n = subsong; n > 0; n--) {
        while (entry_at(m, entry++).repeats != 0) {}
    }
    s->first_entry = entry;
    s->entry = entry;
    const Entry e = entry_at(m, entry);
    s->line = e.line;
    s->repeats = e.repeats;
    s->tempo = (uint8_t)(((int32_t)m->tempo / 2 + TEMPO_SCALE) / m->tempo);
    s->tempo_counter = 1;

    for (uint32_t i = 0; i < DSC_VOICES; i++) {
        DscVoice* v = &s->voices[i];
        v->sound = -1;
        NuadeVoiceState state;
        nuade_voice_stop(voices, i);
        nuade_voice_get_state(voices, i, &state);
        v->wraps_seen = state.wraps;
    }
    return true;
}

DscTickResult dsc_tick(DscPlayer* player, NuadeVoices* voices) {
    DscSong* s = &player->song;
    s->looped = false;
    for (uint32_t i = 0; i < DSC_VOICES; i++) {
        answer_interrupts(&s->voices[i], i, voices);
    }
    s->tempo_counter--;
    if (s->tempo_counter == 0) {
        s->tempo_counter = s->tempo;
        play_line(player);
    }
    for (uint32_t i = 0; i < DSC_VOICES; i++) {
        nuade_paula_channel_sync(&s->voices[i].paula, i, voices);
    }
    return (DscTickResult) { .looped = s->looped };
}

int dsc_tempo(const DscPlayer* player) {
    return player->song.tempo;
}

DscVoiceState dsc_voice_state(const DscPlayer* player, int voice) {
    return (DscVoiceState) { .sound = player->song.voices[voice].sound };
}
