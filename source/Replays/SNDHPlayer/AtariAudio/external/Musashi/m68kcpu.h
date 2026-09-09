/* ======================================================================== */
/* ========================= LICENSING & COPYRIGHT ======================== */
/* ======================================================================== */
/*
 *                                  MUSASHI
 *                                Version 4.5
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




#ifndef M68KCPU__HEADER
#define M68KCPU__HEADER

#include "m68k.h"

#include <assert.h>
#include <limits.h>

#include <setjmp.h>

/* ======================================================================== */
/* ==================== ARCHITECTURE-DEPENDANT DEFINES ==================== */
/* ======================================================================== */

/* Check for > 32bit sizes */
#if UINT_MAX > 0xffffffff
	#define M68K_INT_GT_32_BIT  1
#else
	#define M68K_INT_GT_32_BIT  0
#endif

/* Data types used in this emulation core */
#undef sint8
#undef sint16
#undef sint32
#undef sint64
#undef uint8
#undef uint16
#undef uint32
#undef uint64
#undef sint
#undef uint

typedef signed   char  sint8;  		/* ASG: changed from char to signed char */
typedef signed   short sint16;
typedef signed   int   sint32; 		/* AWJ: changed from long to int */
typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32; 			/* AWJ: changed from long to int */
typedef sint32 int32;
typedef sint16 int16;
typedef sint8 int8;

/* signed and unsigned int must be at least 32 bits wide */
typedef signed   int sint;
typedef unsigned int uint;


#if M68K_USE_64_BIT
typedef signed   long long sint64;
typedef unsigned long long uint64;
#else
typedef sint32 sint64;
typedef uint32 uint64;
#endif /* M68K_USE_64_BIT */

/* U64 and S64 are used to wrap long integer constants. */
#ifdef __GNUC__
#define U64(val) val##ULL
#define S64(val) val##LL
#else
#define U64(val) val
#define S64(val) val
#endif

#define MAKE_INT_8(A) (sint8)(A)
#define MAKE_INT_16(A) (sint16)(A)
#define MAKE_INT_32(A) (sint32)(A)

/* ======================================================================== */
/* ============================ GENERAL DEFINES =========================== */
/* ======================================================================== */

/* Exception Vectors handled by emulation */
#define EXCEPTION_RESET                    0
#define EXCEPTION_BUS_ERROR                2 /* This one is not emulated! */
#define EXCEPTION_ADDRESS_ERROR            3 /* This one is partially emulated (doesn't stack a proper frame yet) */
#define M68K_EXCEPTION_ILLEGAL_INSTRUCTION 4
#define EXCEPTION_ZERO_DIVIDE              5
#define EXCEPTION_CHK                      6
#define EXCEPTION_TRAPV                    7
#define EXCEPTION_PRIVILEGE_VIOLATION      8
#define EXCEPTION_TRACE                    9
#define EXCEPTION_1010                    10
#define EXCEPTION_1111                    11
#define EXCEPTION_FORMAT_ERROR            14
#define EXCEPTION_UNINITIALIZED_INTERRUPT 15
#define EXCEPTION_SPURIOUS_INTERRUPT      24
#define EXCEPTION_INTERRUPT_AUTOVECTOR    24
#define EXCEPTION_TRAP_BASE               32

/* Function codes set by CPU during data/address bus activity */
#define FUNCTION_CODE_USER_DATA          1
#define FUNCTION_CODE_USER_PROGRAM       2
#define FUNCTION_CODE_SUPERVISOR_DATA    5
#define FUNCTION_CODE_SUPERVISOR_PROGRAM 6
#define FUNCTION_CODE_CPU_SPACE          7

/* CPU types for deciding what to emulate */
#define CPU_TYPE_000	(0x00000001)
#define CPU_TYPE_008    (0x00000002)
#define CPU_TYPE_010    (0x00000004)
#define CPU_TYPE_EC020  (0x00000008)
#define CPU_TYPE_020    (0x00000010)
#define CPU_TYPE_EC030  (0x00000020)
#define CPU_TYPE_030    (0x00000040)
#define CPU_TYPE_EC040  (0x00000080)
#define CPU_TYPE_LC040  (0x00000100)
#define CPU_TYPE_040    (0x00000200)
#define CPU_TYPE_SCC070 (0x00000400)

/* Different ways to stop the CPU */
#define STOP_LEVEL_STOP 1
#define STOP_LEVEL_HALT 2

/* Used for 68000 address error processing */
#define INSTRUCTION_YES 0
#define INSTRUCTION_NO  0x08
#define MODE_READ       0x10
#define MODE_WRITE      0

#define RUN_MODE_NORMAL              0
#define RUN_MODE_BERR_AERR_RESET_WSF 1 /* writing stack frame */
#define RUN_MODE_BERR_AERR_RESET     2 /* stack frame done */

#ifndef NULL
#define NULL ((void*)0)
#endif

/* ======================================================================== */
/* ================================ MACROS ================================ */
/* ======================================================================== */


/* ---------------------------- General Macros ---------------------------- */

/* Bit Isolation Macros */
#define BIT_0(A)  ((A) & 0x00000001)
#define BIT_1(A)  ((A) & 0x00000002)
#define BIT_2(A)  ((A) & 0x00000004)
#define BIT_3(A)  ((A) & 0x00000008)
#define BIT_4(A)  ((A) & 0x00000010)
#define BIT_5(A)  ((A) & 0x00000020)
#define BIT_6(A)  ((A) & 0x00000040)
#define BIT_7(A)  ((A) & 0x00000080)
#define BIT_8(A)  ((A) & 0x00000100)
#define BIT_9(A)  ((A) & 0x00000200)
#define BIT_A(A)  ((A) & 0x00000400)
#define BIT_B(A)  ((A) & 0x00000800)
#define BIT_C(A)  ((A) & 0x00001000)
#define BIT_D(A)  ((A) & 0x00002000)
#define BIT_E(A)  ((A) & 0x00004000)
#define BIT_F(A)  ((A) & 0x00008000)
#define BIT_10(A) ((A) & 0x00010000)
#define BIT_11(A) ((A) & 0x00020000)
#define BIT_12(A) ((A) & 0x00040000)
#define BIT_13(A) ((A) & 0x00080000)
#define BIT_14(A) ((A) & 0x00100000)
#define BIT_15(A) ((A) & 0x00200000)
#define BIT_16(A) ((A) & 0x00400000)
#define BIT_17(A) ((A) & 0x00800000)
#define BIT_18(A) ((A) & 0x01000000)
#define BIT_19(A) ((A) & 0x02000000)
#define BIT_1A(A) ((A) & 0x04000000)
#define BIT_1B(A) ((A) & 0x08000000)
#define BIT_1C(A) ((A) & 0x10000000)
#define BIT_1D(A) ((A) & 0x20000000)
#define BIT_1E(A) ((A) & 0x40000000)
#define BIT_1F(A) ((A) & 0x80000000)

/* Get the most significant bit for specific sizes */
#define GET_MSB_8(A)  ((A) & 0x80)
#define GET_MSB_9(A)  ((A) & 0x100)
#define GET_MSB_16(A) ((A) & 0x8000)
#define GET_MSB_17(A) ((A) & 0x10000)
#define GET_MSB_32(A) ((A) & 0x80000000)
#if M68K_USE_64_BIT
#define GET_MSB_33(A) ((A) & 0x100000000)
#endif /* M68K_USE_64_BIT */

/* Isolate nibbles */
#define LOW_NIBBLE(A)  ((A) & 0x0f)
#define HIGH_NIBBLE(A) ((A) & 0xf0)

/* These are used to isolate 8, 16, and 32 bit sizes */
#define MASK_OUT_ABOVE_2(A)  ((A) & 3)
#define MASK_OUT_ABOVE_8(A)  ((A) & 0xff)
#define MASK_OUT_ABOVE_16(A) ((A) & 0xffff)
#define MASK_OUT_BELOW_2(A)  ((A) & ~3)
#define MASK_OUT_BELOW_8(A)  ((A) & ~0xff)
#define MASK_OUT_BELOW_16(A) ((A) & ~0xffff)

/* No need to mask if we are 32 bit */
#if M68K_INT_GT_32_BIT || M68K_USE_64_BIT
	#define MASK_OUT_ABOVE_32(A) ((A) & 0xffffffff)
	#define MASK_OUT_BELOW_32(A) ((A) & ~0xffffffff)
#else
	#define MASK_OUT_ABOVE_32(A) (A)
	#define MASK_OUT_BELOW_32(A) 0
#endif /* M68K_INT_GT_32_BIT || M68K_USE_64_BIT */

/* Simulate address lines of 68000 only */
#define ADDRESS_68K(A) ((A)&0x00ffffff)


/* Shift & Rotate Macros. */
#define LSL(A, C) ((A) << (C))
#define LSR(A, C) ((A) >> (C))

/* Some > 32-bit optimizations */
#if M68K_INT_GT_32_BIT
	/* Shift left and right */
	#define LSR_32(A, C) ((A) >> (C))
	#define LSL_32(A, C) ((A) << (C))
#else
	/* We have to do this because the morons at ANSI decided that shifts
	 * by >= data size are undefined.
	 */
	#define LSR_32(A, C) ((C) < 32 ? (A) >> (C) : 0)
	#define LSL_32(A, C) ((C) < 32 ? (A) << (C) : 0)
#endif /* M68K_INT_GT_32_BIT */

#if M68K_USE_64_BIT
	#define LSL_32_64(A, C) ((A) << (C))
	#define LSR_32_64(A, C) ((A) >> (C))
	#define ROL_33_64(A, C) (LSL_32_64(A, C) | LSR_32_64(A, 33-(C)))
	#define ROR_33_64(A, C) (LSR_32_64(A, C) | LSL_32_64(A, 33-(C)))
#endif /* M68K_USE_64_BIT */

#define ROL_8(A, C)      MASK_OUT_ABOVE_8(LSL(A, C) | LSR(A, 8-(C)))
#define ROL_9(A, C)                      (LSL(A, C) | LSR(A, 9-(C)))
#define ROL_16(A, C)    MASK_OUT_ABOVE_16(LSL(A, C) | LSR(A, 16-(C)))
#define ROL_17(A, C)                     (LSL(A, C) | LSR(A, 17-(C)))
#define ROL_32(A, C)    MASK_OUT_ABOVE_32(LSL_32(A, C) | LSR_32(A, 32-(C)))
#define ROL_33(A, C)                     (LSL_32(A, C) | LSR_32(A, 33-(C)))

#define ROR_8(A, C)      MASK_OUT_ABOVE_8(LSR(A, C) | LSL(A, 8-(C)))
#define ROR_9(A, C)                      (LSR(A, C) | LSL(A, 9-(C)))
#define ROR_16(A, C)    MASK_OUT_ABOVE_16(LSR(A, C) | LSL(A, 16-(C)))
#define ROR_17(A, C)                     (LSR(A, C) | LSL(A, 17-(C)))
#define ROR_32(A, C)    MASK_OUT_ABOVE_32(LSR_32(A, C) | LSL_32(A, 32-(C)))
#define ROR_33(A, C)                     (LSR_32(A, C) | LSL_32(A, 33-(C)))



/* ------------------------------ CPU Access ------------------------------ */

/* Access the CPU registers */
#define CPU_TYPE         m68ki_cpu.cpu_type

#define REG_DA           m68ki_cpu.dar /* easy access to data and address regs */
#define REG_DA_SAVE           m68ki_cpu.dar_save
#define REG_D            m68ki_cpu.dar
#define REG_A            (m68ki_cpu.dar+8)
#define REG_PPC 		 m68ki_cpu.ppc
#define REG_PC           m68ki_cpu.pc
#define REG_SP_BASE      m68ki_cpu.sp
#define REG_USP          m68ki_cpu.sp[0]
#define REG_ISP          m68ki_cpu.sp[4]
#define REG_MSP          m68ki_cpu.sp[6]
#define REG_SP           m68ki_cpu.dar[15]
#define REG_VBR          m68ki_cpu.vbr
#define REG_SFC          m68ki_cpu.sfc
#define REG_DFC          m68ki_cpu.dfc
#define REG_CACR         m68ki_cpu.cacr
#define REG_CAAR         m68ki_cpu.caar
#define REG_IR           m68ki_cpu.ir

#define REG_FP           m68ki_cpu.fpr
#define REG_FPCR         m68ki_cpu.fpcr
#define REG_FPSR         m68ki_cpu.fpsr
#define REG_FPIAR        m68ki_cpu.fpiar

#define FLAG_T1          m68ki_cpu.t1_flag
#define FLAG_T0          m68ki_cpu.t0_flag
#define FLAG_S           m68ki_cpu.s_flag
#define FLAG_M           m68ki_cpu.m_flag
#define FLAG_X           m68ki_cpu.x_flag
#define FLAG_N           m68ki_cpu.n_flag
#define FLAG_Z           m68ki_cpu.not_z_flag
#define FLAG_V           m68ki_cpu.v_flag
#define FLAG_C           m68ki_cpu.c_flag
#define FLAG_INT_MASK    m68ki_cpu.int_mask

#define CPU_INT_LEVEL    m68ki_cpu.int_level /* ASG: changed from CPU_INTS_PENDING */
#define CPU_STOPPED      m68ki_cpu.stopped
#define CPU_PREF_ADDR    m68ki_cpu.pref_addr
#define CPU_PREF_DATA    m68ki_cpu.pref_data
#define CPU_ADDRESS_MASK m68ki_cpu.address_mask
#define CPU_SR_MASK      m68ki_cpu.sr_mask
#define CPU_INSTR_MODE   m68ki_cpu.instr_mode
#define CPU_RUN_MODE     m68ki_cpu.run_mode

#define CYC_INSTRUCTION  m68ki_cpu.cyc_instruction
#define CYC_EXCEPTION    m68ki_cpu.cyc_exception
#define CYC_BCC_NOTAKE_B m68ki_cpu.cyc_bcc_notake_b
#define CYC_BCC_NOTAKE_W m68ki_cpu.cyc_bcc_notake_w
#define CYC_DBCC_F_NOEXP m68ki_cpu.cyc_dbcc_f_noexp
#define CYC_DBCC_F_EXP   m68ki_cpu.cyc_dbcc_f_exp
#define CYC_SCC_R_TRUE   m68ki_cpu.cyc_scc_r_true
#define CYC_MOVEM_W      m68ki_cpu.cyc_movem_w
#define CYC_MOVEM_L      m68ki_cpu.cyc_movem_l
#define CYC_SHIFT        m68ki_cpu.cyc_shift
#define CYC_RESET        m68ki_cpu.cyc_reset
#define HAS_PMMU	 m68ki_cpu.has_pmmu
#define PMMU_ENABLED	 m68ki_cpu.pmmu_enabled
#define RESET_CYCLES	 m68ki_cpu.reset_cycles

/* ----------------------------- Configuration ---------------------------- */

/* These defines are dependant on the configuration defines in m68kconf.h */

#define CPU_TYPE_IS_040_PLUS(A)    0
#define CPU_TYPE_IS_040_LESS(A)    1

#define CPU_TYPE_IS_030_PLUS(A)	0
#define CPU_TYPE_IS_030_LESS(A)    1

#define CPU_TYPE_IS_020_PLUS(A)    0
#define CPU_TYPE_IS_020_LESS(A)    1

#define CPU_TYPE_IS_EC020_PLUS(A)  0
#define CPU_TYPE_IS_EC020_LESS(A)  1

#define CPU_TYPE_IS_010(A)         0
#define CPU_TYPE_IS_010_PLUS(A)    0
#define CPU_TYPE_IS_010_LESS(A)    1

#define CPU_TYPE_IS_020_VARIANT(A) 0

#define CPU_TYPE_IS_000(A)         1


#define m68k_read_immediate_16(A) m68ki_read_program_16(A)
#define m68k_read_immediate_32(A) m68ki_read_program_32(A)

#define m68k_read_pcrelative_8(A) m68ki_read_program_8(A)
#define m68k_read_pcrelative_16(A) m68ki_read_program_16(A)
#define m68k_read_pcrelative_32(A) m68ki_read_program_32(A)

#define m68ki_read_8_fc(A, V) m68ki_read_8(A)
#define m68ki_read_16_fc(A, V) m68ki_read_16(A)
#define m68ki_read_32_fc(A, V) m68ki_read_32(A)
#define m68ki_write_8_fc(A, F, V) m68ki_write_8(A, V)
#define m68ki_write_16_fc(A, F, V) m68ki_write_16(A, V)
#define m68ki_write_32_fc(A, F, V) m68ki_write_32(A, V)

/* Enable or disable callback functions */
#if M68K_EMULATE_INT_ACK
	fail
#else
	/* Default action is to used autovector mode, which is most common */
	#define m68ki_int_ack(A) M68K_INT_ACK_AUTOVECTOR
#endif /* M68K_EMULATE_INT_ACK */

#if M68K_EMULATE_BKPT_ACK
	fail
#else
	#define m68ki_bkpt_ack(A)
#endif /* M68K_EMULATE_BKPT_ACK */

#if M68K_EMULATE_RESET
	#if M68K_EMULATE_RESET == M68K_OPT_SPECIFY_HANDLER
		#define m68ki_output_reset() M68K_RESET_CALLBACK()
	#else
	#define m68ki_output_reset() M68k_Reset_Callback(m_user)
	#endif
#else
	#define m68ki_output_reset()
#endif /* M68K_EMULATE_RESET */

#if M68K_CMPILD_HAS_CALLBACK
	fail
#else
	#define m68ki_cmpild_callback(v,r)
#endif /* M68K_CMPILD_HAS_CALLBACK */

#if M68K_RTE_HAS_CALLBACK
	fail
#else
	#define m68ki_rte_callback()
#endif /* M68K_RTE_HAS_CALLBACK */

#if M68K_TAS_HAS_CALLBACK
	_On_failure_
#else
	#define m68ki_tas_callback() 1
#endif /* M68K_TAS_HAS_CALLBACK */

#if M68K_ILLG_HAS_CALLBACK
	#define m68ki_illg_callback(opcode) M68k_Illegal_Callback(m_user, opcode)
#else
	#define m68ki_illg_callback(opcode) 0 // Default is 0 = not handled, exception will occur
#endif /* M68K_ILLG_HAS_CALLBACK */

#if M68K_TRAP_HAS_CALLBACK
	#define m68ki_trap_callback(n)  M68k_TrapN_Callback(m_user, n)
#else
	#define m68ki_trap_callback(opcode) 0 // Default is 0 = not handled, exception will occur
#endif /* M68K_TRAP_HAS_CALLBACK */

#if M68K_INSTRUCTION_HOOK
	fail
#else
	#define m68ki_instr_hook(pc)
#endif /* M68K_INSTRUCTION_HOOK */

#if M68K_MONITOR_PC
	fail
#else
	#define m68ki_pc_changed(A)
#endif /* M68K_MONITOR_PC */


/* Enable or disable function code emulation */
#if M68K_EMULATE_FC
	fail
#else
	#define m68ki_set_fc(A)
	#define m68ki_use_data_space()
	#define m68ki_use_program_space()
	#define m68ki_get_address_space() FUNCTION_CODE_USER_DATA
#endif /* M68K_EMULATE_FC */

/* Enable or disable trace emulation */
#if M68K_EMULATE_TRACE
	fail
#else
	#define m68ki_trace_t1()
	#define m68ki_trace_t0()
	#define m68ki_clear_trace()
	#define m68ki_exception_if_trace()
#endif /* M68K_EMULATE_TRACE */


/* Address error */
#if M68K_EMULATE_ADDRESS_ERROR
	fail
#else
	#define m68ki_set_address_error_trap()
	#define m68ki_check_address_error(ADDR, WRITE_MODE, FC)
	#define m68ki_check_address_error_010_less(ADDR, WRITE_MODE, FC)
#endif /* M68K_ADDRESS_ERROR */

/* Logging */
#if M68K_LOG_ENABLE
	#include <stdio.h>
	extern FILE* M68K_LOG_FILEHANDLE
	extern const char *const m68ki_cpu_names[];

	#define M68K_DO_LOG(A) if(M68K_LOG_FILEHANDLE) fprintf A
	#if M68K_LOG_1010_1111
		#define M68K_DO_LOG_EMU(A) if(M68K_LOG_FILEHANDLE) fprintf A
	#else
		#define M68K_DO_LOG_EMU(A)
	#endif
#else
	#define M68K_DO_LOG(A)
	#define M68K_DO_LOG_EMU(A)
#endif

/* -------------------------- EA / Operand Access ------------------------- */

/*
 * The general instruction format follows this pattern:
 * .... XXX. .... .YYY
 * where XXX is register X and YYY is register Y
 */
/* Data Register Isolation */
#define DX (REG_D[(REG_IR >> 9) & 7])
#define DY (REG_D[REG_IR & 7])
/* Address Register Isolation */
#define AX (REG_A[(REG_IR >> 9) & 7])
#define AY (REG_A[REG_IR & 7])


/* Effective Address Calculations */
#define EA_AY_AI_8()   AY                                    /* address register indirect */
#define EA_AY_AI_16()  EA_AY_AI_8()
#define EA_AY_AI_32()  EA_AY_AI_8()
#define EA_AY_PI_8()   (AY++)                                /* postincrement (size = byte) */
#define EA_AY_PI_16()  ((AY+=2)-2)                           /* postincrement (size = word) */
#define EA_AY_PI_32()  ((AY+=4)-4)                           /* postincrement (size = long) */
#define EA_AY_PD_8()   (--AY)                                /* predecrement (size = byte) */
#define EA_AY_PD_16()  (AY-=2)                               /* predecrement (size = word) */
#define EA_AY_PD_32()  (AY-=4)                               /* predecrement (size = long) */
#define EA_AY_DI_8()   (AY+MAKE_INT_16(m68ki_read_imm_16())) /* displacement */
#define EA_AY_DI_16()  EA_AY_DI_8()
#define EA_AY_DI_32()  EA_AY_DI_8()
#define EA_AY_IX_8()   m68ki_get_ea_ix(AY)                   /* indirect + index */
#define EA_AY_IX_16()  EA_AY_IX_8()
#define EA_AY_IX_32()  EA_AY_IX_8()

#define EA_AX_AI_8()   AX
#define EA_AX_AI_16()  EA_AX_AI_8()
#define EA_AX_AI_32()  EA_AX_AI_8()
#define EA_AX_PI_8()   (AX++)
#define EA_AX_PI_16()  ((AX+=2)-2)
#define EA_AX_PI_32()  ((AX+=4)-4)
#define EA_AX_PD_8()   (--AX)
#define EA_AX_PD_16()  (AX-=2)
#define EA_AX_PD_32()  (AX-=4)
#define EA_AX_DI_8()   (AX+MAKE_INT_16(m68ki_read_imm_16()))
#define EA_AX_DI_16()  EA_AX_DI_8()
#define EA_AX_DI_32()  EA_AX_DI_8()
#define EA_AX_IX_8()   m68ki_get_ea_ix(AX)
#define EA_AX_IX_16()  EA_AX_IX_8()
#define EA_AX_IX_32()  EA_AX_IX_8()

#define EA_A7_PI_8()   ((REG_A[7]+=2)-2)
#define EA_A7_PD_8()   (REG_A[7]-=2)

#define EA_AW_8()      MAKE_INT_16(m68ki_read_imm_16())      /* absolute word */
#define EA_AW_16()     EA_AW_8()
#define EA_AW_32()     EA_AW_8()
#define EA_AL_8()      m68ki_read_imm_32()                   /* absolute long */
#define EA_AL_16()     EA_AL_8()
#define EA_AL_32()     EA_AL_8()
#define EA_PCDI_8()    m68ki_get_ea_pcdi()                   /* pc indirect + displacement */
#define EA_PCDI_16()   EA_PCDI_8()
#define EA_PCDI_32()   EA_PCDI_8()
#define EA_PCIX_8()    m68ki_get_ea_pcix()                   /* pc indirect + index */
#define EA_PCIX_16()   EA_PCIX_8()
#define EA_PCIX_32()   EA_PCIX_8()


#define OPER_I_8()     m68ki_read_imm_8()
#define OPER_I_16()    m68ki_read_imm_16()
#define OPER_I_32()    m68ki_read_imm_32()



/* --------------------------- Status Register ---------------------------- */

/* Flag Calculation Macros */
#define CFLAG_8(A) (A)
#define CFLAG_16(A) ((A)>>8)

#if M68K_INT_GT_32_BIT
	#define CFLAG_ADD_32(S, D, R) ((R)>>24)
	#define CFLAG_SUB_32(S, D, R) ((R)>>24)
#else
	#define CFLAG_ADD_32(S, D, R) (((S & D) | (~R & (S | D)))>>23)
	#define CFLAG_SUB_32(S, D, R) (((S & R) | (~D & (S | R)))>>23)
#endif /* M68K_INT_GT_32_BIT */

#define VFLAG_ADD_8(S, D, R) ((S^R) & (D^R))
#define VFLAG_ADD_16(S, D, R) (((S^R) & (D^R))>>8)
#define VFLAG_ADD_32(S, D, R) (((S^R) & (D^R))>>24)

#define VFLAG_SUB_8(S, D, R) ((S^D) & (R^D))
#define VFLAG_SUB_16(S, D, R) (((S^D) & (R^D))>>8)
#define VFLAG_SUB_32(S, D, R) (((S^D) & (R^D))>>24)

#define NFLAG_8(A) (A)
#define NFLAG_16(A) ((A)>>8)
#define NFLAG_32(A) ((uint32)((A)>>24))
#define NFLAG_64(A) ((A)>>56)

#define ZFLAG_8(A) MASK_OUT_ABOVE_8(A)
#define ZFLAG_16(A) MASK_OUT_ABOVE_16(A)
#define ZFLAG_32(A) MASK_OUT_ABOVE_32(A)


/* Flag values */
#define NFLAG_SET   0x80
#define NFLAG_CLEAR 0
#define CFLAG_SET   0x100
#define CFLAG_CLEAR 0
#define XFLAG_SET   0x100
#define XFLAG_CLEAR 0
#define VFLAG_SET   0x80
#define VFLAG_CLEAR 0
#define ZFLAG_SET   0
#define ZFLAG_CLEAR 0xffffffff

#define SFLAG_SET   4
#define SFLAG_CLEAR 0
#define MFLAG_SET   2
#define MFLAG_CLEAR 0

/* Turn flag values into 1 or 0 */
#define XFLAG_AS_1() ((FLAG_X>>8)&1)
#define NFLAG_AS_1() ((FLAG_N>>7)&1)
#define VFLAG_AS_1() ((FLAG_V>>7)&1)
#define ZFLAG_AS_1() (!FLAG_Z)
#define CFLAG_AS_1() ((FLAG_C>>8)&1)


/* Conditions */
#define COND_CS() (FLAG_C&0x100)
#define COND_CC() (!COND_CS())
#define COND_VS() (FLAG_V&0x80)
#define COND_VC() (!COND_VS())
#define COND_NE() FLAG_Z
#define COND_EQ() (!COND_NE())
#define COND_MI() (FLAG_N&0x80)
#define COND_PL() (!COND_MI())
#define COND_LT() ((FLAG_N^FLAG_V)&0x80)
#define COND_GE() (!COND_LT())
#define COND_HI() (COND_CC() && COND_NE())
#define COND_LS() (COND_CS() || COND_EQ())
#define COND_GT() (COND_GE() && COND_NE())
#define COND_LE() (COND_LT() || COND_EQ())

/* Reversed conditions */
#define COND_NOT_CS() COND_CC()
#define COND_NOT_CC() COND_CS()
#define COND_NOT_VS() COND_VC()
#define COND_NOT_VC() COND_VS()
#define COND_NOT_NE() COND_EQ()
#define COND_NOT_EQ() COND_NE()
#define COND_NOT_MI() COND_PL()
#define COND_NOT_PL() COND_MI()
#define COND_NOT_LT() COND_GE()
#define COND_NOT_GE() COND_LT()
#define COND_NOT_HI() COND_LS()
#define COND_NOT_LS() COND_HI()
#define COND_NOT_GT() COND_LE()
#define COND_NOT_LE() COND_GT()

/* Not real conditions, but here for convenience */
#define COND_XS() (FLAG_X&0x100)
#define COND_XC() (!COND_XS)


/* Get the condition code register */
#define m68ki_get_ccr() ((COND_XS() >> 4) | \
						 (COND_MI() >> 4) | \
						 (COND_EQ() << 2) | \
						 (COND_VS() >> 6) | \
						 (COND_CS() >> 8))

/* Get the status register */
#define m68ki_get_sr() ( FLAG_T1              | \
						 FLAG_T0              | \
						(FLAG_S        << 11) | \
						(FLAG_M        << 11) | \
						 FLAG_INT_MASK        | \
						 m68ki_get_ccr())



/* ---------------------------- Cycle Counting ---------------------------- */

#define ADD_CYCLES(A)    m68ki_remaining_cycles += (A)
#define USE_CYCLES(A)    m68ki_remaining_cycles -= (A)
#define SET_CYCLES(A)    m68ki_remaining_cycles = A
#define GET_CYCLES()     m68ki_remaining_cycles
#define USE_ALL_CYCLES() m68ki_remaining_cycles %= CYC_INSTRUCTION[REG_IR]



/* ----------------------------- Read / Write ----------------------------- */

#define m68ki_read_8(A)  M68k_Read8(m_user, ADDRESS_68K(A))
#define m68ki_read_16(A) M68k_Read16(m_user, ADDRESS_68K(A))
#define m68ki_read_32(A) M68k_Read32(m_user, ADDRESS_68K(A))
#define m68ki_write_8(A, V)  M68k_Write8(m_user, ADDRESS_68K(A), V)
#define m68ki_write_16(A, V) M68k_Write16(m_user, ADDRESS_68K(A), V)
#define m68ki_write_32(A, V) M68k_Write32(m_user, ADDRESS_68K(A), V)

#if M68K_SIMULATE_PD_WRITES
fail
#define m68ki_write_32_pd(A, V) m68ki_write_32_pd_fc(A, FLAG_S | FUNCTION_CODE_USER_DATA, V)
#else
#define m68ki_write_32_pd(A, V) m68ki_write_32((A), (V))
#endif

extern unsigned int M68k_Read8(void* user, unsigned int address);
extern unsigned int M68k_Read16(void* user, unsigned int address);
extern unsigned int M68k_Read32(void* user, unsigned int address);
extern void M68k_Write8(void* user, unsigned int address, unsigned int value);
extern void M68k_Write16(void* user, unsigned int address, unsigned int value);
extern void M68k_Write32(void* user, unsigned int address, unsigned int value);

extern int M68k_Illegal_Callback(void* user, int);
extern void M68k_Reset_Callback(void* user);
extern int M68k_TrapN_Callback(void* user, int n);



/* Map PC-relative reads */
#define m68ki_read_pcrel_8(A) 	m68ki_read_8(A)
#define m68ki_read_pcrel_16(A) m68ki_read_16(A)
#define m68ki_read_pcrel_32(A) m68ki_read_32(A)

/* Read from the program space */
#define m68ki_read_program_8(A) 	m68ki_read_8(A)
#define m68ki_read_program_16(A) 	m68ki_read_16(A)
#define m68ki_read_program_32(A) 	m68ki_read_32(A)

/* Read from the data space */
#define m68ki_read_data_8(A) 	m68ki_read_8(A)
#define m68ki_read_data_16(A) 	m68ki_read_16(A)
#define m68ki_read_data_32(A) 	m68ki_read_32(A)


/* ======================================================================== */
/* =============================== PROTOTYPES ============================= */
/* ======================================================================== */

typedef struct
{
	uint cpu_type;     /* CPU Type: 68000, 68008, 68010, 68EC020, 68020, 68EC030, 68030, 68EC040, or 68040 */
	uint dar[16];      /* Data and Address Registers */
	uint dar_save[16];  /* Saved Data and Address Registers (pushed onto the
						   stack when a bus error occurs)*/
	uint ppc;		   /* Previous program counter */
	uint pc;           /* Program Counter */
	uint sp[7];        /* User, Interrupt, and Master Stack Pointers */
	uint vbr;          /* Vector Base Register (m68010+) */
	uint sfc;          /* Source Function Code Register (m68010+) */
	uint dfc;          /* Destination Function Code Register (m68010+) */
	uint cacr;         /* Cache Control Register (m68020, unemulated) */
	uint caar;         /* Cache Address Register (m68020, unemulated) */
	uint ir;           /* Instruction Register */
	uint fpiar;        /* FPU Instruction Address Register (m68040) */
	uint fpsr;         /* FPU Status Register (m68040) */
	uint fpcr;         /* FPU Control Register (m68040) */
	uint t1_flag;      /* Trace 1 */
	uint t0_flag;      /* Trace 0 */
	uint s_flag;       /* Supervisor */
	uint m_flag;       /* Master/Interrupt state */
	uint x_flag;       /* Extend */
	uint n_flag;       /* Negative */
	uint not_z_flag;   /* Zero, inverted for speedups */
	uint v_flag;       /* Overflow */
	uint c_flag;       /* Carry */
	uint int_mask;     /* I0-I2 */
	uint int_level;    /* State of interrupt pins IPL0-IPL2 -- ASG: changed from ints_pending */
	uint stopped;      /* Stopped state */
	uint pref_addr;    /* Last prefetch address */
	uint pref_data;    /* Data in the prefetch queue */
	uint address_mask; /* Available address pins */
	uint sr_mask;      /* Implemented status register bits */
	uint instr_mode;   /* Stores whether we are in instruction mode or group 0/1 exception mode */
	uint run_mode;     /* Stores whether we are processing a reset, bus error, address error, or something else */
	int    has_pmmu;     /* Indicates if a PMMU available (yes on 030, 040, no on EC030) */
	int    pmmu_enabled; /* Indicates if the PMMU is enabled */
	int    fpu_just_reset; /* Indicates the FPU was just reset */
	uint reset_cycles;

	/* Clocks required for instructions / exceptions */
	uint cyc_bcc_notake_b;
	uint cyc_bcc_notake_w;
	uint cyc_dbcc_f_noexp;
	uint cyc_dbcc_f_exp;
	uint cyc_scc_r_true;
	uint cyc_movem_w;
	uint cyc_movem_l;
	uint cyc_shift;
	uint cyc_reset;

	/* Virtual IRQ lines state */
	uint virq_state;
	uint nmi_pending;

	/* PMMU registers */
	uint mmu_crp_aptr, mmu_crp_limit;
	uint mmu_srp_aptr, mmu_srp_limit;
	uint mmu_tc;
	uint16 mmu_sr;

	const uint8* cyc_instruction;
	const uint8* cyc_exception;

} m68ki_cpu_core;

extern const uint8    m68ki_shift_8_table[];
extern const uint16   m68ki_shift_16_table[];
extern const uint     m68ki_shift_32_table[];
extern const uint8    m68ki_exception_cycle_table[][256];
extern const uint8    m68ki_ea_idx_cycle_table[];

/* ======================================================================== */
/* =========================== UTILITY FUNCTIONS ========================== */
/* ======================================================================== */

/* ---------------------------- Read Immediate ---------------------------- */


/* ------------------------- Top level read/write ------------------------- */

/* --------------------- Effective Address Calculation -------------------- */


/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */

#endif /* M68KCPU__HEADER */
