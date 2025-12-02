// C language wrapper library for TFMX & FC audio decoder library

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
// You should have received a copy of the GNU General Public License along
// with this program; if not, see <https://www.gnu.org/licenses/>.

#include "tfmxaudiodecoder.h"
#include "DecoderProxy.h"
#include "LamePaulaMixer.h"

#define TFMX_DECLARE_DECODER \
    tfmxdec *p = (tfmxdec*)ptr

struct tfmxdec {
    tfmxaudiodecoder::DecoderProxy dec;
    tfmxaudiodecoder::LamePaulaMixer mixer;
};

void* tfmxdec_new() {
    tfmxdec *p = new tfmxdec;
    p->dec.setMixer(&p->mixer);
    return (void*)p;
}

void tfmxdec_delete(void* ptr) {
    TFMX_DECLARE_DECODER;
    delete p;
}

void tfmxdec_set_path(void* ptr, const char* path) {
    TFMX_DECLARE_DECODER;
    p->dec.setPath(path);
}

int tfmxdec_detect(void* ptr, void* data, uint32_t length) {
    TFMX_DECLARE_DECODER;
    return p->dec.maybeOurs(data,length);
}

int tfmxdec_init(void* ptr, void* data, uint32_t length, int songNumber) {
    TFMX_DECLARE_DECODER;
    return p->dec.init(data,length,songNumber);
}

int tfmxdec_reinit(void* ptr, int songNumber) {
    TFMX_DECLARE_DECODER;
    return p->dec.reinit(songNumber);
}

void tfmxdec_seek(void* ptr, int32_t ms) {
    TFMX_DECLARE_DECODER;
    p->dec.seek(ms);
}

void tfmxdec_mixer_init(void* ptr, int freq, int bits, int channels, int zero, int panning) {
    TFMX_DECLARE_DECODER;
    p->mixer.init(freq,bits,channels,zero,panning);
}

void tfmxdec_buffer_fill(void* ptr, void* buffer, uint32_t length) {
    TFMX_DECLARE_DECODER;
    p->dec.mixerFillBuffer(buffer,length);
}

int tfmxdec_song_end(void* ptr) {
    TFMX_DECLARE_DECODER;
    return p->dec.getSongEndFlag();
}

uint32_t tfmxdec_duration(void* ptr) {
    TFMX_DECLARE_DECODER;
    return p->dec.getDuration();
}

const char* tfmxdec_format_id(void* ptr) {
    TFMX_DECLARE_DECODER;
    return p->dec.getFormatID();
}

const char* tfmxdec_format_name(void* ptr) {
    TFMX_DECLARE_DECODER;
    return p->dec.getFormatName();
}

int tfmxdec_songs(void* ptr) {
    TFMX_DECLARE_DECODER;
    return p->dec.getSongs();
}

void tfmxdec_end_shorts(void* ptr, int flag, int secs) {
    TFMX_DECLARE_DECODER;
    p->dec.endShorts(flag,secs);
}

void tfmxdec_set_loop_mode(void* ptr, int flag) {
    TFMX_DECLARE_DECODER;
    p->dec.setLoopMode(flag);
}

int tfmxdec_voices(void* ptr) {
    TFMX_DECLARE_DECODER;
    return p->dec.getVoices();
}

const char* tfmxdec_get_name(void* ptr) {
    TFMX_DECLARE_DECODER;
    return p->dec.getInfoString("name");
}

const char* tfmxdec_get_artist(void* ptr) {
    TFMX_DECLARE_DECODER;
    return p->dec.getInfoString("artist");
}

const char* tfmxdec_get_title(void* ptr) {
    TFMX_DECLARE_DECODER;
    return p->dec.getInfoString("title");
}

const char* tfmxdec_get_game(void* ptr) {
    TFMX_DECLARE_DECODER;
    return p->dec.getInfoString("game");
}

void tfmxdec_mute_voice(void* ptr, bool mute, unsigned int voice) {
    TFMX_DECLARE_DECODER;
    p->mixer.mute(voice,mute);
}

uint16_t tfmxdec_get_voice_volume(void* ptr, unsigned int voice) {
    TFMX_DECLARE_DECODER;
    if ( !p->mixer.isMuted(voice) ) {
        return (p->mixer.getVoice(voice)->paula.volume/64.0)*100;
    }
    else {
        return 0;
    }
}

int tfmxdec_load(void* ptr, const char* path, int songNumber) {
    TFMX_DECLARE_DECODER;
    return p->dec.load( path, songNumber );
}

void tfmxdec_set_filtering(void* ptr, int flag) {
    TFMX_DECLARE_DECODER;
    p->mixer.setFiltering(flag);
}
