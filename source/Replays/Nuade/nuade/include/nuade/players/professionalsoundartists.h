// SPDX-License-Identifier: MIT
// Professional Sound Artists player (Dave "Sinbad" Hasler, the Punisher songs), translated from the UADE eagleplayer
// "ProfessionalSoundArtists". A song is a module of its own layout ("PSA" header, subsong table, 64-byte instruments,
// a table of pattern numbers and 64-row patterns of 4 x 4 bytes); an instrument is a sample or a pulse-width synth with
// an ADSR volume envelope. The replayer is in the eagleplayer.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/nuade.h>
#include <nuade/paula.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PSA_VOICES 4

// The CIA clock: the timebase of PSA_TICK_LENGTH.
#define PSA_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / PSA_TIMEBASE_HZ seconds.
#define PSA_TICK_LENGTH 14187

#define PSA_TITLE_BYTES 32

typedef struct PsaPlayer PsaPlayer;

typedef struct PsaTickResult {
    bool looped; // the replayer called SongEnd
    bool ended;  // the song stopped and no voice plays
} PsaTickResult;

typedef struct PsaVoiceState {
    int period;     // the period of the last note, 0 if none
    int instrument; // the instrument of the last note, -1 if none
} PsaVoiceState;

typedef struct PsaLayout {
    uint32_t instruments; // module offsets
    uint32_t orders;
    uint32_t patterns;
    uint32_t subsong_count;
} PsaLayout;

typedef struct PsaSubsong {
    uint16_t start; // byte offsets into the pattern number table
    uint16_t end;
    uint16_t loop; // PSA_NO_LOOP: the song stops at its end
} PsaSubsong;

typedef struct PsaNote {
    uint8_t note; // 0: none, else the period table index + 1
    uint8_t instrument;
    bool period_only; // effect byte bit 7: the note only sets the period, no instrument and no row effect
    uint8_t effect;   // the low seven bits of the effect byte
    uint8_t param;
} PsaNote;

typedef struct PsaPwm {
    bool enabled;
    uint8_t start; // the wave's offset at the note start
    uint8_t up_step;
    uint8_t up_delay;
    uint8_t up_count;
    uint8_t down_step;
    uint8_t down_delay;
    uint8_t down_count;
} PsaPwm;

typedef struct PsaEnvelope {
    uint8_t state;
    uint8_t attack_step;
    uint8_t attack_delay;
    uint8_t attack_count;
    uint8_t decay_step;
    uint8_t decay_delay;
    uint8_t decay_count;
    uint8_t sustain_count; // 0: sustains until the release effect
    uint8_t release_step;
    uint8_t release_delay;
    uint8_t release_count;
} PsaEnvelope;

typedef struct PsaVoice {
    NuadePaulaChannel paula;
    int64_t wave;        // chip address: the loop of a sample, or the wave buffer window of a pulse-width note
    uint16_t loop_words; // the repeat length
    uint16_t period;
    uint16_t note;       // the period table index of the last note
    int16_t porta_speed; // tone portamento: speed and target period
    uint16_t porta_target;
    uint16_t volume;
    int16_t vibrato_speed;
    int16_t vibrato_offset;
    uint8_t vibrato_count;
    uint8_t vibrato_length;
    PsaPwm pwm;
    uint8_t pwm_position;
    uint8_t pwm_count;
    uint8_t pwm_delay;
    bool pwm_down;
    PsaEnvelope envelope;
    uint8_t envelope_timer;
    PsaNote row; // the voice's note of the current row
    bool dma_start;
    bool repeat_pending; // a sample note: the repeat registers are written once its DMA runs
    int last_period;
    int last_instrument;
} PsaVoice;

struct PsaPlayer {
    uint8_t* chip; // the chip image
    size_t size;   // the module's size
    PsaLayout layout;
    char title[PSA_TITLE_BYTES + 1];
    bool fault; // the replayer left what the port models (dry run only)
    bool looped;
    bool playing;
    uint8_t speed_counter;
    uint8_t speed;
    uint8_t row_counter;
    uint16_t position; // byte offset of the current entry of the pattern number table
    PsaSubsong subsong;
    int64_t next_row; // module offset of the next row to read
    uint8_t arpeggio_tick;
    PsaVoice voices[PSA_VOICES];
};

// Check2 of the eagleplayer plus the limits of what this port models: every subsong is played through without sound,
// and a song that leaves the module or the replayer's own chip buffer is refused (UADE plays it).
bool psa_probe(const uint8_t* data, size_t size);

// Copies and parses the song; null when `psa_probe` rejects it.
PsaPlayer* psa_create(const uint8_t* data, size_t size);
void psa_destroy(PsaPlayer* player);

uint32_t psa_subsong_count(const PsaPlayer* player);

// The module's title (up to 32 characters, trailing spaces removed).
const char* psa_title(const PsaPlayer* player);

// (Re)start `subsong` from its beginning; the player drives `voices` (Paula channel n = voice n).
bool psa_start(PsaPlayer* player, uint32_t subsong, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
PsaTickResult psa_tick(PsaPlayer* player, NuadeVoices* voices);

// Ticks per row the song is at.
uint32_t psa_grid_ticks(const PsaPlayer* player);

PsaVoiceState psa_voice_state(const PsaPlayer* player, uint32_t voice);

// The player's state behind a song it opened; 0 when the song is another player's.
PsaPlayer* psa_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
