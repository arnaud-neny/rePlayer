/* ======================================================================== */
/* ========================= LICENSING & COPYRIGHT ======================== */
/* ======================================================================== */
/*
 *                                  MUSASHI
 *                                Version 4.60
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


/* ======================================================================== */
/* ================================= NOTES ================================ */
/* ======================================================================== */



/* ======================================================================== */
/* ================================ INCLUDES ============================== */
/* ======================================================================== */

extern void m68040_fpu_op0(void);
extern void m68040_fpu_op1(void);
extern void m68881_mmu_ops(void);
extern unsigned char m68ki_cycles[][0x10000];
//extern void (*m68ki_instruction_jump_table[0x10000])(void); /* opcode handler jump table */
extern void m68ki_build_opcode_table(void);

#include "m68kops.h"
#include "m68kcpu.h"

/* ======================================================================== */
/* ================================= DATA ================================= */
/* ======================================================================== */

#ifdef M68K_LOG_ENABLE
const char *const m68ki_cpu_names[] =
{
	"Invalid CPU",
	"M68000",
	"M68010",
	"M68EC020",
	"M68020",
	"M68EC030",
	"M68030",
	"M68EC040",
	"M68LC040",
	"M68040",
	"SCC68070",
};
#endif /* M68K_LOG_ENABLE */

/* The CPU core */
//m68ki_cpu_core m68ki_cpu = {0};

#if M68K_EMULATE_ADDRESS_ERROR
#ifdef _BSD_SETJMP_H
sigjmp_buf m68ki_aerr_trap;
#else
jmp_buf m68ki_aerr_trap;
#endif
#endif /* M68K_EMULATE_ADDRESS_ERROR */

//uint    m68ki_aerr_address;
//uint    m68ki_aerr_write_mode;
//uint    m68ki_aerr_fc;

/* Used by shift & rotate instructions */
const uint8 m68ki_shift_8_table[65] =
{
	0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff
};
const uint16 m68ki_shift_16_table[65] =
{
	0x0000, 0x8000, 0xc000, 0xe000, 0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00,
	0xff80, 0xffc0, 0xffe0, 0xfff0, 0xfff8, 0xfffc, 0xfffe, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff
};
const uint m68ki_shift_32_table[65] =
{
	0x00000000, 0x80000000, 0xc0000000, 0xe0000000, 0xf0000000, 0xf8000000,
	0xfc000000, 0xfe000000, 0xff000000, 0xff800000, 0xffc00000, 0xffe00000,
	0xfff00000, 0xfff80000, 0xfffc0000, 0xfffe0000, 0xffff0000, 0xffff8000,
	0xffffc000, 0xffffe000, 0xfffff000, 0xfffff800, 0xfffffc00, 0xfffffe00,
	0xffffff00, 0xffffff80, 0xffffffc0, 0xffffffe0, 0xfffffff0, 0xfffffff8,
	0xfffffffc, 0xfffffffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};


/* Number of clock cycles to use for exception processing.
 * I used 4 for any vectors that are undocumented for processing times.
 */
const uint8 m68ki_exception_cycle_table[5][256] =
{
	{ /* 000 */
		 40, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		 50, /*  2: Bus Error                             (unemulated) */
		 50, /*  3: Address Error                         (unemulated) */
		 34, /*  4: Illegal Instruction                                */
		 38, /*  5: Divide by Zero                                     */
		 40, /*  6: CHK                                                */
		 34, /*  7: TRAPV                                              */
		 34, /*  8: Privilege Violation                                */
		 34, /*  9: Trace                                              */
		 34, /* 10: 1010                                               */
		 34, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 44, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 44, /* 24: Spurious Interrupt                                 */
		 44, /* 25: Level 1 Interrupt Autovector                       */
		 44, /* 26: Level 2 Interrupt Autovector                       */
		 44, /* 27: Level 3 Interrupt Autovector                       */
		 44, /* 28: Level 4 Interrupt Autovector                       */
		 44, /* 29: Level 5 Interrupt Autovector                       */
		 44, /* 30: Level 6 Interrupt Autovector                       */
		 44, /* 31: Level 7 Interrupt Autovector                       */
		 34, /* 32: TRAP #0                                            */
		 34, /* 33: TRAP #1                                            */
		 34, /* 34: TRAP #2                                            */
		 34, /* 35: TRAP #3                                            */
		 34, /* 36: TRAP #4                                            */
		 34, /* 37: TRAP #5                                            */
		 34, /* 38: TRAP #6                                            */
		 34, /* 39: TRAP #7                                            */
		 34, /* 40: TRAP #8                                            */
		 34, /* 41: TRAP #9                                            */
		 34, /* 42: TRAP #10                                           */
		 34, /* 43: TRAP #11                                           */
		 34, /* 44: TRAP #12                                           */
		 34, /* 45: TRAP #13                                           */
		 34, /* 46: TRAP #14                                           */
		 34, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	},
	{ /* 010 */
		 40, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		126, /*  2: Bus Error                             (unemulated) */
		126, /*  3: Address Error                         (unemulated) */
		 38, /*  4: Illegal Instruction                                */
		 44, /*  5: Divide by Zero                                     */
		 44, /*  6: CHK                                                */
		 34, /*  7: TRAPV                                              */
		 38, /*  8: Privilege Violation                                */
		 38, /*  9: Trace                                              */
		  4, /* 10: 1010                                               */
		  4, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 44, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 46, /* 24: Spurious Interrupt                                 */
		 46, /* 25: Level 1 Interrupt Autovector                       */
		 46, /* 26: Level 2 Interrupt Autovector                       */
		 46, /* 27: Level 3 Interrupt Autovector                       */
		 46, /* 28: Level 4 Interrupt Autovector                       */
		 46, /* 29: Level 5 Interrupt Autovector                       */
		 46, /* 30: Level 6 Interrupt Autovector                       */
		 46, /* 31: Level 7 Interrupt Autovector                       */
		 38, /* 32: TRAP #0                                            */
		 38, /* 33: TRAP #1                                            */
		 38, /* 34: TRAP #2                                            */
		 38, /* 35: TRAP #3                                            */
		 38, /* 36: TRAP #4                                            */
		 38, /* 37: TRAP #5                                            */
		 38, /* 38: TRAP #6                                            */
		 38, /* 39: TRAP #7                                            */
		 38, /* 40: TRAP #8                                            */
		 38, /* 41: TRAP #9                                            */
		 38, /* 42: TRAP #10                                           */
		 38, /* 43: TRAP #11                                           */
		 38, /* 44: TRAP #12                                           */
		 38, /* 45: TRAP #13                                           */
		 38, /* 46: TRAP #14                                           */
		 38, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	},
	{ /* 020 */
		  4, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		 50, /*  2: Bus Error                             (unemulated) */
		 50, /*  3: Address Error                         (unemulated) */
		 20, /*  4: Illegal Instruction                                */
		 38, /*  5: Divide by Zero                                     */
		 40, /*  6: CHK                                                */
		 20, /*  7: TRAPV                                              */
		 34, /*  8: Privilege Violation                                */
		 25, /*  9: Trace                                              */
		 20, /* 10: 1010                                               */
		 20, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 30, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 30, /* 24: Spurious Interrupt                                 */
		 30, /* 25: Level 1 Interrupt Autovector                       */
		 30, /* 26: Level 2 Interrupt Autovector                       */
		 30, /* 27: Level 3 Interrupt Autovector                       */
		 30, /* 28: Level 4 Interrupt Autovector                       */
		 30, /* 29: Level 5 Interrupt Autovector                       */
		 30, /* 30: Level 6 Interrupt Autovector                       */
		 30, /* 31: Level 7 Interrupt Autovector                       */
		 20, /* 32: TRAP #0                                            */
		 20, /* 33: TRAP #1                                            */
		 20, /* 34: TRAP #2                                            */
		 20, /* 35: TRAP #3                                            */
		 20, /* 36: TRAP #4                                            */
		 20, /* 37: TRAP #5                                            */
		 20, /* 38: TRAP #6                                            */
		 20, /* 39: TRAP #7                                            */
		 20, /* 40: TRAP #8                                            */
		 20, /* 41: TRAP #9                                            */
		 20, /* 42: TRAP #10                                           */
		 20, /* 43: TRAP #11                                           */
		 20, /* 44: TRAP #12                                           */
		 20, /* 45: TRAP #13                                           */
		 20, /* 46: TRAP #14                                           */
		 20, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	},
	{ /* 030 - not correct */
		  4, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		 50, /*  2: Bus Error                             (unemulated) */
		 50, /*  3: Address Error                         (unemulated) */
		 20, /*  4: Illegal Instruction                                */
		 38, /*  5: Divide by Zero                                     */
		 40, /*  6: CHK                                                */
		 20, /*  7: TRAPV                                              */
		 34, /*  8: Privilege Violation                                */
		 25, /*  9: Trace                                              */
		 20, /* 10: 1010                                               */
		 20, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 30, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 30, /* 24: Spurious Interrupt                                 */
		 30, /* 25: Level 1 Interrupt Autovector                       */
		 30, /* 26: Level 2 Interrupt Autovector                       */
		 30, /* 27: Level 3 Interrupt Autovector                       */
		 30, /* 28: Level 4 Interrupt Autovector                       */
		 30, /* 29: Level 5 Interrupt Autovector                       */
		 30, /* 30: Level 6 Interrupt Autovector                       */
		 30, /* 31: Level 7 Interrupt Autovector                       */
		 20, /* 32: TRAP #0                                            */
		 20, /* 33: TRAP #1                                            */
		 20, /* 34: TRAP #2                                            */
		 20, /* 35: TRAP #3                                            */
		 20, /* 36: TRAP #4                                            */
		 20, /* 37: TRAP #5                                            */
		 20, /* 38: TRAP #6                                            */
		 20, /* 39: TRAP #7                                            */
		 20, /* 40: TRAP #8                                            */
		 20, /* 41: TRAP #9                                            */
		 20, /* 42: TRAP #10                                           */
		 20, /* 43: TRAP #11                                           */
		 20, /* 44: TRAP #12                                           */
		 20, /* 45: TRAP #13                                           */
		 20, /* 46: TRAP #14                                           */
		 20, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	},
	{ /* 040 */ // TODO: these values are not correct
		  4, /*  0: Reset - Initial Stack Pointer                      */
		  4, /*  1: Reset - Initial Program Counter                    */
		 50, /*  2: Bus Error                             (unemulated) */
		 50, /*  3: Address Error                         (unemulated) */
		 20, /*  4: Illegal Instruction                                */
		 38, /*  5: Divide by Zero                                     */
		 40, /*  6: CHK                                                */
		 20, /*  7: TRAPV                                              */
		 34, /*  8: Privilege Violation                                */
		 25, /*  9: Trace                                              */
		 20, /* 10: 1010                                               */
		 20, /* 11: 1111                                               */
		  4, /* 12: RESERVED                                           */
		  4, /* 13: Coprocessor Protocol Violation        (unemulated) */
		  4, /* 14: Format Error                                       */
		 30, /* 15: Uninitialized Interrupt                            */
		  4, /* 16: RESERVED                                           */
		  4, /* 17: RESERVED                                           */
		  4, /* 18: RESERVED                                           */
		  4, /* 19: RESERVED                                           */
		  4, /* 20: RESERVED                                           */
		  4, /* 21: RESERVED                                           */
		  4, /* 22: RESERVED                                           */
		  4, /* 23: RESERVED                                           */
		 30, /* 24: Spurious Interrupt                                 */
		 30, /* 25: Level 1 Interrupt Autovector                       */
		 30, /* 26: Level 2 Interrupt Autovector                       */
		 30, /* 27: Level 3 Interrupt Autovector                       */
		 30, /* 28: Level 4 Interrupt Autovector                       */
		 30, /* 29: Level 5 Interrupt Autovector                       */
		 30, /* 30: Level 6 Interrupt Autovector                       */
		 30, /* 31: Level 7 Interrupt Autovector                       */
		 20, /* 32: TRAP #0                                            */
		 20, /* 33: TRAP #1                                            */
		 20, /* 34: TRAP #2                                            */
		 20, /* 35: TRAP #3                                            */
		 20, /* 36: TRAP #4                                            */
		 20, /* 37: TRAP #5                                            */
		 20, /* 38: TRAP #6                                            */
		 20, /* 39: TRAP #7                                            */
		 20, /* 40: TRAP #8                                            */
		 20, /* 41: TRAP #9                                            */
		 20, /* 42: TRAP #10                                           */
		 20, /* 43: TRAP #11                                           */
		 20, /* 44: TRAP #12                                           */
		 20, /* 45: TRAP #13                                           */
		 20, /* 46: TRAP #14                                           */
		 20, /* 47: TRAP #15                                           */
		  4, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
		  4, /* 49: FP Inexact Result                     (unemulated) */
		  4, /* 50: FP Divide by Zero                     (unemulated) */
		  4, /* 51: FP Underflow                          (unemulated) */
		  4, /* 52: FP Operand Error                      (unemulated) */
		  4, /* 53: FP Overflow                           (unemulated) */
		  4, /* 54: FP Signaling NAN                      (unemulated) */
		  4, /* 55: FP Unimplemented Data Type            (unemulated) */
		  4, /* 56: MMU Configuration Error               (unemulated) */
		  4, /* 57: MMU Illegal Operation Error           (unemulated) */
		  4, /* 58: MMU Access Level Violation Error      (unemulated) */
		  4, /* 59: RESERVED                                           */
		  4, /* 60: RESERVED                                           */
		  4, /* 61: RESERVED                                           */
		  4, /* 62: RESERVED                                           */
		  4, /* 63: RESERVED                                           */
		     /* 64-255: User Defined                                   */
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4
	}
};

const uint8 m68ki_ea_idx_cycle_table[64] =
{
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0, /* ..01.000 no memory indirect, base NULL             */
	 5, /* ..01..01 memory indirect,    base NULL, outer NULL */
	 7, /* ..01..10 memory indirect,    base NULL, outer 16   */
	 7, /* ..01..11 memory indirect,    base NULL, outer 32   */
	 0,  5,  7,  7,  0,  5,  7,  7,  0,  5,  7,  7,
	 2, /* ..10.000 no memory indirect, base 16               */
	 7, /* ..10..01 memory indirect,    base 16,   outer NULL */
	 9, /* ..10..10 memory indirect,    base 16,   outer 16   */
	 9, /* ..10..11 memory indirect,    base 16,   outer 32   */
	 0,  7,  9,  9,  0,  7,  9,  9,  0,  7,  9,  9,
	 6, /* ..11.000 no memory indirect, base 32               */
	11, /* ..11..01 memory indirect,    base 32,   outer NULL */
	13, /* ..11..10 memory indirect,    base 32,   outer 16   */
	13, /* ..11..11 memory indirect,    base 32,   outer 32   */
	 0, 11, 13, 13,  0, 11, 13, 13,  0, 11, 13, 13
};



/* ======================================================================== */
/* =============================== CALLBACKS ============================== */
/* ======================================================================== */

#if M68K_EMULATE_ADDRESS_ERROR
	fail
	#include <setjmp.h>
	#ifdef _BSD_SETJMP_H
	sigjmp_buf m68ki_aerr_trap;
	#else
	jmp_buf m68ki_aerr_trap;
	#endif
#endif /* M68K_EMULATE_ADDRESS_ERROR */


/* ======================================================================== */
/* ============================== MAME STUFF ============================== */
/* ======================================================================== */


/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */
