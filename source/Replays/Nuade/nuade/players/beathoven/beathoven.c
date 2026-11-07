// SPDX-License-Identifier: MIT
// Beathoven Synthesizer player, translated from the UADE eagleplayer "Beathoven Synthesizer" (Wanted Team) and the
// replayers in the modules: the play interrupt, starting a subsong and the player's life cycle. The replayers' voice
// routines are in beathoven_voice.c and beathoven_legacy.c.

#include "beathoven_internal.h"
#include <stdlib.h>
#include <string.h>

// The play interrupt without a CIA timer value of its own: DeliTracker's 50 Hz
#define DEFAULT_TICK_LENGTH 14187u

// The first tick runs the first commands of every track: the init routine sets the note length counter to 1
#define INITIAL_DURATION 1
// What the eagleplayer's InitSound leaves in the channels before the init routine: a one word sample with period 1
#define INIT_LENGTH_WORDS 1
#define INIT_PERIOD 1

// The hardware ignores the lowest bit of the address
void bth_set_address(const BthPlayer* p, BthVoice* v, uint32_t address, uint16_t length_words) {
    v->paula.lc = bthm_sample(&p->work, address & ~1u, length_words);
    v->paula.len_words = length_words;
}

void bth_wrap_track(BthVoice* v) {
    v->track = v->loop;
    v->wrapped = true;
}

int bth_instrument_index(const BthPlayer* p, uint32_t record, uint32_t record_bytes) {
    const BthHeader* h = &p->header;
    return record >= h->samples && record < h->samples_end ? (int)((record - h->samples) / record_bytes) : -1;
}

uint16_t bth_table_word(const BthPlayer* p, uint32_t address) {
    const BthImage* image = p->image;
    // A longword the loader relocated holds the pointer's value in memory: the offset plus the hunk's address
    if (bthi_is_pointer(image, address)) {
        return (uint16_t)((bthm_u32(&p->work, address) + image->load_address) >> 16);
    }
    if (address >= 2 && bthi_is_pointer(image, address - 2)) {
        return (uint16_t)(bthm_u32(&p->work, address - 2) + image->load_address);
    }
    return bthm_u16(&p->work, address);
}

// The last voice of a module runs only while a word of the module has a value
static bool voice_runs(const BthPlayer* p, uint32_t index) {
    const BthReplayer* r = &p->replayer;
    if (index >= r->voice_count) {
        return false;
    }
    if (index + 1 < r->voice_count || r->last_voice_flag == 0) {
        return true;
    }
    return (bthm_u16(&p->work, r->last_voice_flag) != 0) == r->last_voice_runs_when_set;
}

BthTickResult bth_tick(BthPlayer* p, NuadeVoices* voices) {
    const bool legacy = p->replayer.engine == BthEngine_Legacy;
    const bool playing = p->replayer.play_flag == 0 || bthm_u16(&p->work, p->replayer.play_flag) != 0;
    for (uint32_t i = 0; i < BTH_VOICES; i++) {
        BthVoice* v = &p->voices[i];
        if (p->replayer.play_silences_channels) {
            bth_set_address(p, v, p->replayer.silent, 1);
        }
        v->runs = playing && voice_runs(p, i);
        if (v->runs && legacy) {
            bth_update_voice_legacy(p, v, p->replayer.note_offsets[i]);
        } else if (v->runs) {
            bth_update_voice(p, v);
        }
        nuade_paula_channel_sync(&v->paula, i, voices);
    }
    // SongEnd: every voice that plays has passed the end of its track list
    bool any_runs = false;
    bool all_wrapped = true;
    for (uint32_t i = 0; i < BTH_VOICES; i++) {
        any_runs = any_runs || p->voices[i].runs;
        all_wrapped = all_wrapped && (!p->voices[i].runs || p->voices[i].wrapped);
    }
    all_wrapped = all_wrapped && any_runs;
    if (all_wrapped) {
        for (uint32_t i = 0; i < BTH_VOICES; i++) {
            p->voices[i].wrapped = false;
        }
    }
    return (BthTickResult) { .looped = all_wrapped };
}

// The init routine's record set-up: the first commands of the track
static void init_voice(const BthPlayer* p, BthVoice* v, const BthTrackStart* start) {
    v->track = start->track;
    v->loop = start->loop;
    v->duration = INITIAL_DURATION;
    const BthTrackEntry entry = bthm_track_entry(&p->work, start->track);
    v->command = entry.stream;
    v->note_base = entry.note_base;
}

bool bth_start(BthPlayer* p, uint32_t subsong, NuadeVoices* voices) {
    if (subsong >= p->song_count) {
        return false;
    }
    const BthSong* song = &p->songs[subsong];
    memcpy(p->work.data, p->image->module.data, p->work.size);
    for (uint32_t i = 0; i < song->write_count; i++) {
        bthm_set16(&p->work, song->writes[i].address, song->writes[i].value);
    }
    for (uint32_t i = 0; i < BTH_VOICES; i++) {
        BthVoice* v = &p->voices[i];
        *v = (BthVoice) { .note = -1, .instrument = -1 };
        v->paula = (NuadePaulaChannel) { .len_words = INIT_LENGTH_WORDS, .period = INIT_PERIOD };
        if (i < p->replayer.voice_count) {
            init_voice(p, v, &song->voices[i]);
        }
        if (p->replayer.engine != BthEngine_Legacy) {
            nuade_paula_channel_dma_on(&v->paula);
        }
        nuade_voice_stop(voices, i);
    }
    return true;
}

BthVoiceState bth_voice_state(const BthPlayer* p, int voice) {
    if (voice < 0 || voice >= BTH_VOICES) {
        return (BthVoiceState) { .note = -1, .instrument = -1 };
    }
    return (BthVoiceState) { .note = p->voices[voice].note, .instrument = p->voices[voice].instrument };
}

uint32_t bth_subsong_count(const BthPlayer* p) {
    return p->song_count;
}

uint32_t bth_tick_length(const BthPlayer* p) {
    return p->tick_length;
}

// The unit the lengths of a subsong are multiples of: the module's tempo (the init routine may set it); a BEATHOVEN100
// length counts ticks
uint32_t bth_ticks_per_step(const BthPlayer* p, uint32_t subsong) {
    const BthWord* tempo = &p->replayer.tempo;
    if (p->replayer.engine == BthEngine_Legacy || subsong >= p->song_count) {
        return 1;
    }
    uint32_t value = tempo->in_module ? bthm_u16(&p->image->module, tempo->address) : tempo->constant;
    const BthSong* song = &p->songs[subsong];
    for (uint32_t i = 0; tempo->in_module && i < song->write_count; i++) {
        if (song->writes[i].address == tempo->address) {
            value = song->writes[i].value;
        }
    }
    return value == 0 ? 1 : value;
}

// A string of the module's information: Latin-1 in the module, UTF-8 in the tag. Empty if the pointer is bad or the
// string is not terminated in the module.
static void copy_string(const BthModule* m, uint32_t address, char* out, size_t capacity) {
    size_t length = 0;
    for (uint32_t at = address; at < m->size && m->data[at] != 0; at++) {
        const uint8_t c = m->data[at];
        const size_t bytes = c < 0x80 ? 1 : 2;
        if (length + bytes + 1 > capacity) {
            break;
        }
        if (c < 0x80) {
            out[length++] = (char)c;
        } else {
            out[length++] = (char)(0xC0 | (c >> 6));
            out[length++] = (char)(0x80 | (c & 0x3F));
        }
    }
    out[length] = '\0';
}

const char* bth_title(const BthPlayer* p) {
    return p->title;
}

const char* bth_author(const BthPlayer* p) {
    return p->author;
}

void bth_destroy(BthPlayer* p) {
    if (p != nullptr) {
        bthi_free(p->image);
        free(p->work.data);
        bth_songs_free(p->songs, p->song_count);
        free(p->songs);
        free(p);
    }
}

bool bth_probe(const uint8_t* data, size_t size) {
    BthPlayer* p = bth_create(data, size);
    const bool opens = p != nullptr;
    bth_destroy(p);
    return opens;
}

BthPlayer* bth_create(const uint8_t* data, size_t size) {
    BthImage* image = bthi_load(data, size);
    if (image == nullptr) {
        return nullptr;
    }
    BthPlayer* p = calloc(1, sizeof(*p));
    if (p == nullptr) {
        bthi_free(image);
        return nullptr;
    }
    p->image = image;
    p->header = bthi_header(image);
    BthHeader* header = &p->header;
    const uint32_t capacity = header->subsong_count < BTH_MAX_SUBSONGS ? header->subsong_count : BTH_MAX_SUBSONGS;
    p->songs = calloc(capacity, sizeof(BthSong));
    p->work = (BthModule) { .data = nuade_paula_padded_copy(nullptr, image->module.size), .size = image->module.size };
    if (p->songs == nullptr || p->work.data == nullptr || !bth_apply_once(&image->module, header)
        || !bth_locate(image, header, &p->replayer)) {
        bth_destroy(p);
        return nullptr;
    }
    memcpy(p->work.data, image->module.data, image->module.size);
    p->song_count = bth_decode_songs(image, header, &p->replayer, p->songs, capacity);
    if (p->song_count == 0) {
        bth_destroy(p);
        return nullptr;
    }
    copy_string(&image->module, header->title, p->title, sizeof(p->title));
    copy_string(&image->module, header->author, p->author, sizeof(p->author));
    p->tick_length = header->timer != 0 ? header->timer : DEFAULT_TICK_LENGTH;
    return p;
}
