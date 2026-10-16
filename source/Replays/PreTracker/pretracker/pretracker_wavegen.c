#include "pretracker_internal.h"

#include <math.h>
#include <string.h>

static void gen_filter(PlayerState* player, const WaveInfo* wi);
static void gen_modulator(PlayerState* player, const WaveInfo* wi);
static void gen_volume_envelope(PlayerState* player, const WaveInfo* wi);

static void gen_osc_buffers(PlayerState* player) {
    // Normalize the signed-byte waveform domain to the full symmetric float range.
    // The reference 8-bit output is exactly (normalized * 255/256) - 1/256;
    // retaining this form avoids carrying its attenuation and DC bias into sample generation.
    const f32 bipolar_u8_scale = 2.0f * (1.0f / 255.0f);

    for (int note = 0; note < NOTES_IN_OCTAVE; note++) {
        OscNoteBuffers* nb = &player->osc_buffers[note];
        u16 period = s_log12_table[note];
        nb->wave_length = period;

        u16 frac_inc = 0xFF00 / period;
        u16 half_period = period >> 1;
        u16 quarter_period = half_period >> 1;

        u16 acc = 0;

        // a2 in asm points into tri_waves, starting at offset (period - quarter_period)
        // and wrapping back by period when pos == quarter_period
        int tri_pos = (int)period - (int)quarter_period;

        for (u16 pos = 0; pos < period; pos++) {
            u8 frac = (u8)(acc >> 8);

            // Sawtooth: map 0..255 -> +1.0..-1.0
            nb->saw_waves[pos] = 1.0f - (f32)frac * bipolar_u8_scale;

            // Doubled frac for triangle/square
            u8 doubled = (u8)(frac << 1); // add.b d2,d2 wraps at 8 bits

            // Wrap tri_pos when pos == quarter_period
            if (pos == quarter_period) {
                tri_pos -= period;
            }

            // cmpa.w d0,a3; ble.s .otherhalf — 68k sign-extends d0.w to 32-bit for comparison
            // When acc > 0x7FFF it becomes negative, so pos (always positive) is greater
            if ((i16)pos > (i16)acc) {
                // First half: triangle ramp up, square low
                nb->tri_waves[tri_pos] = 1.0f - (f32)doubled * bipolar_u8_scale;
                nb->sqr_waves[pos] = -1.0f;
            } else {
                // Second half: triangle ramp down, square high (or low at midpoint)
                nb->tri_waves[tri_pos] = -1.0f + (f32)doubled * bipolar_u8_scale;
                nb->sqr_waves[pos] = (pos == half_period) ? -1.0f : 1.0f;
            }

            tri_pos++;
            acc += frac_inc;
        }
    }
}

// Noise generator
// Matches raspberry_casket.asm:1314-1437

static void gen_noise(PlayerState* player, const WaveInfo* wi, i16 octave, i16 basenote, i32 d2_pitch_ramp,
                      bool pitch_linear) {
    i32 base_speed = 0x8000; // d5 — constant base speed, never modified after init

    if (octave < 0) {
        // Negative octave noise speed calculation
        i16 neg_note = -basenote;
        u16 abs_note = (u16)neg_note;

        // Calculate shift amount from division
        u16 div_result = abs_note / NOTES_IN_OCTAVE;
        div_result++;
        u16 shifted = (u16)(0x8000 >> div_result);
        u16 step_per_note = shifted / NOTES_IN_OCTAVE;

        // Cheap mod12: subtract 12 until negative, then negate
        i16 mod_val = (i16)abs_note;
        do {
            mod_val -= NOTES_IN_OCTAVE;
        } while (mod_val >= 0);
        mod_val = -mod_val;

        base_speed = (i32)shifted + (i32)((u16)mod_val * step_per_note);
    }

    u16 noise_seed = (u16)(wi->osc_phase_min + wi->chord_shift + 1);
    // Keep noise gain continuous instead of reproducing the reference player's
    // per-sample asr #7 floor. That quantization adds a negative half-LSB bias
    // and collapses quiet noise to a small number of amplitude levels.
    const f32 signed_byte_scale = 1.0f / 128.0f;
    const f32 noise_gain_f = (f32)wi->osc_gain * signed_byte_scale;

    f32* out = player->wg_curr_sample_ptr;
    i32 noise_speed = base_speed; // a1 — current speed, updated by ramp
    i32 noise_acc = 0x8000;       // a5 — single variable for symmetry AND accumulation
    i32 noise_ramp_acc = 0;       // d6

    for (;;) {
        // PRNG (xorshift)
        u16 ns = noise_seed;
        ns ^= (ns << 13);
        ns ^= (ns >> 9);
        ns ^= (ns << 7);
        noise_seed = ns;

        // Symmetry oscillation on noise_acc (a5)
        if (noise_acc != 0x8000) {
            i32 d4 = noise_acc + (i32)0xFFFF8000;
            i32 d1 = noise_acc + (i32)0xFFFF7FFF;
            d1 &= ~0x7FFF; // andi.w #$8000 — preserves high word, clears bits 0-14
            noise_acc = d4 - d1;
        }

        // Apply gain to noise sample
        f32 noise_sample = (f32)(i8)(ns & 0xFF) * signed_byte_scale;
        f32 gained = noise_gain_f * noise_sample;
        f32 out_val = pretracker_clamp_sample(*out + gained);

        // Inner loop
        for (;;) {
            *out++ = out_val;
            noise_acc += noise_speed; // adda.l a1,a5

            if (d2_pitch_ramp != 0) {
                noise_ramp_acc += d2_pitch_ramp;
                i32 ramp_adj = noise_ramp_acc >> 10;
                i32 new_speed = base_speed + ramp_adj; // d5 + ramp_adj (constant base)

                if (pitch_linear) {
                    i32 decay = d2_pitch_ramp >> 7;
                    d2_pitch_ramp -= decay;
                }

                noise_speed = new_speed;

                if (new_speed <= 0x1FF) { // cmpa.w #$1FF,a1; bgt skips clamp
                    d2_pitch_ramp = 0;
                    noise_acc = 0;
                    noise_speed = 0x200;
                }
            }

            if (out >= player->wg_curr_samend_ptr) {
                return;
            }
            if ((i32)noise_acc > 0x7FFF) { // cmpa.w #$7FFF,a5; ble continues
                break;
            }
        }
    }
}

// Tonal oscillator generator (saw/tri/sqr with chord and pitch ramp)
// Matches raspberry_casket.asm:1439-1525

static void gen_tonal(PlayerState* player, const WaveInfo* wi, f32* osc_buf, i32 d6_step, i32 d7_period,
                      i32 phase_min, i32 a5_limit, i32 phase_speed, i32 d2_pitch_ramp, bool pitch_linear) {

    // Unisono speed adjustment
    i32 osc_speed = d6_step;
    if (player->wg_unisono_run) {
        u8 unisono_bits = (wi->mod_density >> MOD_UNISONO_SHIFT) & MOD_UNISONO_MASK;
        i32 detune_shift = 9 - unisono_bits;
        osc_speed = d6_step + (d6_step >> detune_shift);
    }

    f32* out = player->wg_curr_sample_ptr;
    i32 a1_phase = phase_min; // current phase modulation value
    i32 a2_ramp_acc = 0;      // pitch ramp accumulator
    u32 osc_pos = 0;          // will be set from chord position calc

    // Calculate initial position (done by caller, stored in d0_pos)
    // The caller passes d2_pitch_ramp for the pitch ramp increment
    u32 chord_shift_val = (u32)wi->chord_shift;
    u16 chord_factor = player->wg_chord_flag + player->wg_chord_note_num;
    chord_shift_val *= chord_factor;
    u32 phase_offset = (u32)wi->osc_phase_min + chord_shift_val;
    u32 raw_pos = ((u32)d6_step >> 4) * (phase_offset << 4);

    // Wrap within period
    u32 d7u = (u32)d7_period;
    while (raw_pos > d7u) {
        raw_pos -= d7u;
    }
    osc_pos = raw_pos;

    f32 gain_f = (f32)wi->osc_gain / 128.0f;
    i32 local_speed = osc_speed;
    i32 local_phase_speed = phase_speed;
    i32 local_ramp_inc = d2_pitch_ramp;

    for (;;) {
        // Fetch oscillator sample
        u32 sample_offset = osc_pos - (u32)a1_phase;
        i32 sample_idx = (sample_offset & 0x80000000u) ? 0 : (i32)(sample_offset >> 15);
        i32 wave_length = d7_period >> 15;
        if (sample_idx >= wave_length) {
            sample_idx = wave_length - 1;
        }
        f32 osc_sample = osc_buf[sample_idx];

        // Apply gain and mix with existing sample
        *out = pretracker_clamp_sample(*out + gain_f * osc_sample);
        out++;

        // Advance position
        osc_pos += (u32)local_speed;
        if (osc_pos < 0x80000000u && osc_pos >= (u32)d7_period) {
            osc_pos -= (u32)d7_period;

            // Phase oscillation
            a1_phase += local_phase_speed;
            if (a1_phase >= a5_limit) {
                local_phase_speed = -local_phase_speed;
                a1_phase = a5_limit;
            }
            if (a1_phase <= player->wg_osc_speed) {
                local_phase_speed = -local_phase_speed;
                a1_phase = player->wg_osc_speed;
            }
        }

        // Pitch ramp
        if (local_ramp_inc != 0) {
            a2_ramp_acc += local_ramp_inc;
            i32 ramp_adj = a2_ramp_acc >> 10;
            local_speed = d6_step + ramp_adj;

            if (pitch_linear) {
                i32 decay = local_ramp_inc >> 7;
                local_ramp_inc -= decay;
            }

            if ((u32)local_speed >= (u32)d7_period) {
                local_ramp_inc = 0;
                local_speed = 0;
            }
        }

        if (out >= player->wg_curr_samend_ptr) {
            break;
        }
    }
}

// Filter coefficient calculation per 64-byte chunk
// Handles boundary clamping, direction reversal, and normal interpolation.
// Matches the coefficient logic in raspberry_casket.asm:1542-1640

static i32 calc_filter_coeff(i32 flt_pos, i32* flt_speed, i32* next_pos, i32 flt_min, i32 flt_max,
                             const WaveInfo* wi) {
    if (*flt_speed > 0) {
        // Boundary clamp: position past max and crossing the absolute ceiling
        if (flt_pos > flt_max && flt_pos <= 0xFF00 && *next_pos > 0xFEFF) {
            *flt_speed = -*flt_speed;
            *next_pos = 0xFF00;
            return 0;
        }
        // Reached max: position hasn't passed max yet, but next_pos crosses it
        if (flt_pos <= flt_max && *next_pos >= flt_max) {
            if (flt_min == flt_max) {
                *next_pos = flt_min;
            } else {
                *flt_speed = -*flt_speed;
                *next_pos = flt_max;
            }
            return (u8)(~(u8)wi->flt_max);
        }
    } else {
        // Boundary clamp: position below min AND next_pos crosses zero
        if (flt_pos < flt_min && flt_pos >= 0 && *next_pos <= 0) {
            *flt_speed = -*flt_speed;
            *next_pos = 0;
            return 255;
        }
        // Reached min: position hasn't passed min yet, but next_pos crosses it
        if (flt_pos >= flt_min && *next_pos <= flt_min) {
            *next_pos = flt_min;
            if (flt_min != flt_max) {
                *flt_speed = -*flt_speed;
            }
            return (u8)(~(u8)wi->flt_min);
        }
    }

    // Normal case: coefficient from interpolated position
    return (u8)(~(u8)(*next_pos >> 8));
}

// The reference floors every coefficient product to a signed sample LSB. Keep
// the taps in normalized floating point, but retain that audible deadband.
static inline f32 floor_filter_lsb(f32 value) {
    return floorf(value * 128.0f) / 128.0f;
}

// Filter processing
// Matches raspberry_casket.asm:1542-1767

static void gen_filter(PlayerState* player, const WaveInfo* wi) {
    u8 flt_type = wi->flt_type;
    if (flt_type == 0 || player->wg_curr_sample_len == 0) {
        return;
    }

    f32 taps[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    i32 flt_pos = (i32)wi->flt_start << 8;
    i32 flt_min = (i32)wi->flt_min << 8;
    i32 flt_max = (i32)wi->flt_max << 8;
    i32 flt_speed = (i32)(i8)wi->flt_speed << 7;

    f32* out = player->wg_curr_sample_ptr;

    while (out < player->wg_curr_samend_ptr) {
        i32 next_pos = flt_pos + flt_speed;
        i32 d2 = calc_filter_coeff(flt_pos, &flt_speed, &next_pos, flt_min, flt_max, wi);

        // Adjust for highpass/notch (types 2 and 4)
        if (!(flt_type & 1)) {
            d2 = 255 - d2;
        }
        i32 d0 = d2 * 2;

        // Resonance
        i16 d7_resonance = wi->flt_resonance;
        if (d7_resonance != 0) {
            i16 res_divisor = (0xB6 / 2 - d7_resonance) * 2;
            if (res_divisor < 0x36) {
                res_divisor = 0x36;
            }
            d0 = d2 + (d2 * 256) / res_divisor;
        }

        f32 d2_coeff = (f32)d2 / 256.0f;
        f32 d0_coeff = (f32)d0 / 256.0f;

        // Process chunk (64 samples at Amiga rate, scaled for HQ)
        f32* chunk_end = out + (64 * HQ_MAX_PERIOD / AMIGA_MAX_PERIOD);
        if (chunk_end > player->wg_curr_samend_ptr) {
            chunk_end = player->wg_curr_samend_ptr;
        }

        while (out < chunk_end) {
            f32 input = *out;

            f32 d7 = taps[0] - taps[1];
            d7 = floor_filter_lsb(d7 * d0_coeff);
            d7 -= taps[0];
            d7 += input;
            d7 = floor_filter_lsb(d7 * d2_coeff);
            taps[0] += d7;

            for (int tap = 1; tap < 4; tap++) {
                d7 = taps[tap - 1] - taps[tap];
                d7 = floor_filter_lsb(d7 * d2_coeff);
                taps[tap] += d7;
            }

            d7 = taps[3];

            // Apply filter type
            switch (flt_type) {
                case FILTER_LOWPASS: // d7 is already taps[3]
                    break;
                case FILTER_HIGHPASS:
                    d7 -= input;
                    break;
                case FILTER_BANDPASS:
                    d7 -= taps[0];
                    d7 -= taps[1];
                    d7 -= taps[2];
                    d7 = floor_filter_lsb(d7 * 0.5f);
                    break;
                case FILTER_NOTCH:
                    d7 -= taps[0];
                    d7 = -d7;
                    break;
            }

            *out++ = pretracker_clamp_sample(d7);
        }

        flt_pos = next_pos;
    }
}

// Modulator (chorus/delay effect)
// Matches pre_Modulator at raspberry_casket.asm:2204-2288

static void gen_modulator(PlayerState* player, const WaveInfo* wi) {
    if (wi->mod_wetness == 0) {
        return;
    }
    u8 density = wi->mod_density & MOD_DENSITY_MASK;
    if (density == 0) {
        return;
    }

    f32* buf = player->wg_curr_sample_ptr;
    u16 sample_len = player->wg_curr_sample_len;
    f32 wetness_f = (f32)wi->mod_wetness / 256.0f;
    bool is_post = (wi->mod_density & MOD_POST_FLAG) != 0;

    for (u16 run = 0; run < density; run++) {
        // Calculate delay length from modulator ramp table, scaled for HQ rate
        u32 delay_len = (u32)wi->mod_length * s_modulator_ramp_8[run];

        u32 predelay = wi->mod_predelay;
        if (is_post) {
            predelay <<= 8;
        } else {
            delay_len >>= 2;
            predelay <<= 6;
        }
        // Scale delay and predelay by HQ_MAX_PERIOD/AMIGA_MAX_PERIOD for higher sample rate
        delay_len = delay_len * HQ_MAX_PERIOD / AMIGA_MAX_PERIOD;
        predelay = predelay * HQ_MAX_PERIOD / AMIGA_MAX_PERIOD;
        delay_len += predelay;

        u16 d3_acc = 0;

        for (u16 pos = 0; pos < sample_len; pos++) {
            // Scale sweep rate down for HQ: more samples per unit time = less increment per sample
            d3_acc += (u16)((u32)(8 + run) * AMIGA_MAX_PERIOD / HQ_MAX_PERIOD);

            u16 table_idx = (d3_acc >> 11) & 0x1F;
            u32 d1 = s_ramp_up_down_32[table_idx];
            d1 += delay_len;
            d1 >>= 6;

            i16 offset = (i16)pos - (i16)d1;
            if (offset < 0) {
                continue;
            }

            f32 delayed = buf[offset];
            if (run & 1) {
                delayed = -delayed;
            }

            buf[pos] = pretracker_clamp_sample(buf[pos] + wetness_f * delayed);
        }
    }
}

// Volume envelope - attack phase
// Returns true if processing should continue to the delay phase, false if done.
// Matches raspberry_casket.asm:1786-1870

static bool gen_vol_attack(f32** out_ptr, i32* remaining, const WaveInfo* wi, bool boost, bool vol_fast) {
    u8 attack_val = wi->vol_attack;
    if (attack_val == 0) {
        return true;
    }

    i32 vol = 0;
    i32 vol_inc;
    if (attack_val == 1) {
        vol_inc = 0x00020000;
    } else if (attack_val == 2) {
        vol_inc = 0x00010000;
    } else {
        vol_inc = 0x20000 / attack_val;
    }

    if (vol_fast) {
        vol_inc <<= 4;
    }

    // Scale vol_inc for HQ rate so attack duration in musical time stays the same
    vol_inc = (i32)((int64_t)vol_inc * AMIGA_MAX_PERIOD / HQ_MAX_PERIOD);

    vol += vol_inc;
    if (vol > 0xFFFFFF) {
        return true;
    }

    f32* out = *out_ptr;

    // Attack loop
    while (*remaining >= 0) {
        f32 vol_factor = (f32)(vol >> 16) / (boost ? 64.0f : 256.0f);
        *out = pretracker_clamp_sample(*out * vol_factor);
        out++;

        (*remaining)--;
        if (*remaining < 0) {
            *out_ptr = out;
            return false;
        }
        vol += vol_inc;
        if (vol > 0xFFFFFF) {
            break;
        }
    }

    *out_ptr = out;
    return true;
}

// Volume envelope - delay phase
// Returns true if processing should continue to the decay phase, false if done.
// Matches raspberry_casket.asm:1871-1944

static bool gen_vol_delay(f32** out_ptr, i32* remaining, const WaveInfo* wi, bool boost) {
    u16 delay_len = (u16)(((u32)wi->vol_delay << 4) * HQ_MAX_PERIOD / AMIGA_MAX_PERIOD);
    f32* out = *out_ptr;

    if (boost) {
        // Boosted delay: multiply by 4.0 (was two saturating double operations)
        f32* delay_end = out + delay_len + 2;
        for (;;) {
            if (*remaining < 0) {
                *out_ptr = out;
                return false;
            }
            *out = pretracker_clamp_sample(*out * 4.0f);
            out++;
            if (out >= delay_end) {
                break;
            }
            (*remaining)--;
            if (*remaining < 0) {
                *out_ptr = out;
                return false;
            }
        }
    } else {
        // Normal delay: skip samples (pass through unchanged)
        i16 skip = (i16)(delay_len + 1);
        *remaining -= skip;
        if (*remaining < 0) {
            *out_ptr = out;
            return false;
        }
        out += skip + 1;
    }

    (*remaining)--;

    *out_ptr = out;
    return true;
}

// Volume envelope - decay phase
// Returns true if processing should continue to the sustain phase, false if done.
// Matches raspberry_casket.asm:1945-2038

static bool gen_vol_decay(f32** out_ptr, i32* remaining, const WaveInfo* wi, bool boost, bool vol_fast) {
    u8 decay_val = wi->vol_decay;
    if (decay_val == 0) {
        return true;
    }

    i32 d7_inc = (i32)decay_val;
    d7_inc = (d7_inc * d7_inc) / 4 + decay_val; // (d3^2)/4 + d3
    // Scale for HQ rate: more samples per unit time = less position advance per sample
    d7_inc = d7_inc * AMIGA_MAX_PERIOD / HQ_MAX_PERIOD;

    i32 d3_pos;
    if (vol_fast) {
        d3_pos = 0;
    } else {
        d3_pos = (i32)decay_val << 12; // lsl.w #8; lsl.l #4
    }

    u16 table_idx = (u16)(d3_pos >> 16);
    const i16* upper_bound = &s_roll_off_table[table_idx];
    i16 lower_bound = *upper_bound++;

    i32 vol_dec = 0;
    u16 volume = 0xFFFF;

    f32* out = *out_ptr;

    while (*remaining >= 0) {
        d3_pos += d7_inc;
        u16 new_idx = (u16)(d3_pos >> 16);

        if (new_idx <= 0x8E) {
            if (new_idx > table_idx) {
                lower_bound = s_roll_off_table[new_idx];
                upper_bound = &s_roll_off_table[1 + new_idx];
                table_idx = new_idx;
            }

            i16 ub_val = *upper_bound;
            i16 delta = ub_val - lower_bound;
            vol_dec = (i32)lower_bound;
            if (delta != 0) {
                u16 frac = (u16)(d3_pos & 0xFFFF) >> 8;
                vol_dec = (i32)(((i32)delta * frac) >> 8) + lower_bound;
            }
        }

        if (volume <= (u16)vol_dec) {
            break;
        }
        volume -= (u16)vol_dec;

        u8 vol8 = (u8)(volume >> 8);
        if (vol8 <= wi->vol_sustain) {
            break;
        }

        f32 vol_factor = (f32)vol8 / (boost ? 64.0f : 256.0f);
        *out = pretracker_clamp_sample(*out * vol_factor);
        out++;
        (*remaining)--;
    }

    *out_ptr = out;
    return *remaining >= 0;
}

// Volume envelope - sustain phase
// Matches raspberry_casket.asm:2039-2086

static void gen_vol_sustain(f32* out, i32 remaining, const WaveInfo* wi, bool boost) {
    u8 sustain = wi->vol_sustain;
    if (sustain == 0) {
        while (remaining >= 0) {
            *out++ = 0.0f;
            remaining--;
        }
        return;
    }

    f32 sustain_f = (f32)sustain / (boost ? 64.0f : 256.0f);

    // Skip if sustain is effectively 1.0 (no scaling needed)
    if (!boost && sustain == 0xFF) {
        return;
    }

    while (remaining >= 0) {
        *out = pretracker_clamp_sample(*out * sustain_f);
        out++;
        remaining--;
    }
}

// Volume envelope
// Matches raspberry_casket.asm:1786-2086

static void gen_volume_envelope(PlayerState* player, const WaveInfo* wi) {
    f32* out = player->wg_curr_sample_ptr;
    i32 remaining = (i32)player->wg_curr_sample_len;
    if (remaining == 0) {
        return;
    }
    remaining--;

    bool boost = (wi->flags & WI_FLAG_BOOST) != 0;
    bool vol_fast = (wi->flags & WI_FLAG_VOL_FAST) != 0;

    if (wi->vol_attack == 0 && wi->vol_sustain == 0xFF) {
        return; // no envelope needed
    }

    if (!gen_vol_attack(&out, &remaining, wi, boost, vol_fast))
        return;

    if (!gen_vol_delay(&out, &remaining, wi, boost))
        return;

    if (!gen_vol_decay(&out, &remaining, wi, boost, vol_fast))
        return;

    gen_vol_sustain(out, remaining, wi, boost);
}

// Generate a single chord tone: compute octave, select oscillator, set up pitch ramp and phase, then render.
// Matches the per-note body of the chord loop in raspberry_casket.asm:1115-1195

static void gen_chord_tone(PlayerState* player, const WaveInfo* wi, i16 note, bool* was_tonal) {
    // Split into octave and note-within-octave
    i16 adjusted = note + NOTES_IN_OCTAVE * NOTES_IN_OCTAVE;
    i16 octave = adjusted / NOTES_IN_OCTAVE - NOTES_IN_OCTAVE;
    i16 note_in_oct = adjusted % NOTES_IN_OCTAVE;

    i16 saved_note = note;

    OscNoteBuffers* onb = &player->osc_buffers[note_in_oct];
    u8 osc_type = wi->flags & WI_FLAG_OSC_TYPE_MASK;
    f32* osc_buf = NULL;
    f32* oscillator_buffers[] = { onb->saw_waves, onb->tri_waves, onb->sqr_waves };
    if (osc_type <= OSC_TYPE_SQUARE)
        osc_buf = oscillator_buffers[osc_type];

    // d6 = 0x8000 shifted by octave
    i32 d6 = 0x8000;
    if (octave > 0) {
        d6 <<= octave;
    } else if (octave < 0) {
        d6 >>= (-octave);
    }

    // Pitch ramp
    i32 pitch_ramp_val = (i32)(i8)wi->pitch_ramp;
    bool pitch_linear = (wi->flags & WI_FLAG_PITCH_LINEAR) != 0;

    if (!pitch_linear) {
        if (pitch_ramp_val > 0) {
            pitch_ramp_val = pitch_ramp_val * pitch_ramp_val;
        }
    } else {
        if (pitch_ramp_val <= 0) {
            if (octave < 0) {
                pitch_ramp_val = 0;
            } else {
                pitch_ramp_val <<= octave;
            }
            pitch_ramp_val += pitch_ramp_val;
        } else {
            pitch_ramp_val = pitch_ramp_val * pitch_ramp_val;
        }
    }
    i32 d2_ramp = pitch_ramp_val << 10;

    i32 d7_period = (i32)onb->wave_length << 15;
    i16 phase_scale = (15 - octave) * 8;

    i32 phase_min = (i32)wi->osc_phase_min * phase_scale;
    phase_min <<= 6;
    i32 phase_max = (i32)wi->osc_phase_max * phase_scale;
    phase_max <<= 6;

    i32 phase_speed = (i32)wi->osc_phase_spd << 11;
    i32 a5_limit = phase_max;

    if (phase_max < phase_min) {
        phase_speed = -phase_speed;
        a5_limit = phase_min;
    }

    i32 osc_start = (phase_max >= phase_min) ? phase_min : phase_max;
    player->wg_osc_speed = osc_start;

    if (osc_buf == NULL) {
        gen_noise(player, wi, octave, saved_note, d2_ramp, pitch_linear);
    } else {
        *was_tonal = true;
        gen_tonal(player, wi, osc_buf, d6, d7_period, phase_min, a5_limit, phase_speed, d2_ramp, pitch_linear);
    }
}

// Generate the immutable sample buffers used during playback.
// Matches pre_PlayerInit at raspberry_casket.asm:927-2197

void pretracker_wavegen_generate(PlayerState* player) {
    SongState* song = player->my_song;
    gen_osc_buffers(player);

    // Wave generation loop
    if (song->num_waves == 0) {
        return;
    }

    for (player->wg_wave_ord_num = 0; player->wg_wave_ord_num < song->num_waves; player->wg_wave_ord_num++) {

        u8 wave_idx = song->wavegen_order_table[player->wg_wave_ord_num];
        f32* wave_buf = player->wave_sample_table[wave_idx];
        player->wg_curr_sample_ptr = wave_buf;

        u32 std_len = song->wavelength_table[wave_idx];
        player->wg_curr_sample_len = (u16)std_len;

        WaveInfo* wi = song->waveinfo_table[wave_idx];

        memset(wave_buf, 0, std_len * sizeof(f32));
        player->wg_curr_samend_ptr = wave_buf + std_len;

        // Read chord information
        u8 cn1 = wi->chord_note1;
        u8 cn2 = wi->chord_note2;
        u8 cn3 = wi->chord_note3;

        // ASM: seq d4; neg.b d4 — gives 1 when NO chords, 0 when chords exist
        u8 has_chord = (cn1 | cn2 | cn3) ? 0 : 1;
        player->wg_chord_flag = has_chord;

        u8 basenote = wi->osc_basenote;
        u8 chord_offsets[] = { 0, cn1, cn2, cn3 };
        for (int chord = 0; chord < 4; chord++)
            player->wg_chord_pitches[chord] = basenote + chord_offsets[chord];

        player->wg_chord_note_num = 0;
        player->wg_unisono_run = 0;

        bool was_tonal = false;

        for (;;) {     // Outer loop: runs chord loop, then restarts once for unisono
            for (;;) { // Chord loop
                u8 chord_idx = player->wg_chord_note_num;
                i16 note = (i16)(i8)player->wg_chord_pitches[chord_idx];

                if (chord_idx == 0 || (u8)note != basenote) {
                    gen_chord_tone(player, wi, note, &was_tonal);
                }

                player->wg_chord_note_num++;
                if (player->wg_chord_note_num >= 4) {
                    break;
                }
            }

            // Unisono check
            u8 unisono = (wi->mod_density >> MOD_UNISONO_SHIFT) & MOD_UNISONO_MASK;
            if (unisono != 0 && was_tonal && !player->wg_unisono_run) {
                // ASM: move.w #$0001,pv_wg_chord_note_num_b(a4) — word write sets
                // chord_note_num=0 (high byte) and unisono_run=1 (low byte) simultaneously
                player->wg_chord_note_num = 0;
                player->wg_unisono_run = 1;
                continue; // Restart chord loop for unisono pass
            }
            break;
        }

        // Filter
        gen_filter(player, wi);

        // Pre-modulator
        if (!(wi->mod_density & MOD_POST_FLAG)) {
            gen_modulator(player, wi);
        }

        // Volume envelope
        gen_volume_envelope(player, wi);

        // Post-modulator
        if (wi->mod_density & MOD_POST_FLAG) {
            gen_modulator(player, wi);
        }

        // Wave mixing
        SongState* sv = player->my_song;
        if (wi->mix_wave != 0) {
            u8 mix_idx = wi->mix_wave - 1;
            f32* mix_src = player->wave_sample_table[mix_idx];
            u32 mix_len = sv->wavelength_table[mix_idx];
            u16 curr_len = player->wg_curr_sample_len;
            u16 min_len = (curr_len < (u16)mix_len) ? curr_len : (u16)mix_len;

            f32* dst = player->wg_curr_sample_ptr;
            for (u16 j = 0; j < min_len; j++) {
                dst[j] = pretracker_clamp_sample(dst[j] + mix_src[j]);
            }
        }

        // Higher octaves (2:1 downsample with averaging)
        if (wi->flags & WI_FLAG_EXTRA_OCTAVES) {
            u32 base_len = ((u32)wi->sam_len + 1) * HQ_MAX_PERIOD;
            f32* src = player->wg_curr_sample_ptr;
            f32* oct_dst = src + base_len;
            u32 oct_len = (base_len * 7) / 8;
            for (u32 j = 0; j < oct_len; j++) {
                oct_dst[j] = (src[j * 2] + src[j * 2 + 1]) * 0.5f;
            }
        }
    }
}
