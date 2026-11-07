// SPDX-License-Identifier: MIT
// AProSys player: the wave generator of the synthesised voices.
//
// A synthesised voice has a sample record whose first long picks a generator (a pulse or ramp shape, a copy of another
// sample, or a combination of two) and up to two further records that filter the result. The player builds the wave
// into one of the voice's two buffers every tick. The arithmetic is the 68k's (byte and word wrap-around).

#include "aprosys_internal.h"

#define FILTER_PASSES 4 // the comb filter runs four passes over the wave

static int8_t wave_get(const ApsWaveJob* job, int32_t index) {
    const int64_t at = (int64_t)job->dst + index;
    return at >= 0 && at < APS_WAVE_BYTES ? job->memory[at] : 0;
}

static void wave_put(const ApsWaveJob* job, int32_t index, int8_t value) {
    const int64_t at = (int64_t)job->dst + index;
    if (at >= 0 && at < APS_WAVE_BYTES) {
        job->memory[at] = value;
    }
}

static int8_t source_get(const ApsWaveJob* job, uint32_t address) {
    return (int8_t)aps_u8(job->module, address);
}

int16_t aps_divs16(int32_t dividend, int16_t divisor) {
    const int32_t quotient = dividend / divisor;
    if (quotient > INT16_MAX || quotient < INT16_MIN) {
        return (int16_t)(uint16_t)dividend;
    }
    return (int16_t)quotient;
}

int32_t aps_window_offset(const ApsSampleRecord* record, const int16_t levels[APS_ENVELOPES]) {
    switch (record->kind) {
        case ApsKind_Window: {
            const int32_t first = ((int32_t)levels[record->env_a] * record->amount_a) >> 8;
            const int32_t second = ((int32_t)(int16_t)first * levels[record->env_b]) >> 16;
            return (int32_t)(int16_t)second * record->amount_b;
        }
        case ApsKind_Window2: {
            const int32_t first = ((int32_t)levels[record->env_a] * record->amount_a) >> 8;
            return (int32_t)record->amount_b * (int16_t)first;
        }
        default:
            return 0;
    }
}

// The address of the sample a source record names, with its window moved by the envelopes
static uint32_t source_address(const ApsWaveJob* job, uint32_t record) {
    const ApsSampleRecord source = aps_sample_record(job->module, record);
    const uint32_t words = source.address + (uint32_t)aps_window_offset(&source, job->levels);
    return job->module->sample_data + words * 2;
}

// Generators

// Mode 0: a pulse, eight flat segments, or a ramp, from the two envelope-scaled amounts
static void generate_shape(const ApsWaveJob* job, const ApsSampleRecord* record, uint32_t size) {
    const int16_t amount_a = (int16_t)(((int32_t)job->levels[record->env_a] * record->amount_a) >> 8);
    const int16_t amount_b = (int16_t)(((int32_t)job->levels[record->env_b] * record->amount_b) >> 8);
    const int16_t sum = (int16_t)(uint16_t)(amount_a + amount_b);

    switch (record->op) {
        case 0: {
            // A pulse: the high part is (a + b) / 16 bytes, left alone when that is not shorter than the wave
            const uint16_t high = (uint16_t)(sum >> 4);
            if (high < size) {
                for (uint32_t i = 0; i < size; i++) {
                    wave_put(job, (int32_t)i, i < high ? 0x64 : (int8_t)0x9c);
                }
            }
            break;
        }
        case 1: {
            const uint8_t x = (uint8_t)(amount_a >> 4);
            const uint8_t y = (uint8_t)(amount_b >> 4);
            const uint8_t z = (uint8_t)(0x7f - x - y);
            const uint8_t segments[8]
                = { (uint8_t)(x + z + y), (uint8_t)(x + z - y), (uint8_t)(x - z + y), (uint8_t)(x - z - y),
                    (uint8_t)(y - x + z), (uint8_t)(z - x - y), (uint8_t)(y - x - z), (uint8_t)(-x - z - y) };
            const uint32_t length = size >> 3;
            for (uint32_t i = 0; i < 8 * length; i++) {
                wave_put(job, (int32_t)i, (int8_t)segments[i / length]);
            }
            break;
        }
        case 2: {
            const int16_t slope = (int16_t)(sum >> 4);
            for (uint32_t i = 0; i < size; i++) {
                wave_put(job, (int32_t)i, (int8_t)(uint8_t)(slope * (int32_t)(size - 1 - i)));
            }
            break;
        }
        default:
            break;
    }
}

// One decimation pass of mode 1 / op 4: out[i] = x[i] / 2 plus the even samples of x, halved over the first half of
// out and quartered over the second
static void decimate(const int8_t* x, int8_t* out, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        out[i] = (int8_t)(x[i] >> 1);
    }
    const uint32_t half = size / 2;
    for (uint32_t i = 0; i < half; i++) {
        out[i] = (int8_t)(out[i] + (x[2 * i] >> 1));
    }
    for (uint32_t i = 0; i < half; i++) {
        out[half + i] = (int8_t)(out[half + i] + (x[2 * i] >> 2));
    }
}

// Mode 1: the second record's sample, copied into the wave or mixed with what is in it
static void generate_copy(const ApsWaveJob* job, const ApsSampleRecord* record, uint32_t size) {
    const ApsSampleRecord source_record = aps_sample_record(job->module, job->record + APS_SAMPLE_RECORD_SIZE);
    if (source_record.kind == ApsKind_Once && !job->fresh) {
        return; // a static sample is copied once, with the note
    }
    const uint32_t source = source_address(job, job->record + APS_SAMPLE_RECORD_SIZE);

    switch (record->op) {
        case 0:
            for (uint32_t i = 0; i < size; i++) {
                wave_put(job, (int32_t)i, source_get(job, source + i));
            }
            break;
        case 1:
            for (uint32_t i = 0; i < size; i++) {
                wave_put(job, (int32_t)i, (int8_t)((wave_get(job, (int32_t)i) >> 1) + source_get(job, source + i)));
            }
            break;
        case 2:
        case 3: {
            // A feedback loop through the wave itself: each sample adds half of the one `lag` before it, the first
            // ones the end of the wave as it was
            const uint32_t lag = record->op - 1;
            if (size < lag + 1) {
                break;
            }
            int8_t previous[2] = { wave_get(job, (int32_t)(size - lag)), wave_get(job, (int32_t)(size - 1)) };
            for (uint32_t i = 0; i < size; i++) {
                const int8_t feedback = i < lag ? previous[i + 2 - lag] : wave_get(job, (int32_t)(i - lag));
                wave_put(job, (int32_t)i, (int8_t)((feedback >> 1) + source_get(job, source + i)));
            }
            break;
        }
        case 4: {
            int8_t input[APS_WAVE_BUFFER_BYTES];
            int8_t first[APS_WAVE_BUFFER_BYTES];
            int8_t second[APS_WAVE_BUFFER_BYTES];
            int8_t third[APS_WAVE_BUFFER_BYTES];
            for (uint32_t i = 0; i < size; i++) {
                input[i] = source_get(job, source + i);
            }
            decimate(input, first, size);
            decimate(first, second, size);
            decimate(second, third, size);
            for (uint32_t i = 0; i < size; i++) {
                wave_put(job, (int32_t)i, third[i]);
            }
            break;
        }
        default:
            break;
    }
}

// Mode 2: two sample records combined sample by sample. Ops 0, 8 and 10..15 of the 68k code read a pointer the code
// never sets; no module uses them and they do nothing here.
static void generate_combine(const ApsWaveJob* job, const ApsSampleRecord* record, uint32_t size) {
    const uint32_t first = source_address(job, job->record + APS_SAMPLE_RECORD_SIZE);
    const uint32_t second = source_address(job, job->record + 2 * APS_SAMPLE_RECORD_SIZE);
    int32_t walk = 0; // op 7: the running sum of the first sample picks from the second

    for (uint32_t i = 0; i < size; i++) {
        const int8_t a = source_get(job, first + i);
        const int8_t b = source_get(job, second + i);
        const int8_t current = wave_get(job, (int32_t)i);
        int8_t result = current;
        switch (record->op) {
            case 1:
                result = (int8_t)((a + b) >> 1);
                break;
            case 2:
                result = (int8_t)(((int16_t)a * b) >> 6);
                break;
            case 3:
                result = (int8_t)(a & b);
                break;
            case 4:
                result = (int8_t)(a | b);
                break;
            case 5:
                result = (int8_t)(a ^ b);
                break;
            case 6:
                result = source_get(job, second + (uint32_t)(int32_t)a);
                break;
            case 7:
                walk += a;
                result = source_get(job, second + (uint32_t)walk);
                break;
            case 9:
                result = (int8_t)((a + b + current) >> 1);
                break;
            default:
                return;
        }
        wave_put(job, (int32_t)i, result);
    }
}

// Filters

// The comb filter: wave[i] = (wave[i] * a + wave[j] * 256 + wave[k] * b) / 256 in 16-bit arithmetic, with i walking the
// wave downwards once over four passes and j and k running over it twice and four times as fast.
static void filter_comb(const ApsWaveJob* job, const ApsSampleRecord* filter, uint32_t size) {
    if (size % 4 != 0) {
        return; // the 68k loop counts k down by 4 to 0 and never ends
    }
    const int16_t gain_a
        = (int16_t)((((int32_t)job->levels[filter->env_a] * filter->filter_gain) >> 8) + filter->filter_offset_a);
    const int16_t gain_b
        = (int16_t)((((int32_t)job->levels[filter->env_b] * filter->amount_b) >> 12) + filter->filter_offset_b);

    uint16_t i = (uint16_t)size;
    uint16_t j = (uint16_t)size;
    uint16_t k = (uint16_t)size;
    for (uint32_t pass = 0; pass < FILTER_PASSES; pass++) {
        if (pass == 2) {
            j = (uint16_t)size;
        }
        if (pass >= 1) {
            k = (uint16_t)size;
        }
        while (k != 0) {
            const int32_t from_i = (int32_t)wave_get(job, (int16_t)i) * gain_a;
            const uint16_t from_j = (uint16_t)((int16_t)wave_get(job, (int16_t)j) * 256);
            const int32_t from_k = (int32_t)wave_get(job, (int16_t)k) * gain_b;
            const uint16_t sum = (uint16_t)((uint32_t)from_i + from_j + (uint32_t)from_k);
            wave_put(job, (int16_t)i, (int8_t)((int16_t)sum >> 8));
            j = (uint16_t)(j - 2);
            i = (uint16_t)(i - 1);
            k = (uint16_t)(k - 4);
        }
    }
}

// The second filter record: two leaky integrators over the wave, kept between ticks
static void filter_leaky(const ApsWaveJob* job, const ApsSampleRecord* filter, uint32_t size) {
    ApsWaveFilter* memory = job->filter;
    if (filter->filter_reset && job->fresh) {
        *memory = (ApsWaveFilter) { 0 };
    }
    const int16_t rate_a
        = (int16_t)((((int32_t)job->levels[filter->env_a] * (int8_t)filter->amount_a) >> 10) + filter->filter_offset_a);
    const int16_t rate_b
        = (int16_t)((((int32_t)job->levels[filter->env_b] * (int8_t)filter->amount_b) >> 10) + filter->filter_offset_b);

    if (!filter->filter_inverse) {
        if (rate_a != 0) {
            int16_t sum = memory->first;
            for (uint32_t i = 0; i < size; i++) {
                const int16_t leak = aps_divs16((int16_t)(sum * 8), rate_a);
                const int8_t value = (int8_t)(wave_get(job, (int32_t)i) - (int8_t)leak);
                wave_put(job, (int32_t)i, value);
                sum = (int16_t)(sum + value);
            }
            memory->first = sum;
        }
        if (rate_b == 0) {
            return;
        }
        int16_t sum = memory->second;
        for (uint32_t i = 0; i < size; i++) {
            const int16_t value = aps_divs16((int16_t)(sum * 8), rate_b);
            wave_put(job, (int32_t)i, (int8_t)value);
            sum = (int16_t)(sum + (int16_t)(wave_get(job, (int32_t)i + 1) - value));
        }
        memory->second = sum;
        return;
    }

    const int16_t rate_c = (int16_t)(-rate_a);
    if (rate_c == 0 || rate_b == 0) {
        return;
    }
    int16_t second = memory->second;
    int16_t first = memory->first;
    for (uint32_t i = 0; i < size; i++) {
        int16_t value = wave_get(job, (int32_t)i);
        int16_t leak = aps_divs16((int16_t)(second * 8), rate_b);
        value = (int16_t)(value - leak);
        leak = aps_divs16((int32_t)leak * 8, rate_c);
        first = (int16_t)(first + leak);
        value = (int16_t)(value - first);
        second = (int16_t)(second + value);
        wave_put(job, (int32_t)i, (int8_t)leak);
    }
    memory->second = second;
    memory->first = first;
}

void aps_wave_build(const ApsWaveJob* job) {
    const ApsSampleRecord record = aps_sample_record(job->module, job->record);
    const uint32_t size = record.wave_bytes;

    switch (record.mode) {
        case 0:
            generate_shape(job, &record, size);
            break;
        case 1:
            generate_copy(job, &record, size);
            break;
        case 2:
            generate_combine(job, &record, size);
            break;
        default:
            break;
    }
    if (record.filter1_count != 0) {
        const uint32_t at = job->record + (uint32_t)record.filter1_count * APS_SAMPLE_RECORD_SIZE;
        const ApsSampleRecord filter = aps_sample_record(job->module, at);
        filter_comb(job, &filter, size);
    }
    if (record.filter2_count != 0) {
        const uint32_t at = job->record + (uint32_t)record.filter2_count * APS_SAMPLE_RECORD_SIZE;
        const ApsSampleRecord filter = aps_sample_record(job->module, at);
        filter_leaky(job, &filter, size);
    }
}
