// SPDX-License-Identifier: MIT
// GlueMon player: the replayer of the UADE eagleplayer "GlueMon". Each tick it makes a new noise buffer, lets the
// volumes decay, and every `speed` ticks plays one row of the pattern. The voices loop their waveform all the time;
// a note only sets period and volume. The fourth voice of a percussion song also restarts its DMA with another
// waveform on a command.

#include <nuade/players/gluemon.h>

#include "gm_internal.h"
#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define NOISE_SEED 0x986FA0B9u
#define NOISE_ROTATE 7
#define NOISE_ADD 0x6ECA756Du
#define NOISE_XOR 0x9E59A92Bu

// The waveform in the eagleplayer's data after the noise buffer: a square start, then a ramp up and down.
static const int8_t k_ramp_wave[GM_WAVE_BYTES] = {
    127, 127,  -127, -127, 0,   10,  20,  30,  40,  50,  60,   70,  80,  90, 100, 110,
    120, -126, -116, -106, -96, -86, -76, -66, -76, -96, -116, 120, 100, 80, 60,  40,
};

typedef enum PercussionWave {
    PercussionWave_Ramp,
    PercussionWave_Noise,
    PercussionWave_Voice, // the waveform of the fourth voice in the module
} PercussionWave;

// What a percussion command sets: the waveform, the start period and its change per tick, the volume and the decay.
typedef struct PercussionCommand {
    PercussionWave wave;
    uint16_t period;
    int16_t step; // the period drops by this every tick
    uint16_t volume;
    uint8_t decrement;
} PercussionCommand;

// Commands 0xC8..0xCF.
static const PercussionCommand k_commands[GM_COMMAND_LAST - GM_COMMAND_FIRST + 1] = {
    { PercussionWave_Ramp, 0x710, -255, 0x40, 8 },  { PercussionWave_Noise, 0x710, 20, 0x40, 4 },
    { PercussionWave_Voice, 0x1C4, -50, 0x40, 4 },  { PercussionWave_Voice, 0x25C, -50, 0x40, 4 },
    { PercussionWave_Voice, 0x388, -50, 0x40, 4 },  { PercussionWave_Voice, 0x4B8, -50, 0x40, 4 },
    { PercussionWave_Noise, 0x25C, -10, 0x30, 16 }, { PercussionWave_Noise, 0x25C, -10, 0x28, 2 },
};

bool gm_probe(const uint8_t* data, size_t size) {
    GmModule module;
    return gm_module_parse(&module, data, size);
}

GmPlayer* gm_create(const uint8_t* data, size_t size) {
    GmPlayer* player = calloc(1, sizeof(*player));
    uint8_t* bytes = malloc(size);
    if (player == nullptr || bytes == nullptr) {
        free(player);
        free(bytes);
        return nullptr;
    }
    memcpy(bytes, data, size);
    player->bytes = bytes;
    if (!gm_module_parse(&player->module, bytes, size)) {
        gm_destroy(player);
        return nullptr;
    }
    return player;
}

void gm_destroy(GmPlayer* player) {
    if (player != nullptr) {
        free(player->bytes);
        free(player);
    }
}

// InitSound: the voices play their waveform from the start (with no period, and volume 0). The sequencer is set to step
// into the first row on the first tick; the position table wraps then (its last entry is the end mark).

bool gm_start(GmPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    const GmModule* module = &player->module;
    *player = (GmPlayer) { .bytes = player->bytes,
                           .module = *module,
                           .tick = (uint8_t)(module->speed - 1u),
                           .row = (uint16_t)(module->rows - 1u),
                           .order = (uint16_t)(module->order_bytes - 2u),
                           .noise_seed = NOISE_SEED };
    for (uint32_t i = 0; i < GM_VOICES; i++) {
        GmVoice* voice = &player->voices[i];
        voice->envelope = player->module.envelope[i];
        voice->note = -1;
        nuade_voice_stop(voices, i);
        voice->paula.lc = gm_wave(&player->module, i);
        voice->paula.len_words = GM_WAVE_WORDS;
        nuade_paula_channel_dma_on(&voice->paula);
    }
    return true;
}

// The next noise buffer: eight longwords of a scrambled seed.

static void make_noise(GmPlayer* player) {
    uint32_t seed = player->noise_seed;
    for (uint32_t i = 0; i < GM_NOISE_LONGS; i++) {
        seed = ((seed << NOISE_ROTATE) | (seed >> (32 - NOISE_ROTATE))) + NOISE_ADD;
        seed ^= NOISE_XOR;
        for (uint32_t byte = 0; byte < 4; byte++) {
            player->noise[i * 4 + byte] = (int8_t)(uint8_t)(seed >> (24 - 8 * byte));
        }
    }
    player->noise_seed = seed;
}

// A volume above the threshold drops by the decrement (as words: it can wrap around).

static void decay_volume(GmVoice* voice) {
    if (voice->volume > voice->envelope.threshold) {
        voice->volume = (uint16_t)(voice->volume - voice->envelope.decrement);
    }
}

// A note byte of a row: a note off, nothing, or a note (period and full volume).

static void start_note(GmVoice* voice, uint8_t entry) {
    if (entry == GM_NOTE_OFF) {
        voice->volume = 0;
    } else if (entry != GM_NOTE_NONE) {
        voice->paula.period = gm_period(entry);
        voice->volume = voice->envelope.reload;
        voice->note = gm_note_index(entry);
    }
}

// The percussion voice's period moves by its step every tick.

static void slide_percussion(GmPlayer* player) {
    player->percussion_period = (uint16_t)(player->percussion_period - (uint16_t)player->percussion_step);
    player->voices[GM_PERCUSSION_VOICE].paula.period = player->percussion_period;
}

// A byte of the fourth voice of a percussion song. A command restarts the DMA with its waveform; a byte that is not
// a command plays the first 32 bytes of the pattern and leaves period, volume and decay as they were. Then the period
// slides.

static void play_percussion(GmPlayer* player, uint8_t entry) {
    GmVoice* voice = &player->voices[GM_PERCUSSION_VOICE];
    if (entry != GM_NOTE_NONE) {
        const int8_t* wave = gm_pattern_wave(&player->module, player->pattern);
        if (gm_is_command(entry)) {
            const PercussionCommand* command = &k_commands[entry - GM_COMMAND_FIRST];
            switch (command->wave) {
                case PercussionWave_Ramp:
                    wave = k_ramp_wave;
                    break;
                case PercussionWave_Noise:
                    wave = player->noise;
                    break;
                case PercussionWave_Voice:
                    wave = gm_wave(&player->module, GM_PERCUSSION_VOICE);
                    break;
            }
            player->percussion_period = command->period;
            player->percussion_step = command->step;
            voice->volume = command->volume;
            voice->envelope.decrement = command->decrement;
        }
        nuade_paula_channel_dma_off(&voice->paula);
        voice->paula.lc = wave;
        nuade_paula_channel_dma_on(&voice->paula);
    }
    slide_percussion(player);
}

// Plays one row: the three note voices, and the fourth as a note voice or as percussion.

static void play_row(GmPlayer* player) {
    const GmRow row = gm_row(&player->module, player->pattern, player->row);
    for (uint32_t i = 0; i < GM_PERCUSSION_VOICE; i++) {
        start_note(&player->voices[i], row.entry[i]);
    }
    if (player->module.percussion) {
        play_percussion(player, row.entry[GM_PERCUSSION_VOICE]);
    } else {
        start_note(&player->voices[GM_PERCUSSION_VOICE], row.entry[GM_PERCUSSION_VOICE]);
    }
}

// The next row; behind the last row the next pattern of the position table. The table wraps at its end mark, and the
// eagleplayer calls SongEnd from the second wrap on (the first is the start of the song).

static void step_row(GmPlayer* player, bool* looped) {
    player->tick = 0;
    player->row++;
    if (player->row < player->module.rows) {
        return;
    }
    player->row = 0;
    player->order++;
    uint8_t pattern = gm_order(&player->module, player->order);
    if (pattern == GM_ORDER_END) {
        player->order = 0;
        pattern = gm_order(&player->module, 0);
        *looped = player->wrapped;
        player->wrapped = true;
    }
    player->pattern = pattern;
}

GmTickResult gm_tick(GmPlayer* player, NuadeVoices* voices) {
    GmTickResult result = { 0 };
    make_noise(player);
    for (uint32_t i = 0; i < GM_VOICES; i++) {
        decay_volume(&player->voices[i]);
    }
    player->tick++;
    if (player->tick >= player->module.speed) {
        step_row(player, &result.looped);
        play_row(player);
    } else if (player->module.percussion) {
        slide_percussion(player);
    }
    for (uint32_t i = 0; i < GM_VOICES; i++) {
        // AUDxVOL is the voice volume (a word) times DeliTracker's master volume and balance (both 64), shifted right
        // by 6, in words: the low 10 bits of the volume, of which Paula reads 7
        player->voices[i].paula.volume = (uint8_t)player->voices[i].volume;
        nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
    }
    return result;
}

void gm_title(const GmPlayer* player, char out[GM_NAME_LENGTH + 1]) {
    size_t length = 0;
    for (size_t i = 0; i < GM_NAME_LENGTH; i++) {
        const char c = player->module.name[i];
        out[length] = isprint((unsigned char)c) ? c : ' ';
        length++;
    }
    while (length > 0 && out[length - 1] == ' ') {
        length--;
    }
    out[length] = '\0';
}

GmPosition gm_position(const GmPlayer* player) {
    GmPosition position
        = { .order = player->order, .row = player->row, .tick = player->tick, .speed = player->module.speed };
    return position;
}

uint32_t gm_order_count(const GmPlayer* player) {
    return player->module.order_count;
}

uint32_t gm_row_count(const GmPlayer* player) {
    return player->module.rows;
}

GmCell gm_cell(const GmPlayer* player, uint32_t order, uint32_t voice, uint32_t row) {
    GmCell cell = { .note = -1, .command = -1 };
    if (order >= player->module.order_count || voice >= GM_VOICES) {
        return cell;
    }
    const uint8_t entry = gm_row(&player->module, gm_order(&player->module, order), row).entry[voice];
    if (entry == GM_NOTE_NONE) {
        return cell;
    }
    if (player->module.percussion && voice == GM_PERCUSSION_VOICE) {
        cell.command = entry;
    } else if (entry == GM_NOTE_OFF) {
        cell.note_off = true;
    } else {
        cell.note = gm_note_index(entry);
    }
    return cell;
}

int gm_voice_note(const GmPlayer* player, uint32_t voice) {
    return voice < GM_VOICES ? player->voices[voice].note : -1;
}
