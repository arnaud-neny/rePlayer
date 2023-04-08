#ifndef _TEXT_SCOPE_H_
#define _TEXT_SCOPE_H_

#include "write_audio.h"

#include <uade/options.h>

#ifdef UADE_CONFIG_TEXT_SCOPE
#define TEXT_SCOPE(cycles, voice, e, value)      \
    do {                                         \
        if (use_text_scope)                      \
            text_scope(cycles, voice, e, value); \
    } while (0)
#else
#define TEXT_SCOPE(cycles, voice, e, value) do {} while (0)
#endif

void text_scope(unsigned long cycles, int voice, enum UADEPaulaEventType e,
		int value);

#endif
