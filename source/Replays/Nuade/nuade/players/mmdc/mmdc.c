// SPDX-License-Identifier: MIT
// MMDC player, translated from the UADE eagleplayer "MMDC" (the OctaMED MMD0 replayer for run-length coded blocks).
//
// One tick: the tick counter advances; on every sixth tick the next line of the block is decoded and its cells start
// notes and set the per-voice effect; on every tick each voice's effect runs; at the end the voices that started a note
// get their DMA switched on and their repeat registers written.

#include <nuade/players/mmdc.h>

#include "mmdc_module.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>
#include <string.h>

// Memory behind the module that Paula can read: a sample may be cut short by the end of the file, and its loop runs on
// past it (the original reads the memory behind the module).
#define PADDING_BYTES 0x40000

// A note index addresses 128 words from the start of the period table.
#define PERIOD_WORDS 128
#define PERIOD_TABLE_ENTRIES 72
// Past the table the words are the replayer's variables (the first ones at the end of its code hunk): the address of
// its data block, the module address plus the position of the line decoder in the module, the run state of the decoder
// and the decoded line.
#define VARIABLES_FIRST_WORD 72
#define DATA_BLOCK_ADDRESS 0xEEB4u
#define MODULE_ADDRESS 0x10000u
#define VIBRATO_TABLE_SIZE 32
#define TEMPO_TIMERS 10
#define TEMPO_TIMER_BASE 470000 // timer value = this / tempo
#define SLOW_TEMPO_LIMIT 10     // tempos up to this one use TEMPO_TIMERS
#define INITIAL_TICK (MMDC_TICKS_PER_LINE - 1)
#define MIN_SLIDE_PERIOD 0x71
#define MAX_SLIDE_PERIOD 0x358
#define MAX_VOLUME 0x40
#define OCTAVE 12
#define MAX_TABLE_NOTE 0x3F
#define VOLUME_MASK 0x7F
#define LOOP_DUMMY_WORDS 1

#define LINE_COUNTER_START 0xFFFF // the first tick wraps it to line 0

// Three octaves, then the top octave three more times.
static const uint16_t k_periods[PERIOD_TABLE_ENTRIES] = {
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320,
    302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
    214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113, 214, 202, 190, 180, 170, 160,
    151, 143, 135, 127, 120, 113, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
};

static const uint8_t k_vibrato[VIBRATO_TABLE_SIZE] = {
    0,   24,  49,  74,  97,  120, 141, 161, 180, 197, 212, 224, 235, 244, 250, 253,
    255, 253, 250, 244, 235, 224, 212, 197, 180, 161, 141, 120, 97,  74,  49,  24,
};

// The timer value of tempos 1..10 (a tempo above 10 is beats per minute).
static const uint16_t k_tempo_timers[TEMPO_TIMERS] = {
    2417, 4833, 7250, 9666, 12083, 14500, 16916, 19332, 21436, 24163,
};

// Parameters of the Misc effect: 1..0xF0 set the tempo.
typedef enum MmdcMisc {
    MmdcMisc_NextBlock = 0x00,
    MmdcMisc_MaxTempo = 0xF0,
    MmdcMisc_RetriggerAt3 = 0xF1,
    MmdcMisc_DelayTo3 = 0xF2,
    MmdcMisc_RetriggerFrom2 = 0xF3,
    MmdcMisc_FilterOn = 0xF8,
    MmdcMisc_FilterOff = 0xF9,
    MmdcMisc_ChangePitch = 0xFD,
    MmdcMisc_StopSong = 0xFE,
    MmdcMisc_VoiceOff = 0xFF,
} MmdcMisc;

// The word at `index` of the period table: the replayer doubles the index as a byte, so only its low 7 bits count.
// The words after the table are zeros up to the first of the replayer's variables.

static uint16_t variable_word(const MmdcPlayer* player, uint32_t word) {
    const uint32_t stream = MODULE_ADDRESS + player->reader.pos;
    switch (word) {
        case VARIABLES_FIRST_WORD + 1:
            return DATA_BLOCK_ADDRESS;
        case VARIABLES_FIRST_WORD + 4:
            return (uint16_t)(stream >> 16);
        case VARIABLES_FIRST_WORD + 5:
            return (uint16_t)stream;
        case VARIABLES_FIRST_WORD + 6:
            return player->reader.literal ? 0x00FF : 0;
        case VARIABLES_FIRST_WORD + 7:
            return (uint16_t)player->reader.run_left;
        case VARIABLES_FIRST_WORD + 8:
            return (uint16_t)(player->block.tracks * MMDC_CELL_SIZE - 1);
        default:
            break;
    }
    const uint32_t line_word = word - (VARIABLES_FIRST_WORD + 9);
    if (word > VARIABLES_FIRST_WORD + 8 && line_word < sizeof(player->cells) / 2) {
        return (uint16_t)(player->cells[2 * line_word] << 8 | player->cells[2 * line_word + 1]);
    }
    return 0;
}

static uint16_t period_at(const MmdcPlayer* player, uint8_t index) {
    const uint32_t word = index & (PERIOD_WORDS - 1);
    return word < PERIOD_TABLE_ENTRIES ? k_periods[word] : variable_word(player, word);
}

static bool slides_wait(const MmdcPlayer* player) {
    return player->module.song.st_slides && (int8_t)player->slide_limit <= (int8_t)player->tick;
}

// The tempo command: 1..10 are the fixed timer values (and set the slide limit), above 10 the timer follows from the
// tempo. The low byte also replaces the one in the start tempo, which is where a restart reads it from.

static void set_tempo(MmdcPlayer* player, uint16_t tempo) {
    const uint8_t low = (uint8_t)tempo;
    player->song_tempo = (uint16_t)((player->song_tempo & 0xFF00) | low);
    if (low <= SLOW_TEMPO_LIMIT) {
        const uint8_t slot = (uint8_t)(low - 1);
        player->slide_limit = slot;
        player->timer = k_tempo_timers[slot];
    } else {
        player->timer = (uint16_t)(TEMPO_TIMER_BASE / tempo);
    }
}

// The volume of a cell: the instrument or command volume scaled by the track and master volume, as 16-bit products.

static uint8_t scale_volume(const MmdcPlayer* player, uint8_t volume, uint8_t track) {
    const uint32_t track_volume = player->module.song.track_volumes[track];
    const uint32_t master_volume = player->module.song.master_volume;
    const uint32_t by_track = (uint32_t)volume * track_volume;
    const uint32_t by_master = (by_track & 0xFFFF) * master_volume;
    return (uint8_t)(((by_master >> 4) & 0xFFFF) >> 8);
}

// Starts the sample of the voice's instrument at `note`: DMA off now, the registers set, DMA on at the end of the tick.
// An instrument without a sample record plays nothing and leaves the voice alone.

static void play_note(MmdcPlayer* player, uint8_t track, uint8_t note) {
    MmdcVoice* voice = &player->voices[track];
    const MmdcSong* song = &player->module.song;
    const MmdcSample sample = player->module.samples[voice->instrument];
    if (!sample.present) {
        return;
    }
    const MmdcInstrument instrument = song->instruments[voice->instrument];
    nuade_paula_channel_dma_off(&voice->paula);

    // Byte arithmetic: the transposes wrap, and a note outside the table is folded by octaves
    int8_t pitch = (int8_t)(note + song->play_transpose + instrument.transpose);
    while (pitch < 0) {
        pitch = (int8_t)(pitch + OCTAVE);
    }
    if (pitch > MAX_TABLE_NOTE) {
        pitch = (int8_t)(pitch - OCTAVE);
    }
    voice->dma_pending = true;
    voice->shown_note = pitch + OCTAVE - 1;
    const uint16_t period = period_at(player, (uint8_t)(pitch - 1));

    const int8_t* data = (const int8_t*)player->memory + sample.data;
    voice->paula.lc = data;
    if (instrument.repeat_words <= LOOP_DUMMY_WORDS) {
        voice->repeat_lc = player->loop_dummy;
        voice->repeat_words = LOOP_DUMMY_WORDS;
        voice->paula.len_words = (uint16_t)(sample.length >> 1);
    } else {
        voice->paula.len_words = instrument.repeat_start != 0 ? instrument.repeat_start : instrument.repeat_words;
        voice->repeat_lc = data + 2u * instrument.repeat_start;
        voice->repeat_words = instrument.repeat_words;
    }
    voice->paula.period = period;
    voice->period = (int16_t)period;
    voice->paula.volume = voice->volume & VOLUME_MASK;
}

// Writes the voice's period and volume registers (the period variable stands in for a period of 0).

static void write_registers(MmdcVoice* voice, uint16_t period) {
    voice->paula.period = period != 0 ? period : (uint16_t)voice->period;
    voice->paula.volume = voice->volume & VOLUME_MASK;
}

// The driver calls SongEnd when the play sequence runs out or the song restarts after the Stop command.

static void song_end(MmdcPlayer* player) {
    player->looped = true;
}

// The block played at a position of the play sequence: its entry, limited to the blocks the song has.

static uint16_t block_at_position(const MmdcSong* song, uint32_t position) {
    const uint32_t entry = song->play_sequence[position];
    return (uint16_t)(entry < song->block_count - 1u ? entry : song->block_count - 1u);
}

// The next position of the play sequence (or the one a jump set).

static void select_position(MmdcPlayer* player) {
    const MmdcSong* song = &player->module.song;
    if (player->position_request != MmdcPositionRequest_Jump) {
        player->position++;
    }
    if (player->position >= song->length) {
        player->position = 0;
        song_end(player);
    }
    player->block_index = block_at_position(song, player->position);
}

// The tone portamento command on a line with a note: the note is the slide target, it does not start.

static void start_portamento(MmdcPlayer* player, MmdcVoice* voice, const MmdcCell* cell) {
    if (cell->note == 0) {
        return;
    }
    const MmdcSong* song = &player->module.song;
    const int8_t pitch
        = (int8_t)(cell->note - 1 + song->play_transpose + song->instruments[voice->instrument].transpose);
    if (pitch >= 0) {
        voice->slide_target = period_at(player, (uint8_t)pitch);
        voice->slide_speed = cell->param;
    }
}

// The Misc effect of a line. Returns the note to start: the commands that change the pitch of a running note or
// delay it take the note away.

static uint8_t run_misc_cell(MmdcPlayer* player, MmdcVoice* voice, const MmdcCell* cell) {
    const uint8_t param = cell->param;
    if (param == MmdcMisc_NextBlock) {
        player->position_request = MmdcPositionRequest_NextBlock;
    } else if (param <= MmdcMisc_MaxTempo) {
        set_tempo(player, param);
    } else if (param == MmdcMisc_DelayTo3) {
        voice->note = cell->note;
        return 0;
    } else if (param == MmdcMisc_StopSong) {
        player->playing = false;
    } else if (param == MmdcMisc_ChangePitch && cell->note != 0) {
        voice->paula.period = period_at(player, (uint8_t)(cell->note - 1));
        return 0;
    }
    return cell->note;
}

// The BCD volume of the set volume command, limited to the maximum.

static uint8_t command_volume(uint8_t param) {
    const uint8_t volume = (uint8_t)((param >> 4) * 10 + (param & 0x0F));
    return volume > MAX_VOLUME ? MAX_VOLUME : volume;
}

static void run_cell(MmdcPlayer* player, uint8_t track, const MmdcCell* cell) {
    MmdcVoice* voice = &player->voices[track];
    uint8_t note = cell->note;
    voice->param = cell->param;
    if (cell->instrument != 0) {
        const uint8_t instrument = (uint8_t)(cell->instrument - 1);
        voice->instrument = instrument;
        voice->vibrato_param = 0;
        voice->volume = scale_volume(player, player->module.song.instruments[instrument].volume, track);
    }
    voice->effect = (MmdcEffect)cell->effect;
    switch (voice->effect) {
        case MmdcEffect_Misc:
            note = run_misc_cell(player, voice, cell);
            break;
        case MmdcEffect_SetVolume:
            voice->volume = scale_volume(player, command_volume(cell->param), track);
            break;
        case MmdcEffect_PositionJump:
            if (cell->param <= player->module.song.length) {
                player->position = cell->param;
                player->position_request = MmdcPositionRequest_Jump;
            }
            break;
        case MmdcEffect_TonePortamento:
            start_portamento(player, voice, cell);
            note = 0;
            break;
        default:
            break;
    }
    if (note != 0) {
        voice->note = note;
        play_note(player, track, note);
    }
}

// The first tick of a line: the line counter and the position move on, the line is decoded and its cells run.

static void run_line(MmdcPlayer* player) {
    const uint16_t next_line = (uint16_t)(player->line + 1);
    if (next_line >= player->block.lines || player->position_request != MmdcPositionRequest_None) {
        player->line = 0;
        select_position(player);
        player->block = player->module.blocks[player->block_index];
    } else {
        player->line = next_line;
    }
    player->position_request = MmdcPositionRequest_None;
    player->tick = 0;
    if (player->line == 0) {
        mmdc_line_reader_start(&player->reader, &player->image, &player->block);
    }
    mmdc_line_reader_next(&player->reader, player->block.tracks, player->cells);
    for (uint8_t track = 0; track < player->block.tracks; track++) {
        const MmdcCell cell = mmdc_read_cell(&player->cells[track * MMDC_CELL_SIZE]);
        run_cell(player, track, &cell);
    }
}

// The arpeggio note of the tick: the line's note, then the two nibbles of the parameter on ticks 0..2 and 3..5.

static uint8_t arpeggio_note(const MmdcPlayer* player, const MmdcVoice* voice) {
    switch (player->tick) {
        case 0:
        case 3:
            return (uint8_t)((voice->param & 0x0F) + voice->note);
        case 1:
        case 4:
            return (uint8_t)((voice->param >> 4) + voice->note);
        default:
            return voice->note;
    }
}

static void run_arpeggio(const MmdcPlayer* player, MmdcVoice* voice) {
    if (voice->param == 0) {
        return;
    }
    const MmdcSong* song = &player->module.song;
    const uint8_t index = (uint8_t)(arpeggio_note(player, voice) - 1 + song->play_transpose
                                    + song->instruments[voice->instrument].transpose);
    write_registers(voice, period_at(player, index));
}

static void run_slide_up(const MmdcPlayer* player, MmdcVoice* voice) {
    if (slides_wait(player)) {
        return;
    }
    voice->period = (int16_t)(voice->period - voice->param);
    if (voice->period < MIN_SLIDE_PERIOD) {
        voice->period = MIN_SLIDE_PERIOD;
    }
    write_registers(voice, (uint16_t)voice->period);
}

static void run_slide_down(const MmdcPlayer* player, MmdcVoice* voice) {
    if (slides_wait(player)) {
        return;
    }
    voice->period = (int16_t)(voice->period + voice->param);
    if (voice->period > MAX_SLIDE_PERIOD) {
        voice->period = MAX_SLIDE_PERIOD;
    }
    write_registers(voice, (uint16_t)voice->period);
}

// The period moves towards the target by the slide speed; arriving clears the target.

static void run_tone_portamento(const MmdcPlayer* player, MmdcVoice* voice) {
    if (slides_wait(player)) {
        return;
    }
    if (voice->slide_target == 0) {
        write_registers(voice, 0);
        return;
    }
    const int16_t target = (int16_t)voice->slide_target;
    int16_t period = voice->period;
    bool arrived;
    if ((uint16_t)period > voice->slide_target) {
        period = (int16_t)(period - voice->slide_speed);
        arrived = !(period > target);
    } else {
        period = (int16_t)(period + voice->slide_speed);
        arrived = period >= target;
    }
    if (arrived) {
        period = target;
        voice->slide_target = 0;
    }
    voice->period = period;
    write_registers(voice, (uint16_t)period);
}

static void run_vibrato(MmdcVoice* voice) {
    if (voice->param != 0) {
        voice->vibrato_param = voice->param;
    }
    const uint16_t depth = voice->vibrato_param & 0x0F;
    const uint16_t delta = (uint16_t)(k_vibrato[(voice->vibrato_pos >> 1) & (VIBRATO_TABLE_SIZE - 1)] * depth) >> 6;
    const uint16_t base = (uint16_t)voice->period;
    const uint16_t period = (voice->vibrato_pos & 0x40) != 0 ? (uint16_t)(base - delta) : (uint16_t)(base + delta);
    voice->vibrato_pos = (uint8_t)(voice->vibrato_pos + (voice->vibrato_param >> 4) + 1);
    write_registers(voice, period);
}

// The volume slide of A and D: a high nibble raises the volume, otherwise the parameter lowers it.

static void run_volume_slide(const MmdcPlayer* player, MmdcVoice* voice) {
    if (slides_wait(player)) {
        return;
    }
    int8_t volume = (int8_t)voice->volume;
    if ((voice->param & 0xF0) == 0) {
        volume = (int8_t)(volume - voice->param);
        if (volume < 0) {
            volume = 0;
        }
    } else {
        volume = (int8_t)(volume + (voice->param >> 4));
        if (volume > MAX_VOLUME) {
            volume = MAX_VOLUME;
        }
    }
    voice->volume = (uint8_t)volume;
    write_registers(voice, 0);
}

// The Misc commands that act on later ticks of a line: the retriggers and the voice off.

static void run_misc_tick(MmdcPlayer* player, uint8_t track, MmdcVoice* voice) {
    bool retrigger = false;
    switch (voice->param) {
        case MmdcMisc_VoiceOff:
            nuade_paula_channel_dma_off(&voice->paula);
            break;
        case MmdcMisc_RetriggerAt3:
        case MmdcMisc_DelayTo3:
            retrigger = player->tick == 3;
            break;
        case MmdcMisc_RetriggerFrom2:
            retrigger = player->tick >= 2;
            break;
        default:
            break;
    }
    if (retrigger) {
        play_note(player, track, voice->note);
    }
}

static void run_effect(MmdcPlayer* player, uint8_t track) {
    MmdcVoice* voice = &player->voices[track];
    switch (voice->effect) {
        case MmdcEffect_Arpeggio:
            run_arpeggio(player, voice);
            break;
        case MmdcEffect_SlideUp:
            run_slide_up(player, voice);
            break;
        case MmdcEffect_SlideDown:
            run_slide_down(player, voice);
            break;
        case MmdcEffect_TonePortamento:
            run_tone_portamento(player, voice);
            break;
        case MmdcEffect_Vibrato:
            run_vibrato(voice);
            break;
        case MmdcEffect_SlideUpFirstTicks:
            write_registers(voice, player->tick < 3 ? (uint16_t)(voice->period - voice->param) : 0);
            break;
        case MmdcEffect_VolumeSlideA:
        case MmdcEffect_VolumeSlideD:
            run_volume_slide(player, voice);
            break;
        case MmdcEffect_SetVolume:
            if (player->tick == 0) {
                write_registers(voice, 0);
            }
            break;
        case MmdcEffect_Misc:
            run_misc_tick(player, track, voice);
            break;
        default:
            break;
    }
}

// DMA goes on for the voices that started a note, then their repeat registers are written (the replayer waits for the
// DMA start in between).

static void start_dma(MmdcPlayer* player) {
    for (uint32_t i = 0; i < MMDC_VOICES; i++) {
        if (player->voices[i].dma_pending) {
            nuade_paula_channel_dma_on(&player->voices[i].paula);
        }
    }
    for (uint32_t i = 0; i < MMDC_VOICES; i++) {
        MmdcVoice* voice = &player->voices[i];
        if (voice->dma_pending) {
            voice->paula.lc = voice->repeat_lc;
            voice->paula.len_words = voice->repeat_words;
        }
    }
}

// InitSound / the start of the song: the first line is read by the sixth tick counted from here.

static void restart_song(MmdcPlayer* player) {
    player->playing = true;
    player->block_index = player->module.song.play_sequence[0];
    player->position = 0;
    player->line = LINE_COUNTER_START;
    player->tick = INITIAL_TICK;
    player->block = player->module.blocks[player->block_index];
    set_tempo(player, player->song_tempo);
}

bool mmdc_probe(const uint8_t* data, size_t size) {
    if (size > UINT32_MAX / 2) {
        return false;
    }
    const MmdcImage image = { .data = data, .size = (uint32_t)size };
    MmdcModule module;
    return mmdc_parse_module(&image, &module);
}

MmdcPlayer* mmdc_create(const uint8_t* data, size_t size) {
    if (size > UINT32_MAX / 2) {
        return nullptr;
    }
    MmdcPlayer* player = (MmdcPlayer*)calloc(1, sizeof(*player));
    uint8_t* memory = (uint8_t*)calloc(1, size + PADDING_BYTES);
    if (player == nullptr || memory == nullptr) {
        free(player);
        free(memory);
        return nullptr;
    }
    memcpy(memory, data, size);
    player->memory = memory;
    player->image = (MmdcImage) { .data = memory, .size = (uint32_t)size };
    if (!mmdc_parse_module(&player->image, &player->module)) {
        mmdc_destroy(player);
        return nullptr;
    }
    return player;
}

void mmdc_destroy(MmdcPlayer* player) {
    if (player != nullptr) {
        free(player->memory);
        free(player);
    }
}

bool mmdc_start(MmdcPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    memset(player->voices, 0, sizeof(player->voices));
    memset(player->cells, 0, sizeof(player->cells));
    memset(&player->reader, 0, sizeof(player->reader));
    player->song_tempo = player->module.song.tempo;
    player->slide_limit = player->module.song.slide_limit;
    player->position_request = MmdcPositionRequest_None;
    player->looped = false;
    for (uint32_t i = 0; i < MMDC_VOICES; i++) {
        player->voices[i].shown_note = -1;
        nuade_voice_stop(voices, i);
    }
    restart_song(player);
    return true;
}

MmdcTickResult mmdc_tick(MmdcPlayer* player, NuadeVoices* voices) {
    // The CIA has reloaded the timer before the interrupt: a new value shows from the tick after the next one
    const uint32_t length = player->timer;
    player->looped = false;
    if (!player->playing) {
        song_end(player);
        restart_song(player);
    } else {
        for (uint32_t i = 0; i < MMDC_VOICES; i++) {
            player->voices[i].dma_pending = false;
        }
        player->tick++;
        if (player->tick == MMDC_TICKS_PER_LINE) {
            run_line(player);
        }
        for (uint8_t track = 0; track < player->block.tracks; track++) {
            run_effect(player, track);
        }
        start_dma(player);
    }
    for (uint32_t i = 0; i < MMDC_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
    }
    return (MmdcTickResult) { .looped = player->looped, .length = length };
}

MmdcVoiceState mmdc_voice_state(const MmdcPlayer* player, uint32_t voice) {
    if (voice >= MMDC_VOICES) {
        return (MmdcVoiceState) { .note = -1, .instrument = -1 };
    }
    const MmdcVoice* v = &player->voices[voice];
    return (MmdcVoiceState) { .note = v->shown_note, .instrument = v->shown_note >= 0 ? v->instrument : -1 };
}

MmdcPosition mmdc_position(const MmdcPlayer* player) {
    const bool started = player->line != LINE_COUNTER_START;
    return (MmdcPosition) { .order = player->position,
                            .block = player->block_index,
                            .line = started ? player->line : 0,
                            .lines = player->block.lines,
                            .tick = player->tick };
}

bool mmdc_block_shape(const MmdcPlayer* player, uint32_t order, MmdcBlockShape* shape) {
    const MmdcSong* song = &player->module.song;
    if (order >= song->length) {
        return false;
    }
    const uint16_t block = block_at_position(song, order);
    *shape = (MmdcBlockShape) { .block = block, .lines = player->module.blocks[block].lines };
    return true;
}

MmdcCell mmdc_pattern_cell(const MmdcPlayer* player, uint32_t order, uint32_t voice, uint32_t line) {
    MmdcBlockShape shape;
    if (!mmdc_block_shape(player, order, &shape)) {
        return (MmdcCell) { 0 };
    }
    return mmdc_block_cell(&player->image, &player->module.blocks[shape.block], line, voice);
}
