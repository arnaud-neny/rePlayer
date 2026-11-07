// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/soundmon.h>

#include "nuade_base.h"
#include "nuade_paula.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SM_CHANNELS 4

// Enums

typedef enum SmOptional {
    SM_OPT_ARPEGGIO_ONCE = 0x0,
    SM_OPT_SET_VOLUME = 0x1,
    SM_OPT_SET_SPEED = 0x2,
    SM_OPT_FILTER = 0x3,
    SM_OPT_PORT_UP = 0x4,
    SM_OPT_PORT_DOWN = 0x5,
    SM_OPT_SET_REP_COUNT = 0x6,  // SoundMon 1.1
    SM_OPT_DBRA_REP_COUNT = 0x7, // SoundMon 1.1
    SM_OPT_VIBRATO = 0x6,        // SoundMon 2.2
    SM_OPT_JUMP = 0x7,           // SoundMon 2.2
    SM_OPT_SET_AUTO_SLIDE = 0x8,
    SM_OPT_SET_ARPEGGIO = 0x9,
    SM_OPT_TRANSPOSE = 0xa,
    SM_OPT_CHANGE_FX = 0xb,
    SM_OPT_CHANGE_INVERSION = 0xd,
    SM_OPT_RESET_ADSR = 0xe,
    SM_OPT_CHANGE_NOTE = 0xf
} SmOptional;

// Tables

static const uint16_t sm_periods[84]
    = { 6848, 6464, 6080, 5760, 5440, 5120, 4832, 4576, 4320, 4064, 3840, 3616, 3424, 3232, 3040, 2880, 2720,
        2560, 2416, 2288, 2160, 2032, 1920, 1808, 1712, 1616, 1520, 1440, 1360, 1280, 1208, 1144, 1080, 1016,
        960,  904,  856,  808,  760,  720,  680,  640,  604,  572,  540,  508,  480,  452,  428,  404,  380,
        360,  340,  320,  302,  286,  270,  254,  240,  226,  214,  202,  190,  180,  170,  160,  151,  143,
        135,  127,  120,  113,  107,  101,  95,   90,   85,   80,   76,   72,   68,   64,   60,   57

      };

static const int16_t sm_vibrato_table[8] = { 0, 64, 128, 64, 0, -64, -128, -64

};

// Structs

// Every wave table offset the synth reaches: a table index (a byte, times 64) plus a 16-bit length or table pointer.
// The working copy of the wave tables is this big and zero past the module's tables, so a damaged instrument cannot
// reach outside it; the mixer and the effects read zeros there.
#define SM_WAVE_REACH (256 * 64 + 65536)

// Big-endian reader helpers

typedef struct SmReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
} SmReader;

static void reader_init(SmReader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
}

static bool reader_eof(const SmReader* r) {
    // Match NostalgicPlayer EndOfStream: only true when a read overshot the end,
    // NOT when we're exactly at the end after a complete read.
    return r->pos > r->size;
}

static uint8_t read_u8(SmReader* r) {
    if (r->pos >= r->size)
        return 0;
    return r->data[r->pos++];
}

static int8_t read_i8(SmReader* r) {
    return (int8_t)read_u8(r);
}

static uint16_t read_b_u16(SmReader* r) {
    uint8_t hi = read_u8(r);
    uint8_t lo = read_u8(r);
    return (uint16_t)((hi << 8) | lo);
}

static void reader_skip(SmReader* r, size_t count) {
    r->pos += count;
    if (r->pos > r->size)
        r->pos = r->size;
}

static void read_string(SmReader* r, char* out, int max_len) {
    for (int i = 0; i < max_len; i++) {
        if (r->pos >= r->size) {
            out[i] = '\0';
            return;
        }
        out[i] = (char)r->data[r->pos++];
    }
    out[max_len] = '\0';
}

static void read_signed_bytes(SmReader* r, int8_t* out, int count) {
    for (int i = 0; i < count; i++) {
        out[i] = read_i8(r);
    }
}

// Position visited tracking

static void clear_visited(SmModule* m) {
    memset(m->visited, 0, sizeof(m->visited));
}

static void mark_position_visited(SmModule* m, uint16_t pos) {
    if (pos < SM_MAX_POSITIONS)
        m->visited[pos] = 1;
}

static bool has_position_been_visited(const SmModule* m, uint16_t pos) {
    if (pos < SM_MAX_POSITIONS)
        return m->visited[pos] != 0;
    return false;
}

// Channel helpers (maps IChannel interface calls to direct channel state)

static void channel_play_sample(SmChannel* ch, int16_t sample_number, const int8_t* adr, uint32_t start_offset,
                                uint32_t length) {
    ch->sample_number = sample_number;
    ch->sample_data = adr;
    ch->sample_offset = start_offset;
    ch->sample_length = start_offset + length;
    ch->loop_start = 0;
    ch->loop_length = 0;
    ch->active = true;
    ch->play_pending = true;
}

static void channel_set_loop(SmChannel* ch, uint32_t start_offset, uint32_t length) {
    ch->loop_start = start_offset;
    ch->loop_length = length;
    ch->sample_length = start_offset + length;
}

// AUDxVOL decodes its low 7 bits, and bit 6 means full volume; 65..127 is 64, 128 wraps to 0.
static uint16_t paula_volume(uint32_t reg) {
    reg &= 0x7F;
    return (uint16_t)((reg & 0x40) ? 64 : reg);
}

static void channel_set_amiga_volume(SmChannel* ch, uint16_t vol) {
    // Amiga volume 0-64 -> internal 0-256
    ch->volume = (uint16_t)(paula_volume(vol) * 4);
}

// The ADSR writes (table value 0-63) * voice volume / 64 to AUDxVOL.
static void channel_set_adsr_volume(SmChannel* ch, int8_t table_byte, uint8_t volume) {
    const uint32_t level = (uint32_t)((table_byte + 128) / 4) * volume / 64;
    channel_set_amiga_volume(ch, (uint16_t)level);
}

static void channel_set_amiga_period(SmChannel* ch, uint16_t period) {
    ch->period = period;
}

static void channel_mute(SmChannel* ch) {
    ch->active = false;
    ch->play_pending = false;
}

// Loading

static bool load_module(SmModule* m, SmReader* r, SmModuleType type) {
    m->module_type = type;

    // Read module name (26 bytes)
    read_string(r, m->module_name, 26);

    // Skip the mark (3 bytes) - already consumed by identify
    reader_skip(r, 3);

    // Get number of waveforms
    m->wave_num = read_u8(r);

    // Get number of positions (steps)
    m->step_num = read_b_u16(r);

    if (reader_eof(r))
        return false;

    // Read instrument information
    for (int i = 0; i < 15; i++) {
        uint8_t first_byte = read_u8(r);

        if (first_byte == 0xff) {
            // Synth instrument
            SmSynthInstrument* si = &m->instruments[i].synth;
            m->instruments[i].is_synth = true;

            if (type == SM_TYPE_SOUNDMON_11) {
                si->wave_table = read_u8(r);
                si->wave_length = (uint16_t)(read_b_u16(r) * 2);
                si->adsr_control = read_u8(r);
                si->adsr_table = read_u8(r);
                si->adsr_length = read_b_u16(r);
                si->adsr_speed = read_u8(r);
                si->lfo_control = read_u8(r);
                si->lfo_table = read_u8(r);
                si->lfo_depth = read_u8(r);
                si->lfo_length = read_b_u16(r);
                reader_skip(r, 1);
                si->lfo_delay = read_u8(r);
                si->lfo_speed = read_u8(r);
                si->eg_control = read_u8(r);
                si->eg_table = read_u8(r);
                si->eg_start = read_u8(r);
                si->eg_length = read_b_u16(r);
                reader_skip(r, 1);
                si->eg_delay = read_u8(r);
                si->eg_speed = read_u8(r);
                si->fx_control = 0;
                si->fx_speed = 1;
                si->fx_delay = 0;
                si->mod_control = 0;
                si->mod_table = 0;
                si->mod_speed = 1;
                si->mod_delay = 0;
                si->volume = read_u8(r);
                si->mod_length = 0;
                reader_skip(r, 6);
            } else {
                si->wave_table = read_u8(r);
                si->wave_length = (uint16_t)(read_b_u16(r) * 2);
                si->adsr_control = read_u8(r);
                si->adsr_table = read_u8(r);
                si->adsr_length = read_b_u16(r);
                si->adsr_speed = read_u8(r);
                si->lfo_control = read_u8(r);
                si->lfo_table = read_u8(r);
                si->lfo_depth = read_u8(r);
                si->lfo_length = read_b_u16(r);
                si->lfo_delay = read_u8(r);
                si->lfo_speed = read_u8(r);
                si->eg_control = read_u8(r);
                si->eg_table = read_u8(r);
                si->eg_length = read_b_u16(r);
                si->eg_delay = read_u8(r);
                si->eg_speed = read_u8(r);
                si->fx_control = read_u8(r);
                si->fx_speed = read_u8(r);
                si->fx_delay = read_u8(r);
                si->mod_control = read_u8(r);
                si->mod_table = read_u8(r);
                si->mod_speed = read_u8(r);
                si->mod_delay = read_u8(r);
                si->volume = read_u8(r);
                si->mod_length = read_b_u16(r);
            }
        } else {
            // Sample instrument
            SmSampleInstrument* si = &m->instruments[i].sample;
            m->instruments[i].is_synth = false;

            // Seek back 1 byte (the first byte is part of the name)
            r->pos--;

            read_string(r, si->name, 24);
            si->length = (uint16_t)(read_b_u16(r) * 2);
            si->loop_start = read_b_u16(r);
            si->loop_length = (uint16_t)(read_b_u16(r) * 2);
            si->volume = read_b_u16(r);
            si->adr = nullptr;

            // Fix for Karate.bp; a loop that starts past the end is no loop
            if (si->loop_start > si->length) {
                si->loop_start = 0;
                si->loop_length = 2;
            } else if ((si->loop_start + si->loop_length) > si->length) {
                si->loop_length = (uint16_t)(si->length - si->loop_start);
            }
        }

        if (reader_eof(r))
            return false;
    }

    if (m->step_num == 0)
        return false;

    // Allocate step structures
    for (int i = 0; i < 4; i++) {
        m->steps[i] = (SmStep*)calloc(m->step_num, sizeof(SmStep));
        if (!m->steps[i])
            return false;
    }

    // Read step data
    m->track_num = 0;

    for (int i = 0; i < m->step_num; i++) {
        for (int j = 0; j < 4; j++) {
            m->steps[j][i].track_number = read_b_u16(r);
            m->steps[j][i].sound_transpose = read_i8(r);
            m->steps[j][i].transpose = read_i8(r);

            if (m->steps[j][i].track_number > m->track_num)
                m->track_num = m->steps[j][i].track_number;
        }

        if (reader_eof(r))
            return false;
    }

    // Allocate tracks
    m->tracks = (SmTrack**)calloc(m->track_num, sizeof(SmTrack*));
    if (!m->tracks)
        return false;

    // Read tracks
    for (int i = 0; i < m->track_num; i++) {
        m->tracks[i] = (SmTrack*)calloc(16, sizeof(SmTrack));
        if (!m->tracks[i])
            return false;

        for (int j = 0; j < 16; j++) {
            m->tracks[i][j].note = read_i8(r);
            uint8_t inst_byte = read_u8(r);
            m->tracks[i][j].optional = inst_byte & 0x0f;
            m->tracks[i][j].instrument = (inst_byte & 0xf0) >> 4;
            m->tracks[i][j].optional_data = read_u8(r);
        }

        if (reader_eof(r))
            return false;
    }

    // Allocate and read wave tables
    int wave_table_size = m->wave_num * 64;
    m->wave_tables = (int8_t*)calloc(wave_table_size, 1);
    if (!m->wave_tables)
        return false;

    read_signed_bytes(r, m->wave_tables, wave_table_size);

    if (reader_eof(r))
        return false;

    // Read samples
    for (int i = 0; i < 15; i++) {
        if (!m->instruments[i].is_synth) {
            SmSampleInstrument* si = &m->instruments[i].sample;
            if (si->length != 0) {
                si->adr = (int8_t*)malloc(si->length);
                if (!si->adr)
                    return false;
                read_signed_bytes(r, si->adr, si->length);

                if (reader_eof(r))
                    return false;
            }
        }
    }

    return true;
}

// InitializeSound

static void initialize_sound(SmModule* m, int start_position) {
    SmGlobalPlayingInfo* pi = &m->playing_info;

    pi->arp_count = 1;
    pi->bp_count = 1;
    pi->bp_delay = 6;
    // The repeat counter starts at 1: the first 1.1 jump is not taken, the second one is
    pi->bp_rep_count = 1;
    pi->rep_fresh = true;
    pi->vib_index = 0;
    pi->bp_step = (uint16_t)start_position;
    pi->bp_pat_count = 0;
    pi->shown_step = (uint16_t)start_position;
    pi->shown_row = 0;
    pi->st = 0;
    pi->tr = 0;
    pi->new_pos = 0;
    pi->pos_flag = false;
    pi->step_changed = false;
    pi->first_repeat = false;

    m->end_reached = false;
    m->loop_count = 0;
    clear_visited(m);
    mark_position_visited(m, 0);

    // Initialize BpCurrent for each channel
    for (int i = 0; i < 4; i++) {
        memset(&m->bp_current[i], 0, sizeof(SmBpCurrent));
        m->bp_current[i].synth_offset = -1;
    }

    // Initialize synth buffers
    for (int i = 0; i < 4; i++)
        memset(pi->synth_buffer[i], 0, 32);

    // Make a copy of wave tables
    int wave_table_size = m->wave_num * 64;
    if (pi->wave_tables)
        free(pi->wave_tables);
    pi->wave_tables = (int8_t*)calloc(SM_WAVE_REACH, 1);
    pi->wave_tables_size = wave_table_size;
    if (pi->wave_tables && m->wave_tables)
        memcpy(pi->wave_tables, m->wave_tables, wave_table_size);

    // Reset mixer channels
    for (int i = 0; i < 4; i++) {
        memset(&m->channels[i], 0, sizeof(SmChannel));
        m->channels[i].sample_number = -1;
    }
}

// Period to frequency helper (for info only, not used in mixer)

static inline int sm_max(int a, int b) {
    return a > b ? a : b;
}
static inline int sm_min(int a, int b) {
    return a < b ? a : b;
}

static uint16_t sm_lookup_period(int note) {
    int idx = sm_max(0, sm_min(note + 36 - 1, 83));
    return sm_periods[idx];
}

// Forward declarations

static void bp_play(SmModule* m);
static void bp_next(SmModule* m);
static void play_it(SmModule* m, int voice);
static void do_optionals(SmModule* m, int voice, uint8_t optional, uint8_t optional_data);
static void do_effects(SmModule* m);
static void do_synths(SmModule* m);
static void do_adsr(SmModule* m, int voice, SmBpCurrent* cur, SmSynthInstrument* si);
static void do_lfo(SmModule* m, int voice, SmBpCurrent* cur, SmSynthInstrument* si);
static void do_eg(SmModule* m, int voice, SmBpCurrent* cur, SmSynthInstrument* si);
static void do_fx(SmModule* m, int voice, SmBpCurrent* cur, SmSynthInstrument* si);
static void do_mod(SmModule* m, SmBpCurrent* cur, SmSynthInstrument* si);
static void averaging(SmModule* m, int voice);
static void transform2(SmModule* m, int voice, int8_t delta);
static void transform3(SmModule* m, int voice, int8_t delta);
static void transform4(SmModule* m, int voice, int8_t delta);

// BpPlay: main player tick

static void bp_play(SmModule* m) {
    // 1.1 runs the synth voices first and the real-time effects after them; 2.x the other way round
    if (m->module_type == SM_TYPE_SOUNDMON_11) {
        do_synths(m);
        do_effects(m);
    } else {
        do_effects(m);
        do_synths(m);
    }

    // At last, update the positions
    m->playing_info.bp_count--;
    if (m->playing_info.bp_count == 0) {
        m->playing_info.bp_count = m->playing_info.bp_delay;
        bp_next(m);

        // The original walks Paula channel 0 first, which is the last voice here; voices sharing a wave table see
        // each other's edits in that order
        for (int i = SM_CHANNELS - 1; i >= 0; i--) {
            // Is the sound restarting?
            if (m->bp_current[i].restart) {
                // Copy temporary synth data back
                if (m->bp_current[i].synth_offset >= 0) {
                    memcpy(m->playing_info.wave_tables + m->bp_current[i].synth_offset, m->playing_info.synth_buffer[i],
                           32);
                    m->bp_current[i].synth_offset = -1;
                }

                // Play the sounds
                play_it(m, i);
            }
        }
    }
}

// BpNext: change pattern position and/or step position

static void bp_next(SmModule* m) {
    SmGlobalPlayingInfo* pi = &m->playing_info;
    pi->shown_step = pi->bp_step;
    pi->shown_row = pi->bp_pat_count;

    // The original handles the columns from 3 down to 0, which matters when a jump changes the step mid-row.
    for (int i = SM_CHANNELS - 1; i >= 0; i--) {
        SmBpCurrent* cur = &m->bp_current[i];

        // A damaged jump can leave the song
        if (pi->bp_step >= m->step_num)
            pi->bp_step = 0;

        // Get the step information
        uint16_t track = m->steps[i][pi->bp_step].track_number;
        pi->st = m->steps[i][pi->bp_step].sound_transpose;
        pi->tr = m->steps[i][pi->bp_step].transpose;

        // Tracks count from 1; a damaged step naming track 0 has no row
        if (track == 0)
            continue;

        // Find the track
        SmTrack* cur_track = &m->tracks[track - 1][pi->bp_pat_count];

        // Is there any note?
        int8_t note = cur_track->note;
        // 1.1 has no options above the transpose, so every note plays; 2.x keeps the voice running on the later ones
        const bool plays_note = m->module_type == SM_TYPE_SOUNDMON_11 || cur_track->optional < SM_OPT_CHANGE_INVERSION;
        if (note != 0) {
            // Stop the effects
            cur->auto_slide = 0;
            cur->auto_arp = 0;
            cur->vibrato = 0;

            // Find the note number and period
            if ((cur_track->optional != SM_OPT_TRANSPOSE) || ((cur_track->optional_data & 0xf0) == 0))
                note += pi->tr;

            cur->note = (uint8_t)note;
            cur->period = sm_lookup_period((int)note);
            cur->restart = false;

            // Should the voice be retrigged?
            if (plays_note) {
                cur->restart = true;
                cur->use_default_volume = true;
            }

            // Find the instrument
            uint8_t inst = cur_track->instrument;
            if (inst == 0)
                inst = cur->instrument;

            if ((inst != 0)
                && ((cur_track->optional != SM_OPT_TRANSPOSE) || ((cur_track->optional_data & 0x0f) == 0))) {
                inst += (uint8_t)pi->st;
                if ((inst < 1) || (inst > 15))
                    inst -= (uint8_t)pi->st;
            }

            if (cur->restart) {
                if (m->module_type == SM_TYPE_SOUNDMON_11) {
                    // 1.1 restarts the sample when the instrument changes, or when the current one has no loop. A
                    // new note on a looping sample just changes its pitch and volume.
                    const bool unlooped = cur->instrument != 0 && !m->instruments[cur->instrument - 1].is_synth
                                          && m->instruments[cur->instrument - 1].sample.loop_length == 2;
                    cur->retrigger = cur->instrument != inst || unlooped;
                } else {
                    // 2.2: a synth voice keeps playing on the same instrument, a sample voice always restarts
                    cur->retrigger = !cur->synth_mode || cur->instrument != inst;
                }
            }

            if (plays_note && (!cur->synth_mode || (cur->instrument != inst)))
                cur->instrument = inst;
        }

        do_optionals(m, i, cur_track->optional, cur_track->optional_data);
    }

    // Change the position
    if (pi->pos_flag) {
        pi->bp_pat_count = 0;
        pi->bp_step = pi->new_pos;
    } else {
        // Next row in the pattern
        pi->bp_pat_count++;
        if (pi->bp_pat_count == 16) {
            // Done with the pattern, now go to the next step
            pi->pos_flag = true;
            pi->bp_pat_count = 0;
            pi->bp_step++;

            if (pi->bp_step == m->step_num) {
                // Done with the module, repeat it
                pi->bp_step = 0;
            }
        }
    }

    if (pi->pos_flag || pi->step_changed) {
        if ((pi->bp_rep_count == 0) || pi->rep_fresh || pi->first_repeat) {
            if (has_position_been_visited(m, pi->bp_step))
                m->end_reached = true;

            mark_position_visited(m, pi->bp_step);
        }

        pi->pos_flag = false;
        pi->step_changed = false;
    }
}

// PlayIt: retrigs a sample or synth sound

static void play_it(SmModule* m, int voice) {
    SmBpCurrent* cur = &m->bp_current[voice];
    SmChannel* ch = &m->channels[voice];
    SmGlobalPlayingInfo* pi = &m->playing_info;

    // Reset the retrig flag
    cur->restart = false;
    channel_set_amiga_period(ch, cur->period);

    // Get the instrument address
    if (cur->instrument != 0) {
        SmInstrument* inst = &m->instruments[cur->instrument - 1];

        // Is the instrument a synth?
        if (inst->is_synth) {
            SmSynthInstrument* si = &inst->synth;

            // Yes it is
            cur->synth_mode = true;
            cur->eg_ptr = 0;
            cur->lfo_ptr = 0;
            cur->adsr_ptr = 0;
            cur->mod_ptr = 0;

            cur->eg_count = (uint8_t)(si->eg_delay + 1);
            cur->lfo_count = (uint8_t)(si->lfo_delay + 1);
            cur->adsr_count = 1; // Start immediate
            cur->mod_count = (uint8_t)(si->mod_delay + 1);
            cur->fx_count = (uint8_t)(si->fx_delay + 1);

            cur->fx_control = si->fx_control;
            cur->eg_control = si->eg_control;
            cur->lfo_control = si->lfo_control;
            cur->adsr_control = si->adsr_control;
            cur->mod_control = si->mod_control;
            cur->old_eg_value = 0;

            // Play the synth sound
            int wave_offset = si->wave_table * 64;
            if (cur->retrigger || !ch->active) {
                channel_play_sample(ch, (int16_t)(cur->instrument - 1), pi->wave_tables, (uint32_t)wave_offset,
                                    si->wave_length);
                channel_set_loop(ch, (uint32_t)wave_offset, si->wave_length);
            }

            // Initialize ADSR
            if (cur->adsr_control != 0) {
                if (cur->use_default_volume) {
                    cur->volume = (uint8_t)si->volume;
                    cur->use_default_volume = false;
                }

                channel_set_adsr_volume(ch, pi->wave_tables[si->adsr_table * 64], cur->volume);
            } else {
                channel_set_amiga_volume(ch, cur->use_default_volume ? (uint8_t)si->volume : cur->volume);
            }

            // Initialize the other effects
            if ((cur->eg_control != 0) || (cur->mod_control != 0) || (cur->fx_control != 0)) {
                cur->synth_offset = wave_offset;
                memcpy(pi->synth_buffer[voice], pi->wave_tables + wave_offset, 32);

                // 1.1 starts the EG part way in: the first bytes of the wave are already inverted
                if (cur->eg_control != 0 && si->eg_start != 0) {
                    cur->old_eg_value = (uint8_t)(si->eg_start >> 3);
                    for (int j = 0; j < cur->old_eg_value; j++)
                        pi->wave_tables[wave_offset + j] = (int8_t)-pi->wave_tables[wave_offset + j];
                }
            }
        } else {
            // No, it is a sample
            SmSampleInstrument* si = &inst->sample;

            cur->synth_mode = false;
            cur->lfo_control = 0;

            if (si->adr == nullptr) {
                channel_mute(ch);
            } else {
                if (cur->retrigger || !ch->active) {
                    // 1.1 writes the loop registers before it switches DMA on, so Paula starts in the loop
                    const bool loops = si->loop_length > 2;
                    const bool starts_in_loop = loops && m->module_type == SM_TYPE_SOUNDMON_11;

                    channel_play_sample(ch, (int16_t)(cur->instrument - 1), si->adr,
                                        starts_in_loop ? si->loop_start : 0, si->length);
                    if (loops)
                        channel_set_loop(ch, si->loop_start, si->loop_length);
                }

                // Set the volume
                channel_set_amiga_volume(ch, cur->use_default_volume ? (uint8_t)si->volume : cur->volume);
            }
        }
    }
}

// DoOptionals: parse track optionals

static void do_optionals(SmModule* m, int voice, uint8_t optional, uint8_t optional_data) {
    SmBpCurrent* cur = &m->bp_current[voice];
    SmChannel* ch = &m->channels[voice];
    SmGlobalPlayingInfo* pi = &m->playing_info;

    if (m->module_type == SM_TYPE_SOUNDMON_11 && optional > SM_OPT_TRANSPOSE)
        return;

    switch (optional) {
        // Arpeggio once
        case SM_OPT_ARPEGGIO_ONCE: {
            cur->arp_value = optional_data;
            break;
        }

        // Set volume
        case SM_OPT_SET_VOLUME: {
            cur->volume = optional_data;
            cur->use_default_volume = false;

            if (m->module_type == SM_TYPE_SOUNDMON_11) {
                channel_set_amiga_volume(ch, optional_data);
            } else {
                if (!cur->synth_mode)
                    channel_set_amiga_volume(ch, optional_data);
            }
            break;
        }

        // Set speed
        case SM_OPT_SET_SPEED: {
            pi->bp_count = optional_data;
            pi->bp_delay = optional_data;
            break;
        }

        // Filter control
        case SM_OPT_FILTER: {
            m->amiga_filter = (optional_data != 0);
            break;
        }

        // Period lift up
        case SM_OPT_PORT_UP: {
            cur->period -= optional_data;
            cur->arp_value = 0;
            break;
        }

        // Period lift down
        case SM_OPT_PORT_DOWN: {
            cur->period += optional_data;
            cur->arp_value = 0;
            break;
        }

        // Set repeat count (SM 1.1) / Vibrato (SM 2.2)
        // Both are value 0x6
        case SM_OPT_VIBRATO: {
            if (m->module_type == SM_TYPE_SOUNDMON_11) {
                if ((pi->bp_rep_count == 0 || pi->rep_fresh) && optional_data != 0)
                    pi->first_repeat = true;
                pi->rep_fresh = false;
                pi->bp_rep_count = optional_data;
            } else {
                cur->vibrato = (int8_t)optional_data;
            }
            break;
        }

        // DBRA repeat count (SM 1.1) / Jump to step (SM 2.2)
        // Both are value 0x7
        case SM_OPT_JUMP: {
            if (m->module_type == SM_TYPE_SOUNDMON_11) {
                // The counter is a byte that wraps, and the jump takes effect at once: the rest of this row, and
                // the rows after it, come from the new step.
                pi->first_repeat = false;
                pi->rep_fresh = false;
                pi->bp_rep_count--;
                if (pi->bp_rep_count != 0) {
                    // Set the "end" mark, if we have to repeat the block a lot
                    if ((pi->bp_rep_count == 254) || ((pi->bp_rep_count >= 15) && (optional_data < pi->bp_step)))
                        m->end_reached = true;

                    pi->bp_step = optional_data;
                    pi->step_changed = true;
                }
            } else {
                // SoundMon 2.2
                pi->new_pos = optional_data;
                pi->pos_flag = true;
            }
            break;
        }

        // Set auto slide
        case SM_OPT_SET_AUTO_SLIDE: {
            cur->auto_slide = (int8_t)optional_data;
            break;
        }

        // Set continuous arpeggio
        case SM_OPT_SET_ARPEGGIO: {
            cur->auto_arp = optional_data;

            if (m->module_type == SM_TYPE_SOUNDMON_22) {
                cur->adsr_ptr = 0;

                if (cur->adsr_control == 0)
                    cur->adsr_control = 1;
            }
            break;
        }

        // Transpose (0xa) -- handled via note modification in BpNext, no separate action
        case SM_OPT_TRANSPOSE: {
            break;
        }

        // Change fx type
        case SM_OPT_CHANGE_FX: {
            cur->fx_control = optional_data;
            break;
        }

        // Changes from inversion to backward inversion (or vice versa)
        case SM_OPT_CHANGE_INVERSION: {
            cur->auto_arp = optional_data;
            cur->fx_control = (uint8_t)(cur->fx_control ^ 1);
            cur->adsr_ptr = 0;

            if (cur->adsr_control == 0)
                cur->adsr_control = 1;

            break;
        }

        // Reset ADSR on synth sound
        case SM_OPT_RESET_ADSR: {
            cur->auto_arp = optional_data;
            cur->adsr_ptr = 0;

            if (cur->adsr_control == 0)
                cur->adsr_control = 1;

            break;
        }

        // Change note (does not reset ADSR)
        case SM_OPT_CHANGE_NOTE: {
            cur->auto_arp = optional_data;
            break;
        }
    }
}

// DoEffects: real-time effects

static void do_effects(SmModule* m) {
    SmGlobalPlayingInfo* pi = &m->playing_info;

    // Adjust the arpeggio counter
    // 1.1 steps the arpeggio through three ticks (2, 1, 0), 2.x through four (3, 2, 1, 0)
    const uint8_t reload = m->module_type == SM_TYPE_SOUNDMON_11 ? 2 : 3;
    pi->arp_count = pi->arp_count == 0 ? reload : (uint8_t)(pi->arp_count - 1);

    // Adjust the vibrato table index
    pi->vib_index = (uint8_t)((pi->vib_index + 1) & 7);

    for (int i = 0; i < 4; i++) {
        SmBpCurrent* cur = &m->bp_current[i];
        SmChannel* ch = &m->channels[i];

        // Auto slide
        cur->period = (uint16_t)(cur->period + cur->auto_slide);

        // Vibrato
        if (cur->vibrato != 0)
            channel_set_amiga_period(ch, (uint16_t)(cur->period + sm_vibrato_table[pi->vib_index] / cur->vibrato));
        else if (m->module_type != SM_TYPE_SOUNDMON_11 || cur->lfo_control == 0) // 1.1: the LFO owns the period
            channel_set_amiga_period(ch, cur->period);

        // Arpeggio
        if ((cur->arp_value != 0) || (cur->auto_arp != 0)) {
            int note = (int8_t)cur->note;

            if (pi->arp_count == 0)
                note += ((cur->arp_value & 0xf0) >> 4) + ((cur->auto_arp & 0xf0) >> 4);
            else {
                if (pi->arp_count == 1)
                    note += (cur->arp_value & 0x0f) + (cur->auto_arp & 0x0f);
            }

            // Find the period
            cur->restart = false;
            cur->period = sm_lookup_period(note);
            channel_set_amiga_period(ch, cur->period);
        }
    }
}

// DoSynths: synth effect processing

static void do_synths(SmModule* m) {
    // Same order as the restarts: Paula channel 0 (the last voice) first
    for (int i = SM_CHANNELS - 1; i >= 0; i--) {
        SmBpCurrent* cur = &m->bp_current[i];

        // Does the current voice play a synth sample?
        if (cur->synth_mode) {
            // Get the instrument
            if (cur->instrument != 0 && m->instruments[cur->instrument - 1].is_synth) {
                SmSynthInstrument* si = &m->instruments[cur->instrument - 1].synth;

                do_adsr(m, i, cur, si);
                do_lfo(m, i, cur, si);

                // Do we have a synth buffer?
                if (cur->synth_offset >= 0) {
                    do_eg(m, i, cur, si);
                    do_fx(m, i, cur, si);
                    do_mod(m, cur, si);
                }
            }
        }
    }
}

// DoAdsr: ADSR envelope

static void do_adsr(SmModule* m, int voice, SmBpCurrent* cur, SmSynthInstrument* si) {
    if (cur->adsr_control != 0) {
        cur->adsr_count--;
        if (cur->adsr_count == 0) {
            // Reset counter
            cur->adsr_count = si->adsr_speed;

            channel_set_adsr_volume(&m->channels[voice],
                                    m->playing_info.wave_tables[si->adsr_table * 64 + cur->adsr_ptr], cur->volume);

            // Update the ADSR pointer
            cur->adsr_ptr++;
            if (cur->adsr_ptr == si->adsr_length) {
                cur->adsr_ptr = 0;

                // Only do the ADSR once?
                if (cur->adsr_control == 1)
                    cur->adsr_control = 0;
            }
        }
    }
}

// DoLfo: LFO effect

static void do_lfo(SmModule* m, int voice, SmBpCurrent* cur, SmSynthInstrument* si) {
    if (cur->lfo_control != 0) {
        cur->lfo_count--;
        if (cur->lfo_count == 0) {
            // Reset counter
            cur->lfo_count = si->lfo_speed;

            // Get the wave table value
            int table_value = m->playing_info.wave_tables[si->lfo_table * 64 + cur->lfo_ptr];

            // Adjust the value by the LFO depth
            if (si->lfo_depth != 0)
                table_value /= si->lfo_depth;

            // Calculate and set the new period
            channel_set_amiga_period(&m->channels[voice], (uint16_t)(cur->period + table_value));

            // Update the LFO pointer
            cur->lfo_ptr++;
            if (cur->lfo_ptr == si->lfo_length) {
                cur->lfo_ptr = 0;

                // Only do the LFO once?
                if (cur->lfo_control == 1)
                    cur->lfo_control = 0;
            }
        }
    }
}

// DoEg: envelope generator

static void do_eg(SmModule* m, int voice, SmBpCurrent* cur, SmSynthInstrument* si) {
    if (cur->eg_control != 0) {
        cur->eg_count--;
        if (cur->eg_count == 0) {
            // Reset counter
            cur->eg_count = si->eg_speed;

            // Calculate new EG value
            int table_value = cur->old_eg_value;
            cur->old_eg_value = (uint8_t)((m->playing_info.wave_tables[si->eg_table * 64 + cur->eg_ptr] + 128) / 8);

            // Do we need to do the EG thing at all?
            if (cur->old_eg_value != (uint8_t)table_value) {
                // Find the source and destination offset
                int8_t* source = m->playing_info.synth_buffer[voice];
                int source_offset = table_value;
                int dest_offset = cur->synth_offset + table_value;

                if (cur->old_eg_value < (uint8_t)table_value) {
                    table_value = table_value - cur->old_eg_value;

                    for (int j = 0; j < table_value; j++)
                        m->playing_info.wave_tables[--dest_offset] = source[--source_offset];
                } else {
                    table_value = cur->old_eg_value - table_value;

                    for (int j = 0; j < table_value; j++)
                        m->playing_info.wave_tables[dest_offset++] = (int8_t)-source[source_offset++];
                }
            }

            // Update the EG pointer
            cur->eg_ptr++;
            if (cur->eg_ptr == si->eg_length) {
                cur->eg_ptr = 0;

                // Only do the EG once?
                if (cur->eg_control == 1)
                    cur->eg_control = 0;
            }
        }
    }
}

// DoFx: synth effects

static void do_fx(SmModule* m, int voice, SmBpCurrent* cur, SmSynthInstrument* si) {
    switch (cur->fx_control) {
        case 1: {
            cur->fx_count--;
            if (cur->fx_count == 0) {
                // Reset counter
                cur->fx_count = si->fx_speed;
                averaging(m, voice);
            }
            break;
        }

        case 2: {
            transform2(m, voice, (int8_t)si->fx_speed);
            break;
        }

        case 3:
        case 5: {
            transform3(m, voice, (int8_t)si->fx_speed);
            break;
        }

        case 4: {
            transform4(m, voice, (int8_t)si->fx_speed);
            break;
        }

        case 6: {
            cur->fx_count--;
            if (cur->fx_count == 0) {
                cur->fx_control = 0;
                cur->fx_count = 1;

                memcpy(m->playing_info.wave_tables + cur->synth_offset,
                       m->playing_info.wave_tables + cur->synth_offset + 64, 32);
            }
            break;
        }
    }
}

// DoMod: modulation

static void do_mod(SmModule* m, SmBpCurrent* cur, SmSynthInstrument* si) {
    if (cur->mod_control != 0) {
        cur->mod_count--;
        if (cur->mod_count == 0) {
            // Reset counter
            cur->mod_count = si->mod_speed;

            // Get the table value and store it in the synth sample
            m->playing_info.wave_tables[cur->synth_offset + 32]
                = m->playing_info.wave_tables[si->mod_table * 64 + cur->mod_ptr];

            // Update the MOD pointer
            cur->mod_ptr++;
            if (cur->mod_ptr == si->mod_length) {
                cur->mod_ptr = 0;

                // Only do the MOD once?
                if (cur->mod_control == 1)
                    cur->mod_control = 0;
            }
        }
    }
}

// Averaging: averages the synth sample

static void averaging(SmModule* m, int voice) {
    int8_t* wave = m->playing_info.wave_tables + m->bp_current[voice].synth_offset;

    // Each sample becomes the mean of the new previous sample and the old next one (the last one reads the byte at
    // +32). The sample before the buffer is the byte before the wave table; before table 0 that is the end of the
    // track data.
    int8_t previous;
    if (m->bp_current[voice].synth_offset > 0)
        previous = wave[-1];
    else
        previous = (int8_t)m->tracks[m->track_num - 1][15].optional_data;

    for (int i = 0; i < 32; i++) {
        previous = (int8_t)((previous + wave[i + 1]) >> 1);
        wave[i] = previous;
    }
}

// Transform2: transform synth sample using method 2

static void transform2(SmModule* m, int voice, int8_t delta) {
    int8_t* source = m->playing_info.synth_buffer[voice];
    int source_offset = 31;
    int dest_offset = m->bp_current[voice].synth_offset;

    for (int i = 0; i < 32; i++) {
        int8_t source_val = source[source_offset];
        int8_t dest_val = m->playing_info.wave_tables[dest_offset];

        if (source_val < dest_val)
            m->playing_info.wave_tables[dest_offset] -= delta;
        else {
            if (source_val > dest_val)
                m->playing_info.wave_tables[dest_offset] += delta;
        }

        source_offset--;
        dest_offset++;
    }
}

// Transform3: transform synth sample using method 3

static void transform3(SmModule* m, int voice, int8_t delta) {
    int8_t* source = m->playing_info.synth_buffer[voice];
    int source_offset = 0;
    int dest_offset = m->bp_current[voice].synth_offset;

    for (int i = 0; i < 32; i++) {
        int8_t source_val = source[source_offset];
        int8_t dest_val = m->playing_info.wave_tables[dest_offset];

        if (source_val < dest_val)
            m->playing_info.wave_tables[dest_offset] -= delta;
        else {
            if (source_val > dest_val)
                m->playing_info.wave_tables[dest_offset] += delta;
        }

        source_offset++;
        dest_offset++;
    }
}

// Transform4: transform synth sample using method 4

static void transform4(SmModule* m, int voice, int8_t delta) {
    int source_offset = m->bp_current[voice].synth_offset + 64;
    int dest_offset = m->bp_current[voice].synth_offset;

    for (int i = 0; i < 32; i++) {
        int8_t source_val
            = (source_offset >= m->playing_info.wave_tables_size) ? 0 : m->playing_info.wave_tables[source_offset];
        int8_t dest_val = m->playing_info.wave_tables[dest_offset];

        if (source_val < dest_val)
            m->playing_info.wave_tables[dest_offset] -= delta;
        else {
            if (source_val > dest_val)
                m->playing_info.wave_tables[dest_offset] += delta;
        }

        source_offset++;
        dest_offset++;
    }
}

// Amiga 4-channel mixer

// SoundMon's voice n plays on Paula channel 3 - n; Paula channels 0 and 3 are left, 1 and 2 right.
static uint32_t paula_channel(int voice) {
    return (uint32_t)(SM_CHANNELS - 1 - voice);
}

// Hand the registers the tick wrote to the voice.
static void sync_voice(SmModule* m, int voice) {
    SmChannel* c = &m->channels[voice];
    const uint32_t index = paula_channel(voice);

    if (!c->active || c->sample_data == nullptr) {
        // A one-shot that ran off its end is already silent; Paula keeps its DMA on
        if (nuade_voice_is_playing(m->voices, index))
            nuade_voice_stop(m->voices, index);
    } else if (c->play_pending) {
        const NuadeSample sample = { .data = c->sample_data,
                                     .format = NuadeSampleFormat_S8,
                                     .length = c->sample_length,
                                     .loop_start = c->loop_start,
                                     .loop_length = c->loop_length };
        nuade_voice_play(m->voices, index, &sample, c->sample_offset);
    }
    c->play_pending = false;

    const bool silent = c->muted || c->period == 0;
    nuade_voice_set_pitch(m->voices, index, silent ? 0.0 : nuade_paula_period_hz(c->period));
    nuade_voice_set_volume(m->voices, index, silent ? 0.0f : (float)c->volume / 256.0f);
    nuade_voice_set_pan(m->voices, index, (voice == 0 || voice == 3) ? -1.0f : 1.0f);
}

bool sm_tick(SmModule* module) {
    for (int i = 0; i < SM_CHANNELS; i++) {
        module->channels[i].active
            = module->channels[i].active && nuade_voice_is_playing(module->voices, paula_channel(i));
    }

    // One player tick; a position played twice is the song's loop.
    const uint32_t loops = module->loop_count;
    bp_play(module);
    if (module->end_reached) {
        module->end_reached = false;
        module->loop_count++;
        mark_position_visited(module, module->playing_info.bp_step);
    }

    for (int i = 0; i < SM_CHANNELS; i++) {
        sync_voice(module, i);
    }
    return module->loop_count != loops;
}

// Public API

// The module mark at offset 26 tells the version.
static SmModuleType sniff_type(const uint8_t* data, size_t size) {
    if (!data || size < 512)
        return SM_TYPE_UNKNOWN;
    if (data[26] == 'V' && data[27] == '.' && data[28] == '2')
        return SM_TYPE_SOUNDMON_11;
    if (data[26] == 'V' && data[27] == '.' && data[28] == '3')
        return SM_TYPE_SOUNDMON_22;
    return SM_TYPE_UNKNOWN;
}

bool sm_probe(const uint8_t* data, size_t size) {
    return sniff_type(data, size) != SM_TYPE_UNKNOWN;
}

SmModule* sm_create(const uint8_t* data, size_t size) {
    const SmModuleType type = sniff_type(data, size);
    if (type == SM_TYPE_UNKNOWN)
        return nullptr;

    SmModule* m = (SmModule*)calloc(1, sizeof(SmModule));
    if (!m)
        return nullptr;

    // Keep original data for export
    m->original_data = (uint8_t*)malloc(size);
    if (m->original_data) {
        memcpy(m->original_data, data, size);
        m->original_size = size;
    }

    SmReader reader;
    reader_init(&reader, data, size);

    if (!load_module(m, &reader, type)) {
        sm_destroy(m);
        return nullptr;
    }

    initialize_sound(m, 0);

    return m;
}

void sm_destroy(SmModule* module) {
    if (!module)
        return;

    for (int i = 0; i < 4; i++) {
        if (module->steps[i])
            free(module->steps[i]);
    }

    if (module->tracks) {
        for (int i = 0; i < module->track_num; i++) {
            if (module->tracks[i])
                free(module->tracks[i]);
        }
        free(module->tracks);
    }

    if (module->wave_tables)
        free(module->wave_tables);

    for (int i = 0; i < 15; i++) {
        if (!module->instruments[i].is_synth) {
            if (module->instruments[i].sample.adr)
                free(module->instruments[i].sample.adr);
        }
    }

    if (module->playing_info.wave_tables)
        free(module->playing_info.wave_tables);

    if (module->original_data)
        free(module->original_data);
    free(module);
}

int sm_subsong_count(const SmModule* module) {
    if (!module)
        return 0;
    return 1;
}

bool sm_start(SmModule* module, int subsong, NuadeVoices* voices) {
    if (!module || !voices || subsong != 0)
        return false;

    module->voices = voices;
    initialize_sound(module, 0);
    return true;
}

int sm_channel_count(const SmModule* module) {
    (void)module;
    return 4;
}

void sm_set_channel_mask(SmModule* module, uint32_t mask) {
    if (!module)
        return;

    for (int i = 0; i < 4; i++)
        module->channels[i].muted = ((mask >> i) & 1) == 0;
}

const char* sm_title(const SmModule* module) {
    return module ? module->module_name : "";
}

const char* sm_format_name(const SmModule* module) {
    return module && module->module_type == SM_TYPE_SOUNDMON_22 ? "BP SoundMon 2.2" : "BP SoundMon 1.1";
}

uint32_t sm_loop_count(const SmModule* module) {
    return module ? module->loop_count : 0;
}

// Display

int sm_step_count(const SmModule* module) {
    return module ? (int)module->step_num : 0;
}

SmPosition sm_position(const SmModule* module) {
    return (SmPosition) { .step = module->playing_info.shown_step,
                          .row = module->playing_info.shown_row,
                          .speed = module->playing_info.bp_delay };
}

SmVoiceState sm_voice_state(const SmModule* module, int voice) {
    const SmBpCurrent* cur = &module->bp_current[voice];
    return (SmVoiceState) { .note = cur->note, .instrument = cur->instrument };
}

SmCell sm_pattern_cell(const SmModule* module, int step, int channel, int row) {
    const SmCell empty = { 0 };
    if (!module || step < 0 || step >= (int)module->step_num || channel < 0 || channel >= SM_CHANNELS || row < 0
        || row >= SM_ROWS) {
        return empty;
    }
    const int track = module->steps[channel][step].track_number;
    if (track < 1 || track > (int)module->track_num || !module->tracks[track - 1]) {
        return empty;
    }
    const SmTrack* t = &module->tracks[track - 1][row];
    return (SmCell) {
        .note = (uint8_t)t->note, .instrument = t->instrument, .effect = t->optional, .effect_arg = t->optional_data
    };
}

// Edit API

int sm_get_instrument_count(const SmModule* module) {
    return module ? 15 : 0;
}

int sm_get_num_tracks(const SmModule* module) {
    return module ? (int)module->track_num : 0;
}

void sm_get_cell(const SmModule* module, int track_idx, int row, uint8_t* note, uint8_t* instrument, uint8_t* effect,
                 uint8_t* effect_arg) {
    if (!module || track_idx < 0 || track_idx >= (int)module->track_num || row < 0 || row >= 16 || !module->tracks
        || !module->tracks[track_idx]) {
        if (note)
            *note = 0;
        if (instrument)
            *instrument = 0;
        if (effect)
            *effect = 0;
        if (effect_arg)
            *effect_arg = 0;
        return;
    }
    const SmTrack* t = &module->tracks[track_idx][row];
    if (note)
        *note = (uint8_t)t->note;
    if (instrument)
        *instrument = t->instrument;
    if (effect)
        *effect = t->optional;
    if (effect_arg)
        *effect_arg = t->optional_data;
}

void sm_set_cell(SmModule* module, int track_idx, int row, uint8_t note, uint8_t instrument, uint8_t effect,
                 uint8_t effect_arg) {
    if (!module || track_idx < 0 || track_idx >= (int)module->track_num || row < 0 || row >= 16 || !module->tracks
        || !module->tracks[track_idx])
        return;
    SmTrack* t = &module->tracks[track_idx][row];
    t->note = (int8_t)note;
    t->instrument = instrument;
    t->optional = effect;
    t->optional_data = effect_arg;
}

const char* sm_get_instrument_name(const SmModule* module, int inst) {
    if (!module || inst < 0 || inst >= 15)
        return "";
    if (!module->instruments[inst].is_synth)
        return module->instruments[inst].sample.name;
    return "";
}

float sm_get_instrument_param(const SmModule* module, int inst, const char* param) {
    if (!module || inst < 0 || inst >= 15 || !param)
        return -1.0f;
    const SmInstrument* in = &module->instruments[inst];

    if (strcmp(param, "isSynth") == 0)
        return (float)in->is_synth;

    // Sample instrument params
    if (strcmp(param, "length") == 0)
        return (float)in->sample.length;
    if (strcmp(param, "loopStart") == 0)
        return (float)in->sample.loop_start;
    if (strcmp(param, "loopLength") == 0)
        return (float)in->sample.loop_length;
    if (strcmp(param, "volume") == 0)
        return in->is_synth ? (float)in->synth.volume : (float)in->sample.volume;

    // Synth instrument params
    if (strcmp(param, "waveTable") == 0)
        return (float)in->synth.wave_table;
    if (strcmp(param, "waveLength") == 0)
        return (float)in->synth.wave_length;
    if (strcmp(param, "adsrControl") == 0)
        return (float)in->synth.adsr_control;
    if (strcmp(param, "adsrTable") == 0)
        return (float)in->synth.adsr_table;
    if (strcmp(param, "adsrLength") == 0)
        return (float)in->synth.adsr_length;
    if (strcmp(param, "adsrSpeed") == 0)
        return (float)in->synth.adsr_speed;
    if (strcmp(param, "lfoControl") == 0)
        return (float)in->synth.lfo_control;
    if (strcmp(param, "lfoTable") == 0)
        return (float)in->synth.lfo_table;
    if (strcmp(param, "lfoDepth") == 0)
        return (float)in->synth.lfo_depth;
    if (strcmp(param, "lfoLength") == 0)
        return (float)in->synth.lfo_length;
    if (strcmp(param, "lfoDelay") == 0)
        return (float)in->synth.lfo_delay;
    if (strcmp(param, "lfoSpeed") == 0)
        return (float)in->synth.lfo_speed;
    if (strcmp(param, "egControl") == 0)
        return (float)in->synth.eg_control;
    if (strcmp(param, "egTable") == 0)
        return (float)in->synth.eg_table;
    if (strcmp(param, "egLength") == 0)
        return (float)in->synth.eg_length;
    if (strcmp(param, "egDelay") == 0)
        return (float)in->synth.eg_delay;
    if (strcmp(param, "egSpeed") == 0)
        return (float)in->synth.eg_speed;
    if (strcmp(param, "fxControl") == 0)
        return (float)in->synth.fx_control;
    if (strcmp(param, "fxSpeed") == 0)
        return (float)in->synth.fx_speed;
    if (strcmp(param, "fxDelay") == 0)
        return (float)in->synth.fx_delay;
    if (strcmp(param, "modControl") == 0)
        return (float)in->synth.mod_control;
    if (strcmp(param, "modTable") == 0)
        return (float)in->synth.mod_table;
    if (strcmp(param, "modSpeed") == 0)
        return (float)in->synth.mod_speed;
    if (strcmp(param, "modDelay") == 0)
        return (float)in->synth.mod_delay;
    if (strcmp(param, "modLength") == 0)
        return (float)in->synth.mod_length;

    return -1.0f;
}

void sm_set_instrument_param(SmModule* module, int inst, const char* param, float value) {
    if (!module || inst < 0 || inst >= 15 || !param)
        return;
    SmInstrument* in = &module->instruments[inst];
    uint16_t v = (uint16_t)value;
    uint8_t b = (uint8_t)value;

    // Sample instrument params
    if (strcmp(param, "length") == 0) {
        in->sample.length = v;
        return;
    }
    if (strcmp(param, "loopStart") == 0) {
        in->sample.loop_start = v;
        return;
    }
    if (strcmp(param, "loopLength") == 0) {
        in->sample.loop_length = v;
        return;
    }
    if (strcmp(param, "volume") == 0) {
        if (in->is_synth)
            in->synth.volume = v;
        else
            in->sample.volume = v;
        return;
    }

    // Synth instrument params
    if (strcmp(param, "waveTable") == 0) {
        in->synth.wave_table = b;
        return;
    }
    if (strcmp(param, "waveLength") == 0) {
        in->synth.wave_length = v;
        return;
    }
    if (strcmp(param, "adsrControl") == 0) {
        in->synth.adsr_control = b;
        return;
    }
    if (strcmp(param, "adsrTable") == 0) {
        in->synth.adsr_table = b;
        return;
    }
    if (strcmp(param, "adsrLength") == 0) {
        in->synth.adsr_length = v;
        return;
    }
    if (strcmp(param, "adsrSpeed") == 0) {
        in->synth.adsr_speed = b;
        return;
    }
    if (strcmp(param, "lfoControl") == 0) {
        in->synth.lfo_control = b;
        return;
    }
    if (strcmp(param, "lfoTable") == 0) {
        in->synth.lfo_table = b;
        return;
    }
    if (strcmp(param, "lfoDepth") == 0) {
        in->synth.lfo_depth = b;
        return;
    }
    if (strcmp(param, "lfoLength") == 0) {
        in->synth.lfo_length = v;
        return;
    }
    if (strcmp(param, "lfoDelay") == 0) {
        in->synth.lfo_delay = b;
        return;
    }
    if (strcmp(param, "lfoSpeed") == 0) {
        in->synth.lfo_speed = b;
        return;
    }
    if (strcmp(param, "egControl") == 0) {
        in->synth.eg_control = b;
        return;
    }
    if (strcmp(param, "egTable") == 0) {
        in->synth.eg_table = b;
        return;
    }
    if (strcmp(param, "egLength") == 0) {
        in->synth.eg_length = v;
        return;
    }
    if (strcmp(param, "egDelay") == 0) {
        in->synth.eg_delay = b;
        return;
    }
    if (strcmp(param, "egSpeed") == 0) {
        in->synth.eg_speed = b;
        return;
    }
    if (strcmp(param, "fxControl") == 0) {
        in->synth.fx_control = b;
        return;
    }
    if (strcmp(param, "fxSpeed") == 0) {
        in->synth.fx_speed = b;
        return;
    }
    if (strcmp(param, "fxDelay") == 0) {
        in->synth.fx_delay = b;
        return;
    }
    if (strcmp(param, "modControl") == 0) {
        in->synth.mod_control = b;
        return;
    }
    if (strcmp(param, "modTable") == 0) {
        in->synth.mod_table = b;
        return;
    }
    if (strcmp(param, "modSpeed") == 0) {
        in->synth.mod_speed = b;
        return;
    }
    if (strcmp(param, "modDelay") == 0) {
        in->synth.mod_delay = b;
        return;
    }
    if (strcmp(param, "modLength") == 0) {
        in->synth.mod_length = v;
        return;
    }
}

size_t sm_export(const SmModule* module, uint8_t* out, size_t max_size) {
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
