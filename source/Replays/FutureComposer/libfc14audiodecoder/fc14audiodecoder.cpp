// C language wrapper library for FC & Hippel audio decoder library

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include "fc14audiodecoder.h"
#include "FC.h"
#include "LamePaulaMixer.h"

#define FC14_DECLARE_DECODER \
    fc14dec *p = (fc14dec*)ptr

struct fc14dec {
    FC decoder;
    LamePaulaMixer mixer;
};

void* fc14dec_new() {
    fc14dec *p = new fc14dec;
    p->decoder.setMixer(&p->mixer);
    return (void*)p;
}

void fc14dec_delete(void* ptr) {
    FC14_DECLARE_DECODER;
    delete p;
}

int fc14dec_detect(void* ptr, void* data, uint32_t length) {
    FC14_DECLARE_DECODER;
    return p->decoder.isOurData(data,length);
}

int fc14dec_init(void* ptr, void* data, uint32_t length, int songNumber) {
    FC14_DECLARE_DECODER;
    return p->decoder.init(data,length,songNumber);
}

int fc14dec_reinit(void* ptr, int songNumber) {
    FC14_DECLARE_DECODER;
    return p->decoder.init(songNumber);
}

int fc14dec_restart(void* ptr) {
    FC14_DECLARE_DECODER;
    return p->decoder.restart();
}

void fc14dec_seek(void* ptr, int32_t ms) {
    FC14_DECLARE_DECODER;
    if ( !p->decoder.restart() ) {
        return;
    }
    while (ms>=0) {
        ms -= p->decoder.run();
        if ( fc14dec_song_end(p) ) {
            break;
        }
    };
    p->mixer.drain();
}

void fc14dec_mixer_init(void* ptr, int freq, int bits, int channels, int zero, int panning) {
    FC14_DECLARE_DECODER;
    p->mixer.init(freq,bits,channels,zero,panning);
}

uint32_t fc14dec_buffer_fill(void* ptr, void* buffer, uint32_t length) { // rePlayer
    FC14_DECLARE_DECODER;
    return p->mixer.fillBuffer(buffer,length,&p->decoder); // rePlayer
}

int fc14dec_song_end(void* ptr) {
    FC14_DECLARE_DECODER;
    return p->decoder.getSongEndFlag();
}

uint32_t fc14dec_duration(void* ptr) {
    FC14_DECLARE_DECODER;
    return p->decoder.getDuration();
}

const char* fc14dec_format_id(void* ptr) {
    FC14_DECLARE_DECODER;
    return p->decoder.getFormatID();
}

const char* fc14dec_format_name(void* ptr) {
    FC14_DECLARE_DECODER;
    return p->decoder.getFormatName();
}

int fc14dec_songs(void* ptr) {
    FC14_DECLARE_DECODER;
    return p->decoder.getSongs();
}

void fc14dec_end_shorts(void* ptr, int flag, int secs) {
    FC14_DECLARE_DECODER;
    p->decoder.endShorts(flag,secs);
}

void fc14dec_set_loop_mode(void* ptr, int flag) {
    FC14_DECLARE_DECODER;
    p->decoder.setLoopMode(flag);
}

int fc14dec_get_stats(void* ptr, struct fc14dec_mod_stats* stats) {
    FC14_DECLARE_DECODER;
    return p->decoder.copyStats(stats);
}

uint16_t fc14dec_get_sample_length(void* ptr, unsigned int num) {
    FC14_DECLARE_DECODER;
    return p->decoder.getSampleLength(num);
}

uint16_t fc14dec_get_sample_rep_offset(void* ptr, unsigned int num) {
    FC14_DECLARE_DECODER;
    return p->decoder.getSampleRepOffset(num);
}

uint16_t fc14dec_get_sample_rep_length(void* ptr, unsigned int num) {
    FC14_DECLARE_DECODER;
    return p->decoder.getSampleRepLength(num);
}

void fc14dec_mute_voice(void* ptr, bool mute, unsigned int voice) {
    FC14_DECLARE_DECODER;
    p->mixer.mute(voice,mute);
}

uint16_t fc14dec_get_voice_volume(void* ptr, unsigned int voice) {
    FC14_DECLARE_DECODER;
    if ( !p->mixer.isMuted(voice) ) {
        return (p->mixer.getVoice(voice)->paula.volume/64.0)*100;
    }
    else {
        return 0;
    }
}

// rePlayer begin
void fc14dec_enableNtsc(void* ptr, bool isEnabled) {
    FC14_DECLARE_DECODER;
    return p->mixer.enableNtsc(isEnabled);
}
// rePlayer end
