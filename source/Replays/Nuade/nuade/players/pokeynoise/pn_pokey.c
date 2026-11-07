// SPDX-License-Identifier: MIT
// From the POKEY registers to what Paula plays. Each POKEY channel becomes one prerecorded waveform from the
// eagleplayer's data hunk, played at a period derived from the channel's divisor, at the volume of its AUDC. Which
// waveform depends on the distortion bits of AUDC (the polynomial counters) and the range of the divisor; the divisor
// is first turned into a period in units of the 64 kHz clock (8.8 fixed point) by the clock the channel runs from.
//
// The eagleplayer has a second set of waveforms for the 5/17 bit and 17 bit distortions that it would pick when a
// configuration bit is set; its own decoding of AUDCTL never sets that bit, so it is not part of this port.

#include "pn_internal.h"

// The clock of a channel as the two bit field of the eagleplayer's decoding.
typedef enum PnClock {
    PnClock_15kHz = 0,
    PnClock_64kHz = 1,
    PnClock_1790kHz_16bit = 2,
    PnClock_1790kHz_8bit = 3,
} PnClock;

typedef struct PnChannelConfig {
    PnClock clock;
    bool linked; // the divisor is 16 bit: this channel's AUDF is the high byte of the pair
} PnChannelConfig;

// AUDCTL bits.
#define PN_AUDCTL_15KHZ 0x01u
#define PN_AUDCTL_LINK_21 0x10u // channel 2 is clocked by channel 1
#define PN_AUDCTL_LINK_43 0x08u // channel 4 is clocked by channel 3
#define PN_AUDCTL_CLOCK_1 0x40u // channel 1 from the 1.79 MHz clock
#define PN_AUDCTL_CLOCK_3 0x20u // channel 3 from the 1.79 MHz clock
#define PN_AUDCTL_POLY9 0x80u

// AUDC bits.
#define PN_AUDC_VOLUME_ONLY 0x10u
#define PN_AUDC_VOLUME_MASK 0x0Fu
#define PN_AUDC_DISTORTION_SHIFT 5

// The state the eagleplayer keeps for each channel in the unused part of the POKEY page: the period (a word at $D240 +
// 2 * channel), the volume (a word at $D250 + 2 * channel) and the distortion mode (a byte at $D260 + 2 * channel) of
// the last sound that was made. The volume is never read back.
#define PN_MEMO_PERIOD 0xD240u
#define PN_MEMO_VOLUME 0xD250u
#define PN_MEMO_MODE 0xD260u

// A note whose pitch or mode just changed is dropped when a random byte is below this threshold (of 256).
#define PN_DROP_THRESHOLD 0x50u

#define PN_PERIOD_MIN 0x7Cu
#define PN_PERIOD_MAX 0xFFFFu

// AUDxVOL for a POKEY volume of 0..15 (the result is at most 64).
#define PN_VOLUME_SCALE 0x445u

// The silent sound: the first words of the data hunk are zero.
#define PN_SILENT_PERIOD 0x100u

// The tone and polynomial families of the eagleplayer; the memo of a channel holds the number of the last one.
typedef enum PnFamily {
    PnFamily_Noise5_17 = 0,
    PnFamily_Noise5 = 1,
    PnFamily_Noise5_4 = 2,
    PnFamily_Noise17 = 4,
    PnFamily_Pure = 5,
    PnFamily_Noise4 = 6,
} PnFamily;

// Rob Hubbard's songs set AUDIOCORRECT=RH in the ".info": the sounds with a short period are muted.
#define PN_RH_MUTE_BELOW 0x2F0u

// The period of a channel in 64 kHz clocks (8.8 fixed point), and the divisor in whole master clocks, the number that
// the polynomial counters repeat on.
typedef struct PnTiming {
    uint32_t period;
    uint32_t clocks;
} PnTiming;

// The waveform sets of the 4 bit counter.
typedef enum PnPolySet {
    PnPolySet_Silent,
    PnPolySet_Common, // the remainders that are not special
    PnPolySet_Third,  // remainders 3, 6, 9 and 12
    PnPolySet_Fifth,  // remainders 5 and 10
} PnPolySet;

// The 32 bit multiplication of the original: a product that does not fit is 0.

static uint32_t multiply(uint32_t value, uint32_t factor) {
    const uint64_t product = (uint64_t)value * factor;
    return product > UINT32_MAX ? 0 : (uint32_t)product;
}

// A division by a 16 bit divisor that keeps a 16 bit quotient. A quotient that does not fit leaves the low word of the
// dividend (the 68000 does not store it and the original masks the register).

static uint32_t divide_word(uint32_t value, uint32_t divisor) {
    const uint32_t quotient = value / divisor;
    return quotient > 0xFFFFu ? (value & 0xFFFFu) : quotient;
}

static PnSound sound_of(uint32_t wave, uint16_t words, uint32_t period, uint32_t volume) {
    return (PnSound) { .wave = wave, .length_words = words, .period = (uint16_t)period, .volume = (uint8_t)volume };
}

static PnSound silence(void) {
    return sound_of(PN_SILENT_WAVE, PN_SILENT_WORDS, PN_SILENT_PERIOD, 0);
}

// The scaling steps the sounds share. `period` is in 64 kHz clocks (8.8 fixed point); the factor brings it to Paula's
// period.

static uint32_t scale_to_paula(uint32_t period, uint32_t factor) {
    return multiply(period, factor) >> 8;
}

static uint32_t clamp_high(uint32_t period) {
    return period >= 0x10000u ? PN_PERIOD_MAX : period;
}

// The two forms of the lower clamp: one compares the whole register, one only its low word.

static uint32_t clamp_low(uint32_t period) {
    return period < PN_PERIOD_MIN ? PN_PERIOD_MIN : period;
}

static uint32_t clamp_low_word(uint32_t period) {
    return (uint16_t)period < PN_PERIOD_MIN ? PN_PERIOD_MIN : period;
}

static bool bit_set(uint8_t value, uint8_t mask) {
    return (value & mask) != 0;
}

// The eagleplayer's decoding of AUDCTL into the clock and the 16 bit link of one channel. Channels 1 and 3 take the
// 1.79 MHz clock from their own bit (16 bit when linked), channels 2 and 4 from the bit of the channel before them and
// run 16 bit then. The 9 bit polynomial bit selects the 1.79 MHz clock for every channel and links it.

static PnChannelConfig channel_config(uint8_t audctl, unsigned channel) {
    const bool first_of_pair = channel == 0 || channel == 2;
    const uint8_t clock_bit = channel < 2 ? PN_AUDCTL_CLOCK_1 : PN_AUDCTL_CLOCK_3;
    const uint8_t link_bit = channel < 2 ? PN_AUDCTL_LINK_21 : PN_AUDCTL_LINK_43;
    PnChannelConfig config = { .clock = bit_set(audctl, PN_AUDCTL_15KHZ) ? PnClock_15kHz : PnClock_64kHz };
    if (bit_set(audctl, clock_bit)) {
        if (first_of_pair) {
            config.clock = bit_set(audctl, link_bit) ? PnClock_1790kHz_16bit : PnClock_1790kHz_8bit;
        } else {
            config.clock = PnClock_1790kHz_16bit;
        }
    }
    if (!first_of_pair && bit_set(audctl, link_bit)) {
        config.linked = true;
    }
    if (bit_set(audctl, PN_AUDCTL_POLY9)) {
        config.clock = (PnClock)(config.clock | PnClock_1790kHz_16bit);
        config.linked = true;
    }
    return config;
}

// The divisor of a channel as 8.8 fixed point: AUDF, or the pair of this channel's AUDF (high) and the one before it.

static uint32_t channel_divisor(const PnMachine* machine, unsigned channel, bool linked) {
    const uint16_t audf = (uint16_t)(PN_POKEY_BASE + 2 * channel);
    if (linked) {
        return ((uint32_t)machine->ram[audf] << 16) | ((uint32_t)machine->ram[(uint16_t)(audf - 2)] << 8);
    }
    return (uint32_t)machine->ram[audf] << 8;
}

// The period of the plain tones and noises (5/17 bit, 5 bit, 17 bit and pure): the divisor in 64 kHz clocks. The 1.79
// MHz clocks add 7 and 4 to the 8.8 number, not to the whole divisor.

static uint32_t plain_period(uint32_t divisor, PnClock clock) {
    switch (clock) {
        case PnClock_15kHz:
            return (multiply(divisor, 0x72) + 0x7200u) / 0x1C;
        case PnClock_64kHz:
            return divisor + 0x100u;
        case PnClock_1790kHz_16bit:
            return (divisor + 7u) / 0x1C;
        default:
            return (divisor + 4u) / 0x1C;
    }
}

// The timing of the 4 bit and 5/4 bit noises: the same period, and the divisor in whole master clocks, the number that
// the polynomial counters repeat on.

static PnTiming polynomial_timing(uint32_t divisor, PnClock clock) {
    uint32_t scaled;
    switch (clock) {
        case PnClock_15kHz:
            scaled = multiply(divisor, 0x72) + 0x7200u;
            break;
        case PnClock_64kHz:
            return (PnTiming) { .period = divisor + 0x100u, .clocks = multiply(divisor + 0x100u, 0x1C) >> 8 };
        case PnClock_1790kHz_16bit:
            scaled = divisor + 0x700u;
            break;
        default:
            scaled = divisor + 0x400u;
            break;
    }
    return (PnTiming) { .period = scaled / 0x1C, .clocks = scaled >> 8 };
}

// The memo of a channel in the 6502 memory. Returns whether the pitch or the family differs from the last sound, and
// updates the memo.

static bool update_memo(PnMachine* machine, unsigned channel, PnFamily family, uint32_t period, uint32_t volume) {
    uint8_t* ram = machine->ram;
    const uint16_t period_at = (uint16_t)(PN_MEMO_PERIOD + 2 * channel);
    const uint16_t volume_at = (uint16_t)(PN_MEMO_VOLUME + 2 * channel);
    const uint16_t mode_at = (uint16_t)(PN_MEMO_MODE + 2 * channel);
    const uint16_t last_period = (uint16_t)((ram[period_at] << 8) | ram[period_at + 1]);
    const bool retrigger = ram[mode_at] != family || last_period != (uint16_t)period;
    ram[period_at] = (uint8_t)(period >> 8);
    ram[period_at + 1] = (uint8_t)period;
    ram[volume_at] = (uint8_t)(volume >> 8);
    ram[volume_at + 1] = (uint8_t)volume;
    ram[mode_at] = (uint8_t)family;
    return retrigger;
}

// A changed note of the polynomial noises is dropped now and then.

static bool dropped(PnMachine* machine, bool retrigger) {
    return retrigger && pn_machine_random_byte(machine) < PN_DROP_THRESHOLD;
}

static uint32_t paula_volume(uint32_t volume) {
    return (volume * PN_VOLUME_SCALE) >> 8;
}

// 17 bit polynomial: one waveform for every period.

static PnSound noise_17(PnMachine* machine, unsigned channel, PnChannelConfig config, uint32_t divisor, uint32_t volume,
                        bool rh_correction) {
    uint32_t period = plain_period(divisor, config.clock);
    update_memo(machine, channel, PnFamily_Noise17, period, volume);
    if (rh_correction && period < PN_RH_MUTE_BELOW) {
        volume = 0;
    }
    period = clamp_low(clamp_high(scale_to_paula(period, 0x84)));
    return sound_of(0x778C, 0x23BF, period, paula_volume(volume));
}

// 5 bit polynomial mixed with the 17 bit one.

static PnSound noise_5_17(PnMachine* machine, unsigned channel, PnChannelConfig config, uint32_t divisor,
                          uint32_t volume) {
    const uint32_t period = plain_period(divisor, config.clock);
    update_memo(machine, channel, PnFamily_Noise5_17, period, volume);
    const uint32_t paula_period = clamp_low(clamp_high(scale_to_paula(period, 0x89)));
    return sound_of(0x277E, 0x2807, paula_period, paula_volume(volume));
}

// 5 bit polynomial: four waveforms by period range.

static PnSound noise_5(PnMachine* machine, unsigned channel, PnChannelConfig config, uint32_t divisor,
                       uint32_t volume) {
    const uint32_t period = plain_period(divisor, config.clock);
    update_memo(machine, channel, PnFamily_Noise5, period, volume);
    const uint32_t level = paula_volume(volume);
    if (period <= 0x280) {
        return sound_of(0x1A4, 0xB, clamp_low_word(scale_to_paula(period, 0x9E)), level);
    }
    if (period <= 0xA00) {
        return sound_of(0x30, 0x1F, scale_to_paula(divide_word(period * 2, 5), 0x8C), level);
    }
    if (period <= 0x2000) {
        return sound_of(0x6E, 0x3E, scale_to_paula(divide_word(period, 5), 0x8C), level);
    }
    return sound_of(0xEA, 0x5D, scale_to_paula(clamp_high((period * 2) / 0xD), 0x8C), level);
}

// Pure tone: four waveforms by period range, the first is the shortest.

static PnSound pure_tone(PnMachine* machine, unsigned channel, PnChannelConfig config, uint32_t divisor,
                         uint32_t volume, bool rh_correction) {
    const uint32_t period = plain_period(divisor, config.clock);
    update_memo(machine, channel, PnFamily_Pure, period, volume);
    if (rh_correction && period < PN_RH_MUTE_BELOW) {
        volume = 0;
    }
    const uint32_t level = paula_volume(volume);
    if (period <= 0x500) {
        return sound_of(0x8, 0x1, scale_to_paula(period, 0x38), level);
    }
    if (period <= 0xC00) {
        return sound_of(0xA, 0x2, scale_to_paula(divide_word(period, 5), 0x8C), level);
    }
    if (period <= 0xC000) {
        return sound_of(0xE, 0x4, scale_to_paula(divide_word(period, 0xA), 0x8C), level);
    }
    return sound_of(0x16, 0xD, scale_to_paula(clamp_high((period * 2) / 0x41), 0x8C), level);
}

// The waveform set of the 4 bit counter is picked by the remainder of the divisor in clocks modulo 15 (its length).

static PnPolySet poly4_set(uint32_t clocks) {
    const uint32_t remainder = clocks % 15;
    if (remainder == 0) {
        return PnPolySet_Silent;
    }
    if (remainder == 3 || remainder == 6 || remainder == 9 || remainder == 12) {
        return PnPolySet_Third;
    }
    if (remainder == 5 || remainder == 10) {
        return PnPolySet_Fifth;
    }
    return PnPolySet_Common;
}

// 4 bit polynomial.

static PnSound noise_4(PnMachine* machine, unsigned channel, PnChannelConfig config, uint32_t divisor,
                       uint32_t volume) {
    const PnTiming timing = polynomial_timing(divisor, config.clock);
    const uint32_t period = timing.period;
    const bool retrigger = update_memo(machine, channel, PnFamily_Noise4, period, volume);
    const uint32_t level = paula_volume(volume);
    switch (poly4_set(timing.clocks)) {
        case PnPolySet_Silent:
            return silence();
        case PnPolySet_Common:
            if (period <= 0x280) {
                return sound_of(0x1BA, 0x3, clamp_low_word(scale_to_paula(period, 0x8C)), level);
            }
            if (period <= 0xA00) {
                return sound_of(0x1C0, 0xF, scale_to_paula(divide_word(period * 2, 5), 0x8C), level);
            }
            if (period <= 0x1800) {
                return sound_of(0x1DE, 0xF, scale_to_paula(divide_word(period, 5), 0x8C), level);
            }
            if (period <= 0x3800) {
                return sound_of(0x1FC, 0x1E, scale_to_paula(divide_word(period, 0xA), 0x8C), level);
            }
            return sound_of(0x238, 0x69, clamp_high(scale_to_paula(period / 0x23, 0x8C)), level);
        case PnPolySet_Fifth:
            if (dropped(machine, retrigger)) {
                return silence();
            }
            if (period <= 0xA00) {
                return sound_of(0x30A, 0x3, clamp_low_word(scale_to_paula(divide_word(period * 2, 5), 0x8C)), level);
            }
            if (period <= 0x2000) {
                return sound_of(0x310, 0x3, scale_to_paula(divide_word(period, 5), 0x8C), level);
            }
            return sound_of(0x316, 0x6, clamp_high(scale_to_paula(period / 0xA, 0x8C)), level);
        default: // PnPolySet_Third
            if (period <= 0xA00) {
                return sound_of(0x322, 0x5, clamp_low_word(scale_to_paula(divide_word(period * 2, 5), 0x8C)), level);
            }
            if (period <= 0x2000) {
                return sound_of(0x32C, 0x5, scale_to_paula(divide_word(period, 5), 0x8C), level);
            }
            return sound_of(0x336, 0xA, clamp_high(scale_to_paula(period / 0xA, 0x8C)), level);
    }
}

// 5 bit polynomial mixed with the 4 bit one: the divisor first decides whether the 5 bit counter (31) lines up.

static PnSound noise_5_4(PnMachine* machine, unsigned channel, PnChannelConfig config, uint32_t divisor,
                         uint32_t volume) {
    const PnTiming timing = polynomial_timing(divisor, config.clock);
    const uint32_t period = timing.period;
    const bool retrigger = update_memo(machine, channel, PnFamily_Noise5_4, period, volume);
    const uint32_t level = paula_volume(volume);
    if (timing.clocks % 31 == 0) {
        if (dropped(machine, retrigger)) {
            return silence();
        }
        if (period <= 0x280) {
            return sound_of(0x1136, 0x5, clamp_low_word(scale_to_paula(period, 0x8C)), level);
        }
        return sound_of(0x1140, 0xA, clamp_high(scale_to_paula(period / 0xA, 0x8C)), level);
    }
    switch (poly4_set(timing.clocks)) {
        case PnPolySet_Silent:
            return silence();
        case PnPolySet_Common:
            if (period <= 0x280) {
                return sound_of(0x390, 0x5D, clamp_low_word(scale_to_paula(period, 0x8C)), level);
            }
            if (period <= 0xA00) {
                return sound_of(0x44A, 0x1D1, scale_to_paula(divide_word(period * 2, 5), 0x8C), level);
            }
            return sound_of(0x7EC, 0x1D1, clamp_high(scale_to_paula(period / 5, 0x8C)), level);
        case PnPolySet_Fifth:
            if (dropped(machine, retrigger)) {
                return silence();
            }
            if (period <= 0xA00) {
                return sound_of(0xB8E, 0x5D, clamp_low_word(scale_to_paula(divide_word(period * 2, 5), 0x8C)), level);
            }
            if (period <= 0x2000) {
                return sound_of(0xC48, 0x5D, scale_to_paula(divide_word(period, 5), 0x8C), level);
            }
            return sound_of(0xD02, 0xBA, clamp_high(scale_to_paula(period / 0xA, 0x8C)), level);
        default: // PnPolySet_Third
            if (period <= 0x280) {
                const uint32_t stretched = multiply(period, 5) / 7;
                return sound_of(0xE76, 0x2A, clamp_low_word(scale_to_paula(stretched, 0x90)), level);
            }
            if (period <= 0xA00) {
                return sound_of(0xECA, 0x9B, clamp_low_word(scale_to_paula(divide_word(period * 2, 5), 0x8C)), level);
            }
            return sound_of(0x1000, 0x9B, clamp_high(scale_to_paula(period / 5, 0x8C)), level);
    }
}

PnSound pn_pokey_sound(PnMachine* machine, unsigned channel, bool rh_correction) {
    const uint8_t audctl = machine->ram[PN_POKEY_BASE + PN_POKEY_AUDCTL];
    const uint8_t audc = machine->ram[PN_POKEY_BASE + 2 * channel + 1];
    if (bit_set(audc, PN_AUDC_VOLUME_ONLY)) {
        return silence();
    }
    const uint32_t volume = audc & PN_AUDC_VOLUME_MASK;
    const PnChannelConfig config = channel_config(audctl, channel);
    const uint32_t divisor = channel_divisor(machine, channel, config.linked);
    switch (audc >> PN_AUDC_DISTORTION_SHIFT) {
        case 0:
            return noise_5_17(machine, channel, config, divisor, volume);
        case 1:
        case 3:
            return noise_5(machine, channel, config, divisor, volume);
        case 2:
            return noise_5_4(machine, channel, config, divisor, volume);
        case 4:
            return noise_17(machine, channel, config, divisor, volume, rh_correction);
        case 6:
            return noise_4(machine, channel, config, divisor, volume);
        default: // 5 and 7
            return pure_tone(machine, channel, config, divisor, volume, rh_correction);
    }
}
