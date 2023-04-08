#ifndef _UADE_WRITE_AUDIO_EXT_H_
#define _UADE_WRITE_AUDIO_EXT_H_

#include <stdint.h>

/* These must have permanent integer values due to write-audio */
enum UADEPaulaEventType {
	PET_NONE = 0,
	PET_VOL = 1,
	PET_PER = 2,
	PET_DAT = 3,
	PET_LEN = 4,
	PET_LCH = 5,
	PET_LCL = 6,
	PET_LOOP = 7,
	PET_OUTPUT = 8,
	PET_MAX_ENUM,  /* This value may change */
};

__pragma(pack(push, 1))
struct uade_paula_event_frame {
	int8_t channel;
	int8_t event_type;
	uint16_t event_value;  /* bigendian */
};// __attribute__((packed));

struct uade_write_audio_frame {
	/*
	 * bigendian: 0xCCTTTTTT. If 0xCC == 0, data.output[i] contains audio
	 * data for channel i. Otherwise, data.uade_paula_event_frame contains
	 * a Paula event, e.g. a register write or a sample loop event.
	 */
	int32_t tdelta;
	union {
		int16_t output[4];  /* bigendian */
		struct uade_paula_event_frame paula_event_frame;
	} data;
};// __attribute__((packed));
__pragma(pack(pop))

#endif
