// SPDX-License-Identifier: MIT
// MusicMaker V8 player: the 8 voice replayer, translated from the UADE eagleplayer "MusicMaker-8V".
//
// The replayer has no timer. It starts the four Paula channels on a silent buffer and lets their audio interrupt drive
// the song: when the buffers have all run out, the interrupt hands every channel the buffer that was mixed last, runs
// one tick of the song and mixes the next buffers (the tick's length in samples depends on the tempo and the periods,
// see mm8_mixer.c). The buffers are played in a loop: Paula keeps playing a buffer until the next one is set.

#include "mm8.h"
#include <stdlib.h>
#include <string.h>

#define BUFFER_EXTRA 64
#define MIN_TICK_LENGTH 1

// The replayer sizes its buffers for the song's tempo at the lowest period, a quarter more.

static uint32_t buffer_size_for(uint16_t tempo) {
    const uint32_t samples = (uint32_t)tempo * MM_BLOCK_PER_TEMPO / MM_PERIOD_MIN;
    return (samples + samples / 4 + BUFFER_EXTRA + 1) & ~1u;
}

bool mm8_start(Mm8* e, const MmSong* song, const MmInstruments* inst, NuadeVoices* voices) {
    mm8_stop(e);
    *e = (Mm8) { .tempo = song->tempo };
    mm_core_start(&e->core, MmVariant_8V, song, inst, MM8_VOICES);
    e->buffer_size = buffer_size_for(song->tempo);
    e->silent_capacity = mm8_silent_words(song->tempo) + MM8_SILENT_SLACK;
    e->memory = calloc((size_t)MM8_CHANNELS * 2 * e->buffer_size + (size_t)e->silent_capacity * 2, 1);
    e->tables = calloc((size_t)MM_VOLUME_ROWS * MM_VOLUME_ROW_SIZE, 1);
    if (e->memory == nullptr || e->tables == nullptr) {
        mm8_stop(e);
        return false;
    }
    mm8_build_tables(e->tables);
    int8_t* silent = e->memory + (size_t)MM8_CHANNELS * 2 * e->buffer_size;
    for (uint32_t c = 0; c < MM8_CHANNELS; c++) {
        Mm8Channel* ch = &e->channel[c];
        ch->buffer[0] = e->memory + (size_t)(2 * c) * e->buffer_size;
        ch->buffer[1] = e->memory + (size_t)(2 * c + 1) * e->buffer_size;
        ch->accum = 0x10000;
        ch->period = MM_SILENT_PERIOD;
        ch->volume = 0;
        // DMA starts on the silent buffer
        ch->paula.lc = silent;
        ch->paula.len_words = (uint16_t)mm8_silent_words(song->tempo);
        ch->paula.period = MM_SILENT_PERIOD;
        ch->paula.volume = 0;
        nuade_paula_channel_dma_on(&ch->paula);
        nuade_voice_stop(voices, c);
    }
    return true;
}

void mm8_stop(Mm8* e) {
    free(e->memory);
    free(e->tables);
    e->memory = nullptr;
    e->tables = nullptr;
}

// What an event wrote into the software registers of a voice.

static void apply_step(Mm8Voice* r, const MmStep* step) {
    if (step->cut) {
        // everything the voice plays stops: no sample, no loop
        *r = (Mm8Voice) { .started = true,
                          .data_ptr = r->data_ptr,
                          .data_remaining = r->data_remaining,
                          .resume_ptr = r->resume_ptr,
                          .resume_remaining = r->resume_remaining };
    }
    if (step->save_resume) {
        r->resume_ptr = r->data_ptr;
        r->resume_remaining = r->data_remaining;
    }
    if (step->has_sample) {
        r->started = true;
        r->sample = step->sample;
        r->length = (uint16_t)step->length;
        r->loop_sample = step->loop_sample;
        r->loop_length = (uint16_t)step->loop_length;
    }
}

// A note of the slot 0x24 continues the sample where the voice's last 0xF1 event left it.

static MmStep resolve_resume(const Mm8Voice* r, MmStep step) {
    if (step.has_sample && step.resume) {
        step.sample = r->resume_ptr;
        step.length = r->resume_remaining;
    }
    return step;
}

// One tick of the song: the events and effects of the eight voices, from the last.

static void run_tick(Mm8* e) {
    MmCore* core = &e->core;
    for (int v = 0; v < MM8_VOICES; v++) {
        e->voice[v].started = false;
    }
    for (int v = MM8_VOICES - 1; v >= 0; v--) {
        Mm8Voice* r = &e->voice[v];
        const MmStep step = resolve_resume(r, mm_core_step(core, (uint32_t)v));
        apply_step(r, &step);
        mm_core_update(core, (uint32_t)v, &step);
        if (core->tempo_requested) {
            core->tempo_requested = false;
            e->tempo_pending = true;
            e->pending_tempo = core->new_tempo;
        }
        r->period = core->voice[v].out_period;
        r->volume = core->voice[v].out_volume;
        if (!core->song->has_track[v]) {
            // a track the song does not have plays its events (they can change the tempo) but not a loop
            r->loop_sample = 0;
            r->loop_length = 0;
        } else if (r->started && r->loop_sample != 0) {
            mm_core_start_table_loop(core, (uint32_t)v, r->loop_length);
        }
    }
}

MmTickResult mm8_tick(Mm8* e, NuadeVoices* voices) {
    MmTickResult result = { 0 };
    // The audio interrupt: a pending tempo takes effect, then the song end is checked
    if (e->tempo_pending) {
        e->tempo_pending = false;
        e->tempo = e->pending_tempo;
        mm_core_set_tempo(&e->core, e->tempo);
    }
    result.looped = mm_core_poll_song_end(&e->core);
    // The buffers that were mixed last are playing (their period and volume are set now, the lengths were set when they
    // were mixed). The next interrupt comes when the last of them has run out; a channel whose buffer ends earlier
    // loops it until then (the next buffer is set by the interrupt).
    uint64_t next = e->now;
    for (uint32_t c = 0; c < MM8_CHANNELS; c++) {
        Mm8Channel* ch = &e->channel[c];
        ch->paula.period = ch->period;
        ch->paula.volume = (uint8_t)ch->volume;
        ch->restart += (uint64_t)ch->paula.len_words * 2 * ch->period;
        if (ch->restart > next) {
            next = ch->restart;
        }
        ch->paula.lc = ch->buffer[0];
    }
    run_tick(e);
    for (uint32_t c = 0; c < MM8_CHANNELS; c++) {
        Mm8Channel* ch = &e->channel[c];
        mm8_mix_channel(e, c);
        ch->paula.len_words = ch->len_words;
        int8_t* mixed = ch->buffer[0];
        ch->buffer[0] = ch->buffer[1];
        ch->buffer[1] = mixed;
        for (int i = 0; i < 2; i++) {
            Mm8Voice* r = &e->voice[mm8_pair_voice_index(c, i)];
            r->data_ptr = ch->stream[i].ptr;
            r->data_remaining = ch->stream[i].remaining;
        }
        nuade_paula_channel_sync(&ch->paula, c, voices);
    }
    result.length = (uint32_t)(next > e->now ? next - e->now : MIN_TICK_LENGTH);
    e->now = next > e->now ? next : e->now + MIN_TICK_LENGTH;
    return result;
}
