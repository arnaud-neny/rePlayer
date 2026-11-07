// SPDX-License-Identifier: MIT
// Kris Hatlelid player: the sound driver as the eagleplayer's patches leave it. Four voices read pairs of bytes from
// their streams: a note (pitch byte, duration byte) or a command. A note picks an octave of the current 8SVX
// instrument, sets the period, starts the DMA with the whole octave and lets the audio interrupt write the repeat
// registers; it ends with a release some ticks before its duration is over. The tempo is the CIA timer, written by
// commands in the streams.
//
// What the eagleplayer's patches change: the writes to the custom chip go through its own routines (the volume is
// written as twice the voice's value, up to 64: at the default master volume and balance that is the whole effect), the
// tempo commands set the timer of DeliTracker, the track-start-over and stop routines call SongEnd, and the audio
// interrupt handler is called as a subroutine and switches itself off after it wrote the repeat registers. The sound
// effect calls of the driver (commands 3 to 12) are not used by the eagleplayer and are not ported.

#include <nuade/players/krishatlelid.h>

#include "kh_internal.h"
#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>

// The stream commands, the bytes from 0xD9 on
typedef enum KhCommand {
    KhCommand_NextPattern = 0xD9,
    KhCommand_Stop = 0xDA,
    KhCommand_StartOver = 0xDB,
    KhCommand_Instrument = 0xDC,
    KhCommand_TempoA = 0xDD,
    KhCommand_TempoB = 0xDE,
    KhCommand_Volume = 0xDF,
    KhCommand_Hold = 0xE0,
} KhCommand;

// A track that does nothing but commands would never return
#define MAX_COMMANDS_PER_TICK 4096u

#define DEFAULT_TIMER 0x3700u // DeliTracker's timer word at the start
#define RELEASE_PERIOD 0x7Cu
#define DEFAULT_VOLUME 0x20u
#define MAX_VOLUME_REGISTER 64
#define PITCH_REFERENCE_NOTE 0x1Bu
#define PITCH_TABLE_SHIFT 2
#define TIE_DELAY 2u
#define HOLD_ON 1u

// lsl.l: the count is taken modulo 64 and moves every bit out past 31.

static uint32_t shift_left_long(uint32_t value, uint32_t count) {
    count &= 63;
    return count >= 32 ? 0 : value << count;
}

// The volume register: twice the voice's volume up to 64, then the low 7 bits.

static uint8_t volume_register(uint16_t volume) {
    uint16_t doubled = (uint16_t)(volume << 1);
    if ((int16_t)doubled > MAX_VOLUME_REGISTER) {
        doubled = MAX_VOLUME_REGISTER;
    }
    return (uint8_t)(doubled & 0x7F);
}

static void dma_off_all(KhPlayer* player) {
    for (uint32_t i = 0; i < KH_VOICES; i++) {
        nuade_paula_channel_dma_off(&player->voices[i].paula);
    }
}

// Everything is switched off and the eagleplayer calls SongEnd. A division by zero ends the song the same way: UADE's
// score answers the exception by switching off the interrupts and the DMA and reporting that it crashed.

static void end_song(KhPlayer* player) {
    player->state = KhState_Idle;
    dma_off_all(player);
    player->finished = true;
    player->tick.ended = true;
}

// What the audio interrupt handler does for one channel. The first interrupt comes when the DMA starts: it writes the
// repeat of the sample (or, for a sample that plays once, only notes that) and the eagleplayer's patch then switches
// the channel's interrupt off. The next one comes where the first block ends. Under the voice API a sample that plays
// once repeats silence: the driver switches the DMA off in the interrupt at the end of the block, within microseconds,
// while a tick player sees the block end only at the next tick.

static void channel_interrupt(KhPlayer* player, KhVoice* voice) {
    if (voice->block_state == KhBlock_Done) {
        return;
    }
    if (voice->block_state == KhBlock_Once) {
        nuade_paula_channel_dma_off(&voice->paula);
        voice->paula.volume = 0;
        voice->paula.period = RELEASE_PERIOD;
        voice->block_state = KhBlock_Done;
    } else if (voice->repeat_words != 0) {
        voice->paula.len_words = voice->repeat_words;
        voice->paula.lc = kh_chip_pointer(&player->memory, voice->loop_start);
        voice->block_state = KhBlock_Done;
        voice->interrupt_enabled = false;
    } else {
        voice->paula.len_words = nuade_sizeof_array(player->silence) / 2;
        voice->paula.lc = player->silence;
        voice->block_state = KhBlock_Once;
    }
}

// The interrupt of a channel that has a request and has it enabled: the handler runs for the four channels, and
// acknowledges every one of them, whether it handled it or not.

static void take_audio_interrupts(KhPlayer* player) {
    bool taken[KH_VOICES];
    bool any = false;
    for (uint32_t i = 0; i < KH_VOICES; i++) {
        taken[i] = player->voices[i].interrupt_pending && player->voices[i].interrupt_enabled;
        any = any || taken[i];
    }
    if (!any) {
        return;
    }
    for (uint32_t i = 0; i < KH_VOICES; i++) {
        if (taken[i]) {
            channel_interrupt(player, &player->voices[i]);
        }
        player->voices[i].interrupt_pending = false;
    }
}

// The requests the voices got since the last tick: one for every block the mixer started.

static void request_audio_interrupts(KhPlayer* player, NuadeVoices* voices) {
    for (uint32_t i = 0; i < KH_VOICES; i++) {
        KhVoice* voice = &player->voices[i];
        NuadeVoiceState state;
        nuade_voice_get_state(voices, i, &state);
        voice->interrupt_pending = voice->interrupt_pending || state.wraps != voice->wraps_seen;
        voice->wraps_seen = state.wraps;
    }
    take_audio_interrupts(player);
}

// The tempo commands write the CIA timer, which the eagleplayer passes to DeliTracker.

static void tempo_command(KhPlayer* player, KhCommand command, uint8_t value) {
    const bool low = command == KhCommand_TempoB && player->module.variant->tempo == KhTempo_HighLow;
    uint8_t byte = value;
    if (command == KhCommand_TempoB) {
        switch (player->module.variant->tempo) {
            case KhTempo_HighLow:
                break;
            case KhTempo_High:
                byte = (uint8_t)((value ^ 0xFFu) >> 1);
                break;
            case KhTempo_HighScaled:
                byte = (uint8_t)((((uint32_t)(value ^ 0xFFu) * 0x90u) / 0xA0u) >> 1);
                break;
        }
    }
    player->timer
        = low ? (uint16_t)((player->timer & 0xFF00u) | byte) : (uint16_t)((player->timer & 0x00FFu) | (byte << 8));
}

// Results of the period routine
typedef enum KhPitch {
    KhPitch_Set = 0,
    KhPitch_Skipped,
    KhPitch_DivideByZero,
} KhPitch;

// The period routine: octave and period of a pitch for the current instrument; the period goes to the register and
// becomes the base of the vibrato, which is reset from the instrument. An octave span of 0 ends the routine at once in
// the compact versions (`octave` stays as the caller left it) and divides by zero in the others, as a divisor of 0
// does.

static KhPitch set_pitch(KhPlayer* player, KhVoice* voice, uint16_t pitch, uint16_t* octave) {
    KhMemory* memory = &player->memory;
    if (voice->octave_span == 0) {
        return player->module.variant->compact_notes ? KhPitch_Skipped : KhPitch_DivideByZero;
    }
    const KhVhdr vhdr = kh_vhdr_read(memory, voice->vhdr);
    uint16_t octave_index = (uint16_t)(vhdr.octaves - pitch / voice->octave_span - 1);
    if ((int16_t)octave_index < 0) {
        octave_index = 0;
    }
    const int16_t table_offset = (int16_t)(uint16_t)((uint16_t)(pitch - PITCH_REFERENCE_NOTE) << PITCH_TABLE_SHIFT);
    const uint32_t constant = kh_driver_u32(&player->module, (uint32_t)((int32_t)KH_PERIOD_TABLE + table_offset));
    const uint16_t divisor = (uint16_t)shift_left_long(vhdr.cycle, octave_index);
    if (divisor == 0) {
        return KhPitch_DivideByZero;
    }
    // divu.w: a quotient over 16 bits leaves the dividend alone
    const uint16_t period = (constant >> 16) >= divisor ? (uint16_t)constant : (uint16_t)(constant / divisor);
    voice->paula.period = period;
    voice->base_period = period;
    voice->shown_period = period;
    voice->period = period;
    const KhInstrument instrument = kh_instrument_read(memory, voice->instrument);
    voice->vibrato_step = instrument.vibrato_step;
    kh_instrument_write_vibrato_counter(memory, voice->instrument, instrument.vibrato_delay);
    *octave = octave_index;
    return KhPitch_Set;
}

// Start the DMA with the whole octave: the one-shot part and the repeat. The interrupt that follows writes the repeat.

static void start_sample(KhPlayer* player, KhVoice* voice, uint16_t octave) {
    const KhVhdr vhdr = kh_vhdr_read(&player->memory, voice->vhdr);
    uint32_t block = voice->body;
    uint32_t octave_bytes = vhdr.one_shot + vhdr.repeat;
    for (uint16_t i = octave; i != 0; i--) {
        block += octave_bytes;
        octave_bytes <<= 1;
    }
    voice->paula.lc = kh_chip_pointer(&player->memory, block);
    voice->loop_start = block + shift_left_long(vhdr.one_shot, octave);
    voice->block_state = KhBlock_First;
    voice->paula.len_words = (uint16_t)(shift_left_long(vhdr.one_shot + vhdr.repeat, octave) >> 1);
    voice->repeat_words = (uint16_t)(shift_left_long(vhdr.repeat, octave) >> 1);
    voice->paula.volume = volume_register(voice->volume);
    // The DMA start requests the interrupt, which the write to INTENA lets through. A channel that already plays takes
    // the new registers at the end of its block; it is interrupted now only if a block started since the handler ran
    voice->interrupt_enabled = true;
    voice->interrupt_pending = voice->interrupt_pending || !voice->paula.dma;
    nuade_paula_channel_dma_on(&voice->paula);
    take_audio_interrupts(player);
}

// The durations of a note: the ticks it lasts and the tick where it is released.

static void set_duration(const KhPlayer* player, KhVoice* voice, uint8_t byte) {
    const KhVariant* variant = player->module.variant;
    if (variant->duration_formula) {
        voice->duration = (uint16_t)(byte - 1);
        voice->release_at = (uint16_t)((byte >> 4) + 1);
        return;
    }
    // Two bytes per entry: the length and the length of the gate; the release is the difference in the low byte
    const uint32_t entry = KH_DURATION_TABLE + 2u * byte;
    const uint16_t length = (uint16_t)(kh_driver_u8(&player->module, entry) - 1);
    voice->duration = length;
    voice->release_at = (uint16_t)((length & 0xFF00u) | ((length - kh_driver_u8(&player->module, entry + 1)) & 0xFFu));
}

// A note event: the two bytes and what the current list entry transposes by.

static void note_event(KhPlayer* player, KhVoice* voice, uint8_t pitch_byte, uint8_t duration_byte) {
    const KhVariant* variant = player->module.variant;
    const KhMemory* memory = &player->memory;
    const bool tied = (pitch_byte & 0x80) != 0 || (variant->compact_notes && (duration_byte & 0x80) != 0);
    if (!tied) {
        voice->tie = 0;
    } else if (voice->tie == 0) {
        voice->tie = TIE_DELAY;
    }
    const uint8_t pitch = pitch_byte & 0x7F;
    const uint8_t duration = variant->compact_notes ? (duration_byte & 0x7F) : duration_byte;
    const KhListEntry entry = kh_list_entry_read(memory, voice->list, voice->list_pos);
    voice->note = (uint16_t)(pitch + entry.transpose);
    uint16_t note = voice->note;
    if (variant->compact_notes) {
        note = (uint16_t)(note + kh_instrument_read(memory, voice->instrument).transpose);
    }
    voice->stream += 2;
    set_duration(player, voice, duration);

    const bool no_sample
        = variant->compact_notes ? (pitch == 0 || voice->vhdr == 0 || (voice->body >> 16) == 0) : voice->vhdr == 0;
    if (no_sample || voice->tie == 1) {
        player->note_seen = true;
        return;
    }
    if (voice->tie != 0) {
        voice->tie--;
    }
    // The octave register still holds the last value of the duration code when the pitch routine returns early
    uint16_t octave = variant->duration_formula ? voice->duration : voice->release_at;
    if (set_pitch(player, voice, note, &octave) == KhPitch_DivideByZero) {
        end_song(player);
        return;
    }
    start_sample(player, voice, octave);
    player->note_seen = true;
}

static void stop_song(KhPlayer* player) {
    if (player->state != KhState_Idle) {
        dma_off_all(player);
        player->state = KhState_Stopping;
    }
}

// The eagleplayer's flag for a track that started over; when all four have, it calls SongEnd.

static void track_started_over(KhPlayer* player, KhVoice* voice) {
    voice->started_over = true;
    for (uint32_t i = 0; i < KH_VOICES; i++) {
        if (!player->voices[i].started_over) {
            return;
        }
    }
    for (uint32_t i = 0; i < KH_VOICES; i++) {
        player->voices[i].started_over = false;
    }
    player->tick.looped = true;
}

// Instrument command: the record, with the addresses its IFF data got at the song start.

static void select_instrument(KhPlayer* player, KhVoice* voice, uint8_t index) {
    const KhMemory* memory = &player->memory;
    const uint32_t record = kh_song_instrument(memory, player->song, index);
    const KhInstrument instrument = kh_instrument_read(memory, record);
    voice->instrument = record;
    voice->vhdr = instrument.vhdr;
    voice->body = instrument.body;
    voice->octave_span = instrument.octave_span;
    voice->instrument_index = index;
}

// The events of a voice whose note is over, up to the next note (or the end of the track).

static void voice_events(KhPlayer* player, KhVoice* voice) {
    const KhMemory* memory = &player->memory;
    for (uint32_t commands = 0; commands < MAX_COMMANDS_PER_TICK; commands++) {
        const uint8_t first = kh_u8(memory, voice->stream);
        const uint8_t second = kh_u8(memory, voice->stream + 1);
        if (first < KhCommand_NextPattern) {
            note_event(player, voice, first, second);
            return;
        }
        switch ((KhCommand)first) {
            case KhCommand_NextPattern:
                voice->list_pos += KH_LIST_ENTRY_BYTES;
                voice->stream = kh_list_entry_read(memory, voice->list, voice->list_pos).stream;
                continue;
            case KhCommand_StartOver:
                voice->list_pos = 0;
                voice->stream = kh_list_entry_read(memory, voice->list, 0).stream;
                track_started_over(player, voice);
                continue;
            case KhCommand_Stop:
                if (player->module.variant->stop_song_command) {
                    stop_song(player);
                } else {
                    voice->active = false;
                }
                return;
            case KhCommand_Instrument:
                select_instrument(player, voice, second);
                break;
            case KhCommand_TempoA:
            case KhCommand_TempoB:
                tempo_command(player, first, second);
                break;
            case KhCommand_Volume:
                voice->paula.volume = volume_register(second);
                voice->volume = second;
                break;
            case KhCommand_Hold:
                voice->hold = second;
                break;
            default:
                break;
        }
        voice->stream += 2;
    }
    voice->active = false;
}

// A note that has run its release time: the DMA stops unless the note is held or tied.

static void release_note(KhVoice* voice) {
    if (voice->hold == HOLD_ON || voice->tie != 0) {
        return;
    }
    nuade_paula_channel_dma_off(&voice->paula);
    voice->paula.volume = 0;
    voice->paula.period = RELEASE_PERIOD;
    voice->block_state = KhBlock_Done;
}

// The ticks of a note: the arpeggio steps the pitch, the vibrato moves the period between two limits.

static void voice_running(KhPlayer* player, KhVoice* voice) {
    KhMemory* memory = &player->memory;
    const KhInstrument instrument = kh_instrument_read(memory, voice->instrument);
    uint16_t arpeggio = 0;
    if (instrument.arpeggio_on != 0) {
        arpeggio = instrument.arpeggio[instrument.arpeggio_pos & (KH_ARPEGGIO_STEPS - 1)];
        kh_instrument_write_arpeggio_pos(memory, voice->instrument, (uint16_t)(instrument.arpeggio_pos + 1));
    }
    voice->duration--;
    if (voice->duration == voice->release_at) {
        release_note(voice);
        return;
    }
    if (instrument.arpeggio_on != 0) {
        uint16_t octave = 0;
        if (set_pitch(player, voice, (uint16_t)(voice->note + arpeggio), &octave) == KhPitch_DivideByZero) {
            end_song(player);
        }
        return;
    }
    if (instrument.vibrato_on == 0) {
        return;
    }
    const int32_t counter = (int32_t)(int16_t)instrument.vibrato_counter - 1;
    kh_instrument_write_vibrato_counter(memory, voice->instrument, (uint16_t)counter);
    if (counter > 0) {
        return;
    }
    kh_instrument_write_vibrato_counter(memory, voice->instrument, instrument.vibrato_rate);
    const uint16_t distance = (uint16_t)(voice->period - voice->base_period);
    const bool turn = voice->vibrato_step >= 0 ? distance > instrument.vibrato_max : distance <= instrument.vibrato_min;
    if (turn) {
        voice->vibrato_step = (int16_t)-voice->vibrato_step;
    }
    voice->period = (uint16_t)(voice->period + voice->vibrato_step);
    voice->paula.period = voice->period;
}

static void voice_step(KhPlayer* player, KhVoice* voice) {
    if (!voice->active) {
        return;
    }
    if (voice->duration != 0) {
        voice_running(player, voice);
    } else {
        voice_events(player, voice);
    }
}

// The first tick after the song start: the tracks begin with their first pattern.

static void start_tracks(KhPlayer* player) {
    for (uint32_t i = 0; i < KH_VOICES; i++) {
        KhVoice* voice = &player->voices[i];
        voice->active = true;
        voice->list = kh_song_list(&player->memory, player->song, i);
        voice->stream = kh_list_entry_read(&player->memory, voice->list, 0).stream;
        voice->list_pos = 0;
        voice->duration = 0;
        voice->volume = DEFAULT_VOLUME;
    }
    player->state = KhState_Playing;
    dma_off_all(player);
}

KhTickResult kh_tick(KhPlayer* player, NuadeVoices* voices) {
    player->tick = (KhTickResult) { 0 };
    // The CIA reloads its counter from the latch when it runs out; a tempo command in this tick changes the latch for
    // the period after the next interrupt
    const uint32_t timer = player->timer;
    // The eagleplayer's interrupt: when no track runs it calls SongEnd before the driver's tick
    bool any_track = false;
    for (uint32_t i = 0; i < KH_VOICES; i++) {
        any_track = any_track || player->voices[i].active;
    }
    if (player->finished || !any_track) {
        player->finished = true;
        player->tick.ended = true;
    } else {
        request_audio_interrupts(player, voices);
        switch (player->state) {
            case KhState_Idle:
                break;
            case KhState_Start:
            case KhState_Playing:
                if (player->state == KhState_Start) {
                    start_tracks(player);
                }
                for (uint32_t i = 0; i < KH_VOICES && !player->finished; i++) {
                    voice_step(player, &player->voices[i]);
                }
                if (!player->note_seen) {
                    end_song(player);
                }
                break;
            case KhState_Stopping:
                end_song(player);
                break;
        }
    }
    for (uint32_t i = 0; i < KH_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
    }
    player->tick.length = timer + 1u;
    return player->tick;
}

// InitSound looks at the first bytes of the first track for the tempo: a lone command 0xDE is run, otherwise 0xDD
// (after an instrument command) with an optional 0xDE after it gives the timer word.

static void initial_tempo(KhPlayer* player) {
    const KhMemory* memory = &player->memory;
    const uint32_t list = kh_song_list(memory, player->song, 0);
    uint32_t at = kh_list_entry_read(memory, list, 0).stream;
    if (kh_u8(memory, at) == KhCommand_TempoB) {
        tempo_command(player, KhCommand_TempoB, kh_u8(memory, at + 1));
        return;
    }
    if (kh_u8(memory, at) == KhCommand_Instrument) {
        at += 2;
    }
    if (kh_u8(memory, at++) != KhCommand_TempoA) {
        return;
    }
    uint16_t timer = (uint16_t)(kh_u8(memory, at++) << 8);
    if (kh_u8(memory, at++) == KhCommand_TempoB) {
        timer |= kh_u8(memory, at);
    }
    player->timer = timer;
}

bool kh_start(KhPlayer* player, uint32_t subsong, NuadeVoices* voices) {
    if (player == nullptr || subsong >= player->module.subsong_count) {
        return false;
    }
    const KhModule* module = &player->module;
    free(player->memory.bytes);
    player->memory.size = module->song.size;
    player->memory.bytes = nuade_paula_padded_copy(module->song.bytes, module->song.size);
    if (player->memory.bytes == nullptr) {
        return false;
    }
    player->song = kh_module_song(module, &player->memory, subsong);
    uint64_t budget = KH_PREPARE_BUDGET_WORDS;
    if (!kh_prepare_instruments(&player->memory, player->song, kh_first_prepared_instrument(module->variant),
                                &budget)) {
        return false;
    }
    player->state = KhState_Start;
    player->note_seen = false;
    player->finished = false;
    player->timer = DEFAULT_TIMER;
    // A song with its own driver starts every track with the table of instruments itself as its record
    const bool embedded = !module->external;
    for (uint32_t i = 0; i < KH_VOICES; i++) {
        KhVoice* voice = &player->voices[i];
        *voice = (KhVoice) { 0 };
        voice->volume = DEFAULT_VOLUME;
        voice->base_period = RELEASE_PERIOD;
        voice->instrument
            = embedded ? player->song + KH_INSTRUMENT_TABLE : kh_song_instrument(&player->memory, player->song, 0);
        voice->instrument_index = embedded ? -1 : 0;
        voice->active = true;
        if (voices != nullptr) {
            nuade_voice_stop(voices, i);
            NuadeVoiceState state;
            nuade_voice_get_state(voices, i, &state);
            voice->wraps_seen = state.wraps;
        }
    }
    initial_tempo(player);
    return true;
}

KhCreateResult kh_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    KhCreateResult result = { .status = KhStatus_InvalidData };
    KhPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        result.status = KhStatus_OutOfMemory;
        return result;
    }
    result.status = kh_module_load(&player->module, data, size, path, io);
    if (result.status == KhStatus_Ok && player->module.variant == nullptr) {
        kh_module_free(&player->module); // a song without a driver file cannot play
        result.status = KhStatus_MissingDriver;
    }
    if (result.status != KhStatus_Ok) {
        free(player);
        return result;
    }
    result.player = player;
    return result;
}

void kh_destroy(KhPlayer* player) {
    if (player != nullptr) {
        kh_module_free(&player->module);
        free(player->memory.bytes);
        free(player);
    }
}

// The probe loads the song; a song without a driver of its own is not checked against its driver file, which needs the
// io.

bool kh_probe(const uint8_t* data, size_t size) {
    KhModule module;
    const bool ok = kh_module_load(&module, data, size, nullptr, nullptr) == KhStatus_Ok;
    kh_module_free(&module);
    return ok;
}

uint32_t kh_subsong_count(const KhPlayer* player) {
    return player->module.subsong_count;
}

uint16_t kh_voice_period(const KhPlayer* player, uint32_t voice) {
    return player->voices[voice].shown_period;
}

int32_t kh_voice_instrument(const KhPlayer* player, uint32_t voice) {
    return player->voices[voice].instrument_index;
}
