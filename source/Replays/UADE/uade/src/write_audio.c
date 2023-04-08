#include "write_audio.h"
#include "write_audio_ext.h"

#include <uade/uade.h>
#include <uade/uadeipc.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *paula_event_string[] = {
	"None", "Vol", "Per", "Dat", "Len", "LCL", "LCH", "Loop"};


struct channel_event {
	enum UADEPaulaEventType active_events[PET_MAX_ENUM];
	uint16_t event_values[PET_MAX_ENUM];
};

struct uade_write_audio {
	FILE *f;
	int output[4];
	int started;
	struct channel_event channel_events[4];
};

struct uade_write_audio *uade_write_audio_init(const char *fname)
{
	int i;
	struct uade_write_audio_header h;
	memcpy(&h.magic, UADE_WRITE_AUDIO_MAGIC, sizeof(h.magic));

	struct uade_write_audio *w = calloc(1, sizeof(*w));
	if (w == NULL)
		goto out;
	w->f = fopen(fname, "wb");
	if (w->f == NULL) {
		fprintf(stderr, "error: Can not open %s to write audio\n",
			fname);
		goto out;
	}

	if (uade_atomic_fwrite(&h, sizeof(h), 1, w->f) != 1) {
		fprintf(stderr,
			"Error: Can not write uade_write_audio_header\n");
		goto out;
	}

	for (i = 0; i < 4; i++)
		w->output[i] = 0x7fffffff;

	return w;
out:
	if (w != NULL) {
		if (w->f != NULL)
			fclose(w->f);
		memset(w, 0, sizeof(*w));
		free(w);
	}
	return NULL;
}

void uade_write_audio_set_state(struct uade_write_audio *w,
				const int channel,
				const enum UADEPaulaEventType event_type,
				const uint16_t value)
{
	assert(channel >= 0 && channel < 4);
	assert(event_type > 0 && event_type < PET_MAX_ENUM);
	w->channel_events[channel].active_events[event_type] = 1;
	w->channel_events[channel].event_values[event_type] = value;
}

void uade_write_audio_write(struct uade_write_audio *w, const int output[4],
			    const unsigned long tdelta)
{
	int ch;
	uint32_t time_to_advance = tdelta;
	struct uade_write_audio_frame frame;

	assert(tdelta <= 0x00ffffff);

	for (ch = 0; ch < 4; ch++) {
		enum UADEPaulaEventType et;

		struct uade_write_audio_frame paula_frame;
		struct channel_event *ce = &w->channel_events[ch];
		struct uade_paula_event_frame *pef = (
			&paula_frame.data.paula_event_frame);

		for (et = 1 ; et < PET_MAX_ENUM; et++) {
			if (ce->active_events[et]) {
				/*
				 * MSB of tdelta to indicate this is a
				 * paula event. Zero the time_to_advance later
				 * not to advance time twice.
				 */
				write_be_u32(
					&paula_frame.tdelta,
					(uint32_t) (time_to_advance |
						    0x80000000));
				time_to_advance = 0;

				pef->channel = ch;
				pef->event_type = et;
				write_be_u16(&pef->event_value,
					     ce->event_values[et]);

				if (uade_atomic_fwrite(&paula_frame,
						       sizeof(paula_frame),
						       1, w->f) != 1) {
					fprintf(stderr,
						"error: Unable to write paula "
						"event frame\n");
				}
			}
		}
	}

	memset(w->channel_events, 0, sizeof(w->channel_events));

	/* Note: time_to_advance may be zeroed in paula loop */
	write_be_u32(&frame.tdelta, (uint32_t) time_to_advance);

	for (ch = 0; ch < 4; ch++) {
		if (!w->started && output[ch] != 0)
			w->started = 1;

		write_be_s16(&frame.data.output[ch], output[ch]);
	}

	if (w->started) {
		if (uade_atomic_fwrite(&frame, sizeof(frame), 1, w->f) != 1)
			fprintf(stderr, "uade: Unable to write audio frame\n");
	}
}

void uade_write_audio_write_left_right(
	struct uade_write_audio *w, const int left, const int right)
{
	struct uade_write_audio_frame frame;
	struct uade_paula_event_frame *pef = &frame.data.paula_event_frame;

	if (!w->started)
		return;

	write_be_u32(&frame.tdelta, 0x80000000);
	pef->channel = 0;
	pef->event_type = PET_OUTPUT;
	write_be_u16(&pef->event_value, left);
	if (uade_atomic_fwrite(&frame, sizeof(frame), 1, w->f) != 1) {
		fprintf(stderr,	"error: Unable to write paula event frame\n");
	}
	pef->channel = 1;
	write_be_u16(&pef->event_value, right);
	if (uade_atomic_fwrite(&frame, sizeof(frame), 1, w->f) != 1) {
		fprintf(stderr,	"error: Unable to write paula event frame\n");
	}
}

void uade_write_audio_close(struct uade_write_audio *w)
{
	if (w == NULL)
		return;
	fclose(w->f);
	memset(w, 0, sizeof(*w));
}
