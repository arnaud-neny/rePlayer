// SPDX-License-Identifier: MIT
// Dave Lowe player: the voice engine, translated from the "Dave Lowe New" eagleplayer (which carries two copies of it)
// and checked against the engines of the game drivers.
//
// A voice plays a list of patterns. A pattern is a stream of words: a word above 0x64 is a note (the period, then the
// length in ticks), a word up to 0x64 is a command (four times its index in the command table), and the commands
// carry their operands after it. One interrupt runs the four voices in order; each does at most one note step and
// then its slide, vibrato and volume envelope.
//
// Some game drivers also play sound effects on channels 2 and 3 (see DlEffectChannel). The effect has its own voice,
// built like the music voices; the music voice of the channel keeps running, but against registers that no channel
// reads.

#include "dl_internal.h"
#include "nuade_tracker.h"
#include <string.h>

// A word at or below this is a command
#define MAX_COMMAND_WORD 0x64
// The fade level that leaves the volume alone
#define FADE_FULL 0x40
// A pattern that reads this many words without a note is broken (the engine would hang the interrupt)
#define MAX_COMMANDS_PER_NOTE 0x10000
// The period an effect leaves in its channel when it ends
#define EFFECT_END_PERIOD 1

typedef enum StepResult {
    StepResult_Continue, // read the next word of the pattern
    StepResult_Tail,     // the voice is done with the pattern for this tick; run the volume envelope
    StepResult_Done,     // the voice is done for this tick
} StepResult;

static void dma_on(const DlVoice* v) {
    nuade_paula_channel_dma_on(v->bus.dma_on);
}

static void dma_off(const DlVoice* v) {
    if (v->bus.dma_off != nullptr) {
        nuade_paula_channel_dma_off(v->bus.dma_off);
    }
}

// The eagleplayer asks for the effect of the subsong when the song starts and every time it loops; a driver without
// effect machinery ignores the request.
static void request_subsong_effect(DlEngine* engine) {
    engine->effect_request = engine->variant.effect_table != 0 ? engine->subsong.effect_request : 0;
}

// A voice reached the end of its pattern list (or a rest with a negative length). Effects do not count. When the song
// loops, the effect is requested again.
static void list_ended(DlEngine* engine, const DlVoice* v) {
    if (v->effect) {
        return;
    }
    DlLoopTracker* loop = &engine->loop;
    if (loop->ends_needed[v->channel] > 0) {
        loop->ends_needed[v->channel]--;
    }
    for (int i = 0; i < DL_VOICES; i++) {
        if (loop->ends_needed[i] != 0) {
            return;
        }
    }
    for (int i = 0; i < DL_VOICES; i++) {
        loop->ends_needed[i] = 1;
    }
    loop->looped = true;
    request_subsong_effect(engine);
}

static DlVoice new_voice(const DlEngine* engine, uint32_t channel, bool effect) {
    const DlBuiltin* builtin = &engine->image->builtin;
    return (DlVoice) { .channel = channel,
                       .effect = effect,
                       .envelope = builtin->silent_envelope,
                       .envelope_start = builtin->silent_envelope,
                       .sequence = builtin->idle_sequence,
                       .list = builtin->empty_list,
                       .list_start = effect ? 0 : engine->subsong.lists[channel],
                       .note = -1 };
}

// The init routine of the driver: every voice gets the idle sequence and its list
static void reset_voices(DlEngine* engine) {
    for (uint32_t i = 0; i < DL_VOICES; i++) {
        engine->voices[i] = new_voice(engine, i, false);
        nuade_paula_channel_dma_off(&engine->channels[i]);
        engine->channels[i].volume = 0;
    }
    for (uint32_t i = 0; i < DL_EFFECT_CHANNELS; i++) {
        engine->effects[i] = (DlEffectChannel) { .voice = new_voice(engine, DL_FIRST_EFFECT_CHANNEL + i, true) };
    }
}

// The Deli engine starts a song with a fade-in when the subsong asks for one
static void reset_fade(DlEngine* engine) {
    const uint16_t speed = engine->subsong.fade_speed;
    engine->fade = (DlFade) {
        .level = speed != 0 ? 0 : FADE_FULL, .step = speed != 0 ? 1 : 0, .reload = speed, .counter = speed
    };
}

// Commands

static StepResult command_sample(const DlImage* image, DlVoice* v, DlCursor* cursor) {
    const uint32_t record = dlc_u32(cursor);
    const DlSampleRecord r = dl_read_sample_record(image, record);
    v->sample_record = record;
    if (r.state == DlSampleState_Immediate) {
        // the immediate form: the next repeat (or the next DMA start) plays it
        v->sample_state = DlSampleState_Immediate;
        v->bus.registers->lc = dli_sample(image, r.address);
        v->bus.registers->len_words = r.length_words;
        return StepResult_Continue;
    }
    v->sample_state = r.state == DlSampleState_Started ? DlSampleState_Started : DlSampleState_Looping;
    v->sample_address = r.address;
    v->sample_length_words = r.length_words;
    v->loop_address = r.loop_address;
    v->loop_length_words = r.loop_length_words;
    return StepResult_Continue;
}

static StepResult command_next_pattern(DlEngine* engine, DlVoice* v, DlCursor* cursor) {
    const DlImage* image = engine->image;
    if (dli_u32(image, v->list) == 0) {
        // a 0 entry ends the list; starting it again is how a song loops
        v->list = v->list_start;
        list_ended(engine, v);
    }
    cursor->address = dli_u32(image, v->list);
    v->list += 4;
    return StepResult_Continue;
}

static StepResult command_envelope(const DlEngine* engine, DlVoice* v, DlCursor* cursor) {
    DlCursor header = { .image = engine->image, .address = dlc_u32(cursor) };
    if (engine->variant.envelope_header_words >= 1) {
        v->volume_cut = dlc_u16(&header);
    }
    if (engine->variant.envelope_header_words >= 2) {
        v->volume_bonus = dlc_u16(&header);
    }
    v->envelope_start = header.address;
    return StepResult_Continue;
}

static StepResult command_slide(DlVoice* v, DlCursor* cursor) {
    const uint16_t mode = dlc_u16(cursor);
    v->slide.mode = mode == 0 ? DlSlideMode_Off : (mode == 1 ? DlSlideMode_Up : DlSlideMode_Down);
    v->slide.step = dlc_u16(cursor);
    v->slide.target = dlc_u16(cursor);
    v->slide.delay = dlc_u16(cursor);
    return StepResult_Continue;
}

static StepResult command_vibrato(const DlVariant* variant, DlVoice* v, DlCursor* cursor) {
    DlVibrato* vib = &v->vibrato;
    vib->active = true;
    vib->phase = DlVibratoPhase_FirstDown;
    vib->counter = dlc_u16(cursor);
    vib->reload = dlc_u16(cursor);
    vib->up = dlc_u16(cursor);
    vib->down = dlc_u16(cursor);
    const uint16_t delay = dlc_u16(cursor);
    const uint16_t delay_reload = dlc_u16(cursor);
    if (variant->vibrato_delay) {
        vib->delay = delay;
        vib->delay_reload = delay_reload;
    }
    return StepResult_Continue;
}

// A rest of `length` ticks (a negative length also tells the loop tracker): silent envelope, volume 0
static StepResult command_rest(DlEngine* engine, DlVoice* v, DlCursor* cursor) {
    const uint16_t length = dlc_u16(cursor);
    if ((length & 0x8000) != 0) {
        list_ended(engine, v);
    }
    v->count = (uint16_t)(length - 1);
    v->sequence = cursor->address;
    v->envelope = engine->image->builtin.silent_envelope;
    v->bus.registers->volume = 0;
    return StepResult_Tail;
}

// Stop the channel: DMA off, volume 0. A music voice reads the command again on every tick; a sound effect is over, and
// its channel goes back to the music.
static StepResult command_stop(DlEngine* engine, DlVoice* v) {
    dma_off(v);
    v->bus.registers->volume = 0;
    if (v->effect) {
        v->bus.registers->period = EFFECT_END_PERIOD;
        engine->effects[v->channel - DL_FIRST_EFFECT_CHANNEL].active = false;
    }
    return StepResult_Done;
}

static StepResult command_jump(const DlImage* image, DlCursor* cursor) {
    cursor->address = dli_u32(image, cursor->address);
    return StepResult_Continue;
}

// operands: a counter word, the address to go to while it counts down, the address to go to when it is 0
static StepResult command_repeat(DlImage* image, DlCursor* cursor) {
    const uint32_t counter_at = cursor->address;
    const uint16_t counter = dli_u16(image, counter_at);
    if (counter == 0) {
        cursor->address = dli_u32(image, counter_at + 6);
    } else {
        dli_set16(image, counter_at, (uint16_t)(counter - 1));
        cursor->address = dli_u32(image, counter_at + 2);
    }
    return StepResult_Continue;
}

static StepResult command_restart(DlEngine* engine) {
    reset_voices(engine);
    return StepResult_Done;
}

static StepResult command_song_end(DlEngine* engine) {
    engine->loop.looped = true;
    reset_fade(engine);
    return command_restart(engine);
}

static StepResult run_command(DlEngine* engine, DlVoice* v, DlCursor* cursor, uint16_t word) {
    const uint32_t index = word >> 2;
    const bool valid = (word & 3) == 0 && index < engine->variant.command_count;
    switch (valid ? engine->variant.commands[index] : DlCommand_Freeze) {
        case DlCommand_Sample:
            return command_sample(engine->image, v, cursor);
        case DlCommand_NextPattern:
            return command_next_pattern(engine, v, cursor);
        case DlCommand_Envelope:
            return command_envelope(engine, v, cursor);
        case DlCommand_Slide:
            return command_slide(v, cursor);
        case DlCommand_Vibrato:
            return command_vibrato(&engine->variant, v, cursor);
        case DlCommand_SlideOff:
            v->slide.mode = DlSlideMode_Off;
            return StepResult_Continue;
        case DlCommand_VibratoOff:
            v->vibrato.active = false;
            return StepResult_Continue;
        case DlCommand_Rest:
            return command_rest(engine, v, cursor);
        case DlCommand_Stop:
            return command_stop(engine, v);
        case DlCommand_Filter:
            return StepResult_Continue;
        case DlCommand_BonusOn:
            v->bonus_active = true;
            return StepResult_Continue;
        case DlCommand_Jump:
            return command_jump(engine->image, cursor);
        case DlCommand_Repeat:
            return command_repeat(engine->image, cursor);
        case DlCommand_Restart:
            return command_restart(engine);
        case DlCommand_SongEnd:
            return command_song_end(engine);
        case DlCommand_Freeze:
            break;
    }
    return StepResult_Done;
}

// Notes

static void start_note(const DlEngine* engine, DlVoice* v, DlCursor* cursor, uint16_t period) {
    NuadePaulaChannel* registers = v->bus.registers;
    v->period = period;
    registers->period = period;
    v->note = nuade_period_to_note(period);
    v->count = (uint16_t)(dlc_u16(cursor) - 1);
    v->sequence = cursor->address;
    v->envelope = v->envelope_start;
    v->vibrato.delay = v->vibrato.delay_reload;
    if (v->vibrato.active) {
        v->vibrato.phase = DlVibratoPhase_FirstDown;
    }
    if (v->sample_state != DlSampleState_Immediate) {
        v->sample_state = DlSampleState_Started;
        registers->lc = dli_sample(engine->image, v->sample_address);
        registers->len_words = v->sample_length_words;
    }
    if (!engine->variant.dma_every_tick) {
        dma_on(v);
    }
}

// Read the pattern until a note starts or a command ends the tick
static StepResult read_pattern(DlEngine* engine, DlVoice* v) {
    DlCursor cursor = { .image = engine->image, .address = v->sequence };
    v->bonus_active = false;
    for (int i = 0; i < MAX_COMMANDS_PER_NOTE; i++) {
        const uint16_t word = dlc_u16(&cursor);
        if (word > MAX_COMMAND_WORD) {
            start_note(engine, v, &cursor, word);
            return StepResult_Tail;
        }
        const StepResult result = run_command(engine, v, &cursor, word);
        if (result != StepResult_Continue) {
            return result;
        }
    }
    return StepResult_Done;
}

// The tick of a running note

// The pitch slide: moves the period by `step` per tick towards the target, then stops
static bool slide_period(DlSlide* slide, uint16_t* period) {
    if (slide->mode == DlSlideMode_Off) {
        return false;
    }
    if (slide->delay != 0) {
        slide->delay--;
        return false;
    }
    if (slide->mode == DlSlideMode_Up) {
        *period = (uint16_t)(*period + slide->step);
        if (slide->target < *period) {
            *period = slide->target;
            slide->mode = DlSlideMode_Off;
        }
    } else {
        *period = (uint16_t)(*period - slide->step);
        if (slide->target >= *period) {
            *period = slide->target;
            slide->mode = DlSlideMode_Off;
        }
    }
    return true;
}

// The vibrato: after the delay, every `reload` + 1 ticks the period steps down twice, then up twice
static bool vibrato_period(DlVibrato* vib, uint16_t* period) {
    if (!vib->active) {
        return false;
    }
    if (vib->delay != 0) {
        vib->delay--;
        return false;
    }
    if (vib->counter != 0) {
        vib->counter--;
        return false;
    }
    vib->counter = vib->reload;
    const bool down = vib->phase == DlVibratoPhase_FirstDown || vib->phase == DlVibratoPhase_SecondDown;
    *period = (uint16_t)(down ? *period - vib->down : *period + vib->up);
    vib->phase = vib->phase == DlVibratoPhase_SecondUp ? DlVibratoPhase_FirstDown : (DlVibratoPhase)(vib->phase + 1);
    return true;
}

// The volume scaled by the fade level (the balance of 0x40 leaves it alone); every volume write also steps the fade
static uint16_t faded_volume(DlFade* fade, uint16_t value) {
    const uint16_t scaled = (uint16_t)((uint16_t)((uint32_t)value * fade->level) >> 6);
    if (fade->level != FADE_FULL) {
        if (fade->counter != 0) {
            fade->counter--;
        } else {
            fade->counter = fade->reload;
            fade->level = (uint8_t)(fade->level + fade->step);
        }
    }
    return scaled;
}

// The next envelope value is the volume. The end mark holds the last value (without the volume bonus). `quieter`: a
// sound effect is playing, and the volume is lowered by the cut of the voice's envelope.
static void volume_envelope(DlEngine* engine, DlVoice* v, bool quieter) {
    const DlImage* image = engine->image;
    uint32_t at = v->envelope;
    bool held = false;
    if (dli_u16(image, at) == DL_ENVELOPE_END) {
        at -= 2;
        held = true;
    }
    uint16_t value = dli_u16(image, at);
    v->envelope = at + 2;
    if (!held && engine->variant.envelope_adds_volume && v->bonus_active) {
        value = (uint16_t)(value + v->volume_bonus);
    }
    if (quieter) {
        value = (uint16_t)(value - v->volume_cut);
    } else if (engine->ramp.level != 0) {
        value = value > engine->ramp.level ? (uint16_t)(value - engine->ramp.level) : 0;
    }
    if (engine->variant.fade) {
        value = faded_volume(&engine->fade, value);
    }
    v->bus.registers->volume = (uint8_t)value;
}

static void note_tick(DlVoice* v) {
    uint16_t period = v->period;
    const bool slid = slide_period(&v->slide, &period);
    const bool vibrated = vibrato_period(&v->vibrato, &period);
    if (slid || vibrated) {
        v->period = period;
        v->bus.registers->period = period;
    }
}

// Returns true when the update switched the DMA off, at the last tick of a note
static bool voice_update(DlEngine* engine, DlVoice* v, bool quieter) {
    if (v->count == 0) {
        const StepResult result = read_pattern(engine, v);
        if (result == StepResult_Tail) {
            volume_envelope(engine, v, quieter);
        }
        return false;
    }
    bool stopped = false;
    NuadePaulaChannel* registers = v->bus.registers;
    if (v->count == 1) {
        // the last tick of the note: the channel stops so the next note restarts it
        if (v->sample_state != DlSampleState_Immediate) {
            dma_off(v);
            stopped = true;
        }
        v->count--;
    } else if (v->sample_state == DlSampleState_Started) {
        // one tick after the DMA start the repeat registers get the loop part
        registers->lc = dli_sample(engine->image, v->loop_address);
        registers->len_words = v->loop_length_words;
        v->count--;
        v->sample_state = DlSampleState_Looping;
    } else {
        v->count--;
    }
    note_tick(v);
    volume_envelope(engine, v, quieter);
    return stopped;
}

static void run_voice(DlEngine* engine, DlVoice* v, DlBus bus, bool quieter) {
    v->bus = bus;
    const bool stopped = voice_update(engine, v, quieter);
    // (a song end or restart command has reset the voice by now, and its bus with it)
    if (engine->variant.dma_every_tick && !stopped) {
        nuade_paula_channel_dma_on(bus.dma_on);
    }
}

void dl_engine_start(DlEngine* engine, DlImage* image, const DlVariant* variant, const DlSubsong* subsong) {
    memset(engine, 0, sizeof(*engine));
    dl_image_restore(image);
    engine->image = image;
    engine->variant = *variant;
    engine->subsong = *subsong;
    reset_fade(engine);
    reset_voices(engine);
    if (subsong->ramp_ticks != 0) {
        engine->ramp = (DlRamp) { .level = DL_RAMP_START_LEVEL, .ticks = subsong->ramp_ticks };
    }
    request_subsong_effect(engine);
    for (int i = 0; i < DL_VOICES; i++) {
        engine->loop.ends_needed[i] = 2;
    }
}

// The ramp lowers its level by one every `ticks` ticks
static void ramp_tick(DlRamp* ramp) {
    if (ramp->level != 0 && ++ramp->counter == ramp->ticks) {
        ramp->counter = 0;
        ramp->level--;
    }
}

static bool any_effect_active(const DlEngine* engine) {
    return engine->variant.effect_table != 0 && (engine->effects[0].active || engine->effects[1].active);
}

// The request for a sound effect goes to a channel that plays none: channel 3 first. (The eagleplayer asks for one
// effect, at the start and at every song end; if no channel is free the request waits.)
static void dispatch_effect_request(DlEngine* engine) {
    if (engine->effect_request == 0) {
        return;
    }
    for (int i = DL_EFFECT_CHANNELS - 1; i >= 0; i--) {
        if (!engine->effects[i].active) {
            engine->effects[i].pending = engine->effect_request;
            engine->effect_request = 0;
            return;
        }
    }
}

// The effect takes the channel: it stops, and the effect voice starts reading its pattern on the next tick
static void start_effect(DlEngine* engine, uint32_t effect_channel) {
    DlEffectChannel* effect = &engine->effects[effect_channel];
    nuade_paula_channel_dma_off(&engine->channels[DL_FIRST_EFFECT_CHANNEL + effect_channel]);
    effect->active = true;
    effect->voice.sequence = dli_u32(engine->image, engine->variant.effect_table + effect->pending * 4);
    effect->voice.count = 0;
    effect->pending = 0;
}

static DlBus own_bus(DlEngine* engine, uint32_t channel) {
    NuadePaulaChannel* hardware = &engine->channels[channel];
    return (DlBus) { .registers = hardware, .dma_on = hardware, .dma_off = hardware };
}

// One channel of the interrupt. A channel that plays an effect runs the effect voice on its registers and the music
// voice on registers of its own; on the tick the effect starts only the music voice runs, and its DMA stop request
// still gets through.
static void tick_channel(DlEngine* engine, uint32_t channel) {
    DlVoice* music = &engine->voices[channel];
    const bool can_play_effects = engine->variant.effect_table != 0 && channel >= DL_FIRST_EFFECT_CHANNEL;
    if (!can_play_effects) {
        run_voice(engine, music, own_bus(engine, channel), any_effect_active(engine));
        return;
    }
    const uint32_t index = channel - DL_FIRST_EFFECT_CHANNEL;
    DlEffectChannel* effect = &engine->effects[index];
    DlBus music_bus = own_bus(engine, channel);
    music_bus.registers = &engine->unconnected;
    if (effect->pending != 0) {
        start_effect(engine, index);
        run_voice(engine, music, music_bus, any_effect_active(engine));
    } else if (effect->active) {
        // the first effect channel is quieter while the second plays; the second never is
        const bool effect_quieter = index == 0 && engine->effects[1].active;
        run_voice(engine, &effect->voice, own_bus(engine, channel), effect_quieter);
        music_bus.dma_off = nullptr;
        run_voice(engine, music, music_bus, any_effect_active(engine));
    } else {
        run_voice(engine, music, own_bus(engine, channel), any_effect_active(engine));
    }
}

void dl_engine_tick(DlEngine* engine) {
    engine->loop.looped = false;
    bool skip = false;
    if (engine->variant.tick_skip) {
        engine->tick_counter++;
        if (engine->tick_counter > engine->subsong.tick_limit) {
            skip = true;
            engine->tick_counter = 0;
        }
    }
    if (skip) {
        return;
    }
    ramp_tick(&engine->ramp);
    dispatch_effect_request(engine);
    for (uint32_t channel = 0; channel < DL_VOICES; channel++) {
        tick_channel(engine, channel);
    }
}

void dl_engine_sync(DlEngine* engine, NuadeVoices* voices) {
    for (uint32_t i = 0; i < DL_VOICES; i++) {
        nuade_paula_channel_sync(&engine->channels[i], i, voices);
    }
}
