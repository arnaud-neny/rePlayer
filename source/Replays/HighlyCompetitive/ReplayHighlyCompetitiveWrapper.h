#pragma once

#include <stdint.h>

void Snes9xInit(const uint8_t* rom, size_t romSize, const uint8_t* sram, size_t sramSize);
void Snes9xRelease();
void Snes9xSetInterpolationMethod(int32_t interpostionMethod);
void Snes9xRender(int16_t* buf, uint32_t numSamples);
