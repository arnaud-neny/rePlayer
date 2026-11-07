// SPDX-License-Identifier: MIT
// EMS v6 player, translated from the UADE eagleplayer "EMS-6" (EMS v6 player by Sean Connolly, audio mixer code by
// Jarno Paananen, adapted for UADE by shd).
//
// Ems6Module: "E.M.S. V6.xx", the number of subsongs (word), three section sizes (longs) at 0x0E, then from 0x22 the
// arpeggio lists, the subsong headers with the track lists, the patterns and the instruments.
//
// The eagleplayer mixes in software, from a CIA timer interrupt: it keeps the four Paula buffers filled up to two
// interrupts behind a model of the DMA position (which channel 0's audio interrupt, once per lap, sets back to the
// start), and runs the sequencer once per 1/50 s of mixed output.

#include "emsv6_internal.h"
#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <nuade/players/emsv6.h>
#include <stdlib.h>
#include <string.h>

#define SIGNATURE "E.M.S. V6."
#define SIGNATURE_LENGTH 10
#define VERSION_OFFSET 8     // "6.05" of "E.M.S. V6.05"
#define HEADER_SECTIONS 0x0E // three section sizes
#define HEADER_SIZE 0x22
#define SUBSONG_HEADER_SIZE 0x84
#define SUBSONG_TRACKS 4 // offset of the track list offsets in a header
#define INTERRUPT_GUARD 100000

#define SVX_TAG_OFFSET 8
#define SVX_TAG 0x38535658     // "8SVX"
#define SVX_ONE_SHOT 0x14      // samples before the loop
#define SVX_REPEAT 0x18        // samples of the loop
#define SVX_DATA 0x68          // the sample data, at a fixed offset
#define WAVETABLE_TAG 0x53594E // "SYN" in the high three bytes of the long at the start
#define WAVETABLE_HEADER 6     // "SYN", the wave size, 2 bytes
#define RAW_TAIL 2             // a raw sample ends two bytes before the next instrument

// The Paula channels play the rings in this order; the low halves play at volume 1 next to the high halves' 64.
#define PAULA_BLOCK_WORDS (E6_RING_SIZE / 2)
#define VOLUME_HIGH 64
#define VOLUME_LOW 1

static const E6Ring k_channel_ring[EMS6_VOICES] = {
    E6Ring_HighLeft,
    E6Ring_HighRight,
    E6Ring_LowRight,
    E6Ring_LowLeft,
};

uint8_t e6_module_u8(const Ems6Module* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

uint16_t e6_module_u16(const Ems6Module* m, uint32_t offset) {
    return (uint16_t)(e6_module_u8(m, offset) << 8 | e6_module_u8(m, offset + 1));
}

uint32_t e6_module_u32(const Ems6Module* m, uint32_t offset) {
    return (uint32_t)e6_module_u16(m, offset) << 16 | e6_module_u16(m, offset + 2);
}

Ems6Subsong e6_module_subsong(const Ems6Module* m, uint32_t index) {
    const uint32_t header = m->subsongs + index * SUBSONG_HEADER_SIZE;
    Ems6Subsong subsong = { .channels = e6_module_u16(m, header) };
    for (int i = 0; i < E6_TRACKS; i++) {
        subsong.track_lists[i] = m->subsongs + e6_module_u32(m, header + SUBSONG_TRACKS + (uint32_t)i * 4);
    }
    return subsong;
}

static uint32_t instrument_start(const Ems6Module* m, uint32_t instrument) {
    return m->instruments + e6_module_u32(m, m->instruments + instrument * 4);
}

// Instrument n runs from its offset in the table to the next one's. An 8SVX with a repeat part loops that part (a
// header of fixed size, then the data); without one it plays once from its data. "SYN" + a wave size of 32, 64 or 128
// is a wavetable; anything else is raw data.

E6Sample e6_module_sample(const Ems6Module* m, uint32_t instrument) {
    const uint32_t start = instrument_start(m, instrument);
    const uint32_t end = instrument_start(m, instrument + 1);
    E6Sample sample = { .kind = E6SampleKind_Raw, .data = start };
    if (e6_module_u32(m, start + SVX_TAG_OFFSET) == SVX_TAG) {
        const uint32_t one_shot = e6_module_u32(m, start + SVX_ONE_SHOT);
        const uint32_t repeat = e6_module_u32(m, start + SVX_REPEAT);
        sample.data = start + SVX_DATA;
        if (repeat != 0) {
            sample.kind = E6SampleKind_Looped;
            sample.length = one_shot + repeat;
            sample.loop_start = one_shot;
            sample.loop_length = repeat;
            return sample;
        }
    } else if (e6_module_u32(m, start) >> 8 == WAVETABLE_TAG) {
        const uint8_t size = e6_module_u8(m, start + 3);
        if (size == 0x20 || size == 0x40 || size == 0x80) {
            sample.kind = E6SampleKind_Wavetable;
            sample.data = start + WAVETABLE_HEADER;
            sample.length = size;
            sample.loop_length = size;
            return sample;
        }
    }
    sample.length = end - sample.data - RAW_TAIL;
    return sample;
}

uint32_t e6_module_wave_data(const Ems6Module* m, uint32_t instrument) {
    return instrument_start(m, instrument) + WAVETABLE_HEADER;
}

uint8_t e6_module_arpeggio_entry(const Ems6Module* m, uint32_t table, uint32_t pos) {
    const uint32_t list = m->arpeggios + e6_module_u32(m, m->arpeggios + table * 4);
    return e6_module_u8(m, list + pos);
}

uint32_t e6_module_pattern(const Ems6Module* m, uint32_t number) {
    return m->patterns + e6_module_u32(m, m->patterns + number * 4);
}

PatternCommand e6_module_pattern_command(const Ems6Module* m, uint32_t offset) {
    PatternCommand cmd = { .code = e6_module_u8(m, offset) };
    for (int i = 0; i < 4; i++) {
        cmd.args[i] = e6_module_u8(m, offset + 1 + (uint32_t)i);
    }
    return cmd;
}

// Check2: "E.M.S. V6.". The section sizes must also fit the file, which the replayer does not look at.

bool ems6_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size < HEADER_SIZE || size > 0x7FFFFFFF || memcmp(data, SIGNATURE, SIGNATURE_LENGTH) != 0) {
        return false;
    }
    const Ems6Module m = { .data = (uint8_t*)data, .size = (uint32_t)size };
    const uint64_t subsongs = HEADER_SIZE + (uint64_t)e6_module_u32(&m, HEADER_SECTIONS);
    const uint64_t patterns = subsongs + e6_module_u32(&m, HEADER_SECTIONS + 4);
    const uint64_t instruments = patterns + e6_module_u32(&m, HEADER_SECTIONS + 8);
    return subsongs + SUBSONG_HEADER_SIZE <= size && instruments <= size;
}

Ems6Player* ems6_create(const uint8_t* data, size_t size) {
    if (!ems6_probe(data, size)) {
        return nullptr;
    }
    Ems6Player* p = calloc(1, sizeof(Ems6Player));
    uint8_t* copy = malloc(size);
    if (p == nullptr || copy == nullptr) {
        free(p);
        free(copy);
        return nullptr;
    }
    memcpy(copy, data, size);
    Ems6Module* m = &p->module;
    m->data = copy;
    m->size = (uint32_t)size;
    memcpy(p->version, data + VERSION_OFFSET, EMS6_VERSION_LENGTH);
    m->arpeggios = HEADER_SIZE;
    m->subsongs = m->arpeggios + e6_module_u32(m, HEADER_SECTIONS);
    m->patterns = m->subsongs + e6_module_u32(m, HEADER_SECTIONS + 4);
    m->instruments = m->patterns + e6_module_u32(m, HEADER_SECTIONS + 8);
    p->subsong = e6_module_subsong(m, 0);
    p->used_tracks = e6_sequencer_used_tracks(m, &p->subsong);
    if (!e6_mixer_init(&p->mixer, p->subsong.channels)) {
        free(copy);
        free(p);
        return nullptr;
    }
    return p;
}

void ems6_destroy(Ems6Player* player) {
    if (player == nullptr) {
        return;
    }
    e6_mixer_free(&player->mixer);
    free(player->module.data);
    free(player);
}

const char* ems6_version(const Ems6Player* player) {
    return player->version;
}

// The Paula channels loop the four rings from the start.

static void start_paula(Ems6Player* p, NuadeVoices* voices) {
    for (int i = 0; i < EMS6_VOICES; i++) {
        NuadePaulaChannel* c = &p->paula[i];
        *c = (NuadePaulaChannel) { 0 };
        nuade_voice_stop(voices, (uint32_t)i);
        c->lc = p->mixer.ring[k_channel_ring[i]];
        c->len_words = PAULA_BLOCK_WORDS;
        c->period = E6_PAULA_PERIOD;
        c->volume = i < 2 ? VOLUME_HIGH : VOLUME_LOW;
        nuade_paula_channel_dma_on(c);
    }
}

bool ems6_start(Ems6Player* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    e6_sequencer_init(&player->seq, &player->subsong);
    e6_mixer_reset(&player->mixer);
    start_paula(player, voices);
    player->seen_wraps = 0;
    player->first_tick = true;
    player->finished = false;
    return true;
}

// Fill the rings with as much as fits, one sequencer tick of output at a time.

static void mix_ahead(Ems6Player* p) {
    Ems6Mixer* mx = &p->mixer;
    uint16_t room = e6_mixer_room(mx);
    if (room == 0) {
        return;
    }
    for (int guard = 0; guard < INTERRUPT_GUARD; guard++) {
        const uint16_t until = mx->until_seq_tick;
        if ((int16_t)until > (int16_t)room) {
            mx->until_seq_tick = (uint16_t)(until - room);
            if (room != 0) {
                e6_mixer_render_block(mx, &p->module, room);
            }
            return;
        }
        room = (uint16_t)(room - until);
        mx->until_seq_tick = 0;
        if (until != 0) {
            e6_mixer_render_block(mx, &p->module, until);
        }
        e6_sequencer_tick(&p->seq, mx, &p->module);
        mx->until_seq_tick = (uint16_t)(mx->until_seq_tick + mx->seq_tick_samples);
    }
}

// Channel 0's audio interrupt (one per lap of the rings): the model of the DMA position starts over.

static void answer_audio_interrupts(Ems6Player* p, NuadeVoices* voices) {
    NuadeVoiceState state;
    nuade_voice_get_state(voices, 0, &state);
    if (state.wraps != p->seen_wraps) {
        p->seen_wraps = state.wraps;
        p->mixer.read_pos = 0;
    }
}

// The CIA timer interrupt.

Ems6TickResult ems6_tick(Ems6Player* player, NuadeVoices* voices) {
    Ems6TickResult result = { 0 };
    if (player->first_tick) {
        // The DMA is on and the timer started; the first interrupt is one period away.
        player->first_tick = false;
    } else {
        answer_audio_interrupts(player, voices);
        Ems6Mixer* mx = &player->mixer;
        mx->read_pos = (mx->read_pos + mx->per_tick) & E6_READ_POS_MASK;
        mix_ahead(player);
        if (!player->finished && e6_sequencer_finished(&player->seq)) {
            player->finished = true;
            result.looped = true;
        }
    }
    for (int i = 0; i < EMS6_VOICES; i++) {
        nuade_paula_channel_sync(&player->paula[i], (uint32_t)i, voices);
    }
    return result;
}

Ems6TrackState ems6_track_state(const Ems6Player* player, uint32_t track) {
    if (track >= EMS6_TRACKS || player->seq.tracks[track].note_starts == 0) {
        return (Ems6TrackState) { .note = -1, .instrument = -1 };
    }
    const Ems6Track* t = &player->seq.tracks[track];
    const Ems6TrackState state
        = { .note = t->note & E6_NOTE_MASK, .instrument = t->instrument, .note_starts = t->note_starts };
    return state;
}

uint32_t ems6_track_count(const Ems6Player* player) {
    return player->used_tracks;
}
