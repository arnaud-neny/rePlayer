
// Driving a Paula channel (NuadePaulaChannel, include/nuade/paula.h): the registers a tick player writes during a tick,
// handed to a voice of the mixer at the end of the tick.

#pragma once

#include <nuade/paula.h>
#include <nuade/voice.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The most bytes one AUDxLC/AUDxLEN pair plays: AUDxLEN 0 is 65536 words.
#define NUADE_PAULA_MAX_SAMPLE_BYTES 0x20000u

// DMACON set: the channel latches AUDxLC/AUDxLEN as they are now; registers written after this call in the same tick
// are the repeat. That is Paula's behaviour for a player that waits for the DMA start before it writes the repeat (the
// usual raster wait). A player that writes them right after DMACON, before Paula latched, must write them before this
// call instead. A channel that already runs is not restarted.
void nuade_paula_channel_dma_on(NuadePaulaChannel* c);

void nuade_paula_channel_dma_off(NuadePaulaChannel* c);

// Hand the registers the tick wrote to the voice. A sample never runs off its end, so the voice needs no read-back: a
// changed AUDxLC/AUDxLEN is the latch for the next repeat and goes in as the queued sample. A channel whose DMA started
// in the tick plays the sample latched at nuade_paula_channel_dma_on, then the registers as they are now.
void nuade_paula_channel_sync(NuadePaulaChannel* c, uint32_t voice, NuadeVoices* voices);

// Chip memory for a module: `size` bytes copied from `data` (zeros when data is null), then
// NUADE_PAULA_MAX_SAMPLE_BYTES zero bytes. Any AUDxLC inside the first `size` bytes can play any AUDxLEN, 0 too: what
// runs past the module plays zeros. Writable, for players that patch their module; free() it. Null when out of memory.
uint8_t* nuade_paula_padded_copy(const void* data, size_t size);
