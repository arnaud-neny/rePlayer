// SPDX-License-Identifier: MIT
// MusicMaker V8 player: the 4 voice replayer, translated from the UADE eagleplayer "MusicMaker-4V".
//
// The replayer runs from a CIA timer interrupt. A tick reads the next event of every track whose row has run out,
// updates the effects of the four voices and writes the Paula registers. A tick that started a sample does not switch
// the DMA on itself: it re-arms the timer for 0x170 cycles, and that second interrupt sets DMACON, waits for the DMA to
// start and then writes the repeat (the loop) into the registers.

#include "mm4.h"

#define TIMER_PER_TEMPO 23       // CIA cycles of a tick = tempo * 23
#define SECOND_HALF_CYCLES 0x170 // timer value between the two interrupts of a tick that starts samples
// The original re-arms its one-shot CIA timer inside the interrupt, so a tick lasts the timer value plus the time from
// the timer running out to the re-arm. That sets the song's tempo, not the DMA phase: without it the song runs 0.2 %
// fast (0.6 % with the second interrupt). Measured in UADE's trace (the interval of the register writes of a voice that
// writes them every tick): 35 CIA cycles for a tick, 128 for the tick with the second interrupt (its own entry and the
// raster waits of the DMA start).
#define TICK_LATENCY 35
#define SECOND_HALF_LATENCY 128

// The sample the registers point to, AUDxLEN as the replayer writes it (0 plays 65536 words). A sample that runs past
// the instrument block plays zeros there (the block has the Paula tail); a sample outside it nothing.

static void set_sample(NuadePaulaChannel* c, const MmInstruments* inst, uint32_t offset, uint32_t words) {
    c->lc = offset < inst->block.size ? (const int8_t*)inst->block.data + offset : nullptr;
    c->len_words = (uint16_t)words;
}

// What the event wrote into the hardware registers: a cut stops the voice, a note stops its DMA and a new sample goes
// into the registers (the repeat is kept for the DMA start).

static void apply_step(Mm4* e, uint32_t v, const MmStep* step) {
    Mm4Channel* ch = &e->channel[v];
    if (step->cut) {
        ch->paula.volume = 0;
        nuade_paula_channel_dma_off(&ch->paula);
    }
    if (step->stop_dma) {
        nuade_paula_channel_dma_off(&ch->paula);
        ch->restarted = true;
    }
    if (step->enable_dma) {
        ch->dma_enable = true;
    }
    if (step->has_sample) {
        ch->started = true;
        set_sample(&ch->paula, e->core.inst, step->sample, step->length / 2);
        ch->loop_sample = step->loop_sample;
        ch->loop_words = (uint16_t)(step->loop_length / 2);
    }
}

// DMACON set for the voices that have started a note, then the repeat of the voices that started a sample in this
// tick. The replayer waits for the DMA start between the two.

static void finish_tick(Mm4* e) {
    bool started = false;
    for (int v = 0; v < MM4_VOICES; v++) {
        if (e->channel[v].dma_enable) {
            nuade_paula_channel_dma_on(&e->channel[v].paula);
        }
        started = started || e->channel[v].started;
    }
    if (!started) {
        return;
    }
    for (int v = MM4_VOICES - 1; v >= 0; v--) {
        Mm4Channel* ch = &e->channel[v];
        if (!ch->started) {
            continue;
        }
        set_sample(&ch->paula, e->core.inst, ch->loop_sample, ch->loop_words);
        if (ch->loop_sample != e->core.inst->silent) {
            mm_core_start_table_loop(&e->core, (uint32_t)v, (uint16_t)(ch->loop_words * 2));
        }
    }
}

// A tick that started a sample in a voice whose DMA has been stopped for a note needs the second interrupt.

static bool needs_second_half(const Mm4* e) {
    bool started = false;
    bool restarted = false;
    for (int v = 0; v < MM4_VOICES; v++) {
        started = started || e->channel[v].started;
        restarted = restarted || (e->channel[v].dma_enable && e->channel[v].restarted);
    }
    return started && restarted;
}

void mm4_start(Mm4* e, const MmSong* song, const MmInstruments* inst, NuadeVoices* voices) {
    *e = (Mm4) { .timer = (uint16_t)(song->tempo * TIMER_PER_TEMPO), .first_tick = true };
    mm_core_start(&e->core, MmVariant_4V, song, inst, MM4_VOICES);
    for (uint32_t v = 0; v < MM4_VOICES; v++) {
        nuade_voice_stop(voices, v);
    }
}

// One normal tick: the events of the voices (from the last), and the registers.

static void run_tick(Mm4* e) {
    for (int v = 0; v < MM4_VOICES; v++) {
        e->channel[v].started = false;
        e->channel[v].restarted = false;
        e->channel[v].dma_enable = false;
    }
    for (int v = MM4_VOICES - 1; v >= 0; v--) {
        const MmStep step = mm_core_step(&e->core, (uint32_t)v);
        apply_step(e, (uint32_t)v, &step);
        mm_core_update(&e->core, (uint32_t)v, &step);
        if (e->core.tempo_requested) {
            e->core.tempo_requested = false;
            e->timer = (uint16_t)(e->core.new_tempo * TIMER_PER_TEMPO);
        }
        e->channel[v].paula.volume = e->core.voice[v].out_volume;
        e->channel[v].paula.period = e->core.voice[v].out_period;
    }
}

MmTickResult mm4_tick(Mm4* e, NuadeVoices* voices) {
    MmTickResult result = { .length = e->timer }; // the first interrupt is one timer period after the song start
    if (e->first_tick) {
        e->first_tick = false;
    } else if (e->second_half_pending) {
        e->second_half_pending = false;
        finish_tick(e);
        result.length = (uint32_t)e->timer + SECOND_HALF_LATENCY - SECOND_HALF_CYCLES;
    } else {
        result.looped = mm_core_poll_song_end(&e->core);
        run_tick(e);
        if (needs_second_half(e)) {
            e->second_half_pending = true;
            result.length = SECOND_HALF_CYCLES;
        } else {
            finish_tick(e);
            result.length = (uint32_t)e->timer + TICK_LATENCY;
        }
    }
    for (uint32_t v = 0; v < MM4_VOICES; v++) {
        nuade_paula_channel_sync(&e->channel[v].paula, v, voices);
    }
    return result;
}
