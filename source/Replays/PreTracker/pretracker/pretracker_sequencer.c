#include "pretracker_internal.h"

#include <string.h>

typedef enum {
    INST_RESET_ALL,
    INST_RESET_PRESERVE_PORT_PITCH,
} InstrumentResetPolicy;

typedef struct {
    u8 pitch_ctrl;
    u8 effect_cmd;
    u8 effect_data;
    u8 instrument;
    u8 pitch;
    bool has_arpeggio;
} PatternRow;

typedef struct {
    u8 effect_cmd;
    u8 effect_data;
    u8 pitch;
    u16 inst_num4;
    i16 pitch_shift;
    u8 arp_flag;
    u8 alternate_instrument;
    bool resolve_portamento;
} PatternResolution;

typedef struct {
    u8 pitch;
    u8 command;
    u8 command_data;
    bool stitched;
    bool pitch_pinned;
} InstrumentStep;

typedef enum {
    INST_STEP_ADVANCE,
    INST_STEP_FETCH_NEXT,
    INST_STEP_EXIT,
} InstrumentStepControl;

typedef enum {
    WAVE_ACTIVATE_SYNC,
    WAVE_ACTIVATE_NOSYNC,
    WAVE_ACTIVATE_FALLBACK,
} WaveActivationPolicy;

static const u8 s_octave_note_offset_table[] = {
    1 * NOTES_IN_OCTAVE * 4, 1 * NOTES_IN_OCTAVE * 4, 1 * NOTES_IN_OCTAVE * 4, 2 * NOTES_IN_OCTAVE * 4,
    2 * NOTES_IN_OCTAVE * 4, 2 * NOTES_IN_OCTAVE * 4, 3 * NOTES_IN_OCTAVE * 4, 3 * NOTES_IN_OCTAVE * 4,
    3 * NOTES_IN_OCTAVE * 4, 3 * NOTES_IN_OCTAVE * 4, 3 * NOTES_IN_OCTAVE * 4, 3 * NOTES_IN_OCTAVE * 4,
    3 * NOTES_IN_OCTAVE * 4, 3 * NOTES_IN_OCTAVE * 4, 3 * NOTES_IN_OCTAVE * 4, 3 * NOTES_IN_OCTAVE * 4,
    3 * NOTES_IN_OCTAVE * 4, 3 * NOTES_IN_OCTAVE * 4, 3 * NOTES_IN_OCTAVE * 4,
};

static const u8 s_octave_select_table[] = {
    1, 1, 1, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
};

_Static_assert(sizeof(s_octave_note_offset_table) >= sizeof(s_octave_select_table),
               "octave_note_offset_table must cover same range as octave_select_table");

// Scale an Amiga-rate sample offset/length to HQ rate

static inline u16 scale_offset(u16 amiga_val) {
    return (u16)((u32)amiga_val * HQ_MAX_PERIOD / AMIGA_MAX_PERIOD);
}

void pretracker_init_channel(PerChannelData* pcd, const SongState* song, u8 channel_num) {
    memset(pcd->arp_notes, 0, sizeof(pcd->arp_notes));
    memset(pcd->track_delay_buffer, 0, sizeof(pcd->track_delay_buffer));
    pcd->pat_vol = MAX_VOLUME;
    pcd->track_delay_offset = 0xFF;
    pcd->waveinfo_ptr = song->num_waves > 0 ? &song->waveinfo_ptr[0] : NULL;
    pcd->adsr_phase = ADSR_PHASE_RELEASE;
    pcd->out.sam_ptr_offset = 0;
    pcd->out.length = 2;
    pcd->out.loop_offset = 0xFFFF;
    pcd->out.period = 0x7B;
    pcd->out.volume = MAX_VOLUME; // rePlayer fix clubtown_hop by Curt Cool
    pcd->out.trigger = 0;
    pcd->inst_info_ptr = NULL;
    pcd->new_inst_num = 0;
    pcd->inst_num4 = 0;
    pcd->inst_wave_num = 0xFFFF;
    pcd->note_delay = 0;
    pcd->note_off_delay = 0;
    pcd->pat_portamento_dest = 0;
    pcd->pat_pitch_slide = 0;
    pcd->pat_vol_ramp_speed = 0;
    pcd->pat_2nd_inst_num4 = 0;
    pcd->inst_pitch = 0x10;
    pcd->inst_vol = MAX_VOLUME;
    pcd->inst_curr_port_pitch = 0;
    pcd->inst_sel_arp_note = 0;
    pcd->inst_note_pitch = 0;
    pcd->inst_pitch_slide = 0;
    pcd->inst_line_ticks = 0;
    pcd->inst_pitch_pinned = 0;
    pcd->inst_vol_slide = 0;
    pcd->inst_step_pos = 0;
    pcd->inst_speed_stop = 1;
    pcd->track_delay_steps = 0;
    pcd->adsr_volume = 0;
    pcd->vibrato_pos = 0;
    pcd->vibrato_delay = 0;
    pcd->vibrato_depth = 0;
    pcd->vibrato_speed = 0;
    pcd->adsr_release = 0;
    pcd->adsr_phase_speed = 0;
    pcd->adsr_pos = 0;
    pcd->adsr_vol64 = 0;
    pcd->loaded_inst_vol = MAX_VOLUME;
    pcd->last_trigger_pos = 0;
    pcd->wave_offset = 0;
    pcd->inst_loop_offset = 0;
    pcd->inst_subloop_wait = 0;
    pcd->inst_ping_pong_dir = 0xFF;
    pcd->pat_portamento_speed = 0;
    pcd->pat_adsr_rel_delay = 0;
    pcd->inst_pattern_steps = 0;
    pcd->track_delay_vol16 = 0;
    pcd->track_init_delay = 0;
    pcd->channel_num = channel_num;
    pcd->channel_mask = (u8)(1 << channel_num);
}

void pretracker_player_init(PlayerState* player, f32* sample_buffer, SongState* song) {
    memset(player, 0, sizeof(PlayerState));
    player->my_song = song;
    player->sample_buffer_ptr = sample_buffer;

    sample_buffer[0] = 0.0f;
    sample_buffer[1] = 0.0f;
    f32* sample = sample_buffer + 2;
    for (int wave = 0; wave < song->num_waves; wave++) {
        player->wave_sample_table[wave] = sample;
        sample += song->wavetotal_table[wave];
    }

    u16* period = player->period_table;
    for (int note = 0; note < 3 * NOTES_IN_OCTAVE; note++) {
        i32 current = (i32)s_period_table[note] << 16;
        i32 increment = ((i32)s_period_table[note + 1] - (i32)s_period_table[note]) << 16;
        increment >>= 4;
        for (int fine = 0; fine < 16; fine++) {
            *period++ = (u16)((u32)current >> 16);
            current += increment;
        }
    }

    pretracker_rebuild_pattern_table(song);
    player->pat_curr_row = 0;
    player->next_pat_row = 0xFF;
    player->next_pat_pos = 0xFF;
    player->pat_speed_even = 0x06;
    player->pat_speed_odd = 0x06;
    player->pat_line_ticks = 0x06;
    player->pat_stopped = 1;
    player->songend_detected = 0;
    for (u8 channel = 0; channel < NUM_CHANNELS; channel++)
        pretracker_init_channel(&player->channeldata[channel], song, channel);
}

void pretracker_player_start(PlayerState* player, SongState* song) {
    song->curr_pat_pos = 0;
    player->pat_curr_row = 0;
    player->next_pat_row = 0xFF;
    player->next_pat_pos = 0xFF;
    player->pat_line_ticks = player->pat_speed_even;
    player->pat_stopped = 1;
    player->songend_detected = 0;
    player->trigger_mask = 0;

    for (u8 channel = 0; channel < NUM_CHANNELS; ++channel)
        pretracker_init_channel(&player->channeldata[channel], song, channel);
}

// Process pattern effect commands (asm:2790-2925)

static void process_pattern_effects(PerChannelData* pcd, u8 effect_cmd, u8 effect_data, PlayerState* player) {
    switch (effect_cmd) {
        case PAT_CMD_SLIDE_UP:
            pcd->pat_pitch_slide = (i16)effect_data;
            break;
        case PAT_CMD_SLIDE_DOWN:
            pcd->pat_pitch_slide = -(i16)effect_data;
            break;
        case PAT_CMD_SET_VIBRATO: { // asm:2836-2849
            pcd->vibrato_pos = 0;
            pcd->vibrato_delay = 1;
            u8 depth_idx = effect_data & 0x0F;
            u8 speed_idx = effect_data >> 4;
            pcd->vibrato_depth = s_vib_depth_table[depth_idx];
            i16 spd = (i16)s_vib_speed_table[speed_idx];
            spd = (i16)((spd * (i16)pcd->vibrato_depth) >> 4);
            pcd->vibrato_speed = (u16)spd;
            break;
        }
        case PAT_CMD_TRACK_DELAY: { // asm:2852-2895
            if (pcd->channel_num >= NUM_CHANNELS - 1) {
                break;
            }
            // Clear next channel's delay buffer volumes
            PerChannelData* next_pcd = &player->channeldata[pcd->channel_num + 1];
            for (int i = 0; i < MAX_TRACK_DELAY; i++) {
                next_pcd->track_delay_buffer[i].volume = 0;
            }
            if (effect_data == 0) {
                pcd->track_delay_steps = 0xFF; // clear signal
            } else {
                u8 steps = (effect_data & 0x0F) * 2;
                pcd->track_delay_steps = steps;
                pcd->track_delay_vol16 = effect_data >> 4;
            }
            break;
        }
        case PAT_CMD_SET_WAVE_OFFSET:
            pcd->wave_offset = effect_data;
            break;
        case PAT_CMD_VOLUME_RAMP: { // asm:2869-2884
            if (effect_data == 0) {
                break;
            }
            u8 down = effect_data & 0x0F;
            if (down != 0) {
                pcd->pat_vol_ramp_speed = -(i8)down;
            } else {
                pcd->pat_vol_ramp_speed = (i8)(effect_data >> 4);
            }
            break;
        }
        case PAT_CMD_POSITION_JUMP:
            player->next_pat_pos = effect_data;
            break;
        case PAT_CMD_SET_VOLUME: { // asm:2920-2925
            u8 vol = effect_data;
            if (vol > MAX_VOLUME) {
                vol = MAX_VOLUME;
            }
            pcd->pat_vol = vol;
            break;
        }
        case PAT_CMD_PATTERN_BREAK:
            player->next_pat_row = effect_data;
            break;
        case PAT_CMD_SET_SPEED: { // asm:2810-2833
            if (effect_data < MAX_SPEED) {
                player->pat_speed_even = effect_data;
                player->pat_speed_odd = effect_data;
                player->pat_line_ticks = effect_data;
                player->pat_stopped = (effect_data != 0) ? 1 : 0;
                if (effect_data == 0) {
                    player->songend_detected = 1;
                }
            } else {
                // Shuffle speed
                u8 even_spd = effect_data >> 4;
                u8 odd_spd = effect_data & 0x0F;
                player->pat_speed_even = even_spd;
                player->pat_speed_odd = odd_spd;
                u8 spd = (player->pat_curr_row & 1) ? odd_spd : even_spd;
                player->pat_line_ticks = spd;
            }
            break;
        }
        default:
            break;
    }
}

// Trigger ADSR release phase: compute vol64, set phase speed from release parameter.

static void trigger_adsr_release(PerChannelData* pcd) {
    i16 vol64 = (i16)pcd->adsr_volume >> 6;
    pcd->adsr_vol64 = (u16)vol64;
    pcd->adsr_pos = 16;
    i16 phase_speed = 16 - vol64;
    phase_speed >>= 1;
    phase_speed += pcd->adsr_release;
    pcd->adsr_phase_speed = (u8)phase_speed;
    pcd->adsr_phase = ADSR_PHASE_RELEASE;
}

// Process track delay handling (asm:3878-3977)
// Copies current channel output into delay buffer, loads delayed data to shadow channel.
// Advances *ch to skip the shadow channel when active.
// Returns true if the outer for-loop should break (delay on last channel pair).

static bool process_track_delay(PerChannelData* pcd, PlayerState* player, int* ch) {
    if (pcd->track_delay_steps == 0) {
        return false; // no delay active
    }
    if (pcd->channel_num >= NUM_CHANNELS - 1) {
        return true; // last channel: break out of loop
    }

    u8 delayed_offset = MAX_TRACK_DELAY - 1;  // asm:3905 — default read from last buffer slot

    if (pcd->track_delay_steps == 0xFF) {
        // Clear track delay (asm:3949-3954)
        pcd->track_delay_steps = 0;
        PerChannelData* next_ch = &player->channeldata[*ch + 1];
        next_ch->pat_vol = 0;
        next_ch->track_delay_steps = 0;
        next_ch->track_delay_offset = 0xFF;
    } else {
        // Normal track delay (asm:3891-3947)
        PerChannelData* next_ch = &player->channeldata[*ch + 1];
        u8 offset = next_ch->track_delay_offset;
        offset = (offset + 1) & (MAX_TRACK_DELAY - 1);
        next_ch->track_delay_offset = offset;

        // Copy current output to delay buffer
        OutputChannelData* dst_buf = &next_ch->track_delay_buffer[offset];
        dst_buf->sam_ptr_offset = pcd->out.sam_ptr_offset;
        dst_buf->length = pcd->out.length;
        dst_buf->loop_offset = pcd->out.loop_offset;
        dst_buf->period = pcd->out.period;
        dst_buf->volume = pcd->out.volume;
        dst_buf->trigger = pcd->out.trigger;

        // Adjust trigger for next channel (asm:3931-3938)
        // ASM: when trigger is set (d2!=0 after shift), always OR into trigger_mask.
        // The next_ch->track_delay_steps check only applies to the d2==0 path (no-op).
        u8 trg = pcd->out.trigger << 1;
        if (trg != 0) {
            player->trigger_mask |= trg;
        }

        // Apply track delay volume (asm:3926-3933)
        u16 vol = (u16)dst_buf->volume * (u16)pcd->track_delay_vol16;
        vol >>= 4;
        dst_buf->volume = (u8)vol;

        next_ch->track_delay_steps = pcd->track_delay_steps;

        // Read delayed data (asm:3937-3963)
        delayed_offset = (offset - pcd->track_delay_steps) & (MAX_TRACK_DELAY - 1);
    }

    // Load track data from delay buffer into shadow channel
    (*ch)++;
    PerChannelData* target = &player->channeldata[*ch];
    OutputChannelData* src_buf = &target->track_delay_buffer[delayed_offset];
    target->out.sam_ptr_offset = src_buf->sam_ptr_offset;
    target->out.length = src_buf->length;
    target->out.loop_offset = src_buf->loop_offset;
    target->out.period = src_buf->period;
    target->out.volume = src_buf->volume;
    target->out.trigger = src_buf->trigger;

    return false;
}

// Process pitch: vibrato, octave selection, trigger detection, period lookup (asm:3723-3874)

static void process_pitch(PerChannelData* pcd, PlayerState* player) {
    WaveInfo* wi = pcd->waveinfo_ptr;

    i16 d0_pitch = pcd->inst_pitch - 0x10;
    if (!pcd->inst_pitch_pinned) {
        d0_pitch += pcd->inst_sel_arp_note;
        d0_pitch += pcd->inst_curr_port_pitch;
        d0_pitch -= 0x10;
    }
    d0_pitch += pcd->inst_note_pitch;

    // Vibrato (asm:3738-3764)
    u8 vib_delay_lo = (u8)(pcd->vibrato_delay & 0xFF);
    if (vib_delay_lo != 0) {
        vib_delay_lo--;
        pcd->vibrato_delay = (pcd->vibrato_delay & 0xFF00) | vib_delay_lo;
    }
    if (vib_delay_lo == 0) {
        // Vibrato active
        i16 vib_speed = (i16)pcd->vibrato_speed;
        if (vib_speed != 0) {
            i16 vib_depth = (i16)pcd->vibrato_depth;
            i16 vib_pos = (i16)pcd->vibrato_pos + vib_speed;
            if (vib_pos > vib_depth || vib_pos < -vib_depth) {
                vib_speed = -vib_speed;
                pcd->vibrato_speed = (u16)vib_speed;
                if (vib_pos > vib_depth) {
                    vib_pos = vib_depth;
                } else {
                    vib_pos = -vib_depth;
                }
            }
            pcd->vibrato_pos = (u16)vib_pos;
            d0_pitch += vib_pos >> 3;
        }
    }

    // Octave selection for high pitches (asm:3770-3847)
    u16 d3_len = pcd->out.length;
    i16 d6_clamp_pitch = d0_pitch;

    if (d0_pitch > 0x219) {
        d6_clamp_pitch = 0x231;
        if (wi->flags & WI_FLAG_EXTRA_OCTAVES) {
            u16 chipram = scale_offset(read_be16((const u8*)&wi->chipram));
            u16 d5_idx = (u16)(d0_pitch - 0x219) >> 6;
            if (d5_idx >= sizeof(s_octave_select_table)) {
                d5_idx = sizeof(s_octave_select_table) - 1;
            }
            u8 shift = s_octave_select_table[d5_idx];

            u16 lof = pcd->out.loop_offset;
            if (lof != 0xFFFF) {
                // Has loop offset - shift it (asm:3789-3793)
                lof >>= shift;
                pcd->out.loop_offset = lof;
                d3_len >>= shift;
                pcd->out.length = d3_len;
            } else {
                // No loop - retrigger handling (asm:3797-3825)
                if (pcd->out.trigger && pcd->inst_wave_num != 0xFFFF) {
                    u16 wave_num = pcd->inst_wave_num;
                    f32* wave_base = player->wave_sample_table[wave_num >> 2];
                    u32 curr_off = pcd->out.sam_ptr_offset - (u32)(wave_base - player->sample_buffer_ptr);
                    u16 d6_total = d3_len + (u16)curr_off;
                    u16 d7_remain = chipram - d6_total;
                    if (d3_len >= d7_remain) {
                        d3_len = d3_len + d6_total - chipram;
                        d3_len >>= shift;
                    } else {
                        d3_len = 2;
                    }
                    pcd->out.length = d3_len;
                    curr_off >>= shift;
                    pcd->out.sam_ptr_offset = (u32)(wave_base - player->sample_buffer_ptr) + curr_off;
                }
            }

            // Add octave offset to sample pointer (asm:3828-3836)
            // Non-triggered one-shots jump to .no_retrigger_new (asm:3798), skipping octave offset
            if (shift >= 1 && (lof != 0xFFFF || pcd->out.trigger)) {
                u32 oct_offset = 0;
                u32 sam_size = chipram;
                for (u8 i = 0; i < shift; i++) {
                    oct_offset += sam_size;
                    sam_size >>= 1;
                }
                pcd->out.sam_ptr_offset += oct_offset;
            }

            // Subtract octave note offset (asm:3840-3844)
            u8 note_off = s_octave_note_offset_table[d5_idx];
            d0_pitch -= (i16)((u16)note_off << 2);

            if (d0_pitch > 0x231) {
                d0_pitch = 0x231;
            }

            if (d0_pitch < 0) {
                d0_pitch = 0;
            }
            d6_clamp_pitch = d0_pitch;
        }
        // else: no extra octaves, d6_clamp_pitch stays 0x231
    } else {
        if (d0_pitch < 0) {
            d0_pitch = 0;
        }
        d6_clamp_pitch = d0_pitch;
    }

    // Trigger handling (asm:3855-3871)
    if (pcd->out.trigger) {
        u16 lof = pcd->out.loop_offset;
        if (lof != 0xFFFF) {
            pcd->out.sam_ptr_offset += lof;
            pcd->out.loop_offset = 0;
        }
    }
    if (d3_len != pcd->last_trigger_pos) {
        pcd->last_trigger_pos = d3_len;
        pcd->out.trigger = pcd->channel_mask;
        player->trigger_mask |= pcd->channel_mask;
    }

    // Period table lookup (asm:3873-3874)
    u16 pitch_idx = (u16)d6_clamp_pitch;
    if (pitch_idx >= 16 * NOTES_IN_OCTAVE * 3) {
        pitch_idx = 16 * NOTES_IN_OCTAVE * 3 - 1;
    }
    pcd->out.period = player->period_table[pitch_idx];
}

// Process subloop / ping-pong and wave offset handling (asm:3596-3721)

static void process_subloop(PerChannelData* pcd, PlayerState* player) {
    WaveInfo* wi = pcd->waveinfo_ptr;
    u16 subloop_len = scale_offset(read_be16((const u8*)&wi->subloop_len));

    if (subloop_len != 0) {
        pcd->out.length = subloop_len;
        u16 subloop_step = scale_offset(read_be16((const u8*)&wi->subloop_step));
        u16 d1_offset;
        bool move_subloop = false;

        if (pcd->wave_offset != 0 && wi->allow_9xx) {
            // Wave offset with subloop (asm:3607-3619)
            d1_offset = scale_offset((u16)pcd->wave_offset << 7);
            pcd->wave_offset = 0;
            if ((i8)pcd->inst_ping_pong_dir >= 0) {
                d1_offset += subloop_step;
            } else {
                d1_offset -= subloop_step;
            }
            move_subloop = true;
        } else {
            // Auto subloop movement (asm:3621-3677)
            pcd->inst_subloop_wait--;
            if ((i16)pcd->inst_subloop_wait > 0) {
                // Still waiting - set loop offset only
                pcd->out.loop_offset = pcd->inst_loop_offset;
            } else {
                d1_offset = pcd->inst_loop_offset;
                move_subloop = true;
            }
        }

        if (move_subloop) {
            pcd->inst_subloop_wait = (u16)wi->subloop_wait;

            if ((i8)pcd->inst_ping_pong_dir < 0) {
                // Moving forward
                d1_offset += subloop_step;
                u16 next_end = d1_offset + subloop_len;
                u16 loop_end = scale_offset(read_be16((const u8*)&wi->loop_end));
                u16 chipram = scale_offset(read_be16((const u8*)&wi->chipram));
                u16 boundary = (d1_offset <= loop_end) ? loop_end : chipram;
                i16 space = (i16)boundary - (i16)next_end;
                if (space <= 0) {
                    d1_offset += (u16)space;
                    pcd->inst_ping_pong_dir = 0; // going backwards
                    if (space == 0) {
                        pcd->inst_subloop_wait--;
                    }
                }
            } else {
                // Moving backward
                d1_offset -= subloop_step;
                u16 loop_start = scale_offset(read_be16((const u8*)&wi->loop_start));
                i16 diff = (i16)loop_start - (i16)d1_offset;
                if (diff >= 0) {
                    d1_offset = loop_start;
                    pcd->inst_ping_pong_dir = 0xFF; // going forward
                    if (diff == 0) {
                        pcd->inst_subloop_wait--;
                    }
                }
            }

            pcd->inst_loop_offset = d1_offset;
            pcd->out.loop_offset = d1_offset;
        }

        // Set sample pointer (asm:3684-3685)
        u16 wave_num = pcd->inst_wave_num;
        if (wave_num != 0xFFFF) {
            pcd->out.sam_ptr_offset = (u32)(player->wave_sample_table[wave_num >> 2] - player->sample_buffer_ptr);
        }
    } else {
        // No subloop - wave offset handling (asm:3688-3721)
        if (pcd->wave_offset != 0 && wi->allow_9xx) {
            u16 d1_off = scale_offset(((u16)pcd->wave_offset << 8) >> 1);
            pcd->wave_offset = 0;

            pcd->out.trigger = pcd->channel_mask;
            player->trigger_mask |= pcd->channel_mask;

            u16 chipram = scale_offset(read_be16((const u8*)&wi->chipram));
            i16 remaining = (i16)chipram - (i16)d1_off;
            if (remaining <= 0) {
                remaining = 2;
                d1_off = 0;
            }
            pcd->out.length = (u16)remaining;

            u16 wave_num = pcd->inst_wave_num;
            if (wave_num != 0xFFFF) {
                f32* base = player->wave_sample_table[wave_num >> 2];
                pcd->out.sam_ptr_offset = (u32)((base + d1_off) - player->sample_buffer_ptr);
            }
        }
    }
}

static InstrumentStep decode_instrument_step(const u8* data) {
    InstrumentStep step;
    step.pitch = data[0] & PITCH_CTRL_NOTE_MASK;
    step.command = data[1] & 0x0F;
    step.command_data = data[2];
    step.stitched = (data[0] & 0x80) != 0;
    step.pitch_pinned = (data[0] & 0x40) != 0;
    return step;
}

static void activate_wave(PerChannelData* pcd, SongState* song, PlayerState* player,
                          u16 wave_idx, WaveActivationPolicy policy) {
    u16 previous_loop = pcd->inst_loop_offset;
    pcd->inst_wave_num = wave_idx << 2;
    WaveInfo* wi = policy == WAVE_ACTIVATE_FALLBACK ? song->waveinfo_ptr : song->waveinfo_table[wave_idx];
    pcd->waveinfo_ptr = wi;
    pcd->out.trigger = pcd->channel_mask;
    player->trigger_mask |= pcd->channel_mask;

    f32* wave_ptr = player->wave_sample_table[wave_idx];
    if (policy == WAVE_ACTIVATE_SYNC) {
        u16 loop_off = scale_offset(read_be16((const u8*)&wi->loop_offset));
        u16 subloop = scale_offset(read_be16((const u8*)&wi->subloop_len));
        if (subloop == 0) {
            wave_ptr += loop_off;
            u16 chipram = scale_offset(read_be16((const u8*)&wi->chipram));
            u16 length = chipram - loop_off;
            pcd->out.length = length > 1 ? length : 2;
            pcd->out.loop_offset = 0xFFFF;
        } else {
            pcd->out.loop_offset = 0;
        }
        pcd->out.sam_ptr_offset = (u32)(wave_ptr - player->sample_buffer_ptr);
        pcd->inst_ping_pong_dir = 0xFF;
        pcd->inst_subloop_wait = (u16)wi->subloop_wait + 1;
        pcd->inst_loop_offset = scale_offset(read_be16((const u8*)&wi->loop_offset));
        return;
    }

    u16 chipram = scale_offset(read_be16((const u8*)&wi->chipram));
    u16 subloop = scale_offset(read_be16((const u8*)&wi->subloop_len));
    u16 ping_pong_limit = chipram;
    if (subloop == 0) {
        u16 loop_off = scale_offset(read_be16((const u8*)&wi->loop_offset));
        wave_ptr += loop_off;
        if (policy == WAVE_ACTIVATE_FALLBACK) {
            pcd->out.length = (i16)(chipram - 1) > (i16)loop_off ? chipram - loop_off : 2;
        } else {
            u16 length = chipram - loop_off;
            pcd->out.length = length > 1 ? length : 2;
        }
        ping_pong_limit = pcd->out.length;
        pcd->out.loop_offset = 0xFFFF;
    } else {
        pcd->out.loop_offset = 0;
    }
    pcd->out.sam_ptr_offset = (u32)(wave_ptr - player->sample_buffer_ptr);

    if (policy == WAVE_ACTIVATE_NOSYNC) {
        if (ping_pong_limit < previous_loop)
            pcd->inst_ping_pong_dir = 0xFF;
        pcd->inst_subloop_wait = 0;
        u16 step = scale_offset(read_be16((const u8*)&wi->subloop_step));
        pcd->inst_loop_offset = previous_loop - step;
    } else {
        pcd->inst_subloop_wait = (u16)wi->subloop_wait + 1;
        pcd->inst_loop_offset = scale_offset(read_be16((const u8*)&wi->loop_offset));
        pcd->inst_ping_pong_dir = 0xFF;
    }
}

static void select_instrument_wave(PerChannelData* pcd, SongState* song, PlayerState* player,
                                   u8 command, u8 command_data) {
    u16 wave_idx = (u16)(command_data - 1);
    if (wave_idx >= MAX_WAVES)
        return;
    u16 wave_num4 = wave_idx << 2;
    if (wave_num4 == pcd->inst_wave_num)
        return;

    WaveActivationPolicy policy =
        command == INST_CMD_SELECT_WAVE ? WAVE_ACTIVATE_SYNC : WAVE_ACTIVATE_NOSYNC;
    activate_wave(pcd, song, player, wave_idx, policy);
}

static InstrumentStepControl dispatch_instrument_command(PerChannelData* pcd, SongState* song,
                                                         PlayerState* player, InstrumentStep step,
                                                         u8* step_pos, bool already_jumped) {
    switch (step.command) {
        case INST_CMD_SELECT_WAVE:
        case INST_CMD_SELECT_WAVE_NOSYNC:
            select_instrument_wave(pcd, song, player, step.command, step.command_data);
            break;
        case INST_CMD_SLIDE_UP:
            pcd->inst_pitch_slide = (i16)step.command_data;
            break;
        case INST_CMD_SLIDE_DOWN:
            pcd->inst_pitch_slide = -(i16)step.command_data;
            break;
        case INST_CMD_ADSR:
            if (step.command_data == ADSR_CMD_RELEASE) {
                trigger_adsr_release(pcd);
            } else if (step.command_data == ADSR_CMD_RESTART) {
                pcd->adsr_phase = ADSR_PHASE_ATTACK;
                pcd->adsr_volume = 0;
            }
            break;
        case INST_CMD_VOLUME_SLIDE: {
            u8 down = step.command_data & 0x0F;
            pcd->inst_vol_slide = down != 0 ? -(i8)down : (i8)(step.command_data >> 4);
            break;
        }
        case INST_CMD_JUMP_TO_STEP:
            if (step.command_data >= *step_pos)
                break;
            *step_pos = step.command_data;
            if (!already_jumped)
                return INST_STEP_FETCH_NEXT;
            pcd->inst_line_ticks = pcd->inst_speed_stop;
            pcd->inst_step_pos = *step_pos;
            return INST_STEP_EXIT;
        case INST_CMD_SET_VOLUME:
            pcd->inst_vol = step.command_data > MAX_VOLUME ? MAX_VOLUME : step.command_data;
            break;
        case INST_CMD_USE_PAT_ARP: {
            u8 arp_idx = step.command_data & 3;
            if (arp_idx == 0) {
                pcd->inst_sel_arp_note = 0;
                break;
            }
            u8 high = step.command_data >> 4;
            if (high > 1)
                break;
            u8 arp = pcd->arp_notes[arp_idx - 1];
            if (high == 0 && arp == 0) {
                (*step_pos)++;
                return INST_STEP_FETCH_NEXT;
            }
            pcd->inst_sel_arp_note = (i16)((u16)arp << 4);
            break;
        }
        case INST_CMD_SET_SPEED:
            pcd->inst_speed_stop = step.command_data == 0 ? 0xFF : step.command_data;
            break;
        default:
            break;
    }
    return INST_STEP_ADVANCE;
}

static void process_inst_pattern_steps(PerChannelData* pcd, SongState* song, PlayerState* player) {
    pcd->inst_pitch_slide = 0;
    pcd->inst_vol_slide = 0;

    u8 step_pos = pcd->inst_step_pos;
    u8 d2_stitched;

    // Past end check (asm:3086-3090)
    if (step_pos >= pcd->inst_pattern_steps) {
        d2_stitched = 0xFF;
        pcd->inst_line_ticks = (u8)(d2_stitched + pcd->inst_speed_stop);
        pcd->inst_step_pos = step_pos;
        return;
    }

    u8 d7_jumped = 0;
    u8 d3_stitched = 0;
    u8* a0 = song->inst_patterns_table[(pcd->inst_num4 >> 2) - 1] + (u32)step_pos * 3;

    for (;;) {
        InstrumentStep step = decode_instrument_step(a0);
        d2_stitched = step.stitched ? 1 : 0;

        // Load pitch (asm:3131-3142)
        if (!d3_stitched) {
            u8 note = step.pitch;
            if (note != 0) {
                note--;
                pcd->inst_note_pitch = (i16)((u16)note << 4);
                pcd->inst_pitch_pinned = step.pitch_pinned ? 0xFF : 0;
            }
        }

        InstrumentStepControl control = dispatch_instrument_command(
            pcd, song, player, step, &step_pos, d7_jumped != 0);
        if (control == INST_STEP_EXIT)
            return;

        // Normal advance (asm:3384-3393)
        if (control == INST_STEP_ADVANCE) {
            step_pos++;
            if (!d2_stitched) {
                // Non-stitched exit (inst_pat_loop_exit2)
                pcd->inst_line_ticks = (u8)(d2_stitched + pcd->inst_speed_stop);
                pcd->inst_step_pos = step_pos;
                return;
            }
        }

        // inst_fetch_next: continue stitching
        d7_jumped = 1;
        d3_stitched = d2_stitched;
        if (step_pos < pcd->inst_pattern_steps) {
            a0 = song->inst_patterns_table[(pcd->inst_num4 >> 2) - 1] + (u32)step_pos * 3;
            continue;
        }
        // Past end (inst_pat_loop_exit)
        d2_stitched = 0xFF;
        pcd->inst_line_ticks = (u8)(d2_stitched + pcd->inst_speed_stop);
        pcd->inst_step_pos = step_pos;
        return;
    }
}

// Reset instrument-owned channel state according to the trigger policy.

static void clear_inst_state(PerChannelData* pcd, InstrumentResetPolicy reset_policy) {
    pcd->pat_portamento_dest = 0;
    pcd->pat_pitch_slide = 0;
    pcd->pat_vol_ramp_speed = 0;
    pcd->pat_2nd_inst_num4 = 0;
    pcd->pat_2nd_inst_delay = 0;
    pcd->wave_offset = 0;
    pcd->inst_pitch_slide = 0;
    pcd->inst_sel_arp_note = 0;
    pcd->inst_note_pitch = 0;
    if (reset_policy == INST_RESET_ALL) {
        pcd->inst_curr_port_pitch = 0;
    }
    pcd->inst_line_ticks = 0;
    pcd->inst_pitch_pinned = 0;
    pcd->inst_vol_slide = 0;
    pcd->inst_step_pos = 0;
    pcd->inst_wave_num = 0xFFFF;
    pcd->track_delay_offset = 0xFF;
    pcd->inst_speed_stop = 1;
    pcd->inst_pitch = 0x10;
    pcd->inst_vol = MAX_VOLUME;
    pcd->loaded_inst_vol = MAX_VOLUME;
    pcd->pat_vol = MAX_VOLUME;
}

static void load_instrument(PerChannelData* pcd, SongState* song, u16 inst_num4,
                            InstrumentResetPolicy reset_policy) {
    pcd->new_inst_num = (u8)inst_num4;
    pcd->inst_num4 = inst_num4;
    pcd->inst_info_ptr = &song->inst_infos[(inst_num4 >> 2) - 1];
    pcd->inst_pattern_steps = pcd->inst_info_ptr->pattern_steps;
    clear_inst_state(pcd, reset_policy);
}

// Process pattern data for a single channel (asm:2373-2925)
// Handles ADSR release delay, 2nd instrument trigger, portamento, volume ramp,
// delayed notes, pattern reading, ARP, instrument triggers, and effect commands.

static PatternRow decode_pattern_row(const u8* data) {
    PatternRow row;
    row.pitch_ctrl = data[0];
    row.effect_cmd = data[1] & 0x0F;
    row.effect_data = data[2];
    row.instrument = data[1] >> 4;
    if (row.pitch_ctrl & PITCH_CTRL_INST_HI)
        row.instrument += 16;
    row.pitch = row.pitch_ctrl & PITCH_CTRL_NOTE_MASK;
    row.has_arpeggio = (row.pitch_ctrl & PITCH_CTRL_HAS_ARP) != 0;
    return row;
}

static bool advance_pending_pattern_state(PerChannelData* pcd, SongState* song) {
    if (pcd->pat_adsr_rel_delay > 0) {
        pcd->pat_adsr_rel_delay--;
        if (pcd->pat_adsr_rel_delay == 0)
            trigger_adsr_release(pcd);
    }

    bool triggered_2nd_inst = false;
    if (pcd->pat_2nd_inst_num4 != 0) {
        if (pcd->pat_2nd_inst_delay == 0) {
            u8 inst_num4 = pcd->pat_2nd_inst_num4;
            load_instrument(pcd, song, inst_num4, INST_RESET_PRESERVE_PORT_PITCH);
            triggered_2nd_inst = true;
        } else {
            pcd->pat_2nd_inst_delay--;
        }
    }

    if (!triggered_2nd_inst) {
        if (pcd->pat_portamento_dest != 0) {
            i16 curr = pcd->inst_curr_port_pitch;
            i16 dest = pcd->pat_portamento_dest;
            i16 speed = (i16)(u16)pcd->pat_portamento_speed;
            if (curr < dest) {
                curr += speed;
                if (curr > dest) {
                    pcd->pat_portamento_dest = 0;
                    curr = dest;
                }
            } else {
                curr -= speed;
                if (curr < dest) {
                    pcd->pat_portamento_dest = 0;
                    curr = dest;
                }
            }
            pcd->inst_curr_port_pitch = curr;
        }

        if (pcd->pat_vol_ramp_speed != 0) {
            i16 vol = (i16)(i8)pcd->pat_vol_ramp_speed + (i16)pcd->pat_vol;
            if (vol < 0) {
                vol = 0;
            }
            if (vol > MAX_VOLUME) {
                vol = MAX_VOLUME;
            }
            pcd->pat_vol = (u8)vol;
        }
    }

    i16 note_delay_val = (i16)(i8)pcd->note_delay;
    if (note_delay_val < 0)
        return false;
    if (note_delay_val > 0) {
        note_delay_val--;
        if (note_delay_val == 0) {
            pcd->note_delay = 0xFF;
        } else {
            pcd->note_delay = (u8)note_delay_val;
            return false;
        }
    }
    return true;
}

static bool read_pattern_row(const PerChannelData* pcd, const SongState* song,
                             const PlayerState* player, PatternRow* row, i16* pitch_shift) {
    if (song->curr_pat_pos >= song->pat_pos_len)
        return false;

    u16 pos_offset = song->curr_pat_pos * 4 + pcd->channel_num;
    pos_offset *= 2;
    const u8* pos_ptr = song->pos_data_adr + pos_offset;

    u8 curr_row = player->pat_curr_row;
    if (curr_row >= song->num_steps)
        return false;

    u8 pattern_num = pos_ptr[0];
    if (pattern_num == 0 || pattern_num > song->num_patterns ||
        song->pattern_table[pattern_num - 1] == NULL)
        return false;

    const u8* pattern_data = song->pattern_table[pattern_num - 1] + (u32)curr_row * 3;
    *pitch_shift = (i16)(i8)pos_ptr[1];
    *row = decode_pattern_row(pattern_data);
    return true;
}

static bool apply_extended_pattern_command(PerChannelData* pcd, const PatternResolution* resolution) {
    if (resolution->effect_cmd != PAT_CMD_EXTENDED || (i8)pcd->note_delay < 0)
        return false;

    u8 value = resolution->effect_data & 0x0F;
    u8 command = resolution->effect_data >> 4;
    if (command == PAT_EXT_NOTE_DELAY) {
        pcd->note_delay = value;
        return true;
    }
    if (command == PAT_EXT_NOTE_OFF_DELAY)
        pcd->note_off_delay = value;
    return false;
}

static void resolve_arpeggio_and_second_instrument(PerChannelData* pcd, SongState* song,
                                                   const PatternRow* row, PatternResolution* resolution) {
    if (row->has_arpeggio) {
        if ((resolution->effect_cmd | resolution->effect_data) != 0) {
            pcd->arp_notes[0] = resolution->effect_cmd;
            pcd->arp_notes[1] = resolution->effect_data >> 4;
            pcd->arp_notes[2] = resolution->effect_data & 0x0F;
        }
        resolution->arp_flag = 1;
        resolution->effect_cmd = 0;
        return;
    }

    if (resolution->effect_cmd != PAT_CMD_PLAY_2ND_INST || resolution->effect_data == 0)
        return;

    u8 second_instrument = resolution->effect_data & 0x0F;
    u16 second_instrument4 = (u16)second_instrument << 2;
    if (resolution->pitch != 0) {
        resolution->alternate_instrument = (u8)resolution->inst_num4;
        resolution->inst_num4 = second_instrument4;
        return;
    }

    resolution->pitch_shift += 1;
    resolution->pitch_shift <<= 4;
    if (second_instrument == 0) {
        memset(pcd->arp_notes, 0, 4);
        pcd->inst_pitch = 0x10;
        pcd->inst_curr_port_pitch = resolution->pitch_shift;
        pcd->pat_portamento_dest = 0;
        resolution->alternate_instrument = (u8)resolution->inst_num4;
        return;
    }

    resolution->alternate_instrument = (u8)resolution->inst_num4;
    resolution->inst_num4 = second_instrument4;
    load_instrument(pcd, song, resolution->inst_num4, INST_RESET_ALL);
    resolution->resolve_portamento = true;
}

static void resolve_pattern_note(PerChannelData* pcd, SongState* song, PatternResolution* resolution) {
    if (resolution->resolve_portamento)
        return;

    if (resolution->inst_num4 != 0 && resolution->pitch == 0) {
        pcd->pat_vol = pcd->loaded_inst_vol;
        if (resolution->inst_num4 == pcd->inst_num4) {
            pcd->adsr_phase = ADSR_PHASE_ATTACK;
            pcd->adsr_volume = 0;
        }
    }

    if (resolution->pitch == NOTE_OFF_PITCH) {
        trigger_adsr_release(pcd);
        return;
    }
    if (resolution->pitch == 0)
        return;

    resolution->pitch_shift += resolution->pitch;
    resolution->pitch_shift <<= 4;
    if (resolution->inst_num4 != 0 && resolution->effect_cmd != PAT_CMD_TONE_PORTAMENTO)
        load_instrument(pcd, song, resolution->inst_num4, INST_RESET_ALL);
    resolution->resolve_portamento = true;
}

static void resolve_pattern_portamento(PerChannelData* pcd, PatternResolution* resolution) {
    if (!resolution->resolve_portamento)
        return;

    if (!resolution->arp_flag)
        memset(pcd->arp_notes, 0, 4);
    if (resolution->effect_cmd == PAT_CMD_TONE_PORTAMENTO) {
        resolution->pitch_shift += 0x10;
        pcd->pat_portamento_dest = resolution->pitch_shift;
        pcd->inst_curr_port_pitch += pcd->inst_pitch;
        pcd->inst_pitch = 0;
        if (resolution->effect_data != 0)
            pcd->pat_portamento_speed = resolution->effect_data;
    } else {
        pcd->inst_pitch = 0x10;
        pcd->inst_curr_port_pitch = resolution->pitch_shift;
        pcd->pat_portamento_dest = 0;
    }
}

static void finalize_pattern_effect(PerChannelData* pcd, PlayerState* player,
                                    const PatternResolution* resolution) {
    if (resolution->alternate_instrument != 0) {
        pcd->pat_2nd_inst_num4 = resolution->alternate_instrument;
        pcd->pat_2nd_inst_delay = resolution->effect_data >> 4;
    }
    pcd->pat_vol_ramp_speed = 0;
    pcd->pat_pitch_slide = 0;
    if (!resolution->arp_flag)
        process_pattern_effects(pcd, resolution->effect_cmd, resolution->effect_data, player);
}

static void apply_pattern_row(PerChannelData* pcd, SongState* song, PlayerState* player,
                              PatternRow row, i16 d0_shift) {
    PatternResolution resolution = {
        .effect_cmd = row.effect_cmd,
        .effect_data = row.effect_data,
        .pitch = row.pitch,
        .inst_num4 = (u16)row.instrument << 2,
        .pitch_shift = d0_shift,
    };

    if (apply_extended_pattern_command(pcd, &resolution))
        return;
    pcd->note_delay = 0xFF;
    resolve_arpeggio_and_second_instrument(pcd, song, &row, &resolution);
    resolve_pattern_note(pcd, song, &resolution);
    resolve_pattern_portamento(pcd, &resolution);
    finalize_pattern_effect(pcd, player, &resolution);
}

static void process_pattern_channel(PerChannelData* pcd, SongState* song, PlayerState* player) {
    if (!advance_pending_pattern_state(pcd, song))
        return;

    PatternRow row;
    i16 pitch_shift;
    if (read_pattern_row(pcd, song, player, &row, &pitch_shift))
        apply_pattern_row(pcd, song, player, row, pitch_shift);
}

static void process_pattern_channels(PlayerState* player, SongState* song) {
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        PerChannelData* pcd = &player->channeldata[ch];
        process_pattern_channel(pcd, song, player);

        if (pcd->track_delay_steps != 0) {
            if (pcd->channel_num >= NUM_CHANNELS - 2)
                break;
            ch++;
        }
    }
}

static void advance_song_position(PlayerState* player, SongState* song) {
    player->pat_line_ticks--;
    if (player->pat_line_ticks != 0)
        return;

    for (int channel = 0; channel < NUM_CHANNELS; channel++)
        player->channeldata[channel].note_delay = 0;

    u8 num_steps = song->num_steps;
    u8 curr_row = player->pat_curr_row + 1;
    u16 pat_pos = song->curr_pat_pos;

    if ((i8)player->next_pat_row >= 0) {
        curr_row = player->next_pat_row;
        player->next_pat_row = 0xFF;
        if (curr_row >= num_steps)
            curr_row = num_steps - 1;
        pat_pos++;
    } else if (curr_row >= num_steps) {
        curr_row = 0;
        pat_pos++;
    }

    if ((i8)player->next_pat_pos >= 0) {
        u8 new_pos = player->next_pat_pos;
        player->next_pat_pos = 0xFF;
        if (new_pos <= pat_pos)
            player->songend_detected = 1;
        pat_pos = new_pos;
        curr_row = 0;
    }

    if (pat_pos >= song->pat_pos_len) {
        pat_pos = song->pat_restart_pos;
        player->songend_detected = 1;
    }

    player->pat_curr_row = curr_row;
    song->curr_pat_pos = pat_pos;
    player->pat_line_ticks = (curr_row & 1) ? player->pat_speed_odd : player->pat_speed_even;
}

static void advance_adsr_and_volume(PerChannelData* pcd, const UnpackedInstrumentInfo* inst_info) {
    i16 adsr_volume = (i16)pcd->adsr_volume;

    if (pcd->new_inst_num != 0) {
        pcd->loaded_inst_vol = (u8)(i16)pcd->inst_vol;
        pcd->vibrato_delay = inst_info->vibrato_delay;
        pcd->vibrato_depth = (u16)inst_info->vibrato_depth;
        pcd->vibrato_speed = (u16)inst_info->vibrato_speed;
        pcd->adsr_release = inst_info->adsr_release;
        adsr_volume = 0;
        pcd->adsr_phase = ADSR_PHASE_ATTACK;
        pcd->adsr_volume = 0;
        pcd->new_inst_num = 0;
        pcd->vibrato_pos = 0;
    }

    switch (pcd->adsr_phase) {
        case ADSR_PHASE_ATTACK:
            adsr_volume += inst_info->adsr_attack;
            if (adsr_volume >= MAX_VOLUME << 4) {
                adsr_volume = MAX_VOLUME << 4;
                pcd->adsr_phase = ADSR_PHASE_DECAY;
                pcd->adsr_phase_speed = (u8)(inst_info->adsr_decay & 0xFF);
            }
            break;
        case ADSR_PHASE_DECAY:
        case ADSR_PHASE_RELEASE: {
            if (pcd->adsr_phase == ADSR_PHASE_RELEASE) {
                u16 pos = pcd->adsr_pos + pcd->adsr_vol64;
                pcd->adsr_pos = pos;
                if (pos < 16)
                    break;
                pcd->adsr_pos = pos - 16;
            }

            u8 phase_speed = pcd->adsr_phase_speed;
            i16 decay = 2;
            if (phase_speed < 0x8F) {
                decay = s_roll_off_table[phase_speed];
                pcd->adsr_phase_speed = phase_speed + 1;
            } else {
                pcd->adsr_phase_speed = 0x8F;
            }

            adsr_volume -= decay;
            if (pcd->adsr_phase == ADSR_PHASE_RELEASE) {
                if (adsr_volume < 0)
                    adsr_volume = 0;
            } else if (adsr_volume <= inst_info->adsr_sustain) {
                pcd->adsr_phase = ADSR_PHASE_SUSTAIN;
                adsr_volume = inst_info->adsr_sustain;
            }
            break;
        }
        case ADSR_PHASE_SUSTAIN:
            break;
    }

    pcd->adsr_volume = (u16)adsr_volume;
    if (pcd->note_off_delay != 0) {
        pcd->note_off_delay--;
        if (pcd->note_off_delay == 0) {
            pcd->adsr_volume = 0;
            pcd->adsr_phase = ADSR_PHASE_RELEASE;
        }
    }

    i16 volume = (adsr_volume >> 4) * (i16)(u8)(pcd->inst_vol & 0xFF) >> 6;
    volume = (volume * (i16)pcd->pat_vol) >> 6;
    pcd->out.volume = (u8)volume;
}

static bool update_instrument_channel(PlayerState* player, SongState* song, int* channel) {
    PerChannelData* pcd = &player->channeldata[*channel];
    UnpackedInstrumentInfo* inst_info = pcd->inst_info_ptr;

    if (inst_info != NULL) {
        i16 pitch_delta = pcd->inst_pitch_slide + pcd->pat_pitch_slide;
        if (pitch_delta != 0) {
            i16 pitch = pcd->inst_pitch + pitch_delta;
            if (pitch > (3 * NOTES_IN_OCTAVE) << 4)
                pitch = (3 * NOTES_IN_OCTAVE) << 4;
            pcd->inst_pitch = pitch;
        }

        if (pcd->inst_vol_slide != 0) {
            i16 volume = (i16)(i8)(pcd->inst_vol & 0xFF) + (i16)pcd->inst_vol_slide;
            if (volume < 0)
                volume = 0;
            if (volume > MAX_VOLUME)
                volume = MAX_VOLUME;
            pcd->inst_vol = (pcd->inst_vol & 0xFF00) | (u16)(u8)volume;
        }

        if (pcd->inst_line_ticks == 0)
            process_inst_pattern_steps(pcd, song, player);

        if (pcd->inst_wave_num == 0xFFFF || (pcd->inst_wave_num & 0xFF) >= 0x80) {
            activate_wave(pcd, song, player, 0, WAVE_ACTIVATE_FALLBACK);
        }

        if (pcd->inst_line_ticks != 0xFF)
            pcd->inst_line_ticks--;
    }

    advance_adsr_and_volume(pcd, inst_info);
    process_subloop(pcd, player);
    process_pitch(pcd, player);
    return process_track_delay(pcd, player, channel);
}

// Called once per 50 Hz tick. The ordering mirrors the original player.

void pretracker_player_tick(PlayerState* player) {
    SongState* song = player->my_song;

    if (player->pat_stopped) {
        process_pattern_channels(player, song);
        advance_song_position(player, song);
    }

    for (int channel = 0; channel < NUM_CHANNELS; channel++) {
        if (update_instrument_channel(player, song, &channel))
            break;
    }

    // Clear trigger mask for next tick (asm:3999)
    player->trigger_mask = 0;
}

bool pretracker_player_is_finished(const PlayerState* player) {
    return player->songend_detected != 0;
}
