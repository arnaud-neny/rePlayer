// SPDX-License-Identifier: MIT
// Infogrames player: the replayer. One tick steps the four voices in turn; a voice that is due reads its pattern, and
// every voice then derives volume and period from its envelopes and writes its Paula channel.

#include "ig_internal.h"
#include <string.h>

// Pattern byte classes
#define NOTE_COMMAND_BIT 0x80
#define ENVELOPE_COMMAND_BIT 0x40
#define EXTENDED_COMMAND_BIT 0x20
#define COMMAND_NUMBER_MASK 0x1F
#define END_MARK 0xFF

// Commands 0xE0 - 0xE7
typedef enum ExtendedCommand {
    ExtendedCommand_Transpose = 0,
    ExtendedCommand_LoopingPitchEnvelope = 1,
    ExtendedCommand_PitchEnvelope = 2,
    ExtendedCommand_Nothing = 3,
} ExtendedCommand;

#define EXTENDED_COMMAND_MASK 7
#define STEP_COUNT_MASK 0x0F
#define NOTE_C0_INDEX 48    // index 60 is period 856 (C-1): the table starts four octaves below C-0
#define LOOP_SEGMENT 1      // a looping envelope restarts at its second segment
#define FETCH_LIMIT 0x10000 // pattern bytes and track entries one step may read

// Ticks per step of the commands 0x80 - 0x8F. The original reads past the table (the bytes 8, 0, 0, 0) for 12 - 15.
static const uint8_t step_counts[16] = { 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 8, 0, 0, 0 };

static void envelope_start(IgEnvelope* e) {
    e->offset = 0;
    e->level = 0;
    e->segment = 0;
    e->tick = 0;
    e->repeat = 0;
    e->add_delta = true;
    e->done = false;
}

static void envelope_select(IgEnvelope* e, IgEnvelopeRecord record) {
    e->segments = record.segments;
    e->loops = record.loops;
    e->loop_step = record.loop_step;
    envelope_start(e);
}

// One tick of an envelope: `value` plus the level reached, minus the offset; below zero it is 0. A segment adds its
// delta once per repeat and lasts a number of ticks; the loop restarts at the second segment.

static uint16_t envelope_step(IgEnvelope* e, const IgFile* song, uint16_t value) {
    const IgEnvelopeSegment segment = ig_read_envelope_segment(song, e->segments, e->segment);
    if (e->add_delta) {
        e->add_delta = false;
        e->level = (int16_t)(e->level + segment.delta);
    }
    value = (uint16_t)(value + e->level - e->offset);
    if ((value & 0x8000) != 0) {
        value = 0;
    }
    if (e->done) {
        return value;
    }
    e->tick++;
    if (e->tick != segment.ticks) {
        return value;
    }
    e->repeat++;
    e->tick = 0;
    if (e->repeat != segment.repeats) {
        e->add_delta = true;
        return value;
    }
    e->repeat = 0;
    unsigned next = e->segment + 1u;
    if (next == IG_ENVELOPE_SEGMENTS) {
        if (!e->loops) {
            e->done = true;
            return value;
        }
        e->offset = (int16_t)(e->offset + e->loop_step);
        next = LOOP_SEGMENT;
    }
    e->segment = (uint8_t)next;
    e->add_delta = true;
    return value;
}

static void start_note(IgVoice* v, uint8_t note) {
    unsigned index = note;
    if (note != 0) {
        index = (uint8_t)(note + v->transpose);
    }
    v->period = ig_period(index);
    v->display_note = (int)index - NOTE_C0_INDEX;
    envelope_start(&v->amplitude);
    envelope_start(&v->pitch);
}

// A command byte of a pattern. False when the voice stops: commands 0xE4 - 0xE7 jump into data of the original.

static bool run_command(IgPlayer* p, IgVoice* v, uint8_t command) {
    IgSong* song = &p->song;
    if ((command & ENVELOPE_COMMAND_BIT) == 0) {
        if ((command & EXTENDED_COMMAND_BIT) != 0) {
            song->sample_request = command & COMMAND_NUMBER_MASK;
        } else {
            v->step_counter = step_counts[command & STEP_COUNT_MASK];
            v->step_reload = v->step_counter;
        }
        return true;
    }
    if ((command & EXTENDED_COMMAND_BIT) == 0) {
        envelope_select(&v->amplitude, ig_read_envelope_record(&p->module, song->header.amplitude_envelopes,
                                                               command & COMMAND_NUMBER_MASK));
        return true;
    }
    switch ((ExtendedCommand)(command & EXTENDED_COMMAND_MASK)) {
        case ExtendedCommand_Transpose:
            v->transpose = ig_u8(&p->module, v->pattern++);
            return true;
        case ExtendedCommand_LoopingPitchEnvelope:
        case ExtendedCommand_PitchEnvelope: {
            IgEnvelopeRecord record
                = ig_read_envelope_record(&p->module, song->header.pitch_envelopes, ig_u8(&p->module, v->pattern++));
            record.loops = (command & EXTENDED_COMMAND_MASK) == ExtendedCommand_LoopingPitchEnvelope;
            record.loop_step = 0; // the pitch envelope ignores the record's first byte
            envelope_select(&v->pitch, record);
            return true;
        }
        case ExtendedCommand_Nothing:
            return true;
    }
    return false;
}

// Read the pattern up to the next note, running the commands on the way; at the end of a pattern the next one of the
// track list. A track list that ends stops the voice (the song loops only when the original's loop flag is set, and
// nothing sets it). `from_track_list` starts at the track list. False when no note starts.

static bool fetch_note(IgPlayer* p, IgVoice* v, bool from_track_list) {
    const IgFile* song = &p->module;
    for (int guard = 0; guard < FETCH_LIMIT; guard++) {
        if (!from_track_list) {
            const uint8_t byte = ig_u8(song, v->pattern++);
            if (byte == END_MARK) {
                from_track_list = true;
            } else if ((byte & NOTE_COMMAND_BIT) == 0) {
                start_note(v, byte);
                return true;
            } else if (!run_command(p, v, byte)) {
                break;
            }
            continue;
        }
        from_track_list = false;
        const uint8_t entry = ig_u8(song, v->track++);
        if (entry == END_MARK) {
            break;
        }
        v->pattern = p->song.header.base + ig_s16(song, p->song.header.pattern_offsets + 2 * entry);
    }
    v->stopped = true;
    return false;
}

// A sample change a voice asked for: the sample starts, then the repeat registers follow.

static void change_sample(IgPlayer* p, int voice, unsigned number) {
    IgSample sample;
    NuadePaulaChannel* paula = &p->song.paula[voice];
    if (!ig_read_sample(&p->samples, number, &sample)) {
        return;
    }
    const IgFile* file = &p->samples;
    const uint64_t start = (uint64_t)IG_SAMPLE_HEADER_SIZE + sample.start;
    const uint64_t repeat_start = (uint64_t)IG_SAMPLE_HEADER_SIZE + sample.repeat_start;
    paula->lc = start <= file->size ? (const int8_t*)(file->data + (start & ~(uint64_t)1)) : nullptr;
    paula->len_words = sample.length_words;
    nuade_paula_channel_dma_off(paula);
    nuade_paula_channel_dma_on(paula);
    paula->lc = repeat_start <= file->size ? (const int8_t*)(file->data + (repeat_start & ~(uint64_t)1)) : nullptr;
    paula->len_words = sample.repeat_length_words;
    p->song.voices[voice].display_instrument = (int)number;
}

// One voice of one tick. The voice reads its pattern when it is due, then both envelopes run; a stopped voice plays
// volume 0 at period 0.

static void step_voice(IgPlayer* p, int voice) {
    IgSong* song = &p->song;
    IgVoice* v = &song->voices[voice];
    NuadePaulaChannel* paula = &song->paula[voice];
    uint8_t volume = 0;
    uint16_t period = 0;
    if (!v->stopped) {
        bool fetched = true;
        if (v->restart) {
            v->restart = false;
            v->track = v->track_start;
            fetched = fetch_note(p, v, true);
        } else {
            if (song->tempo_counter == 0) {
                v->step_counter--;
            }
            if (v->step_counter == 0) {
                v->step_counter = v->step_reload;
                fetched = fetch_note(p, v, false);
            }
        }
        if (fetched) {
            // The original limits the amplitude to the song's volume, which stays 63 here.
            volume = (uint8_t)envelope_step(&v->amplitude, &p->module, 0);
            period = envelope_step(&v->pitch, &p->module, v->period);
        }
    }
    paula->volume = volume;
    paula->period = period;
    if (song->sample_request != IG_NO_SAMPLE) {
        const unsigned number = (unsigned)song->sample_request;
        song->sample_request = IG_NO_SAMPLE;
        change_sample(p, voice, number);
    }
}

static void start_song(IgPlayer* p) {
    IgSong* song = &p->song;
    memset(song, 0, sizeof(*song));
    song->header = ig_read_song_header(&p->module);
    song->tempo_counter = song->header.tempo;
    song->tempo_reload = song->header.tempo;
    song->sample_request = IG_NO_SAMPLE;
    for (int i = 0; i < IG_VOICES; i++) {
        IgVoice* v = &song->voices[i];
        v->track_start = song->header.track_lists[i];
        v->restart = true;
        v->amplitude.segments = IG_NO_OFFSET;
        v->pitch.segments = IG_NO_OFFSET;
        v->display_note = -1;
        v->display_instrument = -1;
    }
}

bool ig_start(IgPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    start_song(player);
    for (uint32_t i = 0; i < IG_VOICES; i++) {
        nuade_voice_stop(voices, i);
    }
    return true;
}

IgTickResult ig_tick(IgPlayer* player, NuadeVoices* voices) {
    IgSong* song = &player->song;
    song->tempo_counter--;
    for (int i = 0; i < IG_VOICES; i++) {
        step_voice(player, i);
    }
    if (song->tempo_counter == 0) {
        song->tempo_counter = song->tempo_reload;
    }
    bool ended = true;
    for (int i = 0; i < IG_VOICES; i++) {
        nuade_paula_channel_sync(&song->paula[i], (uint32_t)i, voices);
        ended = ended && song->voices[i].stopped;
    }
    return (IgTickResult) { .ended = ended };
}

IgVoiceState ig_voice_state(const IgPlayer* player, int voice) {
    if (voice < 0 || voice >= IG_VOICES) {
        return (IgVoiceState) { .note = -1, .instrument = -1 };
    }
    const IgVoice* v = &player->song.voices[voice];
    return (IgVoiceState) { .note = v->display_note, .instrument = v->display_instrument };
}
