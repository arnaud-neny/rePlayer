/*--------------------------------------------------------------------
	Atari Audio Library v1.08
	Small & accurate ATARI-ST audio emulation
	Arnaud Carré aka Leonard/Oxygene
	@leonard_coder
--------------------------------------------------------------------*/
#pragma once
#include <stdint.h>

int timedbSearch(const void* data, uint32_t size, uint32_t* framesArray, int framesArraySize);
