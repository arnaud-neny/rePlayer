///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// V2M pattern extraction
//
// A .v2m file stores its music as 16 channels of delta-coded MIDI-ish event
// streams (notes, program changes, pitch bends, 7 controllers) plus a global
// tempo/time-signature track. That is enough to lay the song out as a tracker
// grid: one row per 32nd note, one column group per channel.
//
// This header parses ONLY the event section, which sits at the very start of
// the file and has the same layout in every format version (0..6) -- the
// version-dependent parts (globals, patch map) come after it, so no
// canonicalization is needed here.
//
// Reference for the layout: V2MPlayer::InitBase / V2MPlayer::Tick in v2seq.cpp.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <vector>

namespace v2mpat {

enum { CHANNELS = 16, MAX_ROWS = 1 << 18 };

// One cell of the grid. `note` and `pgm` are stored +1 so 0 means "empty";
// `vel` is the raw MIDI velocity (0 = note off).
struct Cell {
    uint8_t note;
    uint8_t vel;
    uint8_t pgm;
};

struct Pattern {
    uint32_t rows = 0;
    uint32_t used_channels = 0;      // bitmask of channels that carry notes
    std::vector<Cell> cells;         // rows * CHANNELS, row-major
    std::vector<uint64_t> row_sample; // rows, sample index at the start of each row
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace detail {

inline uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Event times are 24-bit deltas stored column-wise: byte n of the delta for
// event i lives at ptr[n * count + i].
inline uint32_t delta3(const uint8_t* p, uint32_t count) {
    return (uint32_t)p[0] | ((uint32_t)p[count] << 8) | ((uint32_t)p[2 * count] << 16);
}

// Cursor with bounds checking -- this parses untrusted file data.
struct Cursor {
    const uint8_t* p;
    const uint8_t* end;
    bool ok = true;

    uint32_t u32() {
        if (!ok || (uint64_t)(end - p) < 4) {
            ok = false;
            return 0;
        }
        uint32_t v = rd32(p);
        p += 4;
        return v;
    }

    // Claims `count * stride` bytes and returns the start, or null on overflow.
    const uint8_t* take(uint64_t count, uint64_t stride) {
        uint64_t n = count * stride;
        if (!ok || count > 0x10000000ull || (uint64_t)(end - p) < n) {
            ok = false;
            return nullptr;
        }
        const uint8_t* s = p;
        p += n;
        return s;
    }
};

struct Stream {
    const uint8_t* ptr = nullptr;
    uint32_t count = 0;
};

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Parses `data` and fills `out`. Returns false if the file is malformed or has
// no events at all.
inline bool extract(const uint8_t* data, size_t size, uint32_t samplerate, Pattern* out) {
    using namespace detail;

    Cursor c = { data, data + size };
    uint32_t timediv = c.u32();
    uint32_t maxtime = c.u32();
    uint32_t gdnum = c.u32();
    if (!c.ok || timediv == 0) {
        return false;
    }

    const uint8_t* gptr = c.take(gdnum, 10);

    Stream notes[CHANNELS];
    Stream pgms[CHANNELS];
    Stream bends[CHANNELS];
    Stream ctls[CHANNELS][7];
    for (int ch = 0; ch < CHANNELS && c.ok; ch++) {
        uint32_t notenum = c.u32();
        if (!notenum) {
            continue;
        }
        notes[ch].count = notenum;
        notes[ch].ptr = c.take(notenum, 5);

        pgms[ch].count = c.u32();
        pgms[ch].ptr = c.take(pgms[ch].count, 4);

        bends[ch].count = c.u32();
        bends[ch].ptr = c.take(bends[ch].count, 5);

        for (int cc = 0; cc < 7; cc++) {
            ctls[ch][cc].count = c.u32();
            ctls[ch][cc].ptr = c.take(ctls[ch][cc].count, 4);
        }
    }
    if (!c.ok) {
        return false;
    }

    uint64_t rows = (uint64_t)maxtime * 8 / timediv + 1;
    if (rows > MAX_ROWS) {
        rows = MAX_ROWS;
    }

    out->rows = (uint32_t)rows;
    out->used_channels = 0;
    out->cells.assign(rows * CHANNELS, Cell { 0, 0, 0 });
    out->row_sample.assign(rows, 0);

    // --- events -> cells ------------------------------------------------------
    for (int ch = 0; ch < CHANNELS; ch++) {
        const Stream& s = notes[ch];
        uint32_t time = 0;
        uint8_t note = 0, vel = 0;
        for (uint32_t i = 0; i < s.count; i++) {
            time += delta3(s.ptr + i, s.count);
            note = (uint8_t)(note + s.ptr[3 * s.count + i]);
            vel = (uint8_t)(vel + s.ptr[4 * s.count + i]);
            uint64_t row = (uint64_t)time * 8 / timediv;
            if (row < rows) {
                // A row holds one cell per channel, so events that share a row
                // overwrite each other. A note that struck is what the row
                // sounds like; a note-off must not hide it (legato writes the
                // off and the on at the same tick, in either order).
                Cell& cell = out->cells[row * CHANNELS + ch];
                if (vel != 0 || cell.vel == 0) {
                    cell.note = (uint8_t)(note + 1);
                    cell.vel = vel;
                }
                out->used_channels |= 1u << ch;
            }
        }

        const Stream& p = pgms[ch];
        time = 0;
        uint8_t pgm = 0;
        for (uint32_t i = 0; i < p.count; i++) {
            time += delta3(p.ptr + i, p.count);
            pgm = (uint8_t)(pgm + p.ptr[3 * p.count + i]);
            uint64_t row = (uint64_t)time * 8 / timediv;
            if (row < rows) {
                out->cells[row * CHANNELS + ch].pgm = (uint8_t)(pgm + 1);
            }
        }
    }

    // --- sequencer schedule -> row start times ---------------------------------
    // The sequencer (V2MPlayer::Render/Tick) does not run a continuous clock.
    // It ticks at every event time -- notes, program changes, controllers,
    // pitch bends and the global track, across all channels -- and converts
    // each interval between ticks to samples with a 32-bit division whose
    // fractional part is effectively dropped (the remainder is added to a
    // 32-bit accumulator as if it were a 32.32 fraction, so it almost never
    // carries). Integrating the tempo exactly therefore drifts ahead of the
    // audio by up to half a sample per tick, a row or more over a song. The
    // grid must show the row the sequencer is in, so this walks the same
    // schedule with the same arithmetic. One quirk is reproduced on purpose:
    // the sequencer skips straight to the first event, so sample 0 is that
    // tick, not tick 0. (The pitch-bend stream is read with its own count as
    // the stride, as the patched sequencer does; see
    // patches/v2redux-pitchbend-stride.patch.)
    std::vector<uint32_t> ticks;
    ticks.reserve(gdnum + 256);
    auto add_stream = [&](const Stream& s) {
        uint32_t t = 0;
        for (uint32_t i = 0; i < s.count; i++) {
            t += delta3(s.ptr + i, s.count);
            ticks.push_back(t);
        }
    };
    add_stream(Stream { gptr, gdnum });
    for (int ch = 0; ch < CHANNELS; ch++) {
        if (notes[ch].count == 0) {
            continue;
        }
        add_stream(notes[ch]);
        add_stream(pgms[ch]);
        add_stream(bends[ch]);
        for (int cc = 0; cc < 7; cc++) {
            add_stream(ctls[ch][cc]);
        }
    }
    std::sort(ticks.begin(), ticks.end());
    ticks.erase(std::unique(ticks.begin(), ticks.end()), ticks.end());

    // Sample index at each scheduled tick, in the sequencer's arithmetic
    // (V2MPlayer::Tick for the tempo, UpdateSampleDelta for the interval).
    const uint32_t timediv2 = 10000u * timediv;
    uint32_t usecs = 5000u * samplerate; // V2MPlayer::Reset default
    uint32_t smplrem = 0;
    uint64_t sample = 0;
    uint32_t gnr = 0;
    uint32_t gtick = gdnum ? delta3(gptr, gdnum) : 0;
    std::vector<uint64_t> tick_sample(ticks.size());
    for (size_t i = 0; i < ticks.size(); i++) {
        tick_sample[i] = sample;
        while (gnr < gdnum && gtick <= ticks[i]) {
            usecs = rd32(gptr + 3 * gdnum + 4 * gnr) * (samplerate / 100);
            gnr++;
            if (gnr < gdnum) {
                gtick += delta3(gptr + gnr, gdnum);
            }
        }
        if (i + 1 < ticks.size()) {
            uint64_t prod = (uint64_t)(ticks[i + 1] - ticks[i]) * usecs;
            uint32_t quot = (uint32_t)(prod / timediv2);
            uint32_t rem = (uint32_t)(prod % timediv2);
            uint32_t newrem = smplrem + rem;
            uint32_t carry = newrem < smplrem ? 1 : 0;
            smplrem = newrem;
            sample += quot + carry;
        }
    }

    // Row r starts at the first tick that maps to it (events land on row
    // time * 8 / timediv). Inside an interval the synth just renders, so the
    // row boundary interpolates linearly between the two ticks.
    size_t k = 0;
    for (uint64_t r = 0; r < rows; r++) {
        uint64_t start = (r * timediv + 7) / 8;
        while (k + 1 < ticks.size() && ticks[k + 1] <= start) {
            k++;
        }
        uint64_t at;
        if (ticks.empty() || start <= ticks[k]) {
            at = ticks.empty() ? 0 : tick_sample[k];
        } else if (k + 1 < ticks.size()) {
            uint64_t span = ticks[k + 1] - ticks[k];
            at = tick_sample[k] + (start - ticks[k]) * (tick_sample[k + 1] - tick_sample[k]) / span;
        } else {
            // past the last event: the sequencer has stopped, extend at tempo
            at = tick_sample[k] + (start - ticks[k]) * usecs / timediv2;
        }
        out->row_sample[r] = at;
    }

    return out->used_channels != 0;
}

} // namespace v2mpat
