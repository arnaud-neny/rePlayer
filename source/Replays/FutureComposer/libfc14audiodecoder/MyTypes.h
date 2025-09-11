// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef MYTYPES_H
#define MYTYPES_H

#include "Config.h"
#include <cstdint>

// If the fixed width integer types are not available, define
// strictly compatible types in the related section of 'Config.h'.

// Wanted: 8-bit signed/unsigned.
typedef int8_t sbyte;
typedef uint8_t ubyte;

// Wanted: 16-bit signed/unsigned.
typedef int16_t sword;
typedef uint16_t uword;

// Wanted: 32-bit signed/unsigned.
typedef int32_t sdword;
typedef uint32_t udword;

#endif  // MYTYPES_H
