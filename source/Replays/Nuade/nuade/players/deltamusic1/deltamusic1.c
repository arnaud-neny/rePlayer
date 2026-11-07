// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/deltamusic1.h>

#include "nuade_base.h"
#include "nuade_paula.h"
#include "nuade_tracker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DM1_CHANNELS 4
#define DM1_INSTRUMENTS 20
#define DM1_HEADER_SIZE 104
#define DM1_TABLE_SIZE 48
#define DM1_MARK "ALL "

// Tables

static const uint16_t dm1_periods[]
    = { 0,    6848, 6464, 6096, 5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3616, 3424, 3232, 3048, 2880,
        2712, 2560, 2416, 2280, 2152, 2032, 1920, 1808, 1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076,
        960,  904,  856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  452,  428,  404,  381,
        360,  339,  320,  302,  285,  269,  254,  240,  226,  214,  202,  190,  180,  170,  160,  151,  143,
        135,  127,  120,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113

      };

#define DM1_PERIODS_COUNT (sizeof(dm1_periods) / sizeof(dm1_periods[0]))

// Reader

typedef struct Dm1Reader {
    const uint8_t* data;
    size_t size;
    size_t pos;
} Dm1Reader;

static void reader_init(Dm1Reader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
}

static bool reader_eof(const Dm1Reader* r) {
    return r->pos > r->size;
}

static uint8_t reader_read_uint8(Dm1Reader* r) {
    if (r->pos >= r->size) {
        r->pos = r->size + 1;
        return 0;
    }
    return r->data[r->pos++];
}

static int8_t reader_read_int8(Dm1Reader* r) {
    return (int8_t)reader_read_uint8(r);
}

static uint16_t reader_read_b_uint16(Dm1Reader* r) {
    uint8_t hi = reader_read_uint8(r);
    uint8_t lo = reader_read_uint8(r);
    return (uint16_t)((hi << 8) | lo);
}

static int32_t reader_read_b_int32(Dm1Reader* r) {
    uint16_t hi = reader_read_b_uint16(r);
    uint16_t lo = reader_read_b_uint16(r);
    return (int32_t)(((uint32_t)hi << 16) | lo);
}

static size_t reader_read(Dm1Reader* r, void* dst, size_t count) {
    size_t avail = 0;
    if (r->pos < r->size)
        avail = r->size - r->pos;
    if (count > avail)
        count = avail;
    if (count > 0) {
        memcpy(dst, r->data + r->pos, count);
        r->pos += count;
    }
    if (avail == 0 && count == 0)
        r->pos = r->size + 1;
    return count;
}

// IChannel emulation helpers

static void ch_play_sample(Dm1ChannelInfo* ch, const Dm1Instrument* inst, uint32_t start_offset, uint32_t length) {
    ch->ch_sample_data = inst->sample_data;
    ch->ch_data_size = inst->sample_data_size;
    ch->ch_sample_length = length;
    ch->ch_loop_start = 0;
    ch->ch_loop_length = 0;
    ch->ch_start_offset = start_offset;
    ch->play_pending = true;
    ch->active = true;
}

// Change the sample without retriggering it.
static void ch_set_sample_data(Dm1ChannelInfo* ch, const Dm1Instrument* inst, uint32_t start_offset, uint32_t length) {
    ch->ch_sample_data = inst->sample_data;
    ch->ch_data_size = inst->sample_data_size;
    ch->ch_sample_length = length;
    ch->ch_start_offset = start_offset;
    ch->play_pending = true;
}

static void ch_set_loop(Dm1ChannelInfo* ch, uint32_t start_offset, uint32_t length) {
    ch->ch_loop_start = start_offset;
    ch->ch_loop_length = length;
    ch->ch_sample_length = start_offset + length;
}

static void ch_set_loop_with_data(Dm1ChannelInfo* ch, const Dm1Instrument* inst, uint32_t start_offset,
                                  uint32_t length) {
    ch->ch_sample_data = inst->sample_data;
    ch->ch_data_size = inst->sample_data_size;
    ch->ch_loop_start = start_offset;
    ch->ch_loop_length = length;
    ch->ch_sample_length = start_offset + length;
}

static void ch_set_amiga_volume(Dm1ChannelInfo* ch, uint16_t vol) {
    if (vol > 64)
        vol = 64;
    ch->ch_volume = vol;
}

static void ch_set_amiga_period(Dm1ChannelInfo* ch, uint16_t period) {
    ch->ch_period = period;
}

// Playback methods

static void dm1_sound_table_handler(Dm1Module* m, int channel_number, Dm1ChannelInfo* channel, Dm1Instrument* inst) {
    (void)m;
    (void)channel_number;
    channel->sound_table_delay = inst->table_delay;

    // A table made only of jumps and delay changes never plays a sample: give up after two passes over it.
    for (int steps = 0; steps < 2 * DM1_TABLE_SIZE && channel->sound_table != nullptr; steps++) {
        if (channel->sound_table_counter >= DM1_TABLE_SIZE)
            channel->sound_table_counter = 0;

        uint8_t data = channel->sound_table[channel->sound_table_counter];
        if (data == 0xff)
            channel->sound_table_counter = channel->sound_table_counter + 1 < DM1_TABLE_SIZE
                                               ? channel->sound_table[channel->sound_table_counter + 1]
                                               : 0;
        else if (data >= 0x80) {
            inst->table_delay = (uint8_t)(data & 0x7f);
            channel->sound_table_counter++;
        } else {
            channel->sound_table_counter++;

            data *= 32;

            if (channel->retrigger_sound) {
                ch_play_sample(channel, inst, data, inst->sample_length);
                channel->retrigger_sound = false;
            } else {
                ch_set_sample_data(channel, inst, data, inst->sample_length);
            }

            ch_set_loop_with_data(channel, inst, data, inst->sample_length);
            break;
        }
    }
}

static uint16_t dm1_period(unsigned index) {
    return dm1_periods[index < DM1_PERIODS_COUNT ? index : DM1_PERIODS_COUNT - 1];
}

static void dm1_portamento_handler(Dm1ChannelInfo* channel, Dm1Instrument* inst) {
    if (inst->portamento != 0) {
        if (channel->period == 0)
            channel->period = (uint16_t)(dm1_period(channel->frequency_data) + channel->bend_rate_frequency);
        else {
            uint16_t period = channel->period;
            uint16_t wanted_period = (uint16_t)(dm1_period(channel->frequency_data) + channel->bend_rate_frequency);

            if (period > wanted_period) {
                period -= inst->portamento;
                if (period < wanted_period)
                    channel->period = wanted_period;
                else
                    channel->period = period;
            } else if (period < wanted_period) {
                period += inst->portamento;
                if (period > wanted_period)
                    channel->period = wanted_period;
                else
                    channel->period = period;
            }
        }
    }
}

static void dm1_vibrato_handler(Dm1ChannelInfo* channel, Dm1Instrument* inst) {
    if (channel->vibrato_wait == 0) {
        channel->vibrato_frequency = (uint16_t)(channel->vibrato_position * inst->vibrato_step);

        if ((channel->status & 0x01) != 0) {
            channel->vibrato_position--;
            if (channel->vibrato_position == 0)
                channel->status ^= 0x01;
        } else {
            channel->vibrato_position++;
            if (channel->vibrato_position == channel->vibrato_compare)
                channel->status ^= 0x01;
        }
    } else {
        channel->vibrato_wait--;
    }
}

static void dm1_bendrate_handler(Dm1ChannelInfo* channel, Dm1Instrument* inst) {
    if (inst->bend_rate >= 0)
        channel->bend_rate_frequency = (short)(channel->bend_rate_frequency - inst->bend_rate);
    else
        channel->bend_rate_frequency = (short)(channel->bend_rate_frequency + -(inst->bend_rate));
}

static void dm1_change_speed(Dm1Module* m, uint8_t new_speed) {
    if (new_speed != m->playing_info.play_speed) {
        m->playing_info.play_speed = new_speed;
    }
}

static void dm1_effect_handler(Dm1Module* m, Dm1ChannelInfo* channel, Dm1Instrument* inst) {
    uint8_t data = channel->effect_data;

    switch (channel->effect_number) {
        case DM1_EFFECT_SET_SPEED:
            if (data != 0)
                dm1_change_speed(m, data);
            break;

        case DM1_EFFECT_SLIDE_UP:
            channel->bend_rate_frequency -= data;
            break;

        case DM1_EFFECT_SLIDE_DOWN:
            channel->bend_rate_frequency += data;
            break;

        case DM1_EFFECT_SET_FILTER:
            // AmigaFilter = data == 0; (no-op in WASM)
            break;

        case DM1_EFFECT_SET_VIBRATO_WAIT:
            inst->vibrato_wait = data;
            break;

        case DM1_EFFECT_SET_VIBRATO_STEP:
            inst->vibrato_step = data;
            break;

        case DM1_EFFECT_SET_VIBRATO_LEN:
            inst->vibrato_length = data;
            break;

        case DM1_EFFECT_SET_BEND_RATE:
            inst->bend_rate = (int8_t)data;
            break;

        case DM1_EFFECT_SET_PORTAMENTO:
            inst->portamento = data;
            break;

        case DM1_EFFECT_SET_VOLUME:
            if (data > 64)
                data = 64;
            inst->volume = data;
            break;

        case DM1_EFFECT_SET_ARP1:
            inst->arpeggio[0] = data;
            break;

        case DM1_EFFECT_SET_ARP2:
            inst->arpeggio[1] = data;
            break;

        case DM1_EFFECT_SET_ARP3:
            inst->arpeggio[2] = data;
            break;

        case DM1_EFFECT_SET_ARP4:
            inst->arpeggio[3] = data;
            break;

        case DM1_EFFECT_SET_ARP5:
            inst->arpeggio[4] = data;
            break;

        case DM1_EFFECT_SET_ARP6:
            inst->arpeggio[5] = data;
            break;

        case DM1_EFFECT_SET_ARP7:
            inst->arpeggio[6] = data;
            break;

        case DM1_EFFECT_SET_ARP8:
            inst->arpeggio[7] = data;
            break;

        case DM1_EFFECT_SET_ARP1_5:
            inst->arpeggio[0] = data;
            inst->arpeggio[4] = data;
            break;

        case DM1_EFFECT_SET_ARP2_6:
            inst->arpeggio[1] = data;
            inst->arpeggio[5] = data;
            break;

        case DM1_EFFECT_SET_ARP3_7:
            inst->arpeggio[2] = data;
            inst->arpeggio[6] = data;
            break;

        case DM1_EFFECT_SET_ARP4_8:
            inst->arpeggio[3] = data;
            inst->arpeggio[7] = data;
            break;

        case DM1_EFFECT_SET_ATTACK_STEP:
            if (data > 64)
                data = 64;
            inst->attack_step = data;
            break;

        case DM1_EFFECT_SET_ATTACK_DELAY:
            inst->attack_delay = data;
            break;

        case DM1_EFFECT_SET_DECAY_STEP:
            if (data > 64)
                data = 64;
            inst->decay_step = data;
            break;

        case DM1_EFFECT_SET_DECAY_DELAY:
            inst->decay_delay = data;
            break;

        case DM1_EFFECT_SET_SUSTAIN1:
            inst->sustain = (uint16_t)((inst->sustain & 0x00ff) | (data << 8));
            break;

        case DM1_EFFECT_SET_SUSTAIN2:
            inst->sustain = (uint16_t)((inst->sustain & 0xff00) | data);
            break;

        case DM1_EFFECT_SET_RELEASE_STEP:
            if (data > 64)
                data = 64;
            inst->release_step = data;
            break;

        case DM1_EFFECT_SET_RELEASE_DELAY:
            inst->release_delay = data;
            break;

        default:
            break;
    }
}

static void dm1_arpeggio_handler(int channel_number, Dm1ChannelInfo* channel, Dm1Instrument* inst) {
    uint8_t arp = inst->arpeggio[channel->arpeggio_counter];

    channel->arpeggio_counter++;
    channel->arpeggio_counter &= 0x07;

    uint16_t new_period = dm1_period((unsigned)channel->frequency_data + arp);
    new_period = (uint16_t)((new_period - channel->vibrato_length * inst->vibrato_step) + channel->bend_rate_frequency);

    if (inst->portamento != 0)
        new_period = channel->period;
    else
        channel->period = 0;

    new_period += channel->vibrato_frequency;
    ch_set_amiga_period(channel, new_period);
    (void)channel_number;
}

static void dm1_volume_handler(int channel_number, Dm1ChannelInfo* channel, Dm1Instrument* inst) {
    int actual_volume = channel->actual_volume;
    uint8_t status = (uint8_t)(channel->status & 0x0e);

    if (status == 0) {
        if (channel->attack_delay == 0) {
            channel->attack_delay = inst->attack_delay;
            actual_volume += inst->attack_step;

            if (actual_volume >= 64) {
                actual_volume = 64;
                status |= 0x02;
                channel->status |= 0x02;
            }
        } else {
            channel->attack_delay--;
        }
    }

    if (status == 0x02) {
        if (channel->decay_delay == 0) {
            channel->decay_delay = inst->decay_delay;
            actual_volume -= inst->decay_step;

            if (actual_volume <= inst->volume) {
                actual_volume = inst->volume;
                status |= 0x06;
                channel->status |= 0x06;
            }
        } else {
            channel->decay_delay--;
        }
    }

    if (status == 0x06) {
        if (channel->sustain == 0) {
            status |= 0x0e;
            channel->status |= 0x0e;
        } else {
            channel->sustain--;
        }
    }

    if (status == 0x0e) {
        if (channel->release_delay == 0) {
            channel->release_delay = inst->release_delay;
            actual_volume -= inst->release_step;

            if (actual_volume <= 0) {
                actual_volume = 0;
                channel->status &= 0x09;
            }
        } else {
            channel->release_delay--;
        }
    }

    channel->actual_volume = (uint8_t)actual_volume;
    ch_set_amiga_volume(channel, (uint16_t)actual_volume);
    (void)channel_number;
}

static void dm1_calculate_frequency(Dm1Module* m, int channel_number, Dm1ChannelInfo* channel) {
    Dm1Instrument* inst = channel->sound_data;

    channel->play_speed--;
    if (channel->play_speed == 0) {
        channel->play_speed = m->playing_info.play_speed;

        if (channel->block_counter == 0) {
            Dm1Track new_track;

            if (channel->track_counter >= channel->track_length) {
                channel->track_counter = 0;
                m->end_reached[channel_number] = true;
            }

            // A song made only of jumps would never leave this loop: give up after one pass over the track.
            for (uint32_t jumps = 0;; jumps++) {
                new_track = channel->track[channel->track_counter];
                if ((new_track.block_number != 0xff) || (new_track.transpose != -1) || jumps > channel->track_length)
                    break;

                uint16_t old_track_counter = channel->track_counter;

                Dm1Track next = channel->track[channel->track_counter + 1];
                channel->track_counter = (uint16_t)(((next.block_number << 8) | (uint8_t)next.transpose) & 0x7ff);
                if (channel->track_counter >= channel->track_length)
                    channel->track_counter = 0;

                if (channel->track_counter < old_track_counter)
                    m->end_reached[channel_number] = true;
            }

            channel->transpose = new_track.transpose;
            channel->block = m->blocks[new_track.block_number < m->num_blocks ? new_track.block_number : 0];
            channel->track_counter++;
        }

        Dm1BlockLine* block_line = &channel->block[channel->block_counter];

        if (block_line->effect != DM1_EFFECT_NONE) {
            channel->effect_number = block_line->effect;
            channel->effect_data = block_line->effect_arg;
        }

        if (block_line->note != 0) {
            channel->frequency_data = (uint8_t)(block_line->note + channel->transpose);

            channel->status = 0;
            channel->bend_rate_frequency = 0;
            channel->arpeggio_counter = 0;

            channel->effect_number = block_line->effect;
            channel->effect_data = block_line->effect_arg;

            if (block_line->instrument >= DM1_INSTRUMENTS
                || m->playing_info.instruments[block_line->instrument] == nullptr) {
                channel->block_counter = (channel->block_counter + 1) % 16;
                return;
            }
            inst = m->playing_info.instruments[block_line->instrument];
            channel->sound_data = inst;

            channel->sound_table = inst->table;
            channel->sound_table_counter = 0;

            if (inst->is_sample) {
                ch_play_sample(channel, inst, 0, inst->sample_length);

                if (inst->repeat_length > 1)
                    ch_set_loop(channel, inst->repeat_start, inst->repeat_length);
            } else {
                channel->retrigger_sound = true;
            }

            channel->vibrato_wait = inst->vibrato_wait;
            uint8_t vib_len = inst->vibrato_length;
            channel->vibrato_length = vib_len;
            channel->vibrato_position = vib_len;
            channel->vibrato_compare = (uint8_t)(vib_len * 2);

            channel->actual_volume = 0;
            channel->sound_table_delay = 0;
            channel->sound_table_counter = 0;
            channel->attack_delay = 0;
            channel->decay_delay = 0;
            channel->sustain = inst->sustain;
            channel->release_delay = 0;
        }

        channel->block_counter++;
        if (channel->block_counter == 16)
            channel->block_counter = 0;
    }

    if (inst != nullptr) {
        if (!inst->is_sample) {
            if (channel->sound_table_delay == 0)
                dm1_sound_table_handler(m, channel_number, channel, inst);
            else
                channel->sound_table_delay--;
        }

        dm1_portamento_handler(channel, inst);
        dm1_vibrato_handler(channel, inst);
        dm1_bendrate_handler(channel, inst);
        dm1_effect_handler(m, channel, inst);
        dm1_arpeggio_handler(channel_number, channel, inst);
        dm1_volume_handler(channel_number, channel, inst);
    }
}

static void play_tick(Dm1Module* m) {
    for (int i = 0; i < 4; i++)
        dm1_calculate_frequency(m, i, &m->channels[i]);

    // Check for end reached on any channel
    // rePlayer begin
//     bool ended = false;
//     for (int i = 0; i < 4; i++) {
//         if (m->end_reached[i]) {
//             ended = true;
//             m->end_reached[i] = false;
//         }
//     }
//     if (ended)
//         m->loop_count++;
    int ended = 4;
    for (int i = 0; i < 4; i++) {
        if (m->end_reached[i])
            ended--;
    }
    if (!ended) {
        m->loop_count++;
        for (int i = 0; i < 4; i++)
            m->end_reached[i] = false;
    }
    // rePlayer end
}

// Initialize sound

static bool initialize_sound(Dm1Module* m) {
    m->playing_info.play_speed = 6;

    // Rebuild instruments from backup
    for (int i = 0; i < 20; i++) {
        if (m->backup_instruments[i] != nullptr) {
            // Deep clone the instrument
            Dm1Instrument* src = m->backup_instruments[i];
            Dm1Instrument* dst = (Dm1Instrument*)malloc(sizeof(Dm1Instrument));
            if (dst == nullptr)
                return false;
            *dst = *src;

            // Clone arpeggio array
            memcpy(dst->arpeggio, src->arpeggio, 8);

            // Table and sample_data point to the backup's data (shared, not cloned)
            m->playing_info.instruments[i] = dst;
        } else {
            m->playing_info.instruments[i] = nullptr;
        }
    }

    // Find first non-null instrument for default sound table
    uint8_t* default_table = nullptr;
    for (int i = 0; i < 20; i++) {
        if (m->playing_info.instruments[i] != nullptr) {
            default_table = m->playing_info.instruments[i]->table;
            break;
        }
    }

    m->loop_count = 0;
    for (int i = 0; i < 4; i++)
        m->end_reached[i] = false;

    for (int i = 0; i < 4; i++) {
        Dm1ChannelInfo* ch = &m->channels[i];
        ch->sound_data = nullptr;
        ch->period = 0;
        ch->sound_table = default_table;
        ch->sound_table_counter = 0;
        ch->sound_table_delay = 0;
        ch->track = m->tracks[i];
        ch->track_length = m->track_lengths[i];
        ch->track_counter = 0;
        ch->block = m->blocks[0];
        ch->block_counter = 0;
        ch->vibrato_wait = 0;
        ch->vibrato_length = 0;
        ch->vibrato_position = 0;
        ch->vibrato_compare = 0;
        ch->vibrato_frequency = 0;
        ch->frequency_data = 0;
        ch->actual_volume = 0;
        ch->attack_delay = 0;
        ch->decay_delay = 0;
        ch->sustain = 0;
        ch->release_delay = 0;
        ch->play_speed = 1;
        ch->bend_rate_frequency = 0;
        ch->transpose = 0;
        ch->status = 0;
        ch->arpeggio_counter = 0;
        ch->effect_number = DM1_EFFECT_NONE;
        ch->effect_data = 0;
        ch->retrigger_sound = false;

        ch->ch_sample_data = nullptr;
        ch->ch_sample_length = 0;
        ch->ch_loop_start = 0;
        ch->ch_loop_length = 0;
        ch->ch_volume = 0;
        ch->ch_period = 0;
        ch->ch_start_offset = 0;
        ch->play_pending = false;
        ch->active = false;
        ch->ch_data_size = 0;
    }

    return true;
}

// Loading

static bool dm1_load(Dm1Module* m, const uint8_t* data, size_t size) {
    Dm1Reader reader;
    reader_init(&reader, data, size);

    // Check mark
    uint8_t mark[4];
    reader_read(&reader, mark, 4);
    if (memcmp(mark, "ALL ", 4) != 0)
        return false;

    // Read all the different lengths
    uint32_t track_lengths[4];
    for (int i = 0; i < 4; i++)
        track_lengths[i] = (uint32_t)reader_read_b_int32(&reader);

    uint32_t block_length = (uint32_t)reader_read_b_int32(&reader);

    uint32_t instrument_lengths[20];
    for (int i = 0; i < 20; i++)
        instrument_lengths[i] = (uint32_t)reader_read_b_int32(&reader);

    // Read the tracks
    for (int i = 0; i < 4; i++) {
        uint32_t length = track_lengths[i] / 2;
        m->tracks[i] = (Dm1Track*)calloc(length + 1, sizeof(Dm1Track));
        if (m->tracks[i] == nullptr)
            return false;
        m->track_lengths[i] = (uint16_t)length;

        for (uint32_t j = 0; j < length; j++) {
            m->tracks[i][j].block_number = reader_read_uint8(&reader);
            m->tracks[i][j].transpose = reader_read_int8(&reader);
        }
    }

    if (reader_eof(&reader))
        return false;

    // Read the blocks
    uint32_t count = block_length / 64;
    m->num_blocks = count;
    m->blocks = (Dm1BlockLine**)calloc(count, sizeof(Dm1BlockLine*));
    if (m->blocks == nullptr)
        return false;

    for (uint32_t i = 0; i < count; i++) {
        Dm1BlockLine* lines = (Dm1BlockLine*)calloc(16, sizeof(Dm1BlockLine));
        if (lines == nullptr)
            return false;

        for (int j = 0; j < 16; j++) {
            lines[j].instrument = reader_read_uint8(&reader);
            lines[j].note = reader_read_uint8(&reader);
            lines[j].effect = (Dm1Effect)reader_read_uint8(&reader);
            lines[j].effect_arg = reader_read_uint8(&reader);
        }

        m->blocks[i] = lines;

        if (reader_eof(&reader))
            return false;
    }

    // Read the instruments
    for (short i = 0; i < 20; i++) {
        uint32_t length = instrument_lengths[i];
        if (length != 0) {
            Dm1Instrument* inst = (Dm1Instrument*)calloc(1, sizeof(Dm1Instrument));
            if (inst == nullptr)
                return false;
            inst->number = i;

            inst->attack_step = reader_read_uint8(&reader);
            inst->attack_delay = reader_read_uint8(&reader);
            inst->decay_step = reader_read_uint8(&reader);
            inst->decay_delay = reader_read_uint8(&reader);
            inst->sustain = reader_read_b_uint16(&reader);
            inst->release_step = reader_read_uint8(&reader);
            inst->release_delay = reader_read_uint8(&reader);
            inst->volume = reader_read_uint8(&reader);
            inst->vibrato_wait = reader_read_uint8(&reader);
            inst->vibrato_step = reader_read_uint8(&reader);
            inst->vibrato_length = reader_read_uint8(&reader);
            inst->bend_rate = reader_read_int8(&reader);
            inst->portamento = reader_read_uint8(&reader);
            inst->is_sample = reader_read_uint8(&reader) != 0;
            inst->table_delay = reader_read_uint8(&reader);

            reader_read(&reader, inst->arpeggio, 8);

            inst->sample_length = reader_read_b_uint16(&reader);
            inst->repeat_start = reader_read_b_uint16(&reader);
            inst->repeat_length = reader_read_b_uint16(&reader);

            if (!inst->is_sample) {
                inst->table = (uint8_t*)calloc(DM1_TABLE_SIZE, 1);
                if (inst->table == nullptr) {
                    free(inst);
                    return false;
                }
                reader_read(&reader, inst->table, DM1_TABLE_SIZE);
            }

            if (reader_eof(&reader)) {
                free(inst->table);
                free(inst);
                return false;
            }

            const uint32_t header_size = inst->is_sample ? 30U : 78U;
            const uint32_t sample_data_len = length > header_size ? length - header_size : 0;
            if (sample_data_len > 0) {
                inst->sample_data = (int8_t*)malloc(sample_data_len);
                if (inst->sample_data == nullptr) {
                    free(inst->table);
                    free(inst);
                    return false;
                }
                inst->sample_data_size = (uint32_t)reader_read(&reader, inst->sample_data, sample_data_len);
            }

            if (reader_eof(&reader)) {
                free(inst->table);
                free(inst->sample_data);
                free(inst);
                return false;
            }

            m->backup_instruments[i] = inst;
        }
    }

    return true;
}

// Voices

// Hand the registers the tick wrote to the voice. Channels 0 and 3 are the left ones.
static void sync_voice(Dm1Module* m, uint32_t index) {
    Dm1ChannelInfo* c = &m->channels[index];

    if (!c->active || c->ch_sample_data == nullptr) {
        if (nuade_voice_is_playing(m->voices, index))
            nuade_voice_stop(m->voices, index);
    } else if (c->play_pending) {
        // Data past the end of the instrument is not there to read
        const uint32_t length = c->ch_sample_length < c->ch_data_size ? c->ch_sample_length : c->ch_data_size;
        const NuadeSample sample = { .data = c->ch_sample_data,
                                     .format = NuadeSampleFormat_S8,
                                     .length = length,
                                     .loop_start = c->ch_loop_start,
                                     .loop_length = c->ch_loop_length };
        nuade_voice_play(m->voices, index, &sample, c->ch_start_offset);
    }
    c->play_pending = false;

    const bool silent = c->ch_period == 0;
    nuade_voice_set_pitch(m->voices, index, silent ? 0.0 : nuade_paula_period_hz(c->ch_period));
    nuade_voice_set_volume(m->voices, index, silent ? 0.0f : (float)c->ch_volume / 64.0f);
    nuade_voice_set_pan(m->voices, index, (index == 0 || index == 3) ? -1.0f : 1.0f);
}

bool dm1_tick(Dm1Module* module) {
    if (!module || !module->voices)
        return false;

    for (uint32_t i = 0; i < DM1_CHANNELS; i++) {
        Dm1ChannelInfo* c = &module->channels[i];
        c->active = c->active && nuade_voice_is_playing(module->voices, i);
    }

    const uint32_t loops = module->loop_count;
    play_tick(module);

    for (uint32_t i = 0; i < DM1_CHANNELS; i++)
        sync_voice(module, i);
    return module->loop_count != loops;
}

// Public API

static uint32_t read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

bool dm1_probe(const uint8_t* data, size_t size) {
    if (!data || size < DM1_HEADER_SIZE || memcmp(data, DM1_MARK, 4) != 0)
        return false;

    // The header lists the byte length of every part; they have to fit the file, tracks hold 2-byte entries and
    // blocks 16 lines of 4 bytes.
    uint64_t total = DM1_HEADER_SIZE;
    for (int i = 0; i < DM1_CHANNELS; i++) {
        const uint32_t track_length = read_be32(data + 4 + i * 4);
        if (track_length == 0 || (track_length & 1) != 0)
            return false;
        total += track_length;
    }

    const uint32_t block_length = read_be32(data + 20);
    if (block_length == 0 || (block_length % 64) != 0)
        return false;
    total += block_length;

    for (int i = 0; i < DM1_INSTRUMENTS; i++)
        total += read_be32(data + 24 + i * 4);

    return total <= size;
}

Dm1Module* dm1_create(const uint8_t* data, size_t size) {
    if (!dm1_probe(data, size))
        return nullptr;

    Dm1Module* m = (Dm1Module*)calloc(1, sizeof(Dm1Module));
    if (!m)
        return nullptr;

    if (!dm1_load(m, data, size)) {
        dm1_destroy(m);
        return nullptr;
    }

    return m;
}

void dm1_destroy(Dm1Module* module) {
    if (!module)
        return;

    for (int i = 0; i < 4; i++)
        free(module->tracks[i]);

    if (module->blocks) {
        for (uint32_t i = 0; i < module->num_blocks; i++)
            free(module->blocks[i]);
        free(module->blocks);
    }

    for (int i = 0; i < 20; i++) {
        if (module->backup_instruments[i]) {
            free(module->backup_instruments[i]->table);
            free(module->backup_instruments[i]->sample_data);
            free(module->backup_instruments[i]);
        }
    }

    // Free cloned playing instruments
    for (int i = 0; i < 20; i++) {
        if (module->playing_info.instruments[i] != nullptr) {
            // Check it's not pointing to the same backup
            bool is_backup = false;
            for (int j = 0; j < 20; j++) {
                if (module->playing_info.instruments[i] == module->backup_instruments[j]) {
                    is_backup = true;
                    break;
                }
            }
            if (!is_backup)
                free(module->playing_info.instruments[i]);
        }
    }

    free(module);
}

int dm1_subsong_count(const Dm1Module* module) {
    if (!module)
        return 0;
    return 1;
}

bool dm1_start(Dm1Module* module, int subsong, NuadeVoices* voices) {
    if (!module || !voices || subsong != 0)
        return false;

    // Free old playing instruments
    for (int i = 0; i < 20; i++) {
        if (module->playing_info.instruments[i] != nullptr) {
            bool is_backup = false;
            for (int j = 0; j < 20; j++) {
                if (module->playing_info.instruments[i] == module->backup_instruments[j]) {
                    is_backup = true;
                    break;
                }
            }
            if (!is_backup)
                free(module->playing_info.instruments[i]);
            module->playing_info.instruments[i] = nullptr;
        }
    }

    module->voices = voices;
    return initialize_sound(module);
}

// Display

// The semitone from C-0 + DM1_NOTE_OCTAVE_SHIFT octaves of a note index; the period table skips a semitone.
static int dm1_note_of_index(unsigned index) {
    if (index == 0)
        return -1;
    return nuade_period_to_note(dm1_period(index) / 4);
}

static bool dm1_is_jump(const Dm1Track* t) {
    return t->block_number == 0xff && t->transpose == -1;
}

int dm1_order_count(const Dm1Module* module) {
    int count = 0;
    for (int i = 0; i < DM1_CHANNELS; i++)
        if (module->track_lengths[i] > count)
            count = module->track_lengths[i];
    return count;
}

Dm1VoiceState dm1_voice_state(const Dm1Module* module, int voice) {
    const Dm1ChannelInfo* c = &module->channels[voice];
    Dm1VoiceState s
        = { .note = -1,
            .instrument = -1,
            .order = c->track_counter > 0 ? c->track_counter - 1 : 0,
            .row = c->track_counter > 0 ? (int)((c->block_counter + DM1_BLOCK_LINES - 1) % DM1_BLOCK_LINES) : 0,
            .speed = module->playing_info.play_speed };
    if (c->sound_data != nullptr) {
        s.note = dm1_note_of_index(c->frequency_data);
        s.instrument = c->sound_data->number;
    }
    return s;
}

Dm1Cell dm1_cell(const Dm1Module* module, int order, int voice, int row) {
    const Dm1Cell empty = { -1, -1, 0, 0 };
    if (voice < 0 || voice >= DM1_CHANNELS || order < 0 || order >= module->track_lengths[voice] || row < 0
        || row >= DM1_BLOCK_LINES)
        return empty;
    const Dm1Track* t = &module->tracks[voice][order];
    if (dm1_is_jump(t))
        return empty;
    const Dm1BlockLine* line = &module->blocks[t->block_number < module->num_blocks ? t->block_number : 0][row];
    Dm1Cell cell = empty;
    if (line->note != 0) {
        cell.note = dm1_note_of_index((uint8_t)(line->note + t->transpose));
        cell.instrument = line->instrument;
    }
    cell.effect = line->effect;
    cell.effect_arg = line->effect_arg;
    return cell;
}
