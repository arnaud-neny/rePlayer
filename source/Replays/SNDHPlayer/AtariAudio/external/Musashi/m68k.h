/* ======================================================================== */
/* ========================= LICENSING & COPYRIGHT ======================== */
/* ======================================================================== */
/*
 *                                  MUSASHI
 *                                Version 3.32
 *
 * A portable Motorola M680x0 processor emulation engine.
 * Copyright Karl Stenerud.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef M68K__HEADER
#define M68K__HEADER


#ifndef M68K_ARRAY_LENGTH
#define M68K_ARRAY_LENGTH(x)         (sizeof(x) / sizeof(x[0]))
#endif

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

/* ======================================================================== */
/* ============================= CONFIGURATION ============================ */
/* ======================================================================== */

/* Import the configuration for this build */
#ifdef MUSASHI_CNF
#include MUSASHI_CNF
#else
#include "m68kconf.h"
#endif

/* ======================================================================== */
/* ============================ GENERAL DEFINES =========================== */

/* ======================================================================== */

/* There are 7 levels of interrupt to the 68K.
 * A transition from < 7 to 7 will cause a non-maskable interrupt (NMI).
 */
#define M68K_IRQ_NONE 0
#define M68K_IRQ_1    1
#define M68K_IRQ_2    2
#define M68K_IRQ_3    3
#define M68K_IRQ_4    4
#define M68K_IRQ_5    5
#define M68K_IRQ_6    6
#define M68K_IRQ_7    7


/* Special interrupt acknowledge values.
 * Use these as special returns from the interrupt acknowledge callback
 * (specified later in this header).
 */

/* Causes an interrupt autovector (0x18 + interrupt level) to be taken.
 * This happens in a real 68K if VPA or AVEC is asserted during an interrupt
 * acknowledge cycle instead of DTACK.
 */
#define M68K_INT_ACK_AUTOVECTOR    0xffffffff

/* Causes the spurious interrupt vector (0x18) to be taken
 * This happens in a real 68K if BERR is asserted during the interrupt
 * acknowledge cycle (i.e. no devices responded to the acknowledge).
 */
#define M68K_INT_ACK_SPURIOUS      0xfffffffe


/* CPU types for use in m68k_set_cpu_type() */
enum
{
	M68K_CPU_TYPE_INVALID,
	M68K_CPU_TYPE_68000,
	M68K_CPU_TYPE_68010,
	M68K_CPU_TYPE_68EC020,
	M68K_CPU_TYPE_68020,
	M68K_CPU_TYPE_68EC030,
	M68K_CPU_TYPE_68030,
	M68K_CPU_TYPE_68EC040,
	M68K_CPU_TYPE_68LC040,
	M68K_CPU_TYPE_68040,
	M68K_CPU_TYPE_SCC68070
};

/* Registers used by m68k_get_reg() and m68k_set_reg() */
typedef enum
{
	/* Real registers */
	M68K_REG_D0,		/* Data registers */
	M68K_REG_D1,
	M68K_REG_D2,
	M68K_REG_D3,
	M68K_REG_D4,
	M68K_REG_D5,
	M68K_REG_D6,
	M68K_REG_D7,
	M68K_REG_A0,		/* Address registers */
	M68K_REG_A1,
	M68K_REG_A2,
	M68K_REG_A3,
	M68K_REG_A4,
	M68K_REG_A5,
	M68K_REG_A6,
	M68K_REG_A7,
	M68K_REG_PC,		/* Program Counter */
	M68K_REG_SR,		/* Status Register */
	M68K_REG_SP,		/* The current Stack Pointer (located in A7) */
	M68K_REG_USP,		/* User Stack Pointer */
	M68K_REG_ISP,		/* Interrupt Stack Pointer */
	M68K_REG_MSP,		/* Master Stack Pointer */
	M68K_REG_SFC,		/* Source Function Code */
	M68K_REG_DFC,		/* Destination Function Code */
	M68K_REG_VBR,		/* Vector Base Register */
	M68K_REG_CACR,		/* Cache Control Register */
	M68K_REG_CAAR,		/* Cache Address Register */

	/* Assumed registers */
	/* These are cheat registers which emulate the 1-longword prefetch
	 * present in the 68000 and 68010.
	 */
	M68K_REG_PREF_ADDR,	/* Last prefetch address */
	M68K_REG_PREF_DATA,	/* Last prefetch data */

	/* Convenience registers */
	M68K_REG_PPC,		/* Previous value in the program counter */
	M68K_REG_IR,		/* Instruction register */
	M68K_REG_CPU_TYPE	/* Type of CPU being run */
} m68k_register_t;

/* ======================================================================== */
/* ====================== FUNCTIONS CALLED BY THE CPU ===================== */
/* ======================================================================== */

/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */


#endif /* M68K__HEADER */
