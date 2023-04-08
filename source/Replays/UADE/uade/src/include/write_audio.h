#ifndef _UADE_WRITE_AUDIO_H_
#define _UADE_WRITE_AUDIO_H_

#include <stdio.h>
#include <stdint.h>

#include "write_audio_ext.h"

#define UADE_WRITE_AUDIO_MAGIC "uade_osc_0\x00\xec\x17\x31\x03\x09"

struct uade_write_audio_header {
	char magic[16];  // UADE_WRITE_AUDIO_MAGIC
};

struct uade_write_audio;

struct uade_write_audio *uade_write_audio_init(const char *fname);
void uade_write_audio_write(struct uade_write_audio *w, const int output[4],
			    const unsigned long tdelta);
void uade_write_audio_write_left_right(
	struct uade_write_audio *w, const int left, const int right);
void uade_write_audio_set_state(struct uade_write_audio *w,
				const int channel,
				const enum UADEPaulaEventType event_type,
				const uint16_t value);
void uade_write_audio_close(struct uade_write_audio *w);

#endif
