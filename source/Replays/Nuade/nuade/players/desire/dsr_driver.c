// SPDX-License-Identifier: MIT
// Desire player: the replayer that every module carries, one function per routine of its code. A tick runs the voice
// step of each voice in turn; a voice step starts or stops the voice, or runs the sequencer, envelope, volume, slide,
// pitch modulation and period stages.

#include "dsr_internal.h"
#include "nuade_tracker.h"
#include <string.h>

#define COMMAND_GUARD 4096 // commands a voice reads in a tick; the replayer would hang on a track that never ends
#define MODULATION_GUARD 512
#define MASTER_VOLUME_MAX 0xFF
#define FADE_START 0xF000
#define ENVELOPE_MAX 0xFF
#define VOICE_3 (DSR_VOICES - 1)
#define SHARED_CHANNEL VOICE_3  // voice 3 and the sound effect both play on this channel
#define DUMMY_CHANNEL (-1)      // voice 3 while the sound effect has its channel
#define PITCH_BIAS 0x10         // added to the pitch before it is split into octave and step
#define NOTE_SHIFT 4            // the pitch of a note: note << 4, which the period stage halves
#define MODULATION_END 0x7F     // a modulation step that repeats the previous one
#define MODULATION_BASE_SHIFT 3 // the command's value * 8 is the offset of a modulation sequence
#define MODULATION_ENTRIES 8
#define HOST_PERIOD 150 // what DeliTracker leaves in AUDxPER before the replayer runs

typedef enum DsrCommand {
    DsrCommand_Speed = 0x80,
    DsrCommand_LoopStart = 0x81,
    DsrCommand_LoopEnd = 0x82,
    DsrCommand_NextTrack = 0x83,
    DsrCommand_Transpose = 0x84,
    DsrCommand_Envelope = 0x85,
    DsrCommand_SlideSpeed = 0x86,
    DsrCommand_Modulation = 0x87,
    DsrCommand_Instrument = 0x88,
    DsrCommand_Release = 0x89,
    DsrCommand_PitchOffset = 0x8A,
    DsrCommand_Volume = 0x8B,
    DsrCommand_Jump = 0x8C,
    DsrCommand_Stop = 0x8D,
    DsrCommand_Fade = 0x8E,
    DsrCommand_Arpeggio = 0x8F,
} DsrCommand;

// Commands from 0x90: the high nibble selects the command (bits 4 to 6), the low nibble is its value.
typedef enum DsrNibbleCommand {
    DsrNibbleCommand_ModulationTable = 0x10,
    DsrNibbleCommand_KeepEnvelope = 0x20,
    DsrNibbleCommand_ArpeggioTable = 0x30,
    DsrNibbleCommand_ArpeggioAdd = 0x40,
} DsrNibbleCommand;

#define COMMAND_NIBBLE_MASK 0x70
#define COMMAND_NOTE_LIMIT 0x80

static NuadePaulaChannel* channel_of(DsrPlayer* player, int channel) {
    return channel == DUMMY_CHANNEL ? &player->dummy : &player->channels[channel];
}

// AUDxLC / AUDxLEN. A sample plays zeros past the module's end; one that starts past the module's memory (only voice
// 3's saved registers as the header leaves them can) plays nothing.

static void set_sample(const DsrPlayer* player, NuadePaulaChannel* channel, uint32_t offset, uint16_t length_words) {
    const DsrMemory* m = &player->module.memory;
    channel->lc = offset <= m->capacity ? (const int8_t*)m->data + offset : nullptr;
    channel->len_words = length_words;
}

static void set_low_byte(uint16_t* word, uint8_t value) {
    *word = (uint16_t)((*word & 0xFF00) | value);
}

// The pitch (note << 4, plus the sum of the offsets) becomes a period: the octave halves the table's entry.

static uint16_t pitch_to_period(const DsrModule* module, uint16_t pitch_sum) {
    const uint16_t octave = pitch_sum / DSR_PERIODS;
    uint16_t period = dsr_read_period(module, pitch_sum % DSR_PERIODS);
    if ((octave & 0xFF) != 0) {
        const unsigned count = octave & 63u; // a 68000 shift count is taken modulo 64
        period = count < 16 ? (uint16_t)(period >> count) : 0;
    }
    return (uint16_t)(period >> 1);
}

// The eagleplayer's end flags: the stop command of the voice on a channel clears the channel's flag; when all four are
// clear the song has ended.

static void flag_voice_stopped(DsrPlayer* player, int channel) {
    if (channel != DUMMY_CHANNEL) {
        player->voice_running[channel] = false;
    }
    for (int i = 0; i < DSR_VOICES; i++) {
        if (player->voice_running[i]) {
            return;
        }
    }
    for (int i = 0; i < DSR_VOICES; i++) {
        player->voice_running[i] = true;
    }
    player->song_ended = true;
}

static void start_voice(DsrPlayer* player, DsrVoice* voice, NuadePaulaChannel* channel) {
    voice->modulation_pos = 0;
    voice->modulation_base = 0;
    voice->mute_ticks = 0;
    voice->dma_pending = false;
    voice->keep_envelope = false;
    voice->level = 0;
    voice->envelope = DsrEnvelope_Release;
    voice->volume = ENVELOPE_MAX;
    voice->counter = 1;
    voice->pitch_offset = 0;
    voice->slide_speed = 0;
    voice->transpose = 0;
    player->master_volume = MASTER_VOLUME_MAX;
    player->fade = 0;
    channel->volume = 0;
}

// The order table index of a request: given directly, or the entry of the voice's song list.

static uint8_t order_index(const DsrPlayer* player, const DsrVoice* voice, DsrRequest request) {
    if (!request.from_song_list) {
        return request.index;
    }
    return dsr_read_song_list(&player->module, (uint16_t)(voice->order_base + request.index));
}

static void start_track(const DsrPlayer* player, DsrVoice* voice, uint8_t index) {
    voice->order_pos = (uint16_t)(index * DSR_ORDER_ENTRY);
    voice->pos = dsr_read_order(&player->module, voice->order_pos);
    voice->order_pos += DSR_ORDER_ENTRY;
}

// A note: the pitch to slide to, and the channel's DMA off now and back on at the next tick.

static void start_note(DsrPlayer* player, DsrVoice* voice, uint8_t note) {
    const uint8_t semitone = (uint8_t)(note + voice->transpose + voice->instrument_transpose);
    voice->target = (uint16_t)(semitone << NOTE_SHIFT);
    if (voice->dma_enabled) {
        nuade_paula_channel_dma_off(&player->channels[voice->dma_channel]);
        voice->dma_pending = true;
    }
    const uint16_t pitch_sum = (uint16_t)((voice->target >> 1) + voice->pitch_base + voice->pitch_offset + PITCH_BIAS);
    voice->display_note = nuade_period_to_note(pitch_to_period(&player->module, pitch_sum));
}

// Instrument: the sample registers of the voice's channel and the voice's transposition.

static void set_instrument(DsrPlayer* player, DsrVoice* voice, NuadePaulaChannel* channel, uint8_t number) {
    const DsrInstrument instrument = dsr_read_instrument(&player->module, number);
    voice->instrument_transpose = instrument.transpose;
    voice->saved_sample = instrument.sample;
    voice->saved_length = instrument.length;
    set_sample(player, channel, instrument.sample, instrument.length);
    voice->display_instrument = number;
}

// The commands from 0x90: they set a field and the sequencer reads on, except those that the replayer ends the tick's
// sequencer with (values 0x50 to 0x70). Returns false for those.

static bool run_nibble_command(DsrVoice* voice, uint8_t command) {
    const uint8_t value = command & 0xF;
    switch (command & COMMAND_NIBBLE_MASK) {
        case DsrNibbleCommand_ModulationTable:
            voice->modulation_base = (uint16_t)(value << MODULATION_BASE_SHIFT);
            voice->modulation_length = MODULATION_ENTRIES;
            return true;
        case DsrNibbleCommand_KeepEnvelope:
            voice->keep_envelope = value != 0;
            return true;
        case DsrNibbleCommand_ArpeggioTable:
        case DsrNibbleCommand_ArpeggioAdd:
            return true; // the arpeggio stage is never called
        default:
            return false;
    }
}

static void clear_voice_states(DsrPlayer* player) {
    for (int i = 0; i < dsr_record_count(&player->module.version); i++) {
        player->voices[i].mode = DsrMode_Stop;
        player->voices[i].request = (DsrRequest) { 0 };
    }
}

// One command 0x80 to 0x8F at `at`; the voice's position is already past the command byte and the command moves it
// past its arguments. The sequencer reads on after every command.

static void run_command(DsrPlayer* player, DsrVoice* voice, NuadePaulaChannel* channel, uint8_t command, uint16_t at) {
    const DsrCommandArgs args = dsr_read_command_args(&player->module, at);
    const uint8_t arg = args.byte[0];
    switch (command) {
        case DsrCommand_Speed:
            voice->speed = arg;
            voice->pos++;
            break;
        case DsrCommand_LoopStart:
            voice->loop_count = arg;
            voice->pos++;
            voice->loop_pos = voice->pos;
            break;
        case DsrCommand_LoopEnd:
            if (voice->loop_count != 0) {
                voice->loop_count--;
                if (voice->loop_count == 0) {
                    break;
                }
            }
            voice->pos = voice->loop_pos;
            break;
        case DsrCommand_NextTrack:
            voice->pos = dsr_read_order(&player->module, voice->order_pos);
            voice->order_pos += DSR_ORDER_ENTRY;
            break;
        case DsrCommand_Transpose:
            voice->transpose = arg;
            voice->pos++;
            break;
        case DsrCommand_Envelope:
            set_low_byte(&voice->attack, args.byte[0]);
            voice->hold = args.byte[1];
            set_low_byte(&voice->decay, args.byte[2]);
            set_low_byte(&voice->sustain, args.byte[3]);
            set_low_byte(&voice->release, args.byte[4]);
            voice->pos += DSR_COMMAND_ARGS;
            break;
        case DsrCommand_SlideSpeed:
            set_low_byte(&voice->slide_speed, arg);
            voice->pos++;
            break;
        case DsrCommand_Modulation:
            set_low_byte(&voice->modulation_base, args.byte[0]);
            voice->modulation_length = args.byte[1];
            voice->pos += 2;
            break;
        case DsrCommand_Instrument:
            set_instrument(player, voice, channel, arg);
            voice->pos++;
            break;
        case DsrCommand_Release:
            voice->envelope = DsrEnvelope_Release;
            break;
        case DsrCommand_PitchOffset:
            voice->pitch_offset = (int8_t)arg;
            voice->pos++;
            break;
        case DsrCommand_Volume:
            set_low_byte(&voice->volume, arg);
            voice->pos++;
            break;
        case DsrCommand_Jump:
            start_track(player, voice, order_index(player, voice, dsr_decode_request(arg)));
            break;
        case DsrCommand_Stop:
            voice->mode = DsrMode_Stop;
            break;
        case DsrCommand_Fade:
            voice->mode = DsrMode_Fade;
            break;
        case DsrCommand_Arpeggio:
            voice->pos += 2; // its two values feed the arpeggio stage, which is never called
            break;
        default:
            break;
    }
}

// The sequencer. Returns false when the tick read a note or a rest: the voice's other stages then wait for the next
// tick.

static bool run_sequencer(DsrPlayer* player, DsrVoice* voice, NuadePaulaChannel* channel) {
    const DsrMemory* m = &player->module.memory;
    if (voice->request.pending) {
        start_track(player, voice, order_index(player, voice, voice->request));
        voice->request = (DsrRequest) { 0 };
    }
    voice->counter--;
    if (voice->counter != 0) {
        return true;
    }
    for (int commands = 0; commands < COMMAND_GUARD; commands++) {
        const uint8_t byte = dsr_u8(m, voice->pos);
        if (byte < COMMAND_NOTE_LIMIT) {
            if (byte != 0) {
                start_note(player, voice, byte);
            }
            voice->counter = voice->speed;
            voice->pos++;
            return false;
        }
        const uint16_t at = voice->pos;
        voice->pos++;
        if ((byte & COMMAND_NIBBLE_MASK) != 0) {
            if (!run_nibble_command(voice, byte)) {
                return true;
            }
        } else {
            run_command(player, voice, channel, byte, at);
        }
    }
    return true;
}

// The envelope level (0 to 255) by state: attack up, hold, decay down to the sustain level, sustain, or release.

static void run_envelope(DsrVoice* voice) {
    switch (voice->envelope) {
        case DsrEnvelope_Release: {
            const int16_t level = (int16_t)(voice->level - voice->release);
            voice->level = level > 0 ? (uint16_t)level : 0;
            break;
        }
        case DsrEnvelope_Attack: {
            const int16_t level = (int16_t)(voice->level + voice->attack);
            if (level > ENVELOPE_MAX) {
                voice->envelope = DsrEnvelope_Hold;
                voice->level = ENVELOPE_MAX;
                voice->hold_count = 0;
            } else {
                voice->level = (uint16_t)level;
            }
            break;
        }
        case DsrEnvelope_Hold:
            if (voice->hold_count == voice->hold) {
                voice->envelope = DsrEnvelope_Decay;
            } else {
                voice->hold_count++;
            }
            break;
        case DsrEnvelope_Decay: {
            const int16_t level = (int16_t)(voice->level - voice->decay);
            if (level < (int16_t)voice->sustain) {
                voice->envelope = DsrEnvelope_Sustain;
                voice->level = voice->sustain;
            } else {
                voice->level = (uint16_t)level;
            }
            break;
        }
        case DsrEnvelope_Sustain:
            voice->level = voice->sustain;
            break;
        default:
            break;
    }
}

// AUDxVOL = level * voice volume * master volume >> 18. Each voice that writes its volume also steps the fade; at its
// end every voice is told to stop.

static void write_volume(DsrPlayer* player, DsrVoice* voice, NuadePaulaChannel* channel) {
    const uint32_t voice_level = ((uint32_t)(voice->level & 0xFF) * voice->volume) & 0xFFFF;
    const uint32_t product = voice_level * player->master_volume;
    channel->volume = (uint8_t)(((product >> 16) & 0xFFFF) >> 2);

    if (player->fade != 0) {
        player->master_volume = (uint16_t)((player->master_volume & 0xFF00) | (player->fade >> 8));
        player->fade = (uint16_t)(player->fade - player->module.version.fade_step);
        if (player->fade == 0) {
            clear_voice_states(player);
        }
    }
}

static void run_slide(DsrVoice* voice) {
    if (voice->slide_speed == 0) {
        voice->pitch = voice->target;
        return;
    }
    const int16_t target = (int16_t)voice->target;
    const int16_t current = (int16_t)voice->pitch;
    if (current > target) {
        const int16_t next = (int16_t)(voice->pitch - voice->slide_speed);
        voice->pitch = (uint16_t)(next >= target ? next : target);
    } else {
        const int16_t next = (int16_t)(voice->pitch + voice->slide_speed);
        voice->pitch = (uint16_t)(next <= target ? next : target);
    }
}

// Pitch modulation: the next step of the modulation table. A 0x7F step is the end of the table: the position backs
// up one and the step before it repeats. The index is a byte position plus a word base; the replayer's register keeps
// the previous sum's high byte when it goes round again.

static void run_modulation(const DsrModule* module, DsrVoice* voice) {
    uint16_t index = 0;
    for (int tries = 0; tries < MODULATION_GUARD; tries++) {
        index = (uint16_t)(((index & 0xFF00) | voice->modulation_pos) + voice->modulation_base);
        const int8_t step = dsr_read_modulation(module, index);
        if ((uint8_t)step == MODULATION_END) {
            voice->modulation_pos--;
            continue;
        }
        voice->modulation = step;
        const uint8_t next = (uint8_t)(voice->modulation_pos + 1);
        voice->modulation_pos = next == voice->modulation_length ? 0 : next;
        return;
    }
}

static void write_period(const DsrModule* module, const DsrVoice* voice, NuadePaulaChannel* channel) {
    const uint16_t pitch_sum
        = (uint16_t)((voice->pitch >> 1) + voice->pitch_base + voice->modulation + voice->pitch_offset + PITCH_BIAS);
    channel->period = pitch_to_period(module, pitch_sum);
}

// The stages of a running voice, in the replayer's order.

static void run_voice(DsrPlayer* player, DsrVoice* voice, NuadePaulaChannel* channel) {
    if (!run_sequencer(player, voice, channel)) {
        return;
    }
    run_envelope(voice);
    write_volume(player, voice, channel);
    run_slide(voice);
    run_modulation(&player->module, voice);
    write_period(&player->module, voice, channel);
}

// One voice's step. `channel` is the Paula channel whose registers the voice writes.

static void step_voice(DsrPlayer* player, int voice_index, int channel_index) {
    DsrVoice* voice = &player->voices[voice_index];
    NuadePaulaChannel* channel = channel_of(player, channel_index);
    if (voice->mode == DsrMode_Idle) {
        return;
    }
    if (voice->dma_pending) {
        nuade_paula_channel_dma_on(&player->channels[voice->dma_channel]);
        voice->dma_pending = false;
        if (!voice->keep_envelope) {
            voice->modulation_pos = 0;
            voice->level = 0;
            voice->envelope = DsrEnvelope_Attack;
        }
    }
    switch (voice->mode) {
        case DsrMode_Running:
            run_voice(player, voice, channel);
            break;
        case DsrMode_Stop:
            channel->volume = 0;
            voice->mode = DsrMode_Idle;
            flag_voice_stopped(player, channel_index);
            break;
        case DsrMode_Fade:
            player->fade = FADE_START;
            voice->mode = DsrMode_Running;
            break;
        case DsrMode_Start:
            voice->mode = DsrMode_Running;
            start_voice(player, voice, channel);
            break;
        case DsrMode_Idle:
            break;
    }
}

// Voice 3 and the sound effect share Paula channel 3. While the effect runs, voice 3 writes to registers nobody reads;
// after it, voice 3 is muted for a few ticks (it cannot start DMA) and gets its sample registers back.

static void step_voice_3_and_effect(DsrPlayer* player) {
    DsrVoice* voice = &player->voices[VOICE_3];
    const DsrVoice* effect = &player->voices[DSR_EFFECT_VOICE];
    voice->dma_enabled = true;
    if (voice->mute_ticks != 0) {
        voice->mute_ticks--;
        voice->dma_enabled = false;
        if (effect->mode == DsrMode_Idle) {
            set_sample(player, &player->channels[SHARED_CHANNEL], voice->saved_sample, voice->saved_length);
        }
    }
    step_voice(player, VOICE_3, effect->mode == DsrMode_Idle ? SHARED_CHANNEL : DUMMY_CHANNEL);
    if (effect->mode == DsrMode_Idle) {
        return;
    }
    step_voice(player, DSR_EFFECT_VOICE, SHARED_CHANNEL);
    voice->dma_enabled = false;
    if (player->module.version.effect_resets_envelope) {
        voice->envelope = DsrEnvelope_Release;
        voice->level = 0;
    }
    voice->mute_ticks = player->module.version.effect_mute_ticks;
}

void dsr_replayer_tick(DsrPlayer* player) {
    for (int voice = 0; voice < DSR_VOICES - 1; voice++) {
        step_voice(player, voice, voice);
    }
    if (player->module.version.has_effect) {
        step_voice_3_and_effect(player);
    } else {
        step_voice(player, VOICE_3, VOICE_3);
    }
}

// DeliTracker points every channel at a silent word before the replayer runs; a track that starts a note before it
// names an instrument plays that.

static const int8_t HOST_SAMPLE[2];

// InitSound: each song voice starts the track of the subsong (1 based) in its song list.

void dsr_replayer_start(DsrPlayer* player, int subsong) {
    memset(player->channels, 0, sizeof(player->channels));
    memset(&player->dummy, 0, sizeof(player->dummy));
    for (int i = 0; i < DSR_VOICES; i++) {
        player->channels[i].lc = HOST_SAMPLE;
        player->channels[i].len_words = 1;
        player->channels[i].period = HOST_PERIOD;
    }
    for (int i = 0; i < dsr_record_count(&player->module.version); i++) {
        player->voices[i] = dsr_read_voice(&player->module, i);
    }
    for (int i = 0; i < DSR_VOICES; i++) {
        player->voices[i].mode = DsrMode_Start;
        player->voices[i].request
            = (DsrRequest) { .pending = true, .from_song_list = true, .index = (uint8_t)(subsong + 1) };
        player->voice_running[i] = true;
    }
    player->master_volume = 0;
    player->fade = 0;
    player->song_ended = false;
    player->subsong = subsong;
}
