#ifndef _UADE_STATE_DETECTION_H_
#define _UADE_STATE_DETECTION_H_

#include "sysdeps.h"

void uade_state_detection_init(const uae_u32 addr, const uae_u32 size);
int uade_state_detection_step(void);

#endif
