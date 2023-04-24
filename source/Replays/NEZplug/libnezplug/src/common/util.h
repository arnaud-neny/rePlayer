#ifndef UTIL_H_
#define UTIL_H_

#include "../normalize.h"

__inline static uint32_t GetWordLE(const uint8_t *p);
__inline static uint32_t GetWordBE(const uint8_t *p);
__inline static uint32_t GetDwordLE(const uint8_t *p);
#define GetDwordLEM(p) (uint32_t)((((uint8_t *)p)[0] | (((uint8_t *)p)[1] << 8) | (((uint8_t *)p)[2] << 16) | (((uint8_t *)p)[3] << 24)))
__inline static void SetWordLE(uint8_t *p, uint32_t v);
__inline static void SetDwordLE(uint8_t *p, uint32_t v);


__inline static uint32_t GetWordLE(const uint8_t *p)
{
	return p[0] | (p[1] << 8);
}

__inline static uint32_t GetDwordLE(const uint8_t *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

__inline static uint32_t GetWordBE(const uint8_t *p)
{
	return p[1] | (p[0] << 8);
}

__inline static void SetWordLE(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> (8 * 0)) & 0xFF;
	p[1] = (uint8_t)(v >> (8 * 1)) & 0xFF;
}

__inline static void SetDwordLE(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> (8 * 0)) & 0xFF;
	p[1] = (uint8_t)(v >> (8 * 1)) & 0xFF;
	p[2] = (uint8_t)(v >> (8 * 2)) & 0xFF;
	p[3] = (uint8_t)(v >> (8 * 3)) & 0xFF;
}

#endif
