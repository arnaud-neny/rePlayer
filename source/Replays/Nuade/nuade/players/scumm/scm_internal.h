// SPDX-License-Identifier: MIT
// SCUMM sound drivers: the module (the driver's code, then its song data), the known driver versions and the readers of
// the module's records.
//
// The eagleplayer calls the module at offset 4 once per tick, with a0 = a 16 byte work area of its own, a1 = the
// sample base (module offset 4 plus the signed word at offset 2) and d0 = 0xFFFFFFFF on the first call, 0xF after
// that. The entry is either the driver's code or a `bra.w` to it.
//
// The tracker drivers keep their song data behind the code or in front of it, at addresses that the code forms with
// `adda` immediates from the entry address (module offset 4):
//   instrument records  32 bytes each: the volume envelope number, working fields, the pitch envelope number, the
//                       sample (start offset, bytes) and the loop (offset, bytes)
//   envelopes           one table of 256 words per envelope number: periods or volumes
//   events              four lists of 16 bytes (time, pitch offset, duration, channel, instrument), one per track,
//                       each ends with the pitch offset 0xFFFF
//   samples             8 bit data, addressed from the sample base by signed 16 bit offsets

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/scumm.h>

#define SCM_MAX_MODULE_BYTES (1u << 20)
#define SCM_EVENT_BYTES 16u
#define SCM_EVENT_END 0xFFFFu // the pitch offset of the last event of a track
// The event position is shifted left by 4 in a word: a list longer than this wraps in the original.
#define SCM_MAX_TRACK_EVENTS 0x1000u
#define SCM_RECORD_BYTES 32u
#define SCM_ENVELOPE_POSITIONS 0xFFu // the envelope positions run from 0 to 0xFE
#define SCM_CHANNEL_FREE 0xFFu       // the channel byte of a channel that plays no instrument
#define SCM_SWEEP_WORDS 0x10u        // the sweep effects play 16 words
#define SCM_SWEEP_SAMPLE_BYTES (SCM_SWEEP_WORDS * 2u)
#define SCM_SWEEP_VOLUME 0x3F

const ScmEngine* scm_engine(ScmDriver driver);

bool scm_module_load(ScmModule* module, const uint8_t* data, size_t size);
void scm_module_free(ScmModule* module);

// The module's records. Loading proved that everything the driver can read is inside the module; a read outside
// returns zeros.

typedef struct ScmEvent {
    int16_t time;
    uint16_t pitch_offset; // SCM_EVENT_END: the last event
    int16_t duration;
    uint16_t channel;
    uint16_t instrument;
} ScmEvent;

ScmEvent scm_module_event(const ScmModule* module, uint32_t track, uint16_t position);

typedef struct ScmInstrument {
    uint16_t volume_envelope;
    uint16_t pitch_envelope;
    int16_t start_offset; // the sample the note starts with, from the sample base
    uint16_t start_bytes;
    int16_t loop_offset; // the sample the channel repeats
    uint16_t loop_bytes;
} ScmInstrument;

ScmInstrument scm_module_instrument(const ScmModule* module, uint32_t index);

// The word at `position` of envelope table `envelope`: the table address and the position wrap in 16 bits.
uint16_t scm_module_envelope(const ScmModule* module, uint16_t envelope, uint16_t position);

// The module offset of the sample at `offset` from the sample base.
uint32_t scm_module_sample_offset(const ScmModule* module, int16_t offset);

// A sample as AUDxLC takes it: Paula ignores bit 0.
const int8_t* scm_module_sample(const ScmModule* module, uint32_t sample_offset);

// Where a note start clears a long (ScmOperand_ZeroWrite) for the sample at `sample_offset`; false when the version
// has no such write.
bool scm_module_zero_write(const ScmModule* module, uint32_t sample_offset, uint32_t* at);

// `clr.l` of the long at module offset `at` (the driver clears the first long of a loop of one word). Loading proved
// the write is inside the module.
void scm_module_clear_long(ScmModule* module, uint32_t at);
