#include "pretracker_internal.h"

#include <math.h>
#include <string.h>

// blep_insert: Insert a BLEP correction into a circular buffer

static void blep_insert(f32* buf, i32 read_idx, i32* count, f32 delta, f32 phase) {
    // phase is 0..1, map to table position 0..5
    f32 table_pos = phase * 5.0f;
    i32 idx = (i32)table_pos;
    if (idx >= 4) {
        idx = 4;
        table_pos = 4.0f;
    }
    f32 frac = table_pos - (f32)idx;

    // Always insert starting from current read position so tap[0] aligns with the next drain
    i32 wi = read_idx;
    for (i32 tap = 0; tap < 8; tap++) {
        f32 coef = s_blep_table[tap][idx];
        if (frac != 0.0f)
            coef += (s_blep_table[tap][idx + 1] - coef) * frac;
        buf[wi] += coef * delta;
        wi = (wi + 1) & 7;
    }
    *count = 8;
}

// blep_drain: Drain one tap from a BLEP circular buffer

static f32 blep_drain(f32* buf, i32* read_idx, i32* count) {
    if (*count <= 0)
        return 0.0f;
    i32 ri = *read_idx;
    f32 val = buf[ri];
    buf[ri] = 0.0f;
    *read_idx = (ri + 1) & 7;
    (*count)--;
    return val;
}

// Windowed sinc interpolation — 8-point Lanczos (a=4), 128 sub-sample phases.
// Produces cleaner output than nearest-neighbor + BLEP when source buffers are high-quality floats.

static inline f32 sinc_interpolate(const f32* data, u32 data_len, f64 frac_pos) {
    i32 ipos = (i32)frac_pos;
    f32 frac = (f32)(frac_pos - (f64)ipos);
    i32 phase = (i32)(frac * SINC_PHASES);
    if (phase >= SINC_PHASES)
        phase = SINC_PHASES - 1;
    const f32* kernel = s_sinc_table[phase];
    f32 sum = 0.0f;
    for (i32 t = 0; t < SINC_TAPS; t++) {
        i32 idx = ipos - SINC_TAPS / 2 + 1 + t;
        if (idx < 0)
            idx = 0;
        else if ((u32)idx >= data_len)
            idx = (i32)data_len - 1;
        sum += data[idx] * kernel[t];
    }
    return sum;
}

// pretracker_mixer_init: Initialize mixer state

void pretracker_mixer_init(MixerState* mixer, u32 output_rate) {
    memset(mixer, 0, sizeof(MixerState));
    mixer->output_rate = output_rate;
    mixer->samples_per_tick = output_rate / 50;
    mixer->samples_until_tick = 0;
    mixer->solo_channel = -1;
    mixer->stereo_mix = 0.0f;

    for (int i = 0; i < NUM_CHANNELS; i++) {
        mixer->channels[i].active = false;
        mixer->channels[i].volume = 0.0f;
    }
}

// Establish mixer-owned playback bounds and channel panning.

void pretracker_mixer_start(MixerState* mixer, const PlayerState* player, const SongState* song) {
    // Amiga hard-panning: channels 0,3 left; 1,2 right (matches PreTracker.exe)
    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (i == 0 || i == 3) {
            mixer->channels[i].pan_left = 1.0f;
            mixer->channels[i].pan_right = 0.0f;
        } else {
            mixer->channels[i].pan_left = 0.0f;
            mixer->channels[i].pan_right = 1.0f;
        }
    }

    // Wire up sample buffer info for bounds checking in mixer_sync_channels.
    // Compute total buffer size from wavetotal_table (matches pretracker_parse_song return value).
    u32 total_size = 2; // 2 bytes of silence at start
    for (int i = 0; i < song->num_waves; i++) {
        total_size += song->wavetotal_table[i];
    }
    mixer->sample_buffer_ptr = player->sample_buffer_ptr;
    mixer->sample_buffer_size = total_size;

}

// mixer_sync_channels: Transfer tick engine output to mixer state

static void mixer_sync_channels(PlayerState* player, MixerState* mixer) {
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        PerChannelData* pcd = &player->channeldata[ch];
        MixerChannel* mc = &mixer->channels[ch];

        // Convert period to playback speed
        u16 period = pcd->out.period;
        if (period > 0) {
            f64 freq = AMIGA_CLOCK / (f64)period;
            mc->speed = freq / (f64)mixer->output_rate * ((f64)HQ_MAX_PERIOD / (f64)AMIGA_MAX_PERIOD);
        } else {
            mc->speed = 0.0;
        }

        // Volume: 0-64 -> 0.0-1.0
        mc->volume = (f32)pcd->out.volume / 64.0f;

        // Resolve sample pointer and handle trigger/loop offset
        // Mirrors the Amiga DMA output code (asm:4067-4097):
        // - Triggered channels: DMA writes ac_ptr, ac_len, starts new sample
        // - Non-triggered looping: DMA writes ac_ptr only (length unchanged)
        // - Non-triggered one-shot: DMA unchanged (keep playing current buffer)
        if (pcd->out.trigger) {
            // Triggered: write both pointer and length (asm:4079-4082)
            mc->sample_length = pcd->out.length;
            u32 off = pcd->out.sam_ptr_offset;
            if (off + mc->sample_length <= mixer->sample_buffer_size) {
                mc->sample_data = player->sample_buffer_ptr + off;
            } else {
                mc->sample_data = player->sample_buffer_ptr;
            }
            mc->loop_offset = pcd->out.loop_offset;
            mc->frac_pos = 0.0;
            mc->active = true;
            pcd->out.trigger = 0;

            // Post-trigger loop setup (asm:4136-4156): after the one-shot buffer
            // finishes, Amiga DMA reloads from sam_ptr+loop_offset. Pre-compute
            // the loop pointer so the render loop can switch to it on first wrap.
            mc->loop_data = nullptr;
            if (pcd->out.loop_offset != 0xFFFF) {
                u32 loop_off = pcd->out.sam_ptr_offset + pcd->out.loop_offset;
                if (loop_off + mc->sample_length <= mixer->sample_buffer_size) {
                    mc->loop_data = player->sample_buffer_ptr + loop_off;
                }
            }
        } else if (pcd->out.loop_offset != 0xFFFF) {
            // Non-triggered looping: set pending loop pointer (asm:4086-4091)
            // Amiga DMA finishes current buffer then reloads from new ac_ptr on wrap.
            u32 off = pcd->out.sam_ptr_offset + pcd->out.loop_offset;
            if (off + mc->sample_length <= mixer->sample_buffer_size) {
                mc->loop_data = player->sample_buffer_ptr + off;
            }
            mc->loop_offset = pcd->out.loop_offset;
        }
        // Non-triggered one-shot: keep current sample_data/length unchanged (asm:4088-4089)

        if (mc->sample_length <= 1 || mc->speed <= 0.0) {
            mc->active = false;
        }

        // Solo channel: silence non-selected channels
        if (mixer->solo_channel >= 0 && ch != mixer->solo_channel) {
            mc->active = false;
        }
    }
}

// pretracker_mixer_render: Render N stereo float frames, interleaving tick calls with mixing
// Optional scopes: per-channel mono output (sample * volume, pre-pan). NULL to skip.

int pretracker_mixer_render(PlayerState* player, MixerState* mixer, f32* buffer, int num_frames, f32** scopes,
                    int num_scopes) {
    if (num_scopes > NUM_CHANNELS)
        num_scopes = NUM_CHANNELS;

    // Zero scope buffers upfront so inactive channels read as 0.0f
    if (scopes) {
        for (int ch = 0; ch < num_scopes; ch++) {
            if (scopes[ch])
                memset(scopes[ch], 0, (size_t)num_frames * sizeof(f32));
        }
    }

    int frames_written = 0;

    while (frames_written < num_frames) {
        // rePlayer begin
        if (pretracker_player_is_finished(player)) {
            if (frames_written == 0)
                player->songend_detected = 0;
            break;
        }
        // rePlayer end
        // Time to tick?
        if (mixer->samples_until_tick == 0) {
            pretracker_player_tick(player);
            mixer_sync_channels(player, mixer);
            mixer->samples_until_tick = mixer->samples_per_tick;
        }

        // How many samples until next tick or end of buffer
        int chunk = num_frames - frames_written;
        if ((u32)chunk > mixer->samples_until_tick) {
            chunk = (int)mixer->samples_until_tick;
        }

        f32* out = buffer + frames_written * 2;

        // Mix chunk
        for (int i = 0; i < chunk; i++) {
            f32 left = 0.0f;
            f32 right = 0.0f;

            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                MixerChannel* mc = &mixer->channels[ch];
                if (!mc->active || mc->sample_length <= 1)
                    continue;

                f32 sample_f;
                f32 volume_f = mc->volume;

                if (mixer->interp_mode == PRE_INTERP_SINC) {
                    // Sinc: reconstruct the continuous signal between samples
                    sample_f = sinc_interpolate(mc->sample_data, mc->sample_length, mc->frac_pos) * 0.5f;
                } else {
                    // BLEP: nearest-neighbor + anti-aliasing correction (matches PreTracker.exe)
                    u32 pos = (u32)mc->frac_pos;
                    sample_f = mc->sample_data[pos] * 0.5f;

                    // Update sub-sample accumulator for BLEP phase tracking
                    f32 step_f = (f32)mc->speed;
                    mc->sub_accum += step_f;
                    if (mc->sub_accum >= 1.0f) {
                        mc->sub_accum -= 1.0f;
                        mc->blep_step = step_f;
                        mc->blep_wrap = mc->sub_accum;
                    }

                    // BLEP on sample value change
                    if (sample_f != mc->prev_sample) {
                        f32 delta = mc->prev_sample - sample_f;
                        if (mc->blep_step > 0.0f && mc->blep_step > mc->blep_wrap) {
                            f32 phase = mc->blep_wrap / mc->blep_step;
                            if (phase >= 0.0f && phase <= 1.0f) {
                                blep_insert(mc->blep_buf, mc->blep_read_idx, &mc->blep_count, delta, phase);
                            }
                        }
                        mc->prev_sample = sample_f;
                    }

                    // Drain sample BLEP
                    sample_f += blep_drain(mc->blep_buf, &mc->blep_read_idx, &mc->blep_count);
                }

                // BLEP on volume change (both modes — volume changes are instant step functions)
                if (volume_f != mc->prev_volume) {
                    f32 delta = mc->prev_volume - volume_f;
                    blep_insert(mc->vol_blep_buf, mc->vol_blep_read_idx, &mc->vol_blep_count, delta, 0.0f);
                    mc->prev_volume = volume_f;
                }

                // Drain volume BLEP
                volume_f += blep_drain(mc->vol_blep_buf, &mc->vol_blep_read_idx, &mc->vol_blep_count);

                // Mix to stereo
                f32 mixed = sample_f * volume_f;
                if (scopes && ch < num_scopes && scopes[ch])
                    scopes[ch][frames_written + i] = mixed;

                left += mixed * mc->pan_left;
                right += mixed * mc->pan_right;

                // Advance position
                mc->frac_pos += mc->speed;

                // Handle looping / end (Amiga DMA reload behavior)
                if ((u32)mc->frac_pos >= mc->sample_length) {
                    if (mc->loop_offset != 0xFFFF) {
                        // On first wrap after trigger: switch from one-shot to loop buffer
                        // (asm:4141-4151 post-trigger sets ac_ptr = sam_ptr + loop_offset)
                        if (mc->loop_data) {
                            mc->sample_data = mc->loop_data;
                            mc->loop_data = nullptr;
                        }
                        while ((u32)mc->frac_pos >= mc->sample_length)
                            mc->frac_pos -= (f64)mc->sample_length;
                    } else {
                        mc->active = false;
                    }
                }
            }

            out[i * 2] = left;
            out[i * 2 + 1] = right;
        }

        frames_written += chunk;
        mixer->samples_until_tick -= (u32)chunk;
    }

    // Stereo separation post-pass (matches PreTracker.exe)
    // stereo_mix: 0.0 = full separation (no cross-feed), 1.0 = mono
    f32 sep = 1.0f - mixer->stereo_mix;
    if (sep < 1.0f) {
        f32 blend = 1.0f - sep;
        f32 inv_divisor = 1.0f / (2.0f - sep);
        for (int i = 0; i < frames_written; i++) {
            f32 l = buffer[i * 2];
            f32 r = buffer[i * 2 + 1];
            buffer[i * 2] = (r * blend + l) * inv_divisor;
            buffer[i * 2 + 1] = (l * blend + r) * inv_divisor;
        }
    }

    // Haas effect stereo widening post-pass
    if (mixer->haas_delay_samples > 0) {
        f32 blend = mixer->haas_blend;
        f32 norm = 1.0f / (1.0f + blend);
        u32 delay = mixer->haas_delay_samples;
        for (int i = 0; i < frames_written; i++) {
            f32 l = buffer[i * 2];
            f32 r = buffer[i * 2 + 1];
            u32 read_idx = (mixer->haas_write_idx - delay) & 63;
            f32 delayed_l = mixer->haas_buf_l[read_idx];
            f32 delayed_r = mixer->haas_buf_r[read_idx];
            buffer[i * 2] = (l + blend * delayed_r) * norm;
            buffer[i * 2 + 1] = (r + blend * delayed_l) * norm;
            mixer->haas_buf_l[mixer->haas_write_idx & 63] = l;
            mixer->haas_buf_r[mixer->haas_write_idx & 63] = r;
            mixer->haas_write_idx++;
        }
    }

    return frames_written;
}
// rePlayer begin
int pretracker_mixer_muted(PlayerState* player, MixerState* mixer, int num_frames) {

    int frames_written = 0;

    while (frames_written < num_frames) {
        // Time to tick?
        if (mixer->samples_until_tick == 0) {
            pretracker_player_tick(player);
            mixer_sync_channels(player, mixer);
            mixer->samples_until_tick = mixer->samples_per_tick;
        }

        // How many samples until next tick or end of buffer
        int chunk = num_frames - frames_written;
        if ((u32)chunk > mixer->samples_until_tick) {
            chunk = (int)mixer->samples_until_tick;
        }

        frames_written += chunk;
        mixer->samples_until_tick -= (u32)chunk;
    }

    return frames_written;
}
// rePlayer end