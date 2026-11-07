// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/fred_np.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nuade_base.h"
#include "nuade_paula.h"

#define FRED_CHANNELS 4

// Enums

typedef enum FredSynchronizeFlag {
    FRED_SYNC_PULSE_XSHOT = 0x01,
    FRED_SYNC_PULSE_SYNC = 0x02,
    FRED_SYNC_BLEND_XSHOT = 0x04,
    FRED_SYNC_BLEND_SYNC = 0x08
} FredSynchronizeFlag;

typedef enum FredVibFlags {
    FRED_VIB_NONE = 0,
    FRED_VIB_VIB_DIRECTION = 0x01,
    FRED_VIB_PERIOD_DIRECTION = 0x02
} FredVibFlags;

// Tables

static const uint32_t fred_period_table[72]
    = { 8192, 7728, 7296, 6888, 6504, 6136, 5792, 5464, 5160, 4872, 4600, 4336, 4096, 3864, 3648, 3444, 3252, 3068,
        2896, 2732, 2580, 2436, 2300, 2168, 2048, 1932, 1824, 1722, 1626, 1534, 1448, 1366, 1290, 1218, 1150, 1084,
        1024, 966,  912,  861,  813,  767,  724,  683,  645,  609,  575,  542,  512,  483,  456,  430,  406,  383,
        362,  341,  322,  304,  287,  271,  256,  241,  228,  215,  203,  191,  181,  170,  161,  152,  143,  135

      };

// Track command codes

#define TRACK_END_CODE 0x80
#define TRACK_PORT_CODE 0x81
#define TRACK_TEMPO_CODE 0x82
#define TRACK_INST_CODE 0x83
#define TRACK_PAUSE_CODE 0x84
#define TRACK_MAX_CODE 0xa0

// A synthesized waveform holds this many bytes, whatever length an instrument claims.
#define FRED_SYNTH_SIZE 64
// Bytes after the last byte of a track that read as end codes, so a command cut short by the end of its track still
// ends it.
#define FRED_TRACK_PAD 4
// Longest track: the 16-bit track cursor must reach the end codes after it without wrapping.
#define FRED_MAX_TRACK_SIZE (0x10000 - FRED_TRACK_PAD)
#define FRED_MAX_SUB_SONGS 10
#define FRED_POSITION_COUNT 256
// Steps through one position list in a single track change: every entry once, and a restart.
#define FRED_POSITION_STEP_LIMIT (2 * FRED_POSITION_COUNT)
// Bytes one instrument occupies in the module.
#define FRED_INSTRUMENT_FILE_SIZE 96
// Work one channel may do in a tick without finding a note; a position list of empty tracks never would.
#define FRED_MAX_NEW_LINE_RETRIES 256
// Effect steps that may run without producing a sample change; sane pulse and blend settings need at most two.
#define FRED_MAX_EFFECT_STEPS 4
// Largest blend shift: the blend position is 16 bits wide.
#define FRED_MAX_BLEND_SHIFT 15

// Reader helpers

typedef struct FredReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
} FredReader;

static void reader_init(FredReader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
}

static bool reader_eof(const FredReader* r) {
    return r->pos > r->size;
}

// True when `count` records of `unit` bytes still fit in the data; checked before allocating for them.
static bool reader_can_hold(const FredReader* r, uint64_t count, uint64_t unit) {
    return r->pos <= r->size && count <= (uint64_t)(r->size - r->pos) / unit;
}

static void reader_seek(FredReader* r, size_t pos) {
    r->pos = pos;
}

static void reader_skip(FredReader* r, size_t n) {
    r->pos += n;
}

static uint8_t reader_read_uint8(FredReader* r) {
    if (r->pos >= r->size) {
        r->pos = r->size + 1;
        return 0;
    }
    return r->data[r->pos++];
}

static int8_t reader_read_int8(FredReader* r) {
    return (int8_t)reader_read_uint8(r);
}

static uint16_t reader_read_b_uint16(FredReader* r) {
    uint8_t hi = reader_read_uint8(r);
    uint8_t lo = reader_read_uint8(r);
    return (uint16_t)((hi << 8) | lo);
}

static int32_t reader_read_b_int32(FredReader* r) {
    uint8_t b0 = reader_read_uint8(r);
    uint8_t b1 = reader_read_uint8(r);
    uint8_t b2 = reader_read_uint8(r);
    uint8_t b3 = reader_read_uint8(r);
    return (int32_t)(((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | b3);
}

static uint32_t reader_read_b_uint32(FredReader* r) {
    return (uint32_t)reader_read_b_int32(r);
}

static void reader_read_bytes(FredReader* r, uint8_t* out, size_t count) {
    for (size_t i = 0; i < count; i++)
        out[i] = reader_read_uint8(r);
}

static void reader_read_signed(FredReader* r, int8_t* out, size_t count) {
    for (size_t i = 0; i < count; i++)
        out[i] = reader_read_int8(r);
}

// Mix channel operations (IChannel equivalents)

static void mix_play_sample(FredMixChannel* mc, int8_t* addr, uint32_t data_size, uint32_t length) {
    mc->sample_data = addr;
    mc->data_size = data_size;
    mc->sample_length = length;
    mc->loop_start = 0;
    mc->loop_length = 0;
    mc->active = true;
    mc->play_pending = true;
}

static void mix_set_loop(FredMixChannel* mc, uint32_t start, uint32_t length) {
    mc->loop_start = start;
    mc->loop_length = length;
}

static void mix_set_volume(FredMixChannel* mc, uint16_t vol) {
    mc->volume = vol;
}

// The replayer writes the Paula volume register (0-64) as envelope * instrument volume / 512 and the hardware caps
// it at 64. The mixer volume scale is 0-256, so one Paula step is 4.
static uint16_t paula_volume_to_mix(uint32_t envelope, uint32_t instrument_volume) {
    uint32_t paula = (envelope * instrument_volume) >> 9;
    return (uint16_t)((paula > 64 ? 64 : paula) * 4);
}

static void mix_set_amiga_period(FredMixChannel* mc, uint32_t period) {
    mc->period = period;
}

static void mix_mute(FredMixChannel* mc) {
    mc->active = false;
}

// The period of a note on an instrument; a note past the table plays the highest one.
static uint16_t fred_note_period(uint8_t note, uint16_t inst_period) {
    const uint8_t last = (uint8_t)(sizeof(fred_period_table) / sizeof(fred_period_table[0]) - 1);
    return (uint16_t)((fred_period_table[note > last ? last : note] * inst_period) / 1024);
}

static unsigned blend_shift(const FredInstrument* inst) {
    return inst->blend > FRED_MAX_BLEND_SHIFT ? FRED_MAX_BLEND_SHIFT : inst->blend;
}

// A blend instrument mixes two halves of its sample.
static bool blend_sample_ok(const FredInstrument* inst) {
    return inst->sample_addr != nullptr && inst->sample_size >= 2 * 32;
}

// Create synth sample - Pulse

static void create_synth_sample_pulse(FredInstrument* inst, FredChannelInfo* chanInfo) {
    // Initialize the pulse variables
    chanInfo->pulse_shot = inst->pulse_shot_counter;
    chanInfo->pulse_delay = inst->pulse_delay;
    chanInfo->pulse_speed = inst->pulse_speed;
    chanInfo->pulse_way = false;
    chanInfo->pulse_position = inst->pulse_start;

    // Create first part of the sample
    const int length = inst->length < FRED_SYNTH_SIZE ? inst->length : FRED_SYNTH_SIZE;
    int i;
    for (i = 0; i < inst->pulse_start && i < length; i++)
        chanInfo->synth_sample[i] = inst->pulse_rate_min;

    // Create the second part
    for (; i < length; i++)
        chanInfo->synth_sample[i] = inst->pulse_rate_plus;
}

// Create synth sample - Blend

static void create_synth_sample_blend(FredInstrument* inst, FredChannelInfo* chanInfo) {
    // Initialize the blend variables
    chanInfo->blend_way = false;
    chanInfo->blend_position = 1;
    chanInfo->blend_shot = inst->blend_shot_counter;
    chanInfo->blend_delay = inst->blend_delay;

    if (!blend_sample_ok(inst))
        return;
    for (int i = 0; i < 32; i++)
        chanInfo->synth_sample[i] = inst->sample_addr[i];
}

// ModifySound: runs all effects on a channel

static void modify_sound(FredChannelInfo* chanInfo, FredMixChannel* mc) {
    FredInstrument* inst = chanInfo->instrument;

    // If the channel doesn't have any instruments, don't do anything
    if (inst == nullptr)
        return;

    // Arpeggio
    uint8_t new_note = (uint8_t)(chanInfo->track_note + inst->arpeggio[chanInfo->arp_position]);

    chanInfo->arp_speed--;
    if (chanInfo->arp_speed == 0) {
        chanInfo->arp_speed = inst->arp_speed;

        // Go to the next position
        chanInfo->arp_position++;

        if (chanInfo->arp_position >= inst->arp_count)
            chanInfo->arp_position = 0;
    }

    // Find the new period
    chanInfo->track_period = fred_note_period(new_note, inst->period);

    // Portamento
    if (chanInfo->port_running) {
        // Should we delay the portamento?
        if (chanInfo->port_delay != 0)
            chanInfo->port_delay--;
        else {
            // Do it, calculate the new period
            if (chanInfo->port_speed != 0)
                chanInfo->track_period
                    = (uint16_t)(chanInfo->track_period
                                 + (chanInfo->port_counter * chanInfo->period_diff / chanInfo->port_speed));

            chanInfo->port_counter++;

            if (chanInfo->port_counter > chanInfo->port_speed) {
                chanInfo->track_note = chanInfo->port_target_note;
                chanInfo->port_running = false;
            }
        }
    }

    // Vibrato
    uint16_t period = chanInfo->track_period;

    if (chanInfo->vib_delay != 0)
        chanInfo->vib_delay--;
    else {
        if (chanInfo->vib_flags != FRED_VIB_NONE) {
            // Vibrato is running, now check the vibrato direction
            if ((chanInfo->vib_flags & FRED_VIB_VIB_DIRECTION) != 0) {
                chanInfo->vib_value += chanInfo->vib_speed;

                if (chanInfo->vib_value == chanInfo->vib_ampl)
                    chanInfo->vib_flags &= ~FRED_VIB_VIB_DIRECTION;
            } else {
                chanInfo->vib_value -= chanInfo->vib_speed;

                if (chanInfo->vib_value == 0)
                    chanInfo->vib_flags |= FRED_VIB_VIB_DIRECTION;
            }

            // Change the direction of the period
            if (chanInfo->vib_value == 0)
                chanInfo->vib_flags ^= FRED_VIB_PERIOD_DIRECTION;

            if ((chanInfo->vib_flags & FRED_VIB_PERIOD_DIRECTION) != 0)
                period = (uint16_t)(period + chanInfo->vib_value);
            else
                period = (uint16_t)(period - chanInfo->vib_value);
        }
    }

    // Set the period
    mix_set_amiga_period(mc, period);

    // Envelope
    switch (chanInfo->env_state) {
        case FRED_ENV_ATTACK:
            chanInfo->track_volume += inst->attack_speed;

            if (chanInfo->track_volume >= inst->attack_volume) {
                chanInfo->track_volume = inst->attack_volume;
                chanInfo->env_state = FRED_ENV_DECAY;
            }
            break;

        case FRED_ENV_DECAY:
            chanInfo->track_volume -= inst->decay_speed;

            if (chanInfo->track_volume <= inst->decay_volume) {
                chanInfo->track_volume = inst->decay_volume;
                chanInfo->env_state = FRED_ENV_SUSTAIN;
            }
            break;

        case FRED_ENV_SUSTAIN:
            if (chanInfo->sustain_delay == 0)
                chanInfo->env_state = FRED_ENV_RELEASE;
            else
                chanInfo->sustain_delay--;
            break;

        case FRED_ENV_RELEASE:
            chanInfo->track_volume -= inst->release_speed;

            if (chanInfo->track_volume <= inst->release_volume) {
                chanInfo->track_volume = inst->release_volume;
                chanInfo->env_state = FRED_ENV_DONE;
            }
            break;

        case FRED_ENV_DONE:
            break;
    }

    // Set the volume
    mix_set_volume(mc, paula_volume_to_mix((uint32_t)chanInfo->track_volume, inst->env_vol));

    // Pulse on synth samples
    if (inst->inst_type == FRED_INST_PULSE) {
        if (chanInfo->pulse_delay != 0)
            chanInfo->pulse_delay--;
        else {
            if (chanInfo->pulse_speed != 0)
                chanInfo->pulse_speed--;
            else {
                if (((inst->inst_sync & FRED_SYNC_PULSE_XSHOT) == 0) || (chanInfo->pulse_shot != 0)) {
                    chanInfo->pulse_speed = inst->pulse_speed;

                    for (int steps = 0; steps < FRED_MAX_EFFECT_STEPS; steps++) {
                        if (chanInfo->pulse_way) {
                            if (chanInfo->pulse_position >= inst->pulse_start) {
                                // Change the sample at the pulse position
                                if (chanInfo->pulse_position < FRED_SYNTH_SIZE)
                                    chanInfo->synth_sample[chanInfo->pulse_position] = inst->pulse_rate_plus;
                                chanInfo->pulse_position--;
                                break;
                            }

                            // Switch direction
                            chanInfo->pulse_way = false;
                            chanInfo->pulse_shot--;
                            chanInfo->pulse_position++;
                        } else {
                            if (chanInfo->pulse_position <= inst->pulse_end) {
                                // Change the sample at the pulse position
                                if (chanInfo->pulse_position < FRED_SYNTH_SIZE)
                                    chanInfo->synth_sample[chanInfo->pulse_position] = inst->pulse_rate_min;
                                chanInfo->pulse_position++;
                                break;
                            }

                            // Switch direction
                            chanInfo->pulse_way = true;
                            chanInfo->pulse_shot--;
                            chanInfo->pulse_position--;
                        }
                    }
                }
            }
        }
    }

    // Blend on synth samples
    if (inst->inst_type == FRED_INST_BLEND && blend_sample_ok(inst)) {
        if (chanInfo->blend_delay != 0)
            chanInfo->blend_delay--;
        else {
            for (int steps = 0;; steps++) {
                if (steps >= FRED_MAX_EFFECT_STEPS)
                    return;
                if (((inst->inst_sync & FRED_SYNC_BLEND_XSHOT) == 0) || (chanInfo->blend_shot != 0)) {
                    if (chanInfo->blend_way) {
                        if (chanInfo->blend_position == 1) {
                            chanInfo->blend_way = false;
                            chanInfo->blend_shot--;
                            continue;
                        }

                        chanInfo->blend_position--;
                        break;
                    }

                    if (chanInfo->blend_position == (1u << blend_shift(inst))) {
                        chanInfo->blend_way = true;
                        chanInfo->blend_shot--;
                        continue;
                    }

                    chanInfo->blend_position++;
                    break;
                }

                return; // Well, done with the effects
            }

            // Create new synth sample
            for (int i = 0; i < 32; i++)
                chanInfo->synth_sample[i]
                    = (int8_t)(((chanInfo->blend_position * inst->sample_addr[i + 32]) >> blend_shift(inst))
                               + inst->sample_addr[i]);
        }
    }
}

// DoNewLine: parse the next track line

static uint8_t do_new_line(FredModule* m, int chan_idx) {
    FredChannelInfo* chanInfo = &m->channels[chan_idx];
    FredMixChannel* mc = &m->mix_channels[chan_idx];
    bool inst_change = false;

    // For channels that does not contain any notes, mark them as done immediately
    if (!m->has_notes[m->current_song * 4 + chanInfo->chan_num])
        m->end_reached[chanInfo->chan_num] = true;

    // Get the current track position
    uint16_t track_pos = chanInfo->track_position;

    for (;;) {
        uint8_t cmd = chanInfo->track_table[track_pos++];

        // Is the command a note?
        if (cmd < 0x80) {
            // Yes, play it
            FredInstrument* inst = chanInfo->instrument;

            // Store the new position
            chanInfo->track_position = track_pos;

            // Do we play an invalid instrument?
            if (inst == nullptr) {
                // Stop all effects
                chanInfo->port_running = false;
                chanInfo->vib_flags = 0;
                chanInfo->track_volume = 0;
                mix_mute(mc);

                // Take the next channel
                return 0;
            }

            // Initialize the channel
            chanInfo->track_note = cmd;
            m->last_note[chanInfo->chan_num] = cmd;
            m->last_instrument[chanInfo->chan_num] = (int)(inst - m->instruments);
            chanInfo->arp_position = 0;
            chanInfo->arp_speed = inst->arp_speed;
            chanInfo->vib_delay = inst->vib_delay;
            chanInfo->vib_speed = inst->vib_speed;
            chanInfo->vib_ampl = inst->vib_ampl;
            chanInfo->vib_flags = FRED_VIB_VIB_DIRECTION | FRED_VIB_PERIOD_DIRECTION;
            chanInfo->vib_value = 0;

            // Create synth sample if the instrument used is a synth instrument
            if ((inst->inst_type == FRED_INST_PULSE)
                && (inst_change || ((inst->inst_sync & FRED_SYNC_PULSE_SYNC) != 0)))
                create_synth_sample_pulse(inst, chanInfo);
            else {
                if ((inst->inst_type == FRED_INST_BLEND)
                    && (inst_change || ((inst->inst_sync & FRED_SYNC_BLEND_SYNC) != 0)))
                    create_synth_sample_blend(inst, chanInfo);
            }

            // Set the track duration (speed)
            chanInfo->track_duration = m->current_tempo;

            // Play the instrument
            if (inst->inst_type == FRED_INST_SAMPLE) {
                if (inst->sample_addr != nullptr) {
                    // Play sample
                    mix_play_sample(mc, inst->sample_addr, inst->sample_size, inst->length);

                    if (inst->repeat_len == 0xffff) {
                        // No repeat point: the sample plays round from its start.
                        mix_set_loop(mc, 0, inst->length);
                    } else if ((inst->repeat_len != 0) && (inst->repeat_len < inst->length)) {
                        // There is no bug in this line. The original player calculate
                        // the wrong start and length too!
                        mix_set_loop(mc, inst->repeat_len, (uint32_t)inst->length - inst->repeat_len);
                    }
                }
            } else {
                // Play synth sound
                mix_play_sample(mc, chanInfo->synth_sample, FRED_SYNTH_SIZE, inst->length);
                mix_set_loop(mc, 0, inst->length);
            }

            // Set the volume (mute the channel)
            mix_set_volume(mc, 0);

            chanInfo->track_volume = 0;
            chanInfo->env_state = FRED_ENV_ATTACK;
            chanInfo->sustain_delay = inst->sustain_delay;

            // Set the period
            chanInfo->track_period = fred_note_period(chanInfo->track_note, inst->period);
            mix_set_amiga_period(mc, chanInfo->track_period);

            // Initialize portamento
            if (chanInfo->port_running && (chanInfo->port_start_period == 0)) {
                chanInfo->period_diff = (int16_t)(chanInfo->port_limit - chanInfo->track_period);
                chanInfo->port_counter = 1;
                chanInfo->port_start_period = chanInfo->track_period;
            }

            // Take the next channel
            return 0;
        }

        // It's a command
        switch (cmd) {
            // Change instrument
            case TRACK_INST_CODE: {
                uint8_t new_inst = chanInfo->track_table[track_pos++];

                if (new_inst >= m->inst_num)
                    chanInfo->instrument = nullptr;
                else {
                    // Find the instrument information
                    chanInfo->instrument = &m->instruments[new_inst];

                    if (chanInfo->instrument->inst_type == FRED_INST_UNUSED)
                        chanInfo->instrument = nullptr;
                    else
                        inst_change = true;
                }
                break;
            }

            // Change tempo
            case TRACK_TEMPO_CODE: {
                m->current_tempo = chanInfo->track_table[track_pos++];
                break;
            }

            // Start portamento
            case TRACK_PORT_CODE: {
                uint16_t inst_period = 428;

                if (chanInfo->instrument != nullptr)
                    inst_period = chanInfo->instrument->period;

                chanInfo->port_speed = (uint16_t)(chanInfo->track_table[track_pos++] * m->current_tempo);
                chanInfo->port_target_note = chanInfo->track_table[track_pos++];
                chanInfo->port_limit = fred_note_period(chanInfo->port_target_note, inst_period);
                chanInfo->port_start_period = 0;
                chanInfo->port_delay = (uint16_t)(chanInfo->track_table[track_pos++] * m->current_tempo);
                chanInfo->port_running = true;
                break;
            }

            // Execute pause
            case TRACK_PAUSE_CODE: {
                chanInfo->track_duration = m->current_tempo;
                chanInfo->track_position = track_pos;
                mix_mute(mc);

                // Take the next channel
                return 1;
            }

            // End, goto next pattern
            case TRACK_END_CODE: {
                chanInfo->position++;

                for (int steps = 0;; steps++) {
                    // A position list that never reaches a track leaves the channel silent.
                    if (steps >= FRED_POSITION_STEP_LIMIT || chanInfo->position >= FRED_POSITION_COUNT) {
                        chanInfo->track_duration = 0xffff;
                        m->end_reached[chanInfo->chan_num] = true;
                        mix_mute(mc);
                        return 1;
                    }

                    // If the song ends, start it over
                    if (chanInfo->position_table[chanInfo->position] == -1) {
                        chanInfo->position = 0;
                        m->current_tempo = m->start_tempos[m->current_song];

                        // Tell that the channel has ended
                        m->end_reached[chanInfo->chan_num] = true;
                        continue;
                    }

                    if (chanInfo->position_table[chanInfo->position] < 0) {
                        // Jump to a new position
                        uint16_t old_position = chanInfo->position;
                        chanInfo->position = (uint16_t)(chanInfo->position_table[chanInfo->position] & 0x7f);

                        // Do we jump back in the song
                        if (chanInfo->position < old_position)
                            m->end_reached[chanInfo->chan_num] = true;

                        continue;
                    }

                    // Stop the loop
                    break;
                }

                // Find new track
                chanInfo->track_table = m->tracks[chanInfo->position_table[chanInfo->position]];
                chanInfo->track_position = 0;
                chanInfo->track_duration = 1;

                // Take the same channel again
                return 2;
            }

            // Note delay
            default: {
                chanInfo->track_duration = (uint16_t)(-(int8_t)cmd * m->current_tempo);
                chanInfo->track_position = track_pos;

                // Take the next channel
                return 0;
            }
        }
    }
}

// Play tick: main player method

static void play_tick(FredModule* m) {
    // Well, play all channels
    int retries = 0;
    for (int i = 0; i < 4; i++) {
        FredChannelInfo* chanInfo = &m->channels[i];
        FredMixChannel* mc = &m->mix_channels[i];

        // Decrement the note delay counter
        chanInfo->track_duration--;

        // Check to see if we need to get the next track line
        if (chanInfo->track_duration == 0) {
            uint8_t ret_val = do_new_line(m, i);

            if (ret_val == 1) // Take the next voice
                continue;

            if (ret_val == 2) { // Do the same voice again
                if (++retries > FRED_MAX_NEW_LINE_RETRIES) {
                    // Only empty tracks: the channel stays silent.
                    chanInfo->track_duration = 0xffff;
                    mix_mute(mc);
                } else {
                    i--;
                    continue;
                }
            } else {
                retries = 0;
            }
        } else {
            // Check to see if we need to mute the channel
            if ((chanInfo->track_duration == 1) && (chanInfo->track_table[chanInfo->track_position] < TRACK_MAX_CODE))
                mix_mute(mc);
        }

        modify_sound(chanInfo, mc);
    }

    // Check if any channel wrapped (end detection)
    // rePlayer begin
//     for (int i = 0; i < 4; i++) {
//         if (m->end_reached[i]) {
//             m->loop_count++;
//             m->end_reached[i] = false;
//         }
//     }
    int reached_count = 0;
    for (int i = 0; i < 4; i++) {
        if (m->end_reached[i])
            reached_count++;
    }
    if (reached_count == 4) {
        for (int i = 0; i < 4; i++)
            m->end_reached[i] = false;
        m->loop_count++;
    }
    // rePlayer end
}

// Initialize sound

// What a channel plays when its position list starts with a jump or restart.
static uint8_t fred_empty_track[FRED_TRACK_PAD + 1]
    = { TRACK_END_CODE, TRACK_END_CODE, TRACK_END_CODE, TRACK_END_CODE, TRACK_END_CODE };

static void initialize_sound(FredModule* m, int song_number) {
    m->current_song = song_number;
    m->current_tempo = m->start_tempos[song_number];
    m->loop_count = 0;

    for (int i = 0; i < 4; i++) {
        m->end_reached[i] = false;
        m->last_note[i] = -1;
        m->last_instrument[i] = -1;

        int8_t* pos_table = m->positions[song_number][i];

        FredChannelInfo* ch = &m->channels[i];
        memset(ch, 0, sizeof(FredChannelInfo));

        ch->chan_num = i;
        ch->position_table = pos_table;
        ch->track_table = pos_table[0] >= 0 ? m->tracks[pos_table[0]] : fred_empty_track;
        ch->position = 0;
        ch->track_position = 0;
        ch->track_duration = 1;
        ch->track_note = 0;
        ch->track_period = 0;
        ch->track_volume = 0;
        ch->instrument = nullptr;
        ch->vib_flags = FRED_VIB_NONE;
        ch->vib_delay = 0;
        ch->vib_speed = 0;
        ch->vib_ampl = 0;
        ch->vib_value = 0;
        ch->port_running = false;
        ch->port_delay = 0;
        ch->port_limit = 0;
        ch->port_target_note = 0;
        ch->port_start_period = 0;
        ch->period_diff = 0;
        ch->port_counter = 0;
        ch->port_speed = 0;
        ch->env_state = FRED_ENV_ATTACK;
        ch->sustain_delay = 0;
        ch->arp_position = 0;
        ch->arp_speed = 0;
        ch->pulse_way = false;
        ch->pulse_position = 0;
        ch->pulse_delay = 0;
        ch->pulse_speed = 0;
        ch->pulse_shot = 0;
        ch->blend_way = false;
        ch->blend_position = 0;
        ch->blend_delay = 0;
        ch->blend_shot = 0;
        memset(ch->synth_sample, 0, 64);

        // Reset mix channel
        FredMixChannel* mc = &m->mix_channels[i];
        memset(mc, 0, sizeof(FredMixChannel));
    }
}

// Loading

static bool load_module(FredModule* m, const uint8_t* data, size_t size) {
    FredReader reader;
    reader_init(&reader, data, size);

    // Skip the signature and version (14 bytes: "Fred Editor " + 2 version bytes)
    reader_seek(&reader, 14);

    // Get number of sub-songs
    m->sub_song_num = reader_read_b_uint16(&reader);

    if (reader_eof(&reader) || m->sub_song_num > FRED_MAX_SUB_SONGS)
        return false;

    // Read the sub-song start tempos
    m->start_tempos = (uint8_t*)calloc(m->sub_song_num, sizeof(uint8_t));
    if (!m->start_tempos)
        return false;

    for (int i = 0; i < m->sub_song_num; i++)
        m->start_tempos[i] = reader_read_uint8(&reader);

    if (reader_eof(&reader))
        return false;

    // Allocate memory to hold the positions
    m->positions = (int8_t***)calloc(m->sub_song_num, sizeof(int8_t**));
    if (!m->positions)
        return false;

    // Take one sub-song at the time
    for (int i = 0; i < m->sub_song_num; i++) {
        m->positions[i] = (int8_t**)calloc(4, sizeof(int8_t*));
        if (!m->positions[i])
            return false;

        // There are 4 channels in each sub-song
        for (int j = 0; j < 4; j++) {
            // Read one channel position data
            m->positions[i][j] = (int8_t*)calloc(FRED_POSITION_COUNT, sizeof(int8_t));
            if (!m->positions[i][j])
                return false;

            reader_read_signed(&reader, m->positions[i][j], FRED_POSITION_COUNT);

            if (reader_eof(&reader))
                return false;

            // The list ends in a restart or a jump, so that following it never runs off its end.
            if (m->positions[i][j][FRED_POSITION_COUNT - 1] >= 0)
                m->positions[i][j][FRED_POSITION_COUNT - 1] = -1;
        }
    }

    // Allocate memory to hold the track data
    m->tracks = (uint8_t**)calloc(128, sizeof(uint8_t*));
    m->track_sizes = (int*)calloc(128, sizeof(int));
    if (!m->tracks || !m->track_sizes)
        return false;

    // Now load the track data
    for (int i = 0; i < 128; i++) {
        // Read the current track size
        int track_size = reader_read_b_int32(&reader);
        if (reader_eof(&reader) || track_size < 0 || track_size > FRED_MAX_TRACK_SIZE
            || !reader_can_hold(&reader, (uint64_t)track_size, 1))
            return false;

        // Allocate memory for the track data, followed by end codes
        m->tracks[i] = (uint8_t*)malloc((size_t)track_size + FRED_TRACK_PAD);
        m->track_sizes[i] = track_size;
        if (!m->tracks[i])
            return false;
        memset(m->tracks[i] + track_size, TRACK_END_CODE, FRED_TRACK_PAD);

        // Read the track data
        reader_read_bytes(&reader, m->tracks[i], track_size);

        // Did we get some problems?
        if (reader_eof(&reader))
            return false;
    }

    // Get the number of instruments
    m->inst_num = reader_read_b_uint16(&reader);
    if (reader_eof(&reader) || !reader_can_hold(&reader, m->inst_num, FRED_INSTRUMENT_FILE_SIZE))
        return false;

    // Allocate memory to hold the instrument info
    m->instruments = (FredInstrument*)calloc(m->inst_num, sizeof(FredInstrument));
    if (!m->instruments && m->inst_num > 0)
        return false;

    // Read the instrument info
    // Map from instIndex → instrument pointer
    typedef struct {
        uint32_t key;
        int value;
    } InstIndexEntry;
    InstIndexEntry* inst_index_map = (InstIndexEntry*)calloc(m->inst_num, sizeof(InstIndexEntry));
    if (!inst_index_map && m->inst_num > 0)
        return false;
    int inst_map_count = 0;

    for (int i = 0; i < m->inst_num; i++) {
        FredInstrument* inst = &m->instruments[i];

        inst->instrument_number = i;

        // Skip name (32 bytes)
        reader_skip(&reader, 32);

        uint32_t inst_index = reader_read_b_uint32(&reader);

        // Check if this instIndex is already mapped
        bool found = false;
        for (int k = 0; k < inst_map_count; k++) {
            if (inst_index_map[k].key == inst_index) {
                found = true;
                break;
            }
        }
        if (!found) {
            inst_index_map[inst_map_count].key = inst_index;
            inst_index_map[inst_map_count].value = i;
            inst_map_count++;
        }

        inst->repeat_len = reader_read_b_uint16(&reader);
        inst->length = (uint16_t)(reader_read_b_uint16(&reader) * 2);
        inst->period = reader_read_b_uint16(&reader);
        inst->vib_delay = reader_read_uint8(&reader);
        reader_skip(&reader, 1);

        inst->vib_speed = reader_read_int8(&reader);
        inst->vib_ampl = reader_read_int8(&reader);
        inst->env_vol = reader_read_uint8(&reader);
        inst->attack_speed = reader_read_uint8(&reader);
        inst->attack_volume = reader_read_uint8(&reader);
        inst->decay_speed = reader_read_uint8(&reader);
        inst->decay_volume = reader_read_uint8(&reader);
        inst->sustain_delay = reader_read_uint8(&reader);
        inst->release_speed = reader_read_uint8(&reader);
        inst->release_volume = reader_read_uint8(&reader);

        reader_read_signed(&reader, inst->arpeggio, 16);

        inst->arp_speed = reader_read_uint8(&reader);
        inst->inst_type = (FredInstrumentType)reader_read_uint8(&reader);
        inst->pulse_rate_min = reader_read_int8(&reader);
        inst->pulse_rate_plus = reader_read_int8(&reader);
        inst->pulse_speed = reader_read_uint8(&reader);
        inst->pulse_start = reader_read_uint8(&reader);
        inst->pulse_end = reader_read_uint8(&reader);
        inst->pulse_delay = reader_read_uint8(&reader);
        inst->inst_sync = reader_read_uint8(&reader);
        inst->blend = reader_read_uint8(&reader);
        inst->blend_delay = reader_read_uint8(&reader);
        inst->pulse_shot_counter = reader_read_uint8(&reader);
        inst->blend_shot_counter = reader_read_uint8(&reader);
        inst->arp_count = reader_read_uint8(&reader);
        if (inst->arp_count > FRED_ARPEGGIO_SIZE)
            inst->arp_count = FRED_ARPEGGIO_SIZE;

        if (reader_eof(&reader)) {
            free(inst_index_map);
            return false;
        }

        reader_skip(&reader, 12);
    }

    // Read number of samples
    uint16_t samp_num = reader_read_b_uint16(&reader);

    for (int i = 0; i < samp_num; i++) {
        // Read the instrument index
        uint16_t inst_index = reader_read_b_uint16(&reader);

        // Find the instrument info
        int inst_idx = -1;
        for (int k = 0; k < inst_map_count; k++) {
            if (inst_index_map[k].key == inst_index) {
                inst_idx = inst_index_map[k].value;
                break;
            }
        }

        if (inst_idx < 0) {
            free(inst_index_map);
            return false;
        }

        FredInstrument* inst = &m->instruments[inst_idx];

        // Get the size of the sample data
        uint16_t samp_size = reader_read_b_uint16(&reader);

        // Read the sample data
        free(inst->sample_addr);
        inst->sample_addr = (int8_t*)calloc(samp_size, sizeof(int8_t));
        inst->sample_size = samp_size;
        if (!inst->sample_addr) {
            free(inst_index_map);
            return false;
        }

        reader_read_signed(&reader, inst->sample_addr, samp_size);

        if (reader_eof(&reader)) {
            free(inst_index_map);
            return false;
        }
    }

    free(inst_index_map);

    // Calculate has_notes
    m->has_notes = (bool*)calloc(m->sub_song_num * 4, sizeof(bool));
    if (!m->has_notes && m->sub_song_num > 0)
        return false;

    for (int i = 0; i < m->sub_song_num; i++) {
        for (int j = 0; j < 4; j++) {
            int8_t* pos_list = m->positions[i][j];

            for (int k = 0; k < 256; k++) {
                if (pos_list[k] < 0)
                    break;

                if (!m->has_notes[i * 4 + j]) {
                    const uint8_t* track = m->tracks[pos_list[k]];
                    const int track_size = m->track_sizes[pos_list[k]];

                    for (int mi = 0; mi < track_size; mi++) {
                        uint8_t cmd = track[mi];

                        if (cmd < 0x80) {
                            m->has_notes[i * 4 + j] = true;
                            break;
                        }

                        if (cmd == TRACK_END_CODE)
                            break;

                        switch (cmd) {
                            case TRACK_INST_CODE:
                            case TRACK_TEMPO_CODE:
                                mi++;
                                break;

                            case TRACK_PORT_CODE:
                                mi += 3;
                                break;
                        }
                    }
                }
            }
        }
    }

    return true;
}

// Voices

// Paula channels 0 and 3 are left, 1 and 2 right.
static float channel_pan(int ch) {
    return (ch == 0 || ch == 3) ? -1.0f : 1.0f;
}

static NuadeSample channel_sample(const FredMixChannel* mc) {
    return (NuadeSample) { .data = mc->sample_data,
                           .format = NuadeSampleFormat_S8,
                           .length = mc->sample_length < mc->data_size ? mc->sample_length : mc->data_size,
                           .loop_start = mc->loop_start,
                           .loop_length = mc->loop_length };
}

// Hand the registers the tick wrote to the voice.
static void sync_voice(FredModule* m, int ch) {
    FredMixChannel* mc = &m->mix_channels[ch];
    const uint32_t index = (uint32_t)ch;

    if (!mc->active || mc->sample_data == nullptr) {
        if (nuade_voice_is_playing(m->voices, index))
            nuade_voice_stop(m->voices, index);
    } else if (mc->play_pending) {
        const NuadeSample sample = channel_sample(mc);
        nuade_voice_play(m->voices, index, &sample, 0);
    }
    mc->play_pending = false;

    const bool silent = mc->period == 0;
    nuade_voice_set_pitch(m->voices, index, silent ? 0.0 : nuade_paula_period_hz(mc->period));
    nuade_voice_set_volume(m->voices, index, silent ? 0.0f : (float)mc->volume / 256.0f);
    nuade_voice_set_pan(m->voices, index, channel_pan(ch));
}

bool fred_tick(FredModule* module) {
    for (int ch = 0; ch < FRED_CHANNELS; ch++) {
        FredMixChannel* mc = &module->mix_channels[ch];
        mc->active = mc->active && nuade_voice_is_playing(module->voices, (uint32_t)ch);
    }

    const uint32_t loops = module->loop_count;
    play_tick(module);

    for (int ch = 0; ch < FRED_CHANNELS; ch++)
        sync_voice(module, ch);
    return module->loop_count != loops;
}

// Amiga hunk stripping: extract first CODE section from AmigaOS HUNK executable

static uint16_t rd16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static int16_t rds16(const uint8_t* p) {
    return (int16_t)rd16(p);
}

static uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void wr16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v);
}

static void wr32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v);
}

// Returns pointer into `data` at start of first CODE section, sets *out_size.
// Returns NULL if not a valid HUNK file.
static const uint8_t* strip_amiga_hunk(const uint8_t* data, size_t size, size_t* out_size) {
    if (size < 24)
        return nullptr;
    if (rd32(data) != 0x000003F3)
        return nullptr; // HUNK_HEADER

    size_t pos = 4;
    uint32_t res_libs = rd32(data + pos);
    pos += 4;
    if (res_libs != 0)
        return nullptr;

    uint32_t num_hunks = rd32(data + pos);
    pos += 4;
    if (num_hunks == 0 || num_hunks > 100)
        return nullptr;

    pos += 4; // firstHunk
    pos += 4; // lastHunk

    // Skip hunk size table
    for (uint32_t i = 0; i < num_hunks; i++) {
        if (pos + 4 > size)
            return nullptr;
        pos += 4;
    }

    // Walk hunks to find first CODE section
    while (pos + 8 <= size) {
        uint32_t hunk_type = rd32(data + pos) & 0x3FFFFFFF;
        pos += 4;

        if (hunk_type == 0x000003E9 || hunk_type == 0x000003EA) { // HUNK_CODE or HUNK_DATA
            uint32_t size_longs = rd32(data + pos);
            pos += 4;
            if (size_longs > (size - pos) / 4)
                return nullptr;
            size_t size_bytes = (size_t)size_longs * 4;
            *out_size = size_bytes;
            return data + pos;
        }

        if (hunk_type == 0x000003EB) { // HUNK_BSS
            pos += 4;
            continue;
        }

        if (hunk_type == 0x000003EC) { // HUNK_RELOC32
            while (pos + 4 <= size) {
                uint32_t count = rd32(data + pos);
                pos += 4;
                if (count == 0)
                    break;
                if (size - pos < 4 || count > (size - pos - 4) / 4) // target hunk, then the offsets
                    return nullptr;
                pos += 4;
                pos += (size_t)count * 4;
            }
            continue;
        }

        if (hunk_type == 0x000003F2) { // HUNK_END
            continue;
        }

        break; // unknown hunk
    }

    return nullptr;
}

// Fred Editor Final (0x4EFA) → clean "Fred Editor" format converter
//
// Port of NostalgicPlayer FredEditorFinalFormat.cs Identify + Convert.
// Scans 68k code patterns to locate module data, then serializes as clean format.

// Dynamic buffer for building the converted output
// The converted module is never larger than this; a module that would be is not a real one.
#define CONVERT_MAX_SIZE ((size_t)64 << 20)

typedef struct ConvertBuf {
    uint8_t* data;
    size_t size;
    size_t capacity;
    bool failed; // out of memory or past CONVERT_MAX_SIZE: further writes are dropped
} ConvertBuf;

static void cbuf_init(ConvertBuf* cb) {
    cb->capacity = 16384;
    cb->data = (uint8_t*)malloc(cb->capacity);
    cb->size = 0;
    cb->failed = cb->data == nullptr;
}

// True when `extra` more bytes fit.
static bool cbuf_ensure(ConvertBuf* cb, size_t extra) {
    if (cb->failed)
        return false;
    if (extra > CONVERT_MAX_SIZE - cb->size) {
        cb->failed = true;
        return false;
    }
    while (cb->size + extra > cb->capacity) {
        uint8_t* grown = (uint8_t*)realloc(cb->data, cb->capacity * 2);
        if (!grown) {
            cb->failed = true;
            return false;
        }
        cb->data = grown;
        cb->capacity *= 2;
    }
    return true;
}

static void cbuf_write(ConvertBuf* cb, const void* src, size_t n) {
    if (!cbuf_ensure(cb, n))
        return;
    memcpy(cb->data + cb->size, src, n);
    cb->size += n;
}

static void cbuf_write_u8(ConvertBuf* cb, uint8_t v) {
    if (!cbuf_ensure(cb, 1))
        return;
    cb->data[cb->size++] = v;
}

static void cbuf_write_u16(ConvertBuf* cb, uint16_t v) {
    if (!cbuf_ensure(cb, 2))
        return;
    wr16(cb->data + cb->size, v);
    cb->size += 2;
}

static void cbuf_write_u32(ConvertBuf* cb, uint32_t v) {
    if (!cbuf_ensure(cb, 4))
        return;
    wr32(cb->data + cb->size, v);
    cb->size += 4;
}

static void cbuf_write_s32(ConvertBuf* cb, int32_t v) {
    cbuf_write_u32(cb, (uint32_t)v);
}

static void cbuf_write_zeros(ConvertBuf* cb, size_t n) {
    if (!cbuf_ensure(cb, n))
        return;
    memset(cb->data + cb->size, 0, n);
    cb->size += n;
}

// Convert Fred Editor Final (0x4EFA format) to clean "Fred Editor " format.
// Returns a malloc'd buffer and sets *out_size. Returns NULL on failure.
static uint8_t* convert_fred_final(const uint8_t* mod, size_t mod_len, size_t* out_size) {
    if (mod_len < 0xB0E)
        return nullptr;

    // Verify 4x JMP instructions at 4-byte intervals
    for (int i = 0; i < 16; i += 4) {
        if (rd16(mod + i) != 0x4EFA)
            return nullptr;
    }

    // Get initOffset from first JMP displacement
    int16_t init_offset = (int16_t)(rds16(mod + 2) + 2);
    if (init_offset < 0 || (size_t)init_offset + 128 > mod_len)
        return nullptr;

    // Read 64 words of the init routine
    uint16_t init_func[64];
    for (int i = 0; i < 64 && (size_t)init_offset + i * 2 + 1 < mod_len; i++) {
        init_func[i] = rd16(mod + init_offset + i * 2);
    }

    // Find moduleOffset: pattern 0x123A ... 0xB001
    int module_offset = -1;
    int pattern_pos = -1;
    for (int i = 0; i < 4; i++) {
        if (init_func[i] == 0x123A && init_func[i + 2] == 0xB001) {
            module_offset = (int)init_func[i + 1] + init_offset + i * 2 + 1;
            pattern_pos = i;
            break;
        }
    }
    if (module_offset < 0 || (size_t)module_offset >= mod_len)
        return nullptr;

    // Find offsetDiff: pattern 0x47FA ... 0xD7FA
    int offset_diff = 0;
    bool found_offset_diff = false;
    for (int i = pattern_pos; i < 60; i++) {
        if (init_func[i] == 0x47FA && init_func[i + 2] == 0xD7FA) {
            offset_diff = init_offset + (i + 1) * 2 + (int16_t)init_func[i + 1];
            found_offset_diff = true;
            break;
        }
    }
    if (!found_offset_diff)
        return nullptr;

    // Read module header at moduleOffset
    size_t mpos = (size_t)module_offset;
    if (mpos + 14 > mod_len)
        return nullptr;

    // Skip played song (1 byte)
    mpos += 1;

    // subSongs = byte + 1
    const int sub_song_count = mod[mpos++] + 1;
    if (sub_song_count > FRED_MAX_SUB_SONGS)
        return nullptr;
    const uint8_t sub_songs = (uint8_t)sub_song_count;

    // Skip current tempo (1 byte)
    mpos += 1;

    // Start tempos (10 bytes)
    uint8_t start_tempos[10];
    if (mpos + 10 > mod_len)
        return nullptr;
    memcpy(start_tempos, mod + mpos, 10);
    mpos += 10;

    // Skip 1 byte
    mpos += 1;

    // Get instOffset and trackOffset (relative, add offsetDiff to get absolute)
    if (mpos + 8 > mod_len)
        return nullptr;
    const int64_t inst_offset64 = (int64_t)(int32_t)rd32(mod + mpos) + offset_diff;
    mpos += 4;
    const int64_t track_offset64 = (int64_t)(int32_t)rd32(mod + mpos) + offset_diff;
    mpos += 4;

    if (inst_offset64 < 0 || (uint64_t)inst_offset64 >= mod_len || track_offset64 < 0
        || (uint64_t)track_offset64 >= mod_len)
        return nullptr;
    const uint32_t inst_offset = (uint32_t)inst_offset64;
    const uint32_t track_offset = (uint32_t)track_offset64;

    // Skip replay data: 100 + 128*4 = 612 bytes
    mpos += 100 + 128 * 4;

    // Read sub-song start sequence numbers
    if (mpos + (size_t)sub_songs * 8 > mod_len)
        return nullptr;

    uint16_t start_positions[10][4];
    for (int i = 0; i < sub_songs; i++) {
        for (int j = 0; j < 4; j++) {
            uint16_t raw = rd16(mod + mpos);
            mpos += 2;
            start_positions[i][j] = (uint16_t)((raw - sub_songs * 4 * 2) / 2);
        }
    }

    // Load position offsets
    uint32_t min_offset = inst_offset < track_offset ? inst_offset : track_offset;
    if (mpos > min_offset)
        return nullptr;
    int position_size = (int)((min_offset - mpos) / 2);
    if (position_size <= 0 || position_size > 65536)
        return nullptr;

    int16_t* position_offsets = (int16_t*)calloc((size_t)position_size, sizeof(int16_t));
    if (!position_offsets)
        return nullptr;

    for (int i = 0; i < position_size; i++) {
        if (mpos + 2 > mod_len) {
            free(position_offsets);
            return nullptr;
        }
        position_offsets[i] = rds16(mod + mpos);
        mpos += 2;
    }

    // Load all track data
    int track_size = (int)(inst_offset > track_offset ? inst_offset - track_offset : track_offset - inst_offset);
    if (track_size <= 0 || (size_t)track_offset + (size_t)track_size > mod_len) {
        free(position_offsets);
        return nullptr;
    }

    const uint8_t* track_data = mod + track_offset;

    // Split track data into individual tracks by scanning for 0x80 end markers
    typedef struct {
        int start;
        int length;
    } TrackSpan;
    TrackSpan* track_spans = (TrackSpan*)calloc(256, sizeof(TrackSpan));
    int* offset_to_track = (int*)calloc((size_t)track_size, sizeof(int));
    int num_tracks = 0;

    if (!track_spans || !offset_to_track) {
        free(position_offsets);
        free(track_spans);
        free(offset_to_track);
        return nullptr;
    }

    // Initialize offset_to_track to -1
    for (int i = 0; i < track_size; i++)
        offset_to_track[i] = -1;

    for (int i = 0; i < track_size && num_tracks < 256;) {
        int start_idx = i;

        while (i < track_size && track_data[i] != 0x80)
            i++;

        // Include the 0x80 end marker
        if (i < track_size)
            i++;

        // If last track doesn't end with 0x80, skip it
        if (i == track_size && (i == 0 || track_data[i - 1] != 0x80))
            break;

        track_spans[num_tracks].start = start_idx;
        track_spans[num_tracks].length = i - start_idx;
        offset_to_track[start_idx] = num_tracks;
        num_tracks++;
    }

    // Pad to 128 tracks minimum
    if (num_tracks > 128)
        num_tracks = 128;

    // Build output buffer
    ConvertBuf cb;
    cbuf_init(&cb);
    if (!cb.data) {
        free(position_offsets);
        free(track_spans);
        free(offset_to_track);
        return nullptr;
    }

    // Write header: "Fred Editor " + version 0x0000
    cbuf_write(&cb, "Fred Editor ", 12);
    cbuf_write_u16(&cb, 0x0000);

    // Write sub-song count
    cbuf_write_u16(&cb, (uint16_t)sub_songs);

    // Write start tempos
    cbuf_write(&cb, start_tempos, (size_t)sub_songs);

    // Write position tables (subSongs * 4 channels * 256 bytes each)
    for (int i = 0; i < sub_songs; i++) {
        for (int j = 0; j < 4; j++) {
            uint16_t pos = start_positions[i][j];
            int written = 0;

            while (written < 255 && pos < (uint16_t)position_size && position_offsets[pos] >= 0) {
                int track_off = (int)position_offsets[pos];
                int track_num = (track_off < track_size) ? offset_to_track[track_off] : -1;
                if (track_num < 0)
                    track_num = 0; // fallback
                cbuf_write_u8(&cb, (uint8_t)track_num);
                pos++;
                written++;
            }

            // Write end mark
            if (pos < (uint16_t)position_size) {
                uint8_t end_val = (uint8_t)(((position_offsets[pos] & 0x7FFF) / 2) | 0x80);
                cbuf_write_u8(&cb, end_val);
                written++;
            } else {
                cbuf_write_u8(&cb, 0x80);
                written++;
            }

            // Pad to 256 bytes
            if (written < 256) {
                int pad = 255 - written;
                if (pad > 0)
                    cbuf_write_zeros(&cb, (size_t)pad);
                cbuf_write_u8(&cb, 0x80);
            }
        }
    }

    // Write tracks (up to 128, each with length prefix)
    for (int i = 0; i < 128; i++) {
        if (i < num_tracks) {
            cbuf_write_s32(&cb, track_spans[i].length);
            cbuf_write(&cb, track_data + track_spans[i].start, (size_t)track_spans[i].length);
        } else {
            // Empty track: just end marker
            cbuf_write_s32(&cb, 1);
            cbuf_write_u8(&cb, 0x80);
        }
    }

    // Count instruments at instOffset
    // Instruments in the 68k format: 64 bytes each
    // Structure: sampleOffset(4) + synthField(2) + 33bytes + instType(1) + remaining(24) = 64
    size_t inst_pos = inst_offset;
    uint16_t inst_num = 0;
    int min_sample_offset = 0x7FFFFFFF;

    for (;;) {
        if (inst_pos + 64 > mod_len)
            break;

        int32_t sample_off = (int32_t)rd32(mod + inst_pos);
        // int16_t test_synth = rds16(mod + inst_pos + 4);
        uint8_t test_inst = mod[inst_pos + 39]; // 4 + 2 + 33 = 39

        // The table ends where the first sample starts; an instrument that ends exactly there still belongs to it.
        if (inst_pos + 64 > (size_t)min_sample_offset)
            break;

        if (test_inst != 0xFF) {
            if (sample_off != 0) {
                const int64_t abs_off = (int64_t)sample_off + offset_diff;
                if (abs_off < min_sample_offset)
                    min_sample_offset = abs_off > INT32_MIN ? (int)abs_off : INT32_MIN;
            }
        }

        inst_pos += 64;
        inst_num++;
    }

    // Write instrument count
    cbuf_write_u16(&cb, inst_num);

    // Write instrument data
    typedef struct {
        uint16_t key;
        int offset;
        uint16_t length;
    } SampleEntry;
    SampleEntry* sample_entries = (SampleEntry*)calloc(inst_num + 1, sizeof(SampleEntry));
    if (!sample_entries) {
        free(cb.data);
        free(position_offsets);
        free(track_spans);
        free(offset_to_track);
        return nullptr;
    }
    int num_sample_entries = 0;

    inst_pos = inst_offset;
    for (uint16_t i = 0; i < inst_num; i++) {
        // Write instrument name (32 bytes)
        char name_buf[32];
        memset(name_buf, 0, 32);
        if (inst_num < 100)
            snprintf(name_buf, sizeof(name_buf), "instr%02d", i);
        else
            snprintf(name_buf, sizeof(name_buf), "instr%03d", i);
        cbuf_write(&cb, (const uint8_t*)name_buf, 32);

        // Write instrument index (sequential)
        cbuf_write_u32(&cb, (uint32_t)(i + 1));

        uint8_t inst_type = mod[inst_pos + 39];

        if (inst_type == 0x00) {
            // Sample instrument: read from offset 0
            int32_t sample_off = (int32_t)rd32(mod + inst_pos);
            uint16_t repeat_len = rd16(mod + inst_pos + 4);
            uint16_t length = rd16(mod + inst_pos + 6);

            if (sample_off != 0 && (size_t)sample_off < mod_len) {
                const int64_t abs_off = (int64_t)sample_off + offset_diff;
                sample_entries[num_sample_entries].key = i + 1;
                sample_entries[num_sample_entries].offset = (abs_off < 0 || abs_off > INT32_MAX) ? -1 : (int)abs_off;
                sample_entries[num_sample_entries].length = (uint16_t)(length * 2);
                num_sample_entries++;
            } else {
                repeat_len = 0xFFFF;
                length = 0;
            }

            // Write repeatLen + length
            cbuf_write_u16(&cb, repeat_len);
            cbuf_write_u16(&cb, length);
            // Copy remaining 56 bytes (from offset 8 in the 68k inst)
            cbuf_write(&cb, mod + inst_pos + 8, 56);
        } else {
            // Synth instrument: skip sampleOffset(4), write from offset 4
            cbuf_write(&cb, mod + inst_pos + 4, 60);
        }

        inst_pos += 64;
    }

    // Write sample data
    cbuf_write_u16(&cb, (uint16_t)num_sample_entries);

    for (int i = 0; i < num_sample_entries; i++) {
        cbuf_write_u16(&cb, sample_entries[i].key);
        cbuf_write_u16(&cb, sample_entries[i].length);

        size_t samp_off = (size_t)sample_entries[i].offset;
        size_t samp_len = sample_entries[i].length;
        if (samp_off <= mod_len && samp_len <= mod_len - samp_off) {
            cbuf_write(&cb, mod + samp_off, samp_len);
        } else {
            // Partial or missing: write what we can plus zeros
            size_t avail = (samp_off < mod_len) ? mod_len - samp_off : 0;
            if (avail > 0)
                cbuf_write(&cb, mod + samp_off, avail);
            if (avail < samp_len)
                cbuf_write_zeros(&cb, samp_len - avail);
        }
    }

    // Write end marker
    cbuf_write_u32(&cb, 0x12345678);

    free(position_offsets);
    free(track_spans);
    free(offset_to_track);
    free(sample_entries);

    if (cb.failed) {
        free(cb.data);
        return nullptr;
    }
    *out_size = cb.size;
    return cb.data;
}

// Public API

static FredModule* fred_build(const uint8_t* data, size_t size) {
    if (!data || size < 20)
        return nullptr;

    const uint8_t* mod_data = data;
    size_t mod_size = size;
    uint8_t* converted = nullptr;

    // If wrapped in an Amiga HUNK executable, strip the hunk header
    if (size >= 4 && rd32(data) == 0x000003F3) {
        size_t code_size = 0;
        const uint8_t* code = strip_amiga_hunk(data, size, &code_size);
        if (code && code_size >= 16) {
            mod_data = code;
            mod_size = code_size;
        }
    }

    // Try clean "Fred Editor " format first
    if (mod_size >= 20 && memcmp(mod_data, "Fred Editor ", 12) == 0 && mod_data[12] == 0x00 && mod_data[13] == 0x00) {
        uint16_t num_songs = (uint16_t)((mod_data[14] << 8) | mod_data[15]);
        if (num_songs <= 10 && mod_size >= 4) {
            uint32_t end_mark = rd32(mod_data + mod_size - 4);
            if (end_mark == 0x12345678) {
                goto do_load;
            }
        }
    }

    // Try Fred Editor Final (0x4EFA) format: convert to clean format
    if (mod_size >= 0xB0E && rd16(mod_data) == 0x4EFA) {
        size_t conv_size = 0;
        converted = convert_fred_final(mod_data, mod_size, &conv_size);
        if (converted) {
            mod_data = converted;
            mod_size = conv_size;
            goto do_load;
        }
    }

    return nullptr;

do_load:;
    FredModule* m = (FredModule*)calloc(1, sizeof(FredModule));
    if (!m) {
        free(converted);
        return nullptr;
    }

    // Keep original data for export
    m->original_data = (uint8_t*)malloc(size);
    if (m->original_data) {
        memcpy(m->original_data, data, size);
        m->original_size = size;
    }

    if (!load_module(m, mod_data, mod_size)) {
        fred_destroy(m);
        free(converted);
        return nullptr;
    }

    free(converted);
    return m;
}

bool fred_probe(const uint8_t* data, size_t size) {
    FredModule* m = fred_build(data, size);
    const bool ok = m != nullptr && m->sub_song_num > 0;
    fred_destroy(m);
    return ok;
}

FredModule* fred_create(const uint8_t* data, size_t size) {
    FredModule* m = fred_build(data, size);
    if (m && m->sub_song_num == 0) {
        fred_destroy(m);
        return nullptr;
    }
    return m;
}

// Module swap - grid edits
//
// A grid edit re-encodes the module (src/lib/import/formats/fredEditorGrid.ts
// applyFredGridEdits) and the new module replaces the playing one's song data
// here, without restarting: every voice keeps its place in its track list and,
// within its current track, its place in time (lines, then ticks), so a note
// edited ahead of the playhead sounds when the playhead reaches it.

/** Lines the events in track bytes [0, pos) last (a note or pause 1, a hold 256 - byte). */
static int track_lines_before(const uint8_t* t, int size, int pos) {
    int p = 0, lines = 0;
    while (p < pos && p < size) {
        uint8_t c = t[p];
        if (c == TRACK_END_CODE)
            break;
        if (c == TRACK_PORT_CODE) {
            p += 4;
            continue;
        }
        if (c == TRACK_TEMPO_CODE || c == TRACK_INST_CODE) {
            p += 2;
            continue;
        }
        lines += (c < 0x80 || c == TRACK_PAUSE_CODE) ? 1 : 256 - c;
        p++;
    }
    return lines;
}

/**
 * Where a voice continues in `t` when `cur_tick` ticks of the track have
 * passed: the first event boundary after it (position, and ticks left until
 * that boundary is read).
 */
static void track_seek(const uint8_t* t, int size, int tempo, int cur_tick, uint16_t* pos_out, uint16_t* dur_out) {
    int p = 0, lines = 0;
    int last_p = 0, last_lines = 0;
    for (;;) {
        if (lines * tempo > cur_tick) {
            *pos_out = (uint16_t)p;
            *dur_out = (uint16_t)(lines * tempo - cur_tick);
            return;
        }
        last_p = p;
        last_lines = lines;
        // Skip zero-time commands to the next event.
        while (p < size && (t[p] == TRACK_PORT_CODE || t[p] == TRACK_TEMPO_CODE || t[p] == TRACK_INST_CODE))
            p += (t[p] == TRACK_PORT_CODE) ? 4 : 2;
        if (p >= size || t[p] == TRACK_END_CODE)
            break;
        uint8_t c = t[p++];
        lines += (c < 0x80 || c == TRACK_PAUSE_CODE) ? 1 : 256 - c;
    }
    (void)last_lines;
    *pos_out = (uint16_t)last_p;
    *dur_out = 1;
}

// Points a channel playing one of `from`'s samples at the same place in `to`'s copy of it. False when `to` has no
// such sample, or one too short for what the channel plays: the channel then has no sample data.
static bool remap_channel_sample(FredMixChannel* mc, const FredModule* from, const FredModule* to) {
    if (!mc->sample_data)
        return true;
    const uint32_t played = mc->sample_length < mc->data_size ? mc->sample_length : mc->data_size;
    for (int i = 0; i < from->inst_num; i++) {
        const FredInstrument* a = &from->instruments[i];
        if (!a->sample_addr || mc->sample_data < a->sample_addr || mc->sample_data > a->sample_addr + a->sample_size)
            continue;
        const size_t offset = (size_t)(mc->sample_data - a->sample_addr);
        const FredInstrument* b = i < to->inst_num ? &to->instruments[i] : nullptr;
        if (b && b->sample_addr && offset <= b->sample_size && played <= b->sample_size - offset) {
            mc->sample_data = b->sample_addr + offset;
            mc->data_size = (uint32_t)(b->sample_size - offset);
            return true;
        }
        mc->sample_data = nullptr;
        return false;
    }
    return true;
}

int fred_replace_module(FredModule* module, const uint8_t* data, size_t size) {
    if (!module)
        return 0;
    FredModule* next = fred_build(data, size);
    if (!next)
        return 0;
    if (next->sub_song_num != module->sub_song_num) {
        fred_destroy(next);
        return 0;
    }

    int tempo = module->current_tempo ? module->current_tempo : 1;
    bool sample_lost[4] = { false };
    for (int ch = 0; ch < 4; ch++) {
        FredChannelInfo* c = &module->channels[ch];
        FredMixChannel* mc = &module->mix_channels[ch];
        int8_t idx = c->position_table ? c->position_table[c->position] : 0;
        int track = idx < 0 ? 0 : idx;
        const uint8_t* old_t = module->tracks[track];
        int cur_tick
            = track_lines_before(old_t, module->track_sizes[track], c->track_position) * tempo - c->track_duration;

        c->position_table = next->positions[module->current_song][ch];
        int8_t nidx = c->position_table[c->position];
        int ntrack = nidx < 0 ? 0 : nidx;
        c->track_table = next->tracks[ntrack];
        track_seek(c->track_table, next->track_sizes[ntrack], tempo, cur_tick, &c->track_position, &c->track_duration);

        if (c->instrument) {
            int i = (int)(c->instrument - module->instruments);
            c->instrument = (i >= 0 && i < next->inst_num) ? &next->instruments[i] : nullptr;
        }
        if (mc->sample_data != c->synth_sample)
            sample_lost[ch] = !remap_channel_sample(mc, module, next);
    }

    // Swap the song data; `next` takes the old data with it.
#define FRED_SWAP(field)                              \
    do {                                              \
        __typeof__(module->field) t_ = module->field; \
        module->field = next->field;                  \
        next->field = t_;                             \
    } while (0)
    FRED_SWAP(original_data);
    FRED_SWAP(original_size);
    FRED_SWAP(start_tempos);
    FRED_SWAP(positions);
    FRED_SWAP(tracks);
    FRED_SWAP(track_sizes);
    FRED_SWAP(instruments);
    FRED_SWAP(has_notes);
    FRED_SWAP(inst_num);
#undef FRED_SWAP

    // The voices read the sample data live: point them at the new copy, keeping their place.
    for (int ch = 0; ch < FRED_CHANNELS; ch++) {
        const FredMixChannel* mc = &module->mix_channels[ch];
        if (module->voices != nullptr && sample_lost[ch])
            nuade_voice_stop(module->voices, (uint32_t)ch); // it still reads the old copy, freed below
        if (module->voices == nullptr || !mc->active || mc->sample_data == nullptr)
            continue;
        NuadeVoiceState state;
        nuade_voice_get_state(module->voices, (uint32_t)ch, &state);
        if (!state.playing)
            continue;
        const NuadeSample sample = channel_sample(mc);
        nuade_voice_play(module->voices, (uint32_t)ch, &sample, (uint32_t)state.position);
    }
    fred_destroy(next);
    return 1;
}

/** The period voice `ch` plays now (0 when silent) - for tests and meters. */
int fred_get_channel_period(const FredModule* module, int ch) {
    if (!module || ch < 0 || ch > 3)
        return 0;
    const FredMixChannel* mc = &module->mix_channels[ch];
    return mc->active ? (int)mc->period : 0;
}

void fred_destroy(FredModule* module) {
    if (!module)
        return;

    if (module->start_tempos)
        free(module->start_tempos);

    if (module->positions) {
        for (int i = 0; i < module->sub_song_num; i++) {
            if (module->positions[i]) {
                for (int j = 0; j < 4; j++)
                    free(module->positions[i][j]);
                free(module->positions[i]);
            }
        }
        free(module->positions);
    }

    if (module->tracks) {
        for (int i = 0; i < 128; i++)
            free(module->tracks[i]);
        free(module->tracks);
    }
    if (module->track_sizes)
        free(module->track_sizes);

    if (module->instruments) {
        for (int i = 0; i < module->inst_num; i++)
            free(module->instruments[i].sample_addr);
        free(module->instruments);
    }

    if (module->has_notes)
        free(module->has_notes);

    if (module->original_data)
        free(module->original_data);
    free(module);
}

int fred_subsong_count(const FredModule* module) {
    if (!module)
        return 0;
    return module->sub_song_num;
}

bool fred_start(FredModule* module, int subsong, NuadeVoices* voices) {
    if (!module || !voices || subsong < 0 || subsong >= module->sub_song_num)
        return false;

    module->voices = voices;
    initialize_sound(module, subsong);
    return true;
}

int fred_start_tempo(const FredModule* module, int subsong) {
    return (module && subsong >= 0 && subsong < module->sub_song_num) ? module->start_tempos[subsong] : 0;
}

FredVoiceState fred_voice_state(const FredModule* module, int ch) {
    if (!module || ch < 0 || ch >= FRED_CHANNELS)
        return (FredVoiceState) { .note = -1, .instrument = -1 };
    return (FredVoiceState) { .note = module->last_note[ch], .instrument = module->last_instrument[ch] };
}

int fred_channel_count(const FredModule* module) {
    (void)module;
    return 4;
}

// Edit API

int fred_get_instrument_count(const FredModule* module) {
    return module ? (int)module->inst_num : 0;
}

const char* fred_get_instrument_name(const FredModule* module, int inst) {
    (void)module;
    (void)inst;
    return ""; // Fred instruments have no names
}

float fred_get_instrument_param(const FredModule* module, int inst, const char* param) {
    if (!module || inst < 0 || inst >= (int)module->inst_num || !param || !module->instruments)
        return -1.0f;
    const FredInstrument* in = &module->instruments[inst];

    if (strcmp(param, "instrumentNumber") == 0)
        return (float)in->instrument_number;
    if (strcmp(param, "repeatLen") == 0)
        return (float)in->repeat_len;
    if (strcmp(param, "length") == 0)
        return (float)in->length;
    if (strcmp(param, "period") == 0)
        return (float)in->period;
    if (strcmp(param, "vibDelay") == 0)
        return (float)in->vib_delay;
    if (strcmp(param, "vibSpeed") == 0)
        return (float)in->vib_speed;
    if (strcmp(param, "vibAmpl") == 0)
        return (float)in->vib_ampl;
    if (strcmp(param, "envVol") == 0)
        return (float)in->env_vol;
    if (strcmp(param, "attackSpeed") == 0)
        return (float)in->attack_speed;
    if (strcmp(param, "attackVolume") == 0)
        return (float)in->attack_volume;
    if (strcmp(param, "decaySpeed") == 0)
        return (float)in->decay_speed;
    if (strcmp(param, "decayVolume") == 0)
        return (float)in->decay_volume;
    if (strcmp(param, "sustainDelay") == 0)
        return (float)in->sustain_delay;
    if (strcmp(param, "releaseSpeed") == 0)
        return (float)in->release_speed;
    if (strcmp(param, "releaseVolume") == 0)
        return (float)in->release_volume;
    if (strcmp(param, "arpSpeed") == 0)
        return (float)in->arp_speed;
    if (strcmp(param, "instType") == 0)
        return (float)in->inst_type;
    if (strcmp(param, "pulseRateMin") == 0)
        return (float)in->pulse_rate_min;
    if (strcmp(param, "pulseRatePlus") == 0)
        return (float)in->pulse_rate_plus;
    if (strcmp(param, "pulseSpeed") == 0)
        return (float)in->pulse_speed;
    if (strcmp(param, "pulseStart") == 0)
        return (float)in->pulse_start;
    if (strcmp(param, "pulseEnd") == 0)
        return (float)in->pulse_end;
    if (strcmp(param, "pulseDelay") == 0)
        return (float)in->pulse_delay;
    if (strcmp(param, "instSync") == 0)
        return (float)in->inst_sync;
    if (strcmp(param, "blend") == 0)
        return (float)in->blend;
    if (strcmp(param, "blendDelay") == 0)
        return (float)in->blend_delay;
    if (strcmp(param, "pulseShotCounter") == 0)
        return (float)in->pulse_shot_counter;
    if (strcmp(param, "blendShotCounter") == 0)
        return (float)in->blend_shot_counter;
    if (strcmp(param, "arpCount") == 0)
        return (float)in->arp_count;
    if (strcmp(param, "sampleSize") == 0)
        return (float)in->sample_size;

    return -1.0f;
}

void fred_set_instrument_param(FredModule* module, int inst, const char* param, float value) {
    if (!module || inst < 0 || inst >= (int)module->inst_num || !param || !module->instruments)
        return;
    FredInstrument* in = &module->instruments[inst];
    uint8_t b = (uint8_t)value;
    uint16_t v = (uint16_t)value;

    if (strcmp(param, "repeatLen") == 0) {
        in->repeat_len = v;
        return;
    }
    if (strcmp(param, "length") == 0) {
        in->length = v;
        return;
    }
    if (strcmp(param, "period") == 0) {
        in->period = v;
        return;
    }
    if (strcmp(param, "vibDelay") == 0) {
        in->vib_delay = b;
        return;
    }
    if (strcmp(param, "vibSpeed") == 0) {
        in->vib_speed = (int8_t)value;
        return;
    }
    if (strcmp(param, "vibAmpl") == 0) {
        in->vib_ampl = (int8_t)value;
        return;
    }
    if (strcmp(param, "envVol") == 0) {
        in->env_vol = b;
        return;
    }
    if (strcmp(param, "attackSpeed") == 0) {
        in->attack_speed = b;
        return;
    }
    if (strcmp(param, "attackVolume") == 0) {
        in->attack_volume = b;
        return;
    }
    if (strcmp(param, "decaySpeed") == 0) {
        in->decay_speed = b;
        return;
    }
    if (strcmp(param, "decayVolume") == 0) {
        in->decay_volume = b;
        return;
    }
    if (strcmp(param, "sustainDelay") == 0) {
        in->sustain_delay = b;
        return;
    }
    if (strcmp(param, "releaseSpeed") == 0) {
        in->release_speed = b;
        return;
    }
    if (strcmp(param, "releaseVolume") == 0) {
        in->release_volume = b;
        return;
    }
    if (strcmp(param, "arpSpeed") == 0) {
        in->arp_speed = b;
        return;
    }
    if (strcmp(param, "instType") == 0) {
        in->inst_type = (FredInstrumentType)(int)value;
        return;
    }
    if (strcmp(param, "pulseRateMin") == 0) {
        in->pulse_rate_min = (int8_t)value;
        return;
    }
    if (strcmp(param, "pulseRatePlus") == 0) {
        in->pulse_rate_plus = (int8_t)value;
        return;
    }
    if (strcmp(param, "pulseSpeed") == 0) {
        in->pulse_speed = b;
        return;
    }
    if (strcmp(param, "pulseStart") == 0) {
        in->pulse_start = b;
        return;
    }
    if (strcmp(param, "pulseEnd") == 0) {
        in->pulse_end = b;
        return;
    }
    if (strcmp(param, "pulseDelay") == 0) {
        in->pulse_delay = b;
        return;
    }
    if (strcmp(param, "instSync") == 0) {
        in->inst_sync = b;
        return;
    }
    if (strcmp(param, "blend") == 0) {
        in->blend = b;
        return;
    }
    if (strcmp(param, "blendDelay") == 0) {
        in->blend_delay = b;
        return;
    }
    if (strcmp(param, "pulseShotCounter") == 0) {
        in->pulse_shot_counter = b;
        return;
    }
    if (strcmp(param, "blendShotCounter") == 0) {
        in->blend_shot_counter = b;
        return;
    }
    if (strcmp(param, "arpCount") == 0) {
        in->arp_count = b > FRED_ARPEGGIO_SIZE ? FRED_ARPEGGIO_SIZE : b;
        return;
    }
}

size_t fred_export(const FredModule* module, uint8_t* out, size_t max_size) {
    if (!module || !module->original_data)
        return 0;
    size_t total = module->original_size;
    if (!out)
        return total;
    if (max_size < total)
        return 0;
    memcpy(out, module->original_data, total);
    return total;
}
