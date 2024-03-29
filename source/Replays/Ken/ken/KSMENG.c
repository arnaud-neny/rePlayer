// This file has been modified from Ken Silverman's original release!

// EMSCRIPTEN: it seems there is an "updated" version of this impl in the
// "Build engine" project. But since that uses even more x86 ASM crap I'll
// stick to this one..

long ksmsamplerate, ksmnumspeakers = 2, ksmbytespersample = 2;

#include <stdio.h>
#include <string.h>

#include "ken.h"

#if !defined(max) && !defined(__cplusplus)
#define max(a,b)  (((a) > (b)) ? (a) : (b))
#endif
#if !defined(min) && !defined(__cplusplus)
#define min(a,b)  (((a) < (b)) ? (a) : (b))
#endif

static long scale (long a, long d, long c)
{
	return a * d / c;
}


static unsigned char instsdat[256][11] =		// EMSCRIPTEN (was "char")
{
	0x01,0x01,0x92,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x20,0x02,0xf2,0x33,0x00,0x30,0x0e,0xc2,0x10,0x00,0x0a,
	0xd1,0x01,0xe1,0xc3,0x00,0xd2,0x1e,0x52,0xb0,0x00,0x0c,
	0xb1,0x02,0x80,0xff,0x00,0x23,0x29,0xd1,0xb6,0x00,0x00,
	0x20,0x03,0x80,0xff,0x00,0x21,0x14,0xf0,0xff,0x00,0x08,
	0x21,0x00,0x71,0xae,0x00,0x31,0x16,0xe3,0x4e,0x00,0x0e,
	0x61,0x00,0x53,0x3e,0x00,0x71,0x16,0x31,0x4e,0x00,0x0e,
	0x61,0x00,0xf0,0xff,0x00,0x41,0x2d,0xf1,0xff,0x00,0x0e,
	0xc1,0x00,0x82,0xff,0x00,0xe1,0x1e,0x52,0xff,0x00,0x00,
	0x21,0x04,0xe2,0x00,0x00,0x24,0xd0,0xf2,0x00,0x00,0x00,
	0x15,0x04,0xf2,0x72,0x00,0x74,0x4f,0xf2,0x60,0x00,0x08,
	0x00,0x04,0xf0,0xf8,0x00,0x00,0x10,0xf0,0xff,0x00,0x0a,
	0x91,0x02,0x91,0xff,0x00,0x02,0x12,0xb2,0xb6,0x00,0x0a,
	0x31,0x00,0x41,0x2d,0x00,0x24,0x54,0x35,0xfd,0x00,0x0e,
	0x00,0x00,0xd6,0xbf,0x00,0x00,0x0d,0xa8,0xbc,0x00,0x00,
	0x41,0x02,0xd2,0x7c,0x00,0x41,0x19,0x64,0x00,0x00,0x02,
	0x81,0x00,0xd3,0xff,0x00,0xa0,0x23,0xf3,0x00,0x00,0x00,
	0xc1,0x00,0xf6,0xb5,0x00,0x03,0x06,0x0e,0xf0,0x00,0x04,
	0x01,0x04,0xf0,0xff,0x00,0x04,0x2c,0xf1,0xff,0x00,0x00,
	0xa1,0x00,0xc2,0x1f,0x00,0xa1,0x1c,0x62,0x1f,0x00,0x0e,
	0x61,0x00,0xdd,0x16,0x00,0x31,0x1a,0x32,0x16,0x00,0x0c,
	0x61,0x00,0xb5,0x2e,0x00,0x71,0x1e,0x51,0x4e,0x00,0x0e,
	0x21,0x04,0x8b,0x0e,0x00,0x30,0x0f,0x87,0x17,0x00,0x02,
	0x41,0x02,0xf0,0xff,0x00,0x42,0x26,0xf1,0xff,0x00,0x00,
	0x01,0x00,0x84,0xff,0x00,0x21,0x3c,0xe9,0x1d,0x00,0x00,
	0xb1,0x00,0x83,0xff,0x00,0x31,0x19,0xd1,0xb6,0x00,0x0e,
	0xc0,0x03,0xf3,0xf8,0x00,0xc0,0x12,0xf0,0xff,0x00,0x08,
	0x41,0x04,0x84,0xff,0x00,0x63,0x1e,0xf0,0xff,0x00,0x0e,
	0x21,0x00,0x33,0x31,0x00,0x21,0x10,0x11,0x3c,0x00,0x0a,
	0x20,0x00,0x84,0xff,0x00,0x20,0x0c,0x84,0xff,0x00,0x08,
	0x01,0x00,0xa1,0x84,0x00,0xc0,0x18,0x72,0xb3,0x00,0x0e,
	0xe1,0x0a,0xd2,0xf3,0x00,0xe1,0x14,0xb0,0xf3,0x00,0x0c,
	0x21,0x00,0xff,0x0c,0x00,0x01,0x3f,0x00,0x00,0x00,0x00,
	0x91,0x00,0xd2,0x75,0x00,0x92,0x20,0xc1,0x7f,0x00,0x0e,
	0x20,0x00,0xa2,0xa2,0x00,0xe0,0x10,0xa2,0xa2,0x00,0x0c,
	0x21,0x00,0x63,0x0e,0x00,0x21,0x16,0x63,0x0e,0x00,0x0c,
	0x21,0x00,0x44,0x17,0x00,0x71,0x1c,0x51,0x03,0x00,0x0e,
	0x22,0x00,0xf0,0x54,0x00,0x87,0x91,0xf5,0x55,0x00,0x06,
	0xd1,0x00,0xd1,0x31,0x00,0xf3,0x4a,0xb6,0x32,0x00,0x0e,
	0x01,0x00,0xf0,0xf8,0x00,0x01,0x11,0xf0,0xff,0x00,0x0a,
	0x22,0x00,0x6b,0x0e,0x00,0x70,0x8d,0x6e,0x17,0x00,0x02,
	0xe4,0x41,0xf0,0xf2,0x00,0x11,0x03,0x82,0x97,0x00,0x08,
	0x11,0x80,0xf5,0x03,0x00,0x13,0x8c,0xf5,0x21,0x00,0x0e,
	0x41,0x40,0xf6,0xb5,0x00,0x03,0x00,0xef,0x0f,0x00,0x0e,
	0x15,0x00,0xf2,0x72,0x00,0x08,0x4f,0xf2,0x60,0x00,0x08,
	0x31,0x00,0x50,0x2d,0x00,0x24,0x55,0x55,0xfd,0x00,0x0e,
	0xc1,0x00,0xd3,0x38,0x00,0x01,0x14,0xc5,0x58,0x00,0x0c,
	0x01,0x01,0x91,0x7f,0x00,0xc1,0x15,0xc5,0x6f,0x00,0x0c,
	0x02,0x00,0xd2,0x78,0x00,0x43,0x18,0xd3,0x7c,0x00,0x0c,
	0x02,0x00,0xd2,0xfc,0x00,0x11,0x10,0x52,0xfc,0x00,0x0a,
	0xe2,0x00,0xa2,0xa7,0x00,0x21,0x1c,0xc4,0x87,0x00,0x0e,
	0x01,0x00,0xd2,0x73,0x00,0x40,0x18,0xd2,0x73,0x00,0x0e,
	0x00,0x00,0xd2,0x74,0x00,0x00,0x12,0x93,0x72,0x00,0x0a,
	0xd1,0x01,0x91,0x75,0x00,0x11,0x05,0x53,0xd5,0x00,0x04,
	0x81,0x00,0xd2,0xd6,0x00,0x01,0x10,0xb2,0xb4,0x00,0x0a,
	0x01,0x00,0xd2,0xfc,0x00,0x10,0x10,0x52,0xfc,0x00,0x0a,
	0xa0,0x00,0x92,0x76,0x00,0xb0,0x14,0x82,0x67,0x00,0x0a,
	0xf1,0x00,0x51,0x55,0x00,0xb1,0x2d,0x51,0x55,0x00,0x00,
	0x00,0x00,0xf2,0xc3,0x00,0x00,0x10,0xa2,0xa2,0x00,0x0c,
	0x21,0x40,0x69,0x0e,0x00,0x11,0x43,0x9b,0x30,0x00,0x00,
	0xd1,0x00,0xa1,0x31,0x00,0xf3,0x8b,0xb6,0x32,0x01,0x0e,
	0x00,0x00,0xd2,0x73,0x00,0x81,0x18,0xf2,0x10,0x00,0x0e,
	0x81,0x01,0xf2,0x77,0x01,0x31,0x2d,0xf3,0x7d,0x01,0x0e,
	0x00,0x00,0xd2,0x7c,0x03,0xc0,0x0f,0xa2,0x5a,0x00,0x0c,
	0x01,0x00,0xd2,0x7c,0x03,0x01,0x10,0xb2,0x9d,0x00,0x00,
	0x71,0x40,0xf1,0x0c,0x00,0x81,0x03,0x55,0x0d,0x03,0x00,
	0xe1,0x00,0xf3,0xef,0x00,0x90,0x41,0xd7,0x80,0x02,0x00,
	0xf2,0x00,0xf3,0xef,0x00,0x30,0x40,0x66,0xe0,0x01,0x08,
	0x41,0x00,0x82,0x64,0x00,0x00,0x05,0xa2,0x32,0x01,0x0a,
	0x00,0x00,0x84,0xf8,0x01,0x00,0x06,0xa2,0x88,0x00,0x08,
	0x94,0x00,0xda,0x01,0x00,0xc1,0x06,0xda,0x14,0x00,0x06,
	0xc1,0x00,0x82,0x64,0x00,0x40,0x08,0x63,0x32,0x01,0x0a,
	0x11,0x00,0xd2,0x7c,0x01,0x01,0x20,0x74,0xfc,0x03,0x06,
	0xc0,0x00,0x61,0xa6,0x03,0xc1,0x54,0xa3,0xe6,0x00,0x0e,
	0x40,0x00,0x81,0xa6,0x03,0x40,0xca,0x82,0xa6,0x01,0x0c,
	0xd0,0x00,0xa1,0x32,0x01,0xf1,0x8c,0xb6,0x33,0x03,0x06,
	0x02,0x00,0x62,0xa6,0x02,0xa1,0x0c,0x87,0x20,0x00,0x0c,
	0xc0,0x00,0x83,0xf5,0x01,0xc0,0x1e,0xf1,0xf0,0x03,0x00,
	0x40,0x00,0x82,0xf3,0x03,0x40,0x16,0xf1,0xf3,0x00,0x08,
	0x01,0x00,0xf2,0x75,0x00,0x42,0x10,0x35,0x31,0x01,0x0c,
	0x00,0x00,0xf3,0x75,0x00,0x41,0x10,0x35,0x31,0x01,0x0c,
	0x20,0x00,0x63,0x0e,0x00,0x20,0x16,0x63,0x0e,0x00,0x0c,
	0x41,0x00,0x82,0x64,0x00,0x00,0x08,0xa2,0x32,0x01,0x06,
	0x21,0x00,0x63,0x05,0x00,0x21,0x10,0x52,0xf2,0x00,0x0c,
	0x20,0x00,0x63,0x15,0x00,0x20,0x10,0x51,0x91,0x00,0x0c,
	0x00,0x00,0xd2,0x74,0x00,0x80,0x0c,0x93,0x72,0x00,0x0a,
	0xe0,0x00,0x82,0xee,0x01,0x11,0x0a,0x91,0xdd,0x01,0x0a,
	0x21,0x00,0x82,0x83,0x00,0x90,0x8f,0x93,0x72,0x03,0x0c,
	0x01,0x00,0x82,0x85,0x00,0xd0,0x10,0x51,0x44,0x03,0x0c,
	0x00,0x00,0x61,0xc4,0x03,0x10,0x18,0x21,0xd5,0x00,0x08,
	0xf0,0x00,0x81,0xc4,0x01,0xe0,0x0c,0x61,0xd5,0x00,0x0a,
	0x00,0x00,0x62,0xa6,0x03,0x60,0x56,0x62,0xa6,0x01,0x00,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x01,0x01,0xd2,0x7c,0x00,0x01,0x19,0xd2,0x7c,0x00,0x04,
	0x00,0x00,0xf5,0xc8,0x00,0x00,0x00,0xa4,0xff,0x02,0x00,
	0x00,0x00,0xf7,0xc4,0x02,0x00,0x00,0xf7,0x8f,0x00,0x00,
	0x10,0x00,0xd6,0x68,0x00,0x00,0x09,0xe7,0x77,0x00,0x04,
	0x00,0x00,0xc7,0x38,0x00,0x00,0x00,0xf8,0xff,0x00,0x0e,
	0x00,0x00,0xf5,0xc8,0x00,0x00,0x00,0xf7,0x8f,0x00,0x00,
};

#include <stdio.h>
#include <math.h>

#define log2(a) (log(a)/log(2))

extern float lvol[9], rvol[9];
extern long lplc[9], rplc[9];
void adlibinit(long samplerate, long numspeakers, long bytespersample);
void adlib0(long index, long value);
void adlibgetsample(void *sndptr, long numsamples);

static unsigned long note[8192], musicstatus, count, countstop, loopcnt;
static unsigned int nownote, numnotes = 0, numchans, firstime = 1;
unsigned int speed = 240;
static unsigned int drumstat;
static unsigned long chanage[18];
static unsigned char inst[256][11], chanfreq[18], chantrack[18];
static unsigned char trinst[16], trquant[16], trchan[16], trprio[16];
static unsigned char trvol[16], adlibchanoffs[9] = {0,1,2,8,9,10,16,17,18};
static unsigned int adlibfreq[63] =
{
	0,
	2390,2411,2434,2456,2480,2506,2533,2562,2592,2625,2659,2695,
	3414,3435,3458,3480,3504,3530,3557,3586,3616,3649,3683,3719,
	4438,4459,4482,4504,4528,4554,4581,4610,4640,4673,4707,4743,
	5462,5483,5506,5528,5552,5578,5605,5634,5664,5697,5731,5767,
	6486,6507,6530,6552,6576,6602,6629,6658,6688,6721,6755,6791,
	7510
};

static void setinst (int chan, char *v)
{
	adlib0(chan+0xa0,0);
	adlib0(chan+0xb0,0);
	adlib0(chan+0xc0,v[10]);
	chan = adlibchanoffs[chan];
	adlib0(chan+0x20,v[5]);
	adlib0(chan+0x40,v[6]);
	adlib0(chan+0x60,v[7]);
	adlib0(chan+0x80,v[8]);
	adlib0(chan+0xe0,v[9]);
	adlib0(chan+0x23,v[0]);
	adlib0(chan+0x43,v[1]);
	adlib0(chan+0x63,v[2]);
	adlib0(chan+0x83,v[3]);
	adlib0(chan+0xe3,v[4]);
}

static int watrind, wrand[19] =
{
	0x0000,0x53dc,0x2704,0x5665,0x0daa,0x421f,0x3ead,0x4d1d,
	0x2f5a,0x20da,0x2fe5,0x69ac,0x161b,0x261e,0x525f,0x6513,
	0x7e70,0x6679,0x3b6c
};
static int watrand () { watrind++; return(wrand[watrind-1]); }
static void randoinsts ()
{
	long i, j, k;
	float f;

	watrind = 0;
	if (ksmnumspeakers == 2)
	{
		j = (watrand()&2)-1; k = 0;
		for(i=0;i<9;i++)
		{
			if ((i == 0) || (chantrack[i] != chantrack[i-1]))
			{
				f = (float)watrand()/32768.0;
				if (j > 0)
				{
						//lvol[i] < rvol[i]
					lvol[i] = log2(f+1);
					rvol[i] = log2(3-f);
					lplc[i] = watrand()&255;
					rplc[i] = 0;
				}
				else
				{
						//lvol[i] > rvol[i]
					lvol[i] = log2(3-f);
					rvol[i] = log2(f+1);
					lplc[i] = 0;
					rplc[i] = watrand()&255;
				}
				j = -j;
				if (((drumstat&32) == 0) || (i < 6)) k++;
			}
			else
			{
				lvol[i] = lvol[i-1]; rvol[i] = rvol[i-1];
				lplc[i] = lplc[i-1]; rplc[i] = rplc[i-1];
			}
		}
		if (k < 2)  //If only 1 source, force it to be in center
		{
			if (drumstat&32) i = 5; else i = 8;
			for(;i>=0;i--)
			{
				lvol[i] = rvol[i] = 1;
				lplc[i] = rplc[i] = 0;
			}
		}
	}
}

long ksmload (Loader* loader)
{
	char buffer[256], instbuf[11];
	int i, j;

	adlibinit(ksmsamplerate,ksmnumspeakers,ksmbytespersample);
	if (firstime == 1)
	{
		//if (!(fil = fopen("\\c\\cprog\\sm\\insts.dat","rb"))) return(0);
		for(i=0;i<256;i++)
		{
			//fread(buffer,33,1,fil);
			//for(j=0;j<11;j++) inst[i][j] = buffer[j+20];
			for(j=0;j<11;j++) inst[i][j] = instsdat[i][j];
		}
		//fclose(fil);
		firstime = 0;
	}

	numchans = 9;
	adlib0(0x1,32);  //clear test stuff
	adlib0(0x4,0);   //reset
	adlib0(0x8,0);   //2-operator synthesis

	if (loader->Read(loader,trinst,16)) return 0;
	if (loader->Read(loader,trquant,16)) return 0;
	if (loader->Read(loader,trchan,16)) return 0;
	if (loader->Read(loader,trprio,16)) return 0;
	if (loader->Read(loader,trvol,16)) return 0;
	if (loader->Read(loader,&numnotes,2)||(numnotes==0)||(numnotes>(sizeof(note)/4))) return 0;
	if (loader->Read(loader,note,numnotes<<2)) return 0;
	for (i=0;i<numnotes;i++) if (trquant[(note[i]>>8)&15]==0) return 0;
	if (!trchan[11]) { drumstat = 0; numchans = 9; adlib0(0xbd,drumstat); }
	if (trchan[11] == 1)
	{
		// C operator precedence: 8=&, 9=^, 10=|
		for(i=0;i<11;i++) instbuf[i] = inst[trinst[11]][i];
//		instbuf[1] = ((instbuf[1]&192)|(trvol[11])^63);
		instbuf[1] = ((instbuf[1]&192)|((trvol[11])^63));
		setinst(6,instbuf);
		for(i=0;i<5;i++) instbuf[i] = inst[trinst[12]][i];
		for(i=5;i<11;i++) instbuf[i] = inst[trinst[15]][i];
//		instbuf[1] = ((instbuf[1]&192)|(trvol[12])^63);
//		instbuf[6] = ((instbuf[6]&192)|(trvol[15])^63);
		instbuf[1] = ((instbuf[1]&192)|((trvol[12])^63));
		instbuf[6] = ((instbuf[6]&192)|((trvol[15])^63));
		setinst(7,instbuf);
		for(i=0;i<5;i++) instbuf[i] = inst[trinst[14]][i];
		for(i=5;i<11;i++) instbuf[i] = inst[trinst[13]][i];
//		instbuf[1] = ((instbuf[1]&192)|(trvol[14])^63);
//		instbuf[6] = ((instbuf[6]&192)|(trvol[13])^63);
		instbuf[1] = ((instbuf[1]&192)|((trvol[14])^63));
		instbuf[6] = ((instbuf[6]&192)|((trvol[13])^63));
		setinst(8,instbuf);
		drumstat = 32; numchans = 6; adlib0(0xbd,drumstat);
	}
	randoinsts();
	loopcnt = 0;

	return(scale((note[numnotes-1]>>12)-(note[0]>>12),1000,speed));
}

void ksmmusicon ()
{
	long i, j, k;
	char instbuf[11];

	for(i=0;i<numchans;i++) chantrack[i] = chanage[i] = 0;
	j = 0;
	for(i=0;i<16;i++)
		if ((trchan[i] > 0) && (j < numchans))
		{
			k = trchan[i];
			while ((j < numchans) && (k > 0)) { chantrack[j] = i; k--; j++; }
		}
	for(i=0;i<numchans;i++)
	{
		for(j=0;j<11;j++) instbuf[j] = inst[trinst[chantrack[i]]][j];
		instbuf[1] = ((instbuf[1]&192)|(trvol[chantrack[i]]^63));
		setinst(i,instbuf);
		chanfreq[i] = 0;
	}
	count = countstop = (note[0]>>12)-1;
	nownote = 0; musicstatus = 1;
}

void ksmmusicoff ()
{
	int i;

	for(i=0;i<numchans;i++) { adlib0(0xa0+i,0); adlib0(0xb0+i,0); }
	musicstatus = 0;
}

static void ksmnexttic ()
{
	long i, j, k, track, volevel, templong;

	count++;
	while (count >= countstop)
	{
		templong = note[nownote]; track = ((templong>>8)&15);
		if (!(templong&192))
		{
			i = 0;
			while (((chanfreq[i] != (templong&63)) || (chantrack[i] != ((templong>>8)&15))) && (i < numchans))
				i++;
			if (i < numchans)
			{
				adlib0(i+0xb0,(adlibfreq[templong&63]>>8)&0xdf);
				chanfreq[i] = chanage[i] = 0;
			}
		}
		else
		{
			volevel = trvol[track]^63;
			if ((templong&192) == 128) volevel = min(volevel+4,63);
			if ((templong&192) == 192) volevel = max(volevel-4,0);
			if (track < 11)
			{
				i = numchans; k = 0;
				for(j=0;j<numchans;j++)
					if ((countstop-chanage[j] >= k) && (chantrack[j] == track))
						{ k = countstop-chanage[j]; i = j; }
				if (i < numchans)
				{
					adlib0(i+0xb0,0);
					adlib0(adlibchanoffs[i]+0x43,(inst[trinst[track]][1]&192)+volevel);
					adlib0(i+0xa0,adlibfreq[templong&63]);
					adlib0(i+0xb0,(adlibfreq[templong&63]>>8)|32);
					chanfreq[i] = templong&63; chanage[i] = countstop;
				}
			}
			else if (drumstat&32)
			{
				j = adlibfreq[templong&63];
				switch(track)
				{
					case 11: i = 6; j -= 2048; break;
					case 12: case 15: i = 7; j -= 2048; break;
					case 13: case 14: i = 8; break;
				}
				adlib0(i+0xa0,j);
				adlib0(i+0xb0,(j>>8)&0xdf);
				adlib0(0xbd,drumstat&(~(32768>>track)));
				if ((track == 11) || (track == 12) || (track == 14))
					adlib0(adlibchanoffs[i]+0x43,(inst[trinst[track]][1]&192)+volevel);
				else
					adlib0(adlibchanoffs[i]+0x40,(inst[trinst[track]][6]&192)+volevel);
				drumstat |= (32768>>track);
				adlib0(0xbd,drumstat);
			}
		}
		nownote++; if (nownote >= numnotes) { nownote = 0; loopcnt++; }
		templong = note[nownote];
		if (!nownote) count = (templong>>12)-1;
		i = 240 / trquant[(templong>>8)&15];
		countstop = (templong>>12)+(i>>1); countstop -= countstop%i;
	}
}

static long minicnt = 0;
long ksmrendersound (void *dasnd, long numbytestoprocess)
{
	long i, prepcnt;

	prepcnt = numbytestoprocess;
	while (prepcnt > 0)
	{
		i = min(prepcnt,(minicnt/speed+4)&~3);
		adlibgetsample(dasnd,i);
		dasnd = (void *)(((uintptr_t)dasnd)+i);
		prepcnt -= i;

		minicnt -= speed*i;
		while (minicnt < 0)
		{
			minicnt += ksmsamplerate*ksmnumspeakers*ksmbytespersample;
			ksmnexttic();
		}
	}
	return(loopcnt);
}

void ksmseek (long seektoms)
{
	long i;

	ksmmusicoff();

	i = (((note[0]>>12)+scale(seektoms,speed,1000))<<12);
	nownote = 0;
	while ((note[nownote] < i) && (nownote < numnotes)) nownote++;
	if (nownote >= numnotes) nownote = 0;

	count = countstop = (note[nownote]>>12)-1; musicstatus = 1;
}
