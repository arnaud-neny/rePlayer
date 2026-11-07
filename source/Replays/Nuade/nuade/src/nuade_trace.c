// Voice event trace, see nuade_trace.h.
//
// Lines, one event each: "<cycles> HZ|GAIN|DMA <voice> <value>" and
// "<cycles> BUF <voice> 0 <len_words> <fnv1a32> <nbytes> <allzero> [hex]". A voice is "on" (DMA 1) from its play until
// it is stopped. Pitch and volume are written when they changed by the end of a tick, for voices that are on; BUF is
// the block of sample data the voice starts fetching: at play, and at every loop restart or switch to a queued sample.

#include "nuade_trace.h"
#include "nuade_paula.h"

#include <stdlib.h>

#define MAX_BYTES_SHOWN 512

typedef struct TraceVoice {
    i32 dma;      // -1 unknown, 0 off, 1 on
    f64 last_hz;  // last written; < 0 unknown
    f64 last_vol; // last written; < 0 unknown
    u32 last_buf; // hash of the last BUF written
    bool has_buf;
    bool relog;          // the volume came back from 0: write the next loop restart even if unchanged
    bool mixer_playing;  // the mixer has a sample on the voice
    NuadeSample current; // valid while mixer_playing
    NuadeSample queued;  // valid when has_queued
    bool has_queued;
    f64 hz; // latest values set, written at the end of the tick
    f64 vol;
    bool dirty;
    u64 dirty_frame;
} TraceVoice;

struct NuadeTrace {
    FILE* file;
    bool with_bytes;
    u32 voice_count;
    u32 sample_rate;
    TraceVoice* voices;
};

static void reset_voice(TraceVoice* v) {
    *v = (TraceVoice) { .dma = -1, .last_hz = -1.0, .last_vol = -1.0, .vol = 1.0 };
}

NuadeTrace* nuade_trace_create(FILE* file, u32 voice_count, bool with_bytes) {
    NuadeTrace* trace = (NuadeTrace*)calloc(1, sizeof(NuadeTrace));
    if (trace == nullptr) {
        return nullptr;
    }
    trace->voices = (TraceVoice*)calloc(voice_count ? voice_count : 1, sizeof(TraceVoice));
    if (trace->voices == nullptr) {
        free(trace);
        return nullptr;
    }
    trace->file = file;
    trace->voice_count = voice_count;
    trace->with_bytes = with_bytes;
    nuade_trace_reset(trace, 0);
    return trace;
}

static u64 cycles(const NuadeTrace* trace, u64 frame) {
    return trace->sample_rate ? (u64)((f64)frame * NUADE_PAULA_CLOCK_PAL / (f64)trace->sample_rate) : 0;
}

static void write_value(NuadeTrace* trace, u64 frame, const char* kind, u32 voice, f64 value) {
    fprintf(trace->file, "%llu %s %u %.9g\n", (unsigned long long)cycles(trace, frame), kind, voice, value);
}

// Write the pitch and volume a tick left on a voice that is on.
static void flush_voice(NuadeTrace* trace, u32 index) {
    TraceVoice* v = &trace->voices[index];
    if (!v->dirty) {
        return;
    }
    v->dirty = false;
    if (v->dma != 1) {
        return;
    }
    if (v->hz > 0.0 && v->hz != v->last_hz) {
        v->last_hz = v->hz;
        write_value(trace, v->dirty_frame, "HZ", index, v->hz);
    }
    if (v->vol != v->last_vol) {
        // Fetches while the volume is 0 are inaudible and dropped by the consumer; the loop restart after the volume
        // comes back is then the audible repeat.
        if (v->last_vol == 0.0 && v->vol != 0.0) {
            v->relog = true;
        }
        v->last_vol = v->vol;
        write_value(trace, v->dirty_frame, "GAIN", index, v->vol);
    }
}

static void flush_before(NuadeTrace* trace, u64 frame) {
    for (u32 i = 0; i < trace->voice_count; i++) {
        if (trace->voices[i].dirty && trace->voices[i].dirty_frame < frame) {
            flush_voice(trace, i);
        }
    }
}

static u32 fnv1a(const u8* p, size_t n, bool* allzero) {
    u32 h = 2166136261u;
    u32 any = 0;
    for (size_t i = 0; i < n; i++) {
        any |= p[i];
        h = (h ^ p[i]) * 16777619u;
    }
    *allzero = any == 0;
    return h;
}

// The block a voice starts fetching: frames [from, from + count) of the sample, or the silent dummy block when there is
// none. A DMA start is always written, like UADE does; a repeat of the block already fetched at a loop restart is not.
static void write_buffer(NuadeTrace* trace, u64 frame, u32 index, const NuadeSample* sample, u32 from, u32 count,
                         bool dma_start) {
    static const u8 k_dummy[2] = { 0, 0 };
    TraceVoice* v = &trace->voices[index];
    const u8* bytes = k_dummy;
    size_t n = sizeof(k_dummy);

    if (sample != nullptr && count > 0) {
        const u32 frame_bytes = sample->format == NuadeSampleFormat_S16 ? 2 : 1;
        bytes = (const u8*)sample->data + (size_t)from * frame_bytes;
        n = (size_t)count * frame_bytes;
        n &= ~(size_t)1; // Paula fetches whole words
        if (n == 0) {
            bytes = k_dummy;
            n = sizeof(k_dummy);
        }
    }

    bool zero;
    const u32 hash = fnv1a(bytes, n, &zero);
    if (!dma_start && !v->relog && v->has_buf && hash == v->last_buf) {
        return;
    }
    v->last_buf = hash;
    v->has_buf = true;
    v->relog = false;
    fprintf(trace->file, "%llu BUF %u 0 %zu %08x %zu %u", (unsigned long long)cycles(trace, frame), index, n / 2, hash,
            n, zero ? 1u : 0u);
    if (trace->with_bytes) {
        for (size_t i = 0; i < n && i < MAX_BYTES_SHOWN; i++) {
            fprintf(trace->file, "%s%02x", i ? "" : " ", bytes[i]);
        }
    }
    fputc('\n', trace->file);
}

static u32 sample_end(const NuadeSample* s) {
    return s->loop_length != 0 ? s->loop_start + s->loop_length : s->length;
}

// DMA off with the period parked at 16 (what the asm's StopNOTE and DMA retrigger leave behind).
static void dma_off(NuadeTrace* trace, u64 frame, u32 index) {
    TraceVoice* v = &trace->voices[index];
    if (v->dma != 1) {
        return;
    }
    v->dma = 0;
    write_value(trace, frame, "DMA", index, 0);
    v->last_hz = NUADE_PAULA_CLOCK_PAL / 16.0;
    write_value(trace, frame, "HZ", index, v->last_hz);
}

static void start_sample(NuadeTrace* trace, u64 frame, u32 index, const NuadeSample* sample, u32 start_frame) {
    TraceVoice* v = &trace->voices[index];
    dma_off(trace, frame, index); // a note on a running voice retriggers
    v->current = *sample;
    v->mixer_playing = true;
    v->has_queued = false;
    v->dma = 1;
    v->dirty = true; // pitch and volume are written with the tick's final values
    v->dirty_frame = frame;
    write_value(trace, frame, "DMA", index, 1);
    const u32 end = sample_end(sample);
    write_buffer(trace, frame, index, sample, start_frame < end ? start_frame : 0,
                 end > start_frame ? end - start_frame : 0, true);
}

void nuade_trace_event(NuadeTrace* trace, const NuadeVoiceEvent* event) {
    if (trace == nullptr || event->voice >= trace->voice_count) {
        return;
    }
    flush_before(trace, event->frame);

    const u32 index = event->voice;
    TraceVoice* v = &trace->voices[index];
    switch (event->kind) {
        case NuadeVoiceEvent_Play:
            start_sample(trace, event->frame, index, event->sample, (u32)event->value);
            break;
        case NuadeVoiceEvent_Queue:
            if (v->mixer_playing) {
                v->queued = *event->sample;
                v->has_queued = true;
            } else {
                start_sample(trace, event->frame, index, event->sample, 0);
            }
            break;
        case NuadeVoiceEvent_Stop:
            v->mixer_playing = false;
            v->has_queued = false;
            dma_off(trace, event->frame, index);
            break;
        case NuadeVoiceEvent_Pitch:
            v->hz = event->value;
            v->dirty = true;
            v->dirty_frame = event->frame;
            break;
        case NuadeVoiceEvent_Volume:
            v->vol = event->value;
            v->dirty = true;
            v->dirty_frame = event->frame;
            break;
        case NuadeVoiceEvent_Pan:
            break;
        case NuadeVoiceEvent_Loop:
            write_buffer(trace, event->frame, index, &v->current, v->current.loop_start, v->current.loop_length, false);
            break;
        case NuadeVoiceEvent_Switch:
            v->current = *event->sample;
            v->has_queued = false;
            write_buffer(trace, event->frame, index, &v->current, 0, sample_end(&v->current), false);
            break;
        case NuadeVoiceEvent_End:
            // Paula keeps fetching the silent dummy block until the voice is stopped.
            v->mixer_playing = false;
            write_buffer(trace, event->frame, index, nullptr, 0, 0, false);
            break;
    }
}

void nuade_trace_reset(NuadeTrace* trace, u32 sample_rate) {
    trace->sample_rate = sample_rate;
    for (u32 i = 0; i < trace->voice_count; i++) {
        reset_voice(&trace->voices[i]);
    }
}

void nuade_trace_destroy(NuadeTrace* trace) {
    if (trace == nullptr) {
        return;
    }
    flush_before(trace, UINT64_MAX);
    if (trace->file != nullptr) {
        fclose(trace->file);
    }
    free(trace->voices);
    free(trace);
}
