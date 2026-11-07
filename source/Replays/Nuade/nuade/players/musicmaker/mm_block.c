// SPDX-License-Identifier: MIT
// MusicMaker V8 player: bounded reads of the blocks of bytes the replayers work on (the song, the instruments, the
// chunks of a file). A read past the end gives 0.

#include "mm_internal.h"

uint8_t mm_u8(const MmBlock* b, uint32_t offset) {
    return offset < b->size ? b->data[offset] : 0;
}

uint16_t mm_u16(const MmBlock* b, uint32_t offset) {
    return (uint16_t)(mm_u8(b, offset) << 8 | mm_u8(b, offset + 1));
}

uint32_t mm_u32(const MmBlock* b, uint32_t offset) {
    return (uint32_t)mm_u16(b, offset) << 16 | mm_u16(b, offset + 2);
}
