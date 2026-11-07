// SPDX-License-Identifier: MIT
// Wally Beben: the replayer, translated from the module code and the eagleplayer's patches.
//
// What the eagleplayer's patches add that matters here: the raster waits that separate the writes of a tick (the period
// writes, the DMA start, the repeat registers) and SongEnd, which the sequence list read calls when all four voices
// have met the end of their lists and the volume reset calls on every use but the first. The scope bookkeeping and the
// volume scaling (the identity at full volume) are left out.

#include "wb_replay.h"

#include "nuade_base.h"
#include "nuade_utils.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define CMD_IDLE 0xFFu
#define EVENT_END 0xFFu
#define EVENT_STOP 0xFEu
#define EVENT_SONG 0xFCu
#define EVENT_TEMPO 0xEFu
#define EVENT_WAVE 0xDFu
#define EVENT_ENV 0xBFu
#define EVENT_DURATION 0x7Fu
#define EVENT_NOTE_MAX 0x5Fu
#define EVENT_SLIDE_DOWN 0x7Fu
#define EVENT_SLIDE_UP 0x7Eu
#define EVENT_ARP 0x7Du
#define EVENT_ARP_DOWN 0x7Cu
#define EVENT_SLIDE_TO 0x7Bu
#define EVENT_VIBRATO_DELAY 0x7Au
#define EVENT_WAVE_BYTE 0xE0u
#define ARP_MODE_ENVELOPE 0x7Cu
#define ENVELOPE_FLAG_START_SLIDE 8u
#define INSTRUMENT_FLAG_OFFSET 2u
#define MAX_EVENT_PASSES 4096u
#define MAX_ARP_PASSES 64u
#define MAX_DISPATCH_DEPTH 4u
#define TICK_COST 64u          // work units one replayer tick is charged in the proof
#define PASS_COST 16u          // ... and one pass over an event or a step of a sequence list
#define RELEASE_WRAP_MIN 0x81u // a release from this value on takes a volume of 0 round to a positive one
#define NO_TEMPO (-1)          // the events of a voice set no tempo
#define FIRST_PLAY_TICK 2u     // tick 1 starts the song; tick 2 reads the first events, before anything reads the tempo
#define MODE_NOTE_SLIDE 1u     // the voice mode of a slide or an arpeggio up
#define MODE_ARP_DOWN 2u

static const WbQuirks* quirks(const WbReplay* r) {
    return &r->module->version->quirks;
}

// The proof plays a voice alone to learn whether the voice depends on anything another voice can change. These record
// the places where it does: a read of a register another voice left behind, or of live state, a write into another
// voice's fields.
static void mark_coupled(const WbReplay* r) {
    if (r->track != nullptr && !r->track->period_only) {
        r->track->coupled = true;
    }
}

// The voice depends on state another voice can change, even when the lookup feeds the output period only.
static void track_couple(const WbReplay* r) {
    if (r->track != nullptr) {
        r->track->coupled = true;
    }
}

static void track_period_only(WbReplay* r, bool on) {
    if (r->track != nullptr) {
        r->track->period_only = on;
    }
}

static void track_arp_write(WbReplay* r, uint32_t pos) {
    if (r->track != nullptr) {
        r->track->arp_written |= 1ull << pos;
    }
}

static void track_arp_read(WbReplay* r, uint32_t pos) {
    if (r->track != nullptr) {
        r->track->arp_read |= 1ull << pos;
    }
}

// Charges the proof's budget; the replay faults (and the open refuses the module) when it cannot pay.
static bool charge(WbReplay* r, uint64_t units) {
    if (r->budget != nullptr && !wb_budget_take(r->budget, units)) {
        r->fault = true;
        return false;
    }
    return true;
}

static void set_period_hi(WbReplay* r, uint8_t value) {
    r->period_hi = value;
    if (r->track != nullptr) {
        r->track->period_hi_own = true;
    }
}

static uint8_t period_hi_in(WbReplay* r) {
    if (r->track != nullptr && !r->track->period_hi_own) {
        r->track->coupled = true;
    }
    return r->period_hi;
}

// The upper bytes of the division register decide the table index of a slide and the quotient of a release: a read of
// them is a read of what the voices before left.
static uint32_t divide_work_in(WbReplay* r) {
    if (r->track != nullptr && !r->track->divide_hi_own) {
        r->track->coupled = true;
    }
    return r->divide_work;
}

static void set_divide_work(WbReplay* r, uint32_t value) {
    r->divide_work = value;
    if (r->track != nullptr) {
        r->track->divide_hi_own = true;
    }
}

// A note whose volume the envelope cannot raise: it plays nothing whatever its sample holds.
static bool note_is_silent(const WbReplay* r, uint32_t v) {
    return r->vb.volume[v] == 0 && (r->vb.env_target[v] == 0 || r->vb.env_attack[v] == 0)
           && r->vb.env_release[v] < RELEASE_WRAP_MIN;
}

// One byte of a pattern stream: `ptr` is the stream, `offset` the position in it.
static uint8_t stream_u8(WbReplay* r, uint32_t ptr, uint32_t offset) {
    if (ptr == WB_ZERO_STREAM) {
        return 0;
    }
    uint8_t b = 0;
    if (ptr == WB_BAD_POINTER || !wb_module_data_u8(r->module, ptr + offset, &b)) {
        r->fault = true;
    }
    return b;
}

// The replayer's variables as the module's memory has them: a note past the period table reads whatever lies behind it.
typedef struct VarMap {
    uint8_t role;
    uint8_t elem_bytes; // 1, 2 or 4
    uint16_t count;
    uint16_t offset; // of the field in WbReplay
    bool pointer;    // holds a module address: the memory has load address + offset
} VarMap;

#define VAR(role, member, bytes, count, pointer) { role, bytes, count, (uint16_t)offsetof(WbReplay, member), pointer }

static const VarMap k_vars[] = {
    VAR(WbRole_PeriodOut, period_out, 2, 4, false),
    VAR(WbRole_Voices, vb, 1, WB_VOICE_RECORD_BYTES, false),
    VAR(WbRole_VoiceIdx, voice_idx, 1, 1, false),
    VAR(WbRole_Playing, playing, 1, 1, false),
    VAR(WbRole_Tempo, tempo, 1, 1, false),
    VAR(WbRole_TempoCtr, tempo_ctr, 1, 1, false),
    VAR(WbRole_VibLimit, vib_limit, 1, 1, false),
    VAR(WbRole_VibShift, vib_shift, 1, 1, false),
    VAR(WbRole_ArpMask, arp_mask, 1, 1, false),
    VAR(WbRole_StreamPos, stream_pos, 1, 1, false),
    VAR(WbRole_VoiceHw, voice_hw, 1, 1, false),
    VAR(WbRole_SlideDir, slide_dir, 1, 1, false),
    VAR(WbRole_PeriodBase, period_base, 2, 4, false),
    VAR(WbRole_SlideTargetPeriod, slide_target_period, 2, 4, false),
    VAR(WbRole_SlideStep, slide_step, 2, 1, false),
    VAR(WbRole_LoopLc, loop_lc, 4, 4, true),
    VAR(WbRole_LoopLen, loop_len, 2, 4, false),
    VAR(WbRole_LoopOff, loop_off, 2, 4, false),
    VAR(WbRole_StreamPtr, stream_ptr, 4, 4, true),
    VAR(WbRole_SongSeq, song_seq, 4, 4, true),
    VAR(WbRole_SeqPtr, seq_ptr, 4, 1, true),
    VAR(WbRole_PatPtr, pat_ptr, 4, 1, true),
    VAR(WbRole_VibDelta, vib_delta, 2, 1, false),
    VAR(WbRole_VibAcc, vib_acc, 2, 1, false),
    VAR(WbRole_Voice4, voice4, 4, 1, false),
    VAR(WbRole_Voice2, voice2, 4, 1, false),
    VAR(WbRole_ArpBuf, arp_buf, 1, WB_ARP_BUFFER_BYTES, false),
    VAR(WbRole_SlideSum, slide_sum, 1, 1, false),
    VAR(WbRole_SlideTarget, slide_target, 1, 1, false),
    VAR(WbRole_SlideSpeedA, slide_speed_a, 1, 1, false),
    VAR(WbRole_SlideSpeedB, slide_speed_b, 1, 1, false),
};

static uint32_t read_native(const void* field, uint32_t bytes) {
    switch (bytes) {
        case 1:
            return *(const uint8_t*)field;
        case 2:
            return *(const uint16_t*)field;
        default:
            return *(const uint32_t*)field;
    }
}

static void write_native(void* field, uint32_t bytes, uint32_t value) {
    switch (bytes) {
        case 1:
            *(uint8_t*)field = (uint8_t)value;
            break;
        case 2:
            *(uint16_t*)field = (uint16_t)value;
            break;
        default:
            *(uint32_t*)field = value;
            break;
    }
}

// A byte of a table of pointers (the eagleplayer moved them to the load address): `address` lies in the table that
// starts at `first` and has `count` entries.
static bool pointer_table_byte(uint32_t address, uint32_t first, uint32_t count, const uint32_t* entries,
                               uint8_t* out) {
    if (address < first || address - first >= count * 4u) {
        return false;
    }
    const uint32_t entry = entries[(address - first) / 4u];
    if (entry == WB_BAD_POINTER) {
        return false;
    }
    *out = (uint8_t)((entry + WB_UADE_LOAD_ADDRESS) >> (8 * (3 - (address - first) % 4u)));
    return true;
}

// A byte of UADE's memory as the replayer sees it at `address`, a module offset the replayer computed (it may leave the
// module): the live value of a variable, else the module's byte, else zero behind the module. False for what the port
// does not hold (UADE's own memory below the module, a pointer that is no address of the module).
static bool memory_byte(const WbReplay* r, uint32_t address, uint8_t* out) {
    const WbModule* m = r->module;
    const uint32_t absolute = (WB_UADE_LOAD_ADDRESS + address) & WB_ADDRESS_MASK;
    if (absolute >= WB_UADE_LOAD_ADDRESS + m->size && absolute < WB_UNMAPPED_HIGH) {
        *out = 0; // chip memory behind the module is zero, and there is no other memory up to the CIAs
        return true;
    }
    if (absolute < WB_UADE_LOAD_ADDRESS) {
        return false;
    }
    address = absolute - WB_UADE_LOAD_ADDRESS;
    if (address == m->addr[WbRole_StopMask]) {
        mark_coupled(r);
        *out = 0;
        for (uint32_t c = 0; c < WB_VOICES; c++) {
            *out |= r->voice_stopped[c] ? 1u << c : 0;
        }
        return true;
    }
    for (uint32_t i = 0; i < nuade_sizeof_array(k_vars); i++) {
        const VarMap* var = &k_vars[i];
        if (!m->has[var->role]) {
            continue;
        }
        const uint32_t first = m->addr[var->role];
        const uint32_t count = var->role == WbRole_Voices ? m->voice_bytes : var->count;
        if (address < first || address - first >= (uint32_t)var->elem_bytes * count) {
            continue;
        }
        const uint32_t index = (address - first) / var->elem_bytes;
        const uint32_t byte = (address - first) % var->elem_bytes;
        if (var->pointer) {
            track_couple(r); // other voices move the stream and table pointers: success depends on their values
        } else {
            mark_coupled(r);
        }
        if (var->role == WbRole_LoopLc && r->abstract_entry0 && r->loop_from_entry0[index]) {
            track_couple(r); // the repeat address was taken from the first sample table entry (see check_sample_use)
            return false;
        }
        uint32_t value = read_native((const uint8_t*)r + var->offset + index * var->elem_bytes, var->elem_bytes);
        if (var->pointer) {
            if (value > m->size) {
                return false;
            }
            value += WB_UADE_LOAD_ADDRESS;
        }
        *out = (uint8_t)(value >> (8 * (var->elem_bytes - 1 - byte)));
        return true;
    }
    if (address >= m->var_lo && address < m->var_hi) {
        *out = m->image[address]; // between the variables: bytes nothing writes
        return true;
    }
    const uint32_t songs = m->addr[WbRole_Songs];
    const uint32_t patterns = m->addr[WbRole_Patterns];
    const uint32_t samples = m->addr[WbRole_Samples];
    if ((address >= songs && address < patterns + m->pattern_count * 4u)
        || (address >= samples && address < samples + m->sample_count * 4u)) {
        if (address >= samples) {
            mark_coupled(r); // the sample table is written by the replayer
            if (r->abstract_entry0 && address - samples < 4u) {
                track_couple(r); // the first entry grows without bound in the proof
                return false;
            }
        }
        return pointer_table_byte(address, songs, m->subsong_count * 4u, m->song_seq, out)
               || pointer_table_byte(address, patterns, m->pattern_count, m->pattern_ptr, out)
               || pointer_table_byte(address, samples, m->sample_count, r->sample_ptr, out);
    }
    return wb_module_data_u8(m, address, out);
}

// A word of the period table; a note past the table reads the memory behind it.
static uint16_t period_at(WbReplay* r, uint32_t byte_index) {
    uint16_t p = 0;
    if (wb_module_period(r->module, byte_index, &p)) {
        return p;
    }
    uint8_t hi = 0;
    uint8_t lo = 0;
    const uint32_t address = r->module->addr[WbRole_Periods] + byte_index;
    if (r->track != nullptr && r->track->period_only) {
        r->track->live_read = true; // see start_note
    } else {
        track_couple(r);
    }
    if (!memory_byte(r, address, &hi) || !memory_byte(r, address + 1, &lo)) {
        r->fault = true;
        return 0;
    }
    return (uint16_t)((hi << 8) | lo);
}

// Paula

// AUDxLC for a sample pointer (a module offset). Paula reads chip memory, which repeats every 2 MiB: the module's
// bytes, then zeros up to the end of the memory, then what lies below the module (UADE's own memory, which the port
// does not hold: `outside`).
static const int8_t* sample_address(const WbReplay* r, uint32_t pointer, bool* outside) {
    const uint32_t absolute = (WB_UADE_LOAD_ADDRESS + pointer) & WB_CHIP_MASK;
    *outside = absolute < WB_UADE_LOAD_ADDRESS;
    if (absolute >= WB_UADE_LOAD_ADDRESS && absolute - WB_UADE_LOAD_ADDRESS <= r->module->size) {
        return (const int8_t*)(r->module->image + ((absolute - WB_UADE_LOAD_ADDRESS) & ~1u)); // bit 0 is ignored
    }
    return nullptr;
}

// A sample the port does not hold is fine under a silent note. The first sample table entry grows without bound, so in
// the proof a note that plays from it must be silent, whatever the entry holds.
static void check_sample_use(WbReplay* r, uint32_t v) {
    if (r->voice_stopped[v] || note_is_silent(r, v)) {
        return;
    }
    if (r->abstract_entry0 && r->lc_from_entry0[v]) {
        mark_coupled(r);
        if (r->track == nullptr) {
            r->fault = true;
        }
    } else if (r->lc_outside[v]) {
        r->fault = true;
    }
}

// The volume reset routine: AUDxVOL of all voices to 0. The eagleplayer's patch calls SongEnd from it on every use but
// the first.
static void silence(WbReplay* r) {
    if (r->song_end_armed) {
        r->song_end = true;
    }
    r->song_end_armed = true;
    for (uint32_t c = 0; c < WB_VOICES; c++) {
        r->paula[c].volume = 0;
        r->vb.volume[c] = 0;
    }
}

static void reset(WbReplay* r) {
    silence(r);
    r->tempo_ctr = 0;
    r->playing = 0;
}

// The sequence list read. The eagleplayer's patch clears the voice's mark when the entry is the end marker; when all
// four are clear it calls SongEnd and sets them again.

static void list_end(WbReplay* r, uint32_t voice) {
    r->voice_open[r->vb.hw[voice] >> 4] = false;
    bool any = false;
    for (uint32_t c = 0; c < WB_VOICES; c++) {
        any = any || r->voice_open[c];
    }
    if (!any) {
        for (uint32_t c = 0; c < WB_VOICES; c++) {
            r->voice_open[c] = true;
        }
        r->song_end = true;
    }
}

// Takes the next pattern of the voice's list: the entries up to the maximum are patterns, higher ones set the
// transposition, the end marker restarts the list.
static void load_pattern(WbReplay* r, uint32_t v) {
    r->seq_ptr = r->song_seq[v];
    for (uint32_t pass = 0; pass < MAX_EVENT_PASSES; pass++) {
        if (!charge(r, PASS_COST)) {
            return;
        }
        const uint8_t pos = r->vb.seq_pos[v]++;
        const uint8_t entry = stream_u8(r, r->song_seq[v], pos);
        if (r->fault) {
            return;
        }
        if (entry == EVENT_END) {
            list_end(r, v);
        }
        if (entry <= quirks(r)->seq_pattern_max) {
            uint32_t stream;
            if (!wb_module_pattern(r->module, entry, &stream)) {
                r->fault = true;
                return;
            }
            r->pattern_entry = quirks(r)->pattern_index_wraps ? (uint8_t)(entry * 4u) : entry * 4u;
            r->stream_ptr[v] = stream;
            r->pat_ptr = stream;
            return;
        }
        if (entry == EVENT_END) {
            r->vb.seq_pos[v] = 0;
        } else {
            r->vb.transpose[v] = (uint8_t)(entry + quirks(r)->seq_transpose_add);
        }
    }
    r->fault = true;
}

// A note's period into the base period and the output.
// The lookup works in a register whose second byte is whatever the code before left there: the previous period (or 0
// after an event read, the envelope's release and a division). The word builds index the table with the register's
// low word as a signed number, the others with the whole register.
static void set_period(WbReplay* r, uint32_t v) {
    uint32_t index = ((uint32_t)period_hi_in(r) << 8) | (uint8_t)(r->vb.note[v] * 2u);
    if (quirks(r)->period_index_signed_word && (index & 0x8000u) != 0) {
        index |= 0xFFFF0000u;
    }
    const uint16_t p = period_at(r, index);
    r->period_base[v] = p;
    r->period_out[v] = p;
    set_period_hi(r, (uint8_t)(p >> 8));
}

// The arpeggio offsets named by the mask, bit by bit, into the voice's part of the arpeggio buffer.
static void build_arp(WbReplay* r, uint32_t v) {
    uint32_t pos = (uint8_t)(r->vb.arp_start[v] + 1u);
    for (uint32_t i = 0; i < 8; i++) {
        if ((r->arp_mask & 1u) != 0) {
            uint8_t offset = 0;
            if (pos >= WB_ARP_BUFFER_BYTES || !wb_module_arp_byte(r->module, i, &offset)) {
                r->fault = true;
                return;
            }
            track_arp_write(r, pos);
            r->arp_buf[pos++] = offset;
        }
        r->arp_mask >>= 1;
    }
    if (pos >= WB_ARP_BUFFER_BYTES) {
        r->fault = true;
        return;
    }
    track_arp_write(r, pos);
    r->arp_buf[pos] = 0xFF;
    r->vb.slide_target[v] = 0;
}

// The instrument flag of an envelope record writes byte 0x27 + voice of the voice block: it lands in the slide fields
// of another voice.
static void set_start_slide_flag(WbReplay* r, uint32_t v) {
    mark_coupled(r); // the flag lands in another voice's fields
    if (v == 0) {
        r->vb.slide_target[3] = 1;
    } else {
        r->vb.slide_param[v - 1] = 1;
    }
}

// The envelope record of the voice into its fields.
static void load_envelope(WbReplay* r, uint32_t v) {
    WbEnvelope e;
    if (!wb_module_envelope(r->module, r->vb.env_sel[v], &e)) {
        r->fault = true;
        return;
    }
    r->vb.volume[v] = e.volume;
    r->vb.env_target[v] = e.target;
    r->vb.env_attack[v] = e.attack;
    r->vb.env_release[v] = e.release;
    r->vb.sustain[v] = e.sustain;
    r->vb.vib_param[v] = e.vibrato;
    if (e.arp_mask != 0) {
        r->vb.arp_mask[v] = e.arp_mask;
        r->vb.mode[v] = quirks(r)->arp_mode_is_mask ? e.arp_mask : ARP_MODE_ENVELOPE;
        r->arp_mask = e.arp_mask;
        build_arp(r, v);
    }
    if (quirks(r)->envelope_start_slide && (e.flags & ENVELOPE_FLAG_START_SLIDE) != 0) {
        set_start_slide_flag(r, v);
    }
}

// The instrument record of the voice: the first block of the sample and the repeat, written to the channel.
static void start_instrument(WbReplay* r, uint32_t v) {
    WbInstrument ins;
    if (!wb_module_instrument(r->module, r->vb.wave[v], &ins) || ins.sample >= r->module->sample_count) {
        r->fault = true;
        return;
    }
    r->vb.inst_flags[v] = ins.flags;
    const bool uses_loop = ins.flags != 0 && !quirks(r)->first_block_ignores_flags;
    const uint16_t first = uses_loop ? (uint16_t)(ins.loop_offset + ins.loop_length) : ins.length;
    r->loop_len[v] = ins.loop_length;
    r->loop_off[v] = ins.loop_offset;
    r->voice_hw = r->vb.hw[v];
    NuadePaulaChannel* paula = &r->paula[r->voice_hw >> 4];
    paula->len_words = (uint16_t)(first >> 1);
    if (quirks(r)->sample_offset && (ins.flags & INSTRUMENT_FLAG_OFFSET) != 0) {
        // The replayer adds to the first entry of the sample table (not to the entry of the instrument) and stores that
        // pointer as a long at AUDxLEN: the high word becomes the length, the low word the period. AUDxLC keeps the
        // address the voice's last note left (lc_outside, lc_from_entry0).
        uint32_t* first_entry = &r->sample_ptr[0];
        *first_entry += (uint32_t)ins.sample * 4u + ins.loop_offset;
        const uint32_t address = WB_UADE_LOAD_ADDRESS + *first_entry;
        paula->len_words = (uint16_t)(address >> 16);
        paula->period = (uint16_t)address;
    } else {
        paula->lc = sample_address(r, r->sample_ptr[ins.sample], &r->lc_outside[v]);
        r->lc_from_entry0[v] = ins.sample == 0;
    }
    r->loop_lc[v] = r->sample_ptr[ins.sample];
    r->loop_from_entry0[v] = ins.sample == 0;
}

static void dispatch(WbReplay* r);

// Bit 0 of the envelope's flags raises the first tick of the note by a semitone (the next tick takes it back). The
// replayer reads the flags through an address register that holds the envelope record only when the voice has no
// arpeggio yet; otherwise it holds whatever the event reader last used, and the flag is the low bit of a pointer
// (`stale_address` is the byte it addresses, seven in front of the record's flags).
static void bump_note(WbReplay* r, uint32_t v, uint8_t transposed, uint32_t stale_address) {
    uint8_t flags;
    if (r->vb.arp_mask[v] == 0) {
        WbEnvelope e;
        if (!wb_module_envelope(r->module, r->vb.env_sel[v], &e)) {
            r->fault = true;
            return;
        }
        if (e.arp_mask != 0) {
            r->vb.arp_mask[v] = e.arp_mask;
        }
        flags = e.flags;
    } else if (!memory_byte(r, stale_address + 7, &flags)) {
        r->fault = true;
        return;
    }
    if ((flags & 1u) != 0) {
        r->vb.note_offset[v] = transposed;
        r->vb.note[v]++;
    }
}

// The note of an event: the channel restarts with the instrument and the envelope, the voice takes the note and its
// length.
static void start_note(WbReplay* r, uint32_t v, uint8_t note, uint8_t consumed, uint32_t stale_address) {
    r->voice_stopped[v] = false;
    set_divide_work(r, r->vb.dma_bit[v]);
    nuade_paula_channel_dma_off(&r->paula[v]);
    r->stream_pos = consumed;
    start_instrument(r, v);
    if (r->fault) {
        return;
    }
    load_envelope(r, v);
    check_sample_use(r, v);
    if (r->fault) {
        return;
    }
    const uint8_t transposed = (uint8_t)(note + r->vb.transpose[v]);
    r->vb.note[v] = transposed;
    if (quirks(r)->note_bump_from_envelope) {
        bump_note(r, v, transposed, stale_address);
    } else if (transposed != 0 && r->vb.arp_mask[v] == 0 && r->vb.note_offset[v] != 0) {
        r->vb.note[v] += 2;
    }
    if (r->fault) {
        return;
    }
    // A note past the period table (a rest) takes its period from the variables behind the table, which hold the other
    // voices' values: that period goes to the output and nowhere else (the lookup register is cleared next).
    if (r->track != nullptr) {
        r->track->live_read = false;
    }
    track_period_only(r, true);
    set_period(r, v);
    r->base_from_variable[v] = r->track != nullptr && r->track->live_read;
    track_period_only(r, false);
    if (r->fault) {
        return;
    }
    set_period_hi(r, 0);
    if (r->stream_ptr[v] != WB_ZERO_STREAM) {
        r->stream_ptr[v] += r->stream_pos;
    }
    r->vb.timer[v] = r->vb.duration[v];
    r->vb.vib_delay_cnt[v] = r->vb.vib_delay[v];
    r->vb.arp_pos[v] = (uint8_t)(r->vb.arp_start[v] + 1u);
    r->last_period[r->vb.hw[v] >> 4] = r->period_out[v];
    r->last_instrument[r->vb.hw[v] >> 4] = r->vb.wave[v] >> 3;
}

typedef struct EventReader {
    uint32_t stream;
    uint32_t pos;
    uint8_t consumed;
} EventReader;

static uint8_t next_byte(WbReplay* r, EventReader* e) {
    e->consumed++;
    return stream_u8(r, e->stream, e->pos++);
}

// The next event of the voice's pattern. A pattern that ends loads the next one from the list and reads from there;
// the stop and song commands leave the tick through the dispatcher again.
static void read_event(WbReplay* r, uint32_t v) {
    const WbQuirks* q = quirks(r);
    if (q->stop_at_pattern_end) {
        r->voice_stopped[v] = true;
    }
    if (r->vb.mode[v] != MODE_ARP_DOWN) {
        r->vb.mode[v] = 0;
        r->vb.arp_mask[v] = 0;
    }
    r->stream_pos = 0;
    r->slide_index_reg = 0; // the event reader clears the register
    if (r->track != nullptr) {
        r->track->slide_index_own = true;
    }
    if (!q->event_keeps_period_hi) {
        set_period_hi(r, 0);
    }
    r->vb.slide_target[v] = 0;
    r->vb.slide_down[v] = 0;
    r->vb.slide_up[v] = 0;
    r->vb.released[v] = 0;
    r->pat_ptr = r->stream_ptr[v];
    uint32_t stale_address = r->module->has[WbRole_StreamPtr] ? r->module->addr[WbRole_StreamPtr] + 4u * v : 0;
    for (uint32_t pass = 0; pass < MAX_EVENT_PASSES && !r->fault; pass++) {
        if (!charge(r, PASS_COST)) {
            return;
        }
        EventReader e = { .stream = r->stream_ptr[v], .pos = 0, .consumed = 0 };
        uint8_t b = next_byte(r, &e);
        bool pattern_ended = b == EVENT_END;
        if (b == EVENT_STOP) {
            mark_coupled(r);
            r->cmd = 0;
            r->aborted = true;
            dispatch(r);
            return;
        }
        if (q->song_command && b == EVENT_SONG) {
            mark_coupled(r);
            r->cmd = stream_u8(r, e.stream, e.pos);
            if (r->cmd > r->module->song_limit) {
                r->fault = true; // the replayer would read this event again and recurse without end
                return;
            }
            r->aborted = true;
            dispatch(r);
            return;
        }
        if (!pattern_ended && b > EVENT_TEMPO) {
            r->tempo = b & 0xF;
            if (r->track != nullptr) {
                if (r->track->tempo_value < 0) {
                    r->track->tempo_value = r->tempo;
                    r->track->tempo_tick = r->track->tick;
                } else if (r->track->tempo_value != r->tempo) {
                    track_couple(r); // the tempo changes while the other voices play
                }
            }
            b = next_byte(r, &e);
        }
        if (!pattern_ended) {
            if (q->wave_byte_command) {
                if (b == EVENT_WAVE_BYTE) {
                    r->vb.wave[v] = (uint8_t)(next_byte(r, &e) << 3);
                    b = next_byte(r, &e);
                    pattern_ended = b == EVENT_END;
                }
            } else if (b > EVENT_WAVE) {
                r->vb.wave[v] = (uint8_t)((b & 0xF) << 3);
                b = next_byte(r, &e);
                pattern_ended = b == EVENT_END;
            }
        }
        if (!pattern_ended && b > EVENT_ENV) {
            r->vb.env_sel[v] = (uint8_t)((b & 0x1F) << 3);
            b = next_byte(r, &e);
            pattern_ended = b == EVENT_END;
        }
        if (pattern_ended) {
            load_pattern(r, v);
            stale_address = r->module->addr[WbRole_Patterns] + r->pattern_entry;
            r->stream_pos = 0;
            continue;
        }
        if (b > EVENT_DURATION) {
            r->vb.duration[v] = b & 0x3F;
            b = next_byte(r, &e);
        }
        if (b > EVENT_NOTE_MAX) {
            if (b == EVENT_SLIDE_DOWN || b == EVENT_SLIDE_UP) {
                r->vb.slide_down[v] = b == EVENT_SLIDE_DOWN;
                r->vb.slide_up[v] = b == EVENT_SLIDE_UP;
                r->vb.mode[v] = MODE_NOTE_SLIDE;
                b = next_byte(r, &e);
            } else if (b == EVENT_ARP || b == EVENT_ARP_DOWN) {
                r->vb.mode[v] = b == EVENT_ARP ? MODE_NOTE_SLIDE : MODE_ARP_DOWN;
                r->vb.arp_mask[v] = next_byte(r, &e);
                r->arp_mask = r->vb.arp_mask[v];
                build_arp(r, v);
                b = next_byte(r, &e);
            } else if (b == EVENT_SLIDE_TO) {
                r->vb.arp_mask[v] = 0;
                r->vb.mode[v] = MODE_NOTE_SLIDE;
                r->vb.slide_target[v] = (uint8_t)(next_byte(r, &e) + r->vb.transpose[v]);
                r->vb.slide_param[v] = next_byte(r, &e);
                b = next_byte(r, &e);
            } else if (b == EVENT_VIBRATO_DELAY) {
                r->vb.vib_delay[v] = next_byte(r, &e);
                b = stream_u8(r, e.stream, e.pos);
                e.consumed++;
            }
        }
        start_note(r, v, b, e.consumed, stale_address);
        return;
    }
    r->fault = true;
}

// What a voice does between events.

static void vibrato(WbReplay* r, uint32_t v) {
    track_period_only(r, true); // the swing is added to the base period; the output takes the sum
    const uint8_t param = r->vb.vib_param[v];
    r->vib_shift = param & 0xF;
    // the builds that shift the whole word let the lookup register's second byte into the limit
    r->vib_limit = quirks(r)->vib_limit_keeps_period_hi ? (uint8_t)((((uint32_t)period_hi_in(r) << 8) | param) >> 4)
                                                        : (uint8_t)(param >> 4);
    const uint32_t index = (uint8_t)(r->vb.note[v] * 2u);
    r->vib_delta = (uint16_t)(period_at(r, index) - period_at(r, index + 2));
    for (uint32_t i = 0; i < r->vib_shift; i++) {
        r->vib_delta = (uint16_t)((r->vib_delta >> 1) | 1u);
    }
    r->vib_shift = 0xFF;
    // the swing between the two ends: the position walks up to the limit and back
    bool step_up = (int8_t)r->vb.vib_dir[v] >= 0;
    if (!step_up) {
        r->vb.vib_pos[v]--;
        if (r->vb.vib_pos[v] == 0) {
            r->vb.vib_dir[v]++;
            step_up = (int8_t)r->vb.vib_dir[v] < 0;
        }
    }
    if (step_up) {
        r->vb.vib_pos[v]++;
        const bool turn
            = quirks(r)->vib_flip_at_tempo_ctr ? r->vb.vib_pos[v] >= r->tempo_ctr : r->vb.vib_pos[v] > r->vib_limit;
        if (turn) {
            r->vb.vib_dir[v]--;
        }
    }
    if (r->vb.vib_delay_cnt[v] != 0) {
        r->vb.vib_delay_cnt[v]--;
        track_period_only(r, false);
        return;
    }
    r->vib_acc = r->period_base[v];
    for (uint32_t i = 0; i < (uint32_t)(r->vib_limit >> 1); i++) {
        r->vib_acc = (uint16_t)(r->vib_acc + r->vib_delta);
    }
    for (uint32_t left = r->vb.vib_pos[v]; left > 0 && (int8_t)(uint8_t)(left - 1u) >= 0; left--) {
        r->vib_acc = (uint16_t)(r->vib_acc - r->vib_delta);
    }
    r->period_out[v] = r->vib_acc;
    track_period_only(r, false);
}

static void arpeggio(WbReplay* r, uint32_t v) {
    for (uint32_t pass = 0; pass < MAX_ARP_PASSES; pass++) {
        const uint8_t pos = r->vb.arp_pos[v];
        if (pos >= WB_ARP_BUFFER_BYTES) {
            r->fault = true;
            return;
        }
        track_arp_read(r, pos);
        const uint8_t offset = r->arp_buf[pos];
        if (offset == 0xFF) {
            r->vb.arp_pos[v] = r->vb.arp_start[v];
            continue;
        }
        track_period_only(r, true);
        r->period_out[v] = period_at(r, (uint8_t)((uint8_t)(offset + r->vb.note[v]) * 2u));
        track_period_only(r, false);
        r->vb.arp_pos[v]++;
        return;
    }
    r->fault = true;
}

// How often the slide set-up's loop over the tempo runs: the builds that rotate the step count with subq.l / tst.b /
// bpl (tempo + 1 times up to 0x80, once above), the others with dbra (tempo + 1 times).
static uint32_t slide_tempo_steps(const WbReplay* r) {
    if (!quirks(r)->slide_rotates_step) {
        return r->tempo + 1u;
    }
    uint32_t steps = 1;
    while ((int8_t)(uint8_t)(r->tempo - steps) >= 0) {
        steps++;
    }
    return steps;
}

// The slide's step per tick: the period difference between the note and the target divided by the ticks it takes. The
// routine works in the division register, whose upper half holds the last remainder: its note index is the whole
// register, so a leftover remainder moves the table read far away.
static void slide_setup(WbReplay* r, uint32_t v) {
    const WbQuirks* q = quirks(r);
    if (r->track != nullptr && q->slide_rotates_step && !r->track->slide_index_own) {
        track_couple(r); // the upper bytes of the index register come from the slide of another voice
    }
    uint32_t target_index = (r->slide_index_reg & ~0xFFu) | r->slide_target;
    uint32_t work = divide_work_in(r);
    const bool downwards
        = q->slide_signed_direction ? (int8_t)target_index < (int8_t)work : (uint8_t)work >= (uint8_t)target_index;
    r->slide_dir = downwards ? 0 : 1;
    target_index = (target_index & ~0xFFu) | ((target_index << 1) & 0xFFu);
    const uint16_t target_period = period_at(r, target_index);
    r->slide_target_period[v] = target_period;
    work = (work & ~0xFFu) | ((work << 1) & 0xFFu);
    const uint16_t note_period = period_at(r, work);
    r->slide_step = downwards ? (uint16_t)(target_period - note_period) : (uint16_t)(note_period - target_period);
    const uint32_t tempo_steps = slide_tempo_steps(r);
    const uint8_t ticks = (uint8_t)(r->slide_speed_b * tempo_steps);
    if (q->slide_rotates_step) {
        // the loop that should divide by the ticks never leaves its remainder register other than 0: it rotates the
        // step left by 13 bits
        r->slide_sum = ticks;
        // the register the target note went through keeps its upper bytes for the next slide of the tick: the
        // difference or the target period in its low word, then the count of ticks taken off it
        uint32_t index_reg = (r->slide_index_reg & 0xFFFF0000u) | (downwards ? r->slide_step : target_period);
        index_reg = ((index_reg & ~0xFFu) | r->tempo) - tempo_steps;
        r->slide_index_reg = index_reg;
        r->slide_step = (uint16_t)((r->slide_step << 13) | (r->slide_step >> 3));
        work = 0;
    } else {
        if (ticks == 0) {
            r->fault = true; // divide by zero
            return;
        }
        const uint32_t dividend = r->slide_step;
        r->slide_step = (uint16_t)(dividend / ticks);
        work = ((dividend % ticks) << 16) | r->slide_step;
    }
    if (!q->slide_saves_registers) {
        set_divide_work(r, work);
    }
}

// A slide to a target note: from `window` ticks before the note ends, the base period moves by the step each tick.
static void slide(WbReplay* r, uint32_t v) {
    const WbQuirks* q = quirks(r);
    if (r->vb.slide_target[v] == 0) {
        return;
    }
    r->slide_target = r->vb.slide_target[v];
    r->slide_speed_a = r->vb.slide_param[v] & 0xF;
    r->slide_speed_b = r->slide_speed_a;
    uint8_t d = (uint8_t)((r->vb.slide_param[v] >> 4) + r->vb.timer[v]);
    if (q->slide_window_unsigned) {
        if (r->vb.duration[v] < d) {
            return;
        }
        d = (uint8_t)(d + r->slide_speed_a);
        if (r->vb.duration[v] >= d) {
            return;
        }
    } else {
        if ((int8_t)d > (int8_t)r->vb.duration[v]) {
            return;
        }
        d = (uint8_t)(d + r->slide_speed_a);
        const uint8_t limit = q->slide_window_uses_seq_pos ? r->vb.seq_pos[v] : r->vb.duration[v];
        if (d <= limit) {
            if (q->slide_snaps_to_target) {
                r->period_base[v] = r->slide_target_period[v];
            }
            return;
        }
    }
    if (r->base_from_variable[v]) {
        mark_coupled(r);
    }
    r->divide_work = (r->divide_work & ~0xFFu) | r->vb.note[v];
    slide_setup(r, v);
    if (r->fault) {
        return;
    }
    r->period_base[v] = r->slide_dir != 0 ? (uint16_t)(r->period_base[v] - r->slide_step)
                                          : (uint16_t)(r->period_base[v] + r->slide_step);
    r->period_out[v] = r->period_base[v];
    set_period_hi(r, (uint8_t)(r->period_base[v] >> 8));
}

// The envelope: attack towards the target, then release when the note nears its end. The release point is a division
// (or a shift) in the division register, which some builds do not clear first.
static void envelope(WbReplay* r, uint32_t v) {
    if (quirks(r)->clears_divide_work) {
        set_divide_work(r, 0);
    }
    if (r->vb.timer[v] == 0) {
        return;
    }
    bool release = r->vb.released[v] != 0;
    if (!release) {
        const uint8_t sustain = r->vb.sustain[v];
        if (sustain == 0) {
            r->vb.released[v] = 1;
            return;
        }
        if (sustain == 0xFF) {
            return;
        }
        uint32_t work;
        if (quirks(r)->env_divides) {
            work = (divide_work_in(r) & ~0xFFu) | r->vb.duration[v];
            const uint32_t quotient = work / sustain;
            if (quotient <= 0xFFFFu) {
                work = ((work % sustain) << 16) | quotient;
            }
            release = (uint8_t)(r->vb.timer[v] - (uint8_t)work) == 0;
        } else {
            work = (r->divide_work & ~0xFFu) | r->vb.duration[v];
            const uint8_t shift = sustain & 63u;
            const uint8_t point = shift >= 8 ? 0 : (uint8_t)((uint8_t)work >> shift);
            const uint8_t diff = (uint8_t)(point - r->vb.timer[v]);
            work = (work & ~0xFFu) | diff;
            release = diff == 0;
        }
        r->divide_work = work;
        if (!release) {
            if (r->vb.volume[v] != r->vb.env_target[v]) {
                r->vb.volume[v] = (uint8_t)(r->vb.volume[v] + r->vb.env_attack[v]);
            }
            return;
        }
    }
    r->vb.released[v] = 1;
    set_period_hi(r, 0);
    const uint8_t volume = (uint8_t)(r->vb.volume[v] - r->vb.env_release[v]);
    const bool stop = quirks(r)->zero_volume_stops ? (int8_t)volume <= 0 : (int8_t)volume < 0;
    if (stop) {
        r->vb.volume[v] = 0;
        r->voice_stopped[v] = true;
    } else {
        r->vb.volume[v] = volume;
    }
}

// The vibrato flag copies a byte two voices on: byte 0x4E + voice of the block lies in the instrument or envelope
// fields of another voice.
static uint8_t vib_flag_source(const WbReplay* r, uint32_t v) {
    switch (v) {
        case 0:
            return r->vb.wave[2];
        case 1:
            return r->vb.wave[3];
        case 2:
            return r->vb.env_sel[0];
        default:
            return r->vb.env_sel[1];
    }
}

// What happens between events: vibrato, arpeggio or slide, according to the voice's mode.
static void between_events(WbReplay* r, uint32_t v) {
    if (r->vb.note[v] != 0) {
        bool vibrato_only = false;
        if (r->vb.mode[v] == 0) {
            if (r->vb.vib_param[v] != 0) {
                vibrato(r, v);
                vibrato_only = true;
            } else if (quirks(r)->vibrato_flag) {
                r->vb.vib_on[v] = vib_flag_source(r, v);
            }
        }
        if (!vibrato_only) {
            if (r->vb.arp_mask[v] != 0) {
                arpeggio(r, v);
            } else {
                slide(r, v);
                if (quirks(r)->vibrato_flag && r->vb.slide_target[v] != 0) {
                    mark_coupled(r); // the flag was copied from another voice's fields
                    if (r->vb.vib_on[v] != 0 && !r->fault) {
                        vibrato(r, v);
                    }
                }
            }
        }
    }
    envelope(r, v);
}

// One voice of one tick.
static void voice_tick(WbReplay* r, uint32_t v) {
    if (r->track != nullptr) {
        r->track->period_hi_own = false; // the registers hold what the voices before left
        r->track->divide_hi_own = false;
        r->track->slide_index_own = false;
    }
    r->voice4 = v * 4u;
    r->voice2 = v * 2u;
    r->voice_hw = r->vb.hw[v];
    if (r->tempo_ctr != 0) {
        // between steps: only the one-shot note offset acts
        if (r->vb.note_offset[v] != 0) {
            r->vb.note_offset[v] = 0;
            r->vb.note[v] = (uint8_t)(r->vb.note[v] - quirks(r)->note_undo);
            set_period(r, v);
            envelope(r, v);
            return;
        }
        between_events(r, v);
        return;
    }
    r->vb.timer[v]--;
    bool stopped_now = false;
    if (r->vb.sustain[v] == 0 && r->vb.timer[v] == 0) {
        r->voice_stopped[v] = true;
        stopped_now = true;
    }
    if (!stopped_now && (int8_t)r->vb.timer[v] < 0) {
        read_event(r, v);
        if (!r->fault && !r->aborted) {
            envelope(r, v);
        }
        return;
    }
    if (r->vb.slide_down[v] != 0 || r->vb.slide_up[v] != 0) {
        r->vb.note[v] = (uint8_t)(r->vb.note[v] + (r->vb.slide_down[v] != 0 ? -1 : 1));
        set_period(r, v);
        envelope(r, v);
        return;
    }
    between_events(r, v);
}

// The builds with a silent long repeat an instrument without flags on silence.
static bool repeats_on_silence(const WbReplay* r, uint32_t v) {
    return quirks(r)->loop_mode == WbLoop_FlagSilence && r->vb.inst_flags[v] == 0;
}

// Where the voice repeats: the instrument's loop offset from its sample, or the silent long.
static uint32_t repeat_start(const WbReplay* r, uint32_t v) {
    if (repeats_on_silence(r, v)) {
        return r->module->silence_ptr;
    }
    return r->loop_lc[v] == WB_BAD_POINTER ? WB_BAD_POINTER : r->loop_lc[v] + r->loop_off[v];
}

// The end of the tick: periods, the DMA starts and stops, volumes, then the repeat registers.
static void output(WbReplay* r) {
    for (uint32_t c = 0; c < WB_VOICES; c++) {
        r->paula[c].period = r->period_out[c];
    }
    for (uint32_t c = 0; c < WB_VOICES; c++) {
        if (!r->voice_stopped[c]) {
            nuade_paula_channel_dma_on(&r->paula[c]);
        }
    }
    for (uint32_t c = 0; c < WB_VOICES; c++) {
        if (r->voice_stopped[c]) {
            nuade_paula_channel_dma_off(&r->paula[c]);
        }
    }
    for (uint32_t c = 0; c < WB_VOICES; c++) {
        r->paula[c].volume = r->vb.volume[c];
    }
    uint32_t high_word = 0; // the upper half of the address register the repeat address is built in
    for (int32_t v = WB_VOICES - 1; v >= 0; v--) {
        NuadePaulaChannel* paula = &r->paula[v];
        paula->len_words = (uint16_t)(r->loop_len[v] >> 1);
        const uint32_t start = repeat_start(r, (uint32_t)v);
        if (quirks(r)->loop_keeps_high_word) {
            // the address is (previous high word : loop offset) + sample start, or the silence pointer as a whole
            uint32_t address;
            if (repeats_on_silence(r, (uint32_t)v)) {
                address = start == WB_BAD_POINTER ? 0 : WB_UADE_LOAD_ADDRESS + start;
            } else {
                address = ((high_word << 16) | r->loop_off[v])
                          + (r->loop_lc[v] == WB_BAD_POINTER ? 0 : WB_UADE_LOAD_ADDRESS + r->loop_lc[v]);
            }
            high_word = address >> 16;
            mark_coupled(r); // the address register carries over from the voice before
            paula->lc = sample_address(r, address - WB_UADE_LOAD_ADDRESS, &r->lc_outside[v]);
            r->lc_from_entry0[v] = false;
        } else {
            paula->lc = sample_address(r, start, &r->lc_outside[v]);
            r->lc_from_entry0[v] = !repeats_on_silence(r, (uint32_t)v) && r->loop_from_entry0[v];
        }
        check_sample_use(r, (uint32_t)v);
    }
    if ((int8_t)--r->tempo_ctr < 0) {
        r->tempo_ctr = r->tempo;
    }
}

static void play(WbReplay* r) {
    if (r->playing == 0) {
        return;
    }
    if (r->track != nullptr && r->track->only_voice < WB_VOICES) {
        r->voice_idx = r->track->only_voice;
        voice_tick(r, r->voice_idx);
        if (!r->fault && !r->aborted) {
            output(r);
        }
        return;
    }
    for (r->voice_idx = r->voice_top;; r->voice_idx--) {
        voice_tick(r, r->voice_idx);
        if (r->fault || r->aborted) {
            return;
        }
        if (r->voice_idx == 0) {
            break;
        }
    }
    output(r);
}

static void start_song(WbReplay* r, uint8_t song) {
    if (quirks(r)->song_start_dma_off) {
        for (uint32_t c = 0; c < WB_VOICES; c++) {
            nuade_paula_channel_dma_off(&r->paula[c]);
        }
    }
    if (song - 1u >= r->module->subsong_count) {
        r->fault = true; // the song after the last one: its sequence lists would be the pattern table's longs
        return;
    }
    for (uint32_t i = 0; i < WB_VOICES; i++) {
        r->song_seq[i] = r->module->song_seq[(song - 1u) * 4u + i];
    }
    for (int32_t v = WB_VOICES - 1; v >= 0; v--) {
        r->vb.timer[v] = 0;
        r->vb.mode[v] = 0;
        r->vb.seq_pos[v] = 0;
        load_pattern(r, (uint32_t)v);
    }
    reset(r);
    r->playing = 1;
    for (uint32_t c = 0; c < WB_VOICES; c++) {
        r->voice_stopped[c] = true;
    }
}

// The module's tick routine: a request in the command byte starts a song (0 stops it); otherwise the song plays.
static void dispatch(WbReplay* r) {
    if (++r->depth > MAX_DISPATCH_DEPTH) {
        r->fault = true;
        return;
    }
    const uint8_t cmd = r->cmd;
    r->divide_work = 0; // the tick routine starts by zeroing its registers
    r->slide_index_reg = 0;
    r->period_hi = 0;
    if (cmd == 0) {
        r->playing = 0;
        r->stopped = true;
        silence(r);
    } else if (r->module->song_limit < cmd) {
        play(r);
    } else {
        start_song(r, cmd);
    }
    r->cmd = CMD_IDLE;
    r->depth--;
}

void wb_replay_tick(WbReplay* r) {
    r->song_end = false;
    r->aborted = false;
    if (r->stopped || r->fault) {
        return;
    }
    dispatch(r);
}

void wb_replay_init(WbReplay* r, const WbModule* m, uint32_t song, NuadePaulaChannel* paula, uint32_t* sample_ptr) {
    memset(r, 0, sizeof(*r));
    r->module = m;
    r->paula = paula;
    r->sample_ptr = sample_ptr;
    memcpy(sample_ptr, m->sample_ptr, (size_t)m->sample_count * sizeof(uint32_t));
    // the variables start as the file has them (a pointer variable holds 0, which is no address of the module)
    for (uint32_t i = 0; i < nuade_sizeof_array(k_vars); i++) {
        const VarMap* var = &k_vars[i];
        if (!m->has[var->role]) {
            continue;
        }
        const uint32_t count = var->role == WbRole_Voices ? m->voice_bytes : var->count;
        for (uint32_t e = 0; e < count; e++) {
            uint32_t value = 0;
            for (uint32_t k = 0; k < var->elem_bytes; k++) {
                value = (value << 8) | m->image[m->addr[var->role] + e * var->elem_bytes + k];
            }
            write_native((uint8_t*)r + var->offset + e * var->elem_bytes, var->elem_bytes,
                         var->pointer ? WB_BAD_POINTER : value);
        }
    }
    for (uint32_t c = 0; c < WB_VOICES; c++) {
        r->voice_stopped[c] = ((m->image[m->addr[WbRole_StopMask]] >> c) & 1u) != 0;
    }
    r->voice_top = m->has[WbRole_VoiceTop] ? m->image[m->addr[WbRole_VoiceTop]] : 3;
    for (uint32_t i = 0; i < WB_VOICES; i++) {
        r->voice_open[i] = true;
        r->last_instrument[i] = -1;
    }
    // the eagleplayer's InitSound: the request, then the module's reset routine
    r->cmd = (uint8_t)song;
    reset(r);
    r->song_end_armed = false;
    r->song_end = false;
}

// The proof of a song. The replay is deterministic, so a song that reaches a state it has been in before plays the same
// states for ever: if no fault came up on the way, none ever will. Two ways to find such a repeat:
//
// - Alone: voice v is played on its own, the other voices stay as the start of the song left them. If the voice never
//   touches anything another voice can change or has changed (WbCoupling), the voice does the same in the whole song:
//   the run of the song is the four voices' runs side by side. Each voice repeats after its own cycle, so lists of
//   different lengths cost the sum of the cycles, not the product.
// - Together: all voices are played until the state of the whole replay repeats.
//
// A state repeat is found by comparing with a copy taken at powers of two of the tick count (Brent's cycle detection).
// The Paula channels are outputs only and the sample table is constant but for its first entry (see
// check_sample_use), so only the replay itself is compared; what depends on that entry is left out.

typedef enum Proof {
    Proof_Safe,    // the state repeated (or the replayer stopped) without a fault
    Proof_Coupled, // the voice depends on the others: it has to be played together with them
    Proof_Fault,   // the replayer met something it does not model
    Proof_Budget,  // the work budget ran out
} Proof;

// The part of the replay that decides what it does next.
static void save_state(WbReplay* saved, const WbReplay* r) {
    memcpy(saved, r, sizeof(*saved));
    saved->track = nullptr;
    for (uint32_t v = 0; v < WB_VOICES; v++) {
        if (saved->lc_from_entry0[v]) {
            saved->lc_outside[v] = false;
        }
        if (saved->loop_from_entry0[v]) {
            saved->loop_lc[v] = 0;
        }
    }
}

// Field by field, so that the padding of the struct does not take part.
static bool same_state(const WbReplay* a, const WbReplay* b) {
#define SAME(member) (memcmp(&a->member, &b->member, sizeof(a->member)) == 0)
    return SAME(vb) && SAME(period_out) && SAME(period_base) && SAME(slide_target_period) && SAME(slide_step)
           && SAME(loop_lc) && SAME(loop_from_entry0) && SAME(base_from_variable) && SAME(lc_outside)
           && SAME(lc_from_entry0) && SAME(loop_len) && SAME(loop_off) && SAME(stream_ptr) && SAME(song_seq)
           && SAME(seq_ptr) && SAME(pat_ptr) && SAME(voice4) && SAME(voice2) && SAME(vib_delta) && SAME(vib_acc)
           && SAME(arp_buf) && SAME(cmd) && SAME(playing) && SAME(tempo) && SAME(tempo_ctr) && SAME(vib_limit)
           && SAME(vib_shift) && SAME(arp_mask) && SAME(stream_pos) && SAME(voice_hw) && SAME(slide_dir)
           && SAME(voice_stopped) && SAME(voice_idx) && SAME(slide_target) && SAME(slide_speed_a) && SAME(slide_speed_b)
           && SAME(slide_sum) && SAME(pattern_entry) && SAME(period_hi) && SAME(divide_work) && SAME(slide_index_reg)
           && SAME(voice_top) && SAME(song_end_armed) && SAME(voice_open) && SAME(song_end) && SAME(stopped)
           && SAME(aborted) && SAME(fault) && SAME(depth) && SAME(last_period) && SAME(last_instrument);
#undef SAME
}

static Proof run_to_repeat(WbReplay* r, WbBudget* budget, WbReplay* saved, WbReplay* now) {
    save_state(saved, r);
    uint64_t power = 1;
    uint64_t since_save = 0;
    for (;;) {
        if (!wb_budget_take(budget, TICK_COST)) {
            return Proof_Budget;
        }
        if (r->track != nullptr) {
            r->track->tick++;
        }
        wb_replay_tick(r);
        if (r->track != nullptr && r->track->coupled) {
            return Proof_Coupled;
        }
        if (r->fault) {
            return Proof_Fault;
        }
        save_state(now, r);
        if (r->stopped || same_state(saved, now)) {
            return Proof_Safe;
        }
        if (++since_save == power) {
            memcpy(saved, now, sizeof(*saved));
            power *= 2;
            since_save = 0;
        }
    }
}

typedef struct ProofContext {
    const WbModule* module;
    WbBudget* budget;
    WbReplay* replay;
    WbReplay* saved;
    WbReplay* now;
    NuadePaulaChannel paula[WB_VOICES];
    uint32_t* samples;
} ProofContext;

static void start_replay(ProofContext* c, uint32_t song, WbCoupling* track) {
    memset(c->paula, 0, sizeof(c->paula));
    wb_replay_init(c->replay, c->module, song, c->paula, c->samples);
    c->replay->abstract_entry0 = c->module->version->quirks.sample_offset; // the builds that move the first entry
    c->replay->budget = c->budget;
    c->replay->track = track;
}

// What the voices played alone have in common.
typedef struct AloneResult {
    uint64_t written[WB_VOICES]; // arpeggio buffer bytes the voice wrote
    uint64_t used[WB_VOICES];    // ... wrote or read
    int16_t tempo[WB_VOICES];    // the tempo the voice's events set, NO_TEMPO for none
    uint32_t tempo_tick[WB_VOICES];
    uint8_t initial_tempo;
} AloneResult;

// Each voice that play() ticks alone, with the tempo `preset` (NO_TEMPO: the module's own).
static Proof play_voices_alone(ProofContext* c, uint32_t song, int preset, AloneResult* out) {
    *out = (AloneResult) { 0 };
    for (uint32_t v = 0; v < WB_VOICES; v++) {
        out->tempo[v] = NO_TEMPO;
    }
    for (uint32_t v = 0; v < WB_VOICES; v++) {
        WbCoupling track = { .only_voice = (uint8_t)v, .tempo_value = NO_TEMPO };
        start_replay(c, song, &track);
        if (v > c->replay->voice_top) {
            continue; // play() never ticks it
        }
        if (preset != NO_TEMPO) {
            c->replay->tempo = (uint8_t)preset;
        }
        out->initial_tempo = c->replay->tempo;
        const Proof proof = run_to_repeat(c->replay, c->budget, c->saved, c->now);
        if (proof != Proof_Safe) {
            return proof;
        }
        out->written[v] = track.arp_written;
        out->used[v] = track.arp_written | track.arp_read;
        out->tempo[v] = track.tempo_value;
        out->tempo_tick[v] = track.tempo_tick;
    }
    return Proof_Safe;
}

// The tempo is one variable of all voices. If the voices' events set one tempo T other than the module's, and one of
// them does so in the first tick of play (the first tick starts the song), every voice runs on T from the end of that
// tick, before anything reads the tempo: `tempo` is T, or NO_TEMPO when the voices set no other tempo. Proof_Coupled
// when they depend on each other through it.
static Proof common_tempo(const AloneResult* alone, int* tempo) {
    *tempo = NO_TEMPO;
    uint32_t first_tick = UINT32_MAX;
    for (uint32_t v = 0; v < WB_VOICES; v++) {
        if (alone->tempo[v] != NO_TEMPO && alone->tempo[v] != alone->initial_tempo) {
            if (*tempo != NO_TEMPO && *tempo != alone->tempo[v]) {
                return Proof_Coupled;
            }
            *tempo = alone->tempo[v];
            first_tick = first_tick < alone->tempo_tick[v] ? first_tick : alone->tempo_tick[v];
        }
    }
    return *tempo == NO_TEMPO || first_tick == FIRST_PLAY_TICK ? Proof_Safe : Proof_Coupled;
}

// Each voice alone; Proof_Coupled when one of them is not independent of the others, or when two share a byte of the
// arpeggio buffer.
static Proof prove_voices_alone(ProofContext* c, uint32_t song) {
    AloneResult alone;
    Proof proof = play_voices_alone(c, song, NO_TEMPO, &alone);
    int tempo = NO_TEMPO;
    if (proof == Proof_Safe) {
        proof = common_tempo(&alone, &tempo);
    }
    if (proof == Proof_Safe && tempo != NO_TEMPO) {
        // played again at the tempo every voice runs on; the voices then set no other
        proof = play_voices_alone(c, song, tempo, &alone);
        for (uint32_t v = 0; proof == Proof_Safe && v < WB_VOICES; v++) {
            if (alone.tempo[v] != NO_TEMPO && alone.tempo[v] != tempo) {
                proof = Proof_Coupled;
            }
        }
    }
    if (proof != Proof_Safe) {
        return proof;
    }
    for (uint32_t a = 0; a < WB_VOICES; a++) {
        for (uint32_t b = 0; b < WB_VOICES; b++) {
            if (a != b && (alone.written[a] & alone.used[b]) != 0) {
                return Proof_Coupled;
            }
        }
    }
    return Proof_Safe;
}

// Plays every subsong of the module and reports whether the replayer never met something it does not model, where
// UADE plays on (see above). The ticks are charged to `budget`.
bool wb_replay_prove(const WbModule* module, WbBudget* budget) {
    ProofContext c = { .module = module, .budget = budget };
    c.replay = malloc(sizeof(*c.replay));
    c.saved = malloc(sizeof(*c.saved));
    c.now = malloc(sizeof(*c.now));
    c.samples = calloc(module->sample_count, sizeof(*c.samples));
    bool ok = c.replay != nullptr && c.saved != nullptr && c.now != nullptr && c.samples != nullptr;
    for (uint32_t song = 1; ok && song <= module->subsong_count; song++) {
        Proof proof = prove_voices_alone(&c, song);
        if (proof == Proof_Coupled) {
            start_replay(&c, song, nullptr);
            proof = run_to_repeat(c.replay, budget, c.saved, c.now);
        }
        ok = proof == Proof_Safe;
    }
    free(c.replay);
    free(c.saved);
    free(c.now);
    free(c.samples);
    return ok;
}
