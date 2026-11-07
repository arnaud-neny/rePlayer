// SPDX-License-Identifier: MIT
// Private header for internal instrument-loading functions shared between sonix.c and sonix_io.c.

#pragma once

#include "sonix.h"

#ifdef __cplusplus
extern "C" {
#endif

bool sonix_song_set_instrument_pcm8(SonixSong* song, uint8_t instrument_index, const int8_t* pcm_data,
                                    uint32_t num_samples, uint32_t loop_start, uint32_t loop_len, uint8_t base_note);
bool sonix_song_set_instrument_pcm8_ex(SonixSong* song, uint8_t instrument_index, const int8_t* pcm_data,
                                       uint32_t num_samples, uint32_t loop_start, uint32_t loop_len, uint8_t base_note,
                                       uint32_t base_period, uint32_t source_rate_hz);
bool sonix_song_set_instrument_mod(SonixSong* song, uint8_t instrument_index, uint16_t vib_depth, uint16_t vib_speed,
                                   uint16_t vib_delay);
void sonix_song_set_instrument_ss(SonixSong* song, uint8_t instrument_index, bool is_ss);
void sonix_song_set_instrument_iff(SonixSong* song, uint8_t instrument_index, bool is_iff, uint32_t vhdr_volume);
void sonix_song_set_instrument_synth(SonixSong* song, uint8_t instrument_index, bool is_synth);
void sonix_song_set_instrument_attack_decay(SonixSong* song, uint8_t instrument_index, uint16_t attack_time,
                                            uint16_t decay_time);
void sonix_song_set_synth_vol_params(SonixSong* song, uint8_t instrument_index, uint16_t base_vol, uint16_t port_flag);
void sonix_song_set_ss_envelope(SonixSong* song, uint8_t inst, uint16_t inst_vol, const uint16_t targets[4],
                                const uint16_t speeds[4]);
void sonix_song_set_synth_wave(SonixSong* song, uint8_t instrument_index, const int8_t* wave128);
void sonix_song_set_synth_blend_params(SonixSong* song, uint8_t instrument_index, uint16_t c2, uint16_t c4);
void sonix_song_set_synth_filter_params(SonixSong* song, uint8_t instrument_index, uint16_t filter_base,
                                        uint16_t filter_range, uint16_t filter_env_sens);
void sonix_song_set_synth_env_params(SonixSong* song, uint8_t instrument_index, uint16_t scan_rate, int16_t loop_mode,
                                     uint16_t delay_init, uint16_t vol_scale, uint16_t pitch_scale);
void sonix_song_set_synth_env_table(SonixSong* song, uint8_t instrument_index, const int8_t* table256);
void sonix_song_set_synth_slide_rate(SonixSong* song, uint8_t instrument_index, uint16_t slide_rate);
bool sonix_song_add_instrument_zone(SonixSong* song, uint8_t instrument_index, const int8_t* pcm_data,
                                    uint32_t num_samples, uint32_t loop_start, uint32_t loop_len, uint8_t base_note,
                                    uint8_t low_key, uint8_t high_key, uint32_t source_rate_hz);
void sonix_song_set_zone_attack_decay(SonixSong* song, uint8_t instrument_index, uint8_t zone_index,
                                      uint16_t attack_time, uint16_t decay_time);

bool sonix_song_has_runtime_track_engine(const SonixSong* song);

// Write the Paula-level event stream (period/volume/DMA/buffer, on change) to a FILE*.
void sonix_song_set_paula_trace(SonixSong* song, void* file_handle, bool with_bytes);
uint32_t sonix_song_get_debug_note_events(const SonixSong* song);
uint32_t sonix_song_get_tick_count(const SonixSong* song);
uint32_t sonix_song_get_debug_wait_commands(const SonixSong* song);
uint32_t sonix_song_get_debug_loop_resets(const SonixSong* song);

#ifdef __cplusplus
}
#endif
