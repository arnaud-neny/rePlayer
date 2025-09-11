/* C language wrapper library for FC & Hippel audio decoder library
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef FC14AUDIODECODER_H
#define FC14AUDIODECODER_H

#ifdef __cplusplus
extern "C" {
#endif

    #include <stdint.h>
    
    /* Return ptr to new decoder object. */
    void* fc14dec_new();

    /* Delete decoder object. */
    void fc14dec_delete(void* decoder);

    /* Initialize decoder with input data from a memory buffer.
       Input buffer may be freed as buffer contents get copied.
       Song numbers start at 0.
       Returns: boolean integer 1 = success, 0 = failure */
    int fc14dec_init(void* decoder, void* buffer, uint32_t length, int songNumber);

    /* With an already initialized decoder, this can be called
       to init a different song.
       Song numbers start at 0.
       Returns: boolean integer 1 = success, 0 = failure */
    int fc14dec_reinit(void* decoder, int songNumber);

    /* Initialize decoder's audio sample mixer.
       frequency : output sample frequency
       precision : bits per sample
       channels : 1=mono, 2=stereo
       zero : value of silent output sample
              (e.g. 0x80 for unsigned 8-bit, 0x0000 for signed 16-bit)
       panning : 100 to 0, default = 75
                 100=full stereo, 50=middle, 0=mirrored full stereo */
    void fc14dec_mixer_init(void* decoder, int frequency, int precision,
                            int channels, int zero, int panning);

    /* If planning to ignore a song's end as to loop to the beginning,
       call this with 1 = true. */
    void fc14dec_set_loop_mode(void* decoder, int loop);
    
    /* Return 1 (true) if song end has been reached, else 0 (false). */
    int fc14dec_song_end(void* decoder);

    /* Only to be called after initialization.
       Return number of songs. */
    int fc14dec_songs(void* decoder);
    
    /* Return duration of initialized song in milli-seconds [ms]. */
    uint32_t fc14dec_duration(void* decoder);

    /* Set an initialized decoder's play position in milli-seconds [ms]. */
    void fc14dec_seek(void* decoder, int32_t ms);

    /* Return short C-string identifying the detected input data format. */
    const char* fc14dec_format_id(void* decoder);
    
    /* Return C-string describing the detected input data format. */
    const char* fc14dec_format_name(void* decoder);

    /* Fill output sample buffer. */
    uint32_t fc14dec_buffer_fill(void* decoder, void* buffer, uint32_t length); // rePlayer

    /* Whether to end short tracks/subsongs immediately,
       if they are shorter than N seconds.
       Default: false */
    void fc14dec_end_shorts(void* decoder, int flag, int secs);

    /* Optional!
       Apply input format header check to a memory buffer containing
       at least 0xb80 bytes (= FC::PROBE_SIZE). With a minimum of 5 bytes
       it can detect only raw modules with a tag at the very beginning.
       Returns: boolean integer 1 = recognized data, 0 = unknown data */
    int fc14dec_detect(void* decoder, void* buffer, uint32_t length);
    
    /* Optional!
       Restart an already initialized decoder.
       Returns: boolean integer 1 = success, 0 = not initialized yet */
    int fc14dec_restart(void* decoder);

    struct fc14dec_mod_stats {
        int voices;
        int trackSteps;
        int patterns;
        int sndSeqs;
        int volSeqs;
        int samples;
        int songs;
    };

    /* Only to be called after initialization.
       Returns: boolean integer 1 = success, 0 = not initialized yet */
    int fc14dec_get_stats(void* decoder, struct fc14dec_mod_stats* stats);

    /* With the number of voices retrieved via fc14dec_get_stats,
       use this to mute/unmute individual voices. */
    void fc14dec_mute_voice(void* decoder, bool mute, unsigned int voice);

    /* Returns: 0 to 100 */
    unsigned short int fc14dec_get_voice_volume(void* decoder, unsigned int voice);
    
    /* With the number of samples retrieved via fc14dec_get_stats,
       use this to examine sample data parameters. */
    uint16_t fc14dec_get_sample_length(void* decoder, unsigned int num);
    uint16_t fc14dec_get_sample_rep_offset(void* decoder, unsigned int num);
    uint16_t fc14dec_get_sample_rep_length(void* decoder, unsigned int num);

    void fc14dec_enableNtsc(void* decoder, bool isEnabled); // rePlayer
#ifdef __cplusplus
}
#endif

#endif  /* FC14AUDIODECODER_H */
