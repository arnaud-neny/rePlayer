#include <assert.h>
#include <stdlib.h>
#include "m68kops.h"

/* Execute some instructions until we use up num_cycles clock cycles */
/* ASG: removed per-instruction interrupt checks */
int M68k::Execute(int num_cycles)
{
	/* eat up any reset cycles */
	if (RESET_CYCLES) {
	    int rc = RESET_CYCLES;
	    RESET_CYCLES = 0;
	    num_cycles -= rc;
	    if (num_cycles <= 0)
			return rc;
	}

	/* Set our pool of clock cycles available */
	SET_CYCLES(num_cycles);
	m68ki_initial_cycles = num_cycles;

	/* See if interrupts came in */
	m68ki_check_interrupts();

	/* Make sure we're not stopped */
	if(!CPU_STOPPED)
	{
		/* Return point if we had an address error */
//		m68ki_set_address_error_trap(); /* auto-disable (see m68kcpu.h) */

//		m68ki_check_bus_error_trap();

		/* Main loop.  Keep going until we run out of clock cycles */
		do
		{
			int i;
			/* Set tracing accodring to T1. (T0 is done inside instruction) */
			m68ki_trace_t1(); /* auto-disable (see m68kcpu.h) */

			/* Set the address space for reads */
			m68ki_use_data_space(); /* auto-disable (see m68kcpu.h) */

			/* Call external hook to peek at CPU */
			m68ki_instr_hook(REG_PC); /* auto-disable (see m68kcpu.h) */

			/* Record previous program counter */
			REG_PPC = REG_PC;

			/* Record previous D/A register state (in case of bus error) */
			for (i = 15; i >= 0; i--){
				REG_DA_SAVE[i] = REG_DA[i];
			}

			/* Read an instruction and call its handler */
			REG_IR = m68ki_read_imm_16();
			(this->*m68ki_instruction_jump_table[REG_IR])();
			USE_CYCLES(CYC_INSTRUCTION[REG_IR]);

			/* Trace m68k_exception, if necessary */
			m68ki_exception_if_trace(); /* auto-disable (see m68kcpu.h) */
		} while(GET_CYCLES() > 0);

		/* set previous PC to current PC for the next entry into the loop */
		REG_PPC = REG_PC;
	}
	else
		SET_CYCLES(0);

	/* return how many clocks we used */
	return m68ki_initial_cycles - GET_CYCLES();
}

/* Handles all immediate reads, does address error check, function code setting,
	* and prefetching if they are enabled in m68kconf.h
	*/
uint M68k::m68ki_read_imm_16(void)
{
	m68ki_set_fc(FLAG_S | FUNCTION_CODE_USER_PROGRAM); /* auto-disable (see m68kcpu.h) */
	m68ki_check_address_error(REG_PC, MODE_READ, FLAG_S | FUNCTION_CODE_USER_PROGRAM); /* auto-disable (see m68kcpu.h) */

	#if M68K_EMULATE_PREFETCH
	{
		uint result;
		if(REG_PC != CPU_PREF_ADDR)
		{
			CPU_PREF_ADDR = REG_PC;
			CPU_PREF_DATA = m68k_read_immediate_16(ADDRESS_68K(CPU_PREF_ADDR));
		}
		result = MASK_OUT_ABOVE_16(CPU_PREF_DATA);
		REG_PC += 2;
		CPU_PREF_ADDR = REG_PC;
		CPU_PREF_DATA = m68k_read_immediate_16(ADDRESS_68K(CPU_PREF_ADDR));
		return result;
	}
	#else
	REG_PC += 2;
	return m68k_read_immediate_16(ADDRESS_68K(REG_PC-2));
	#endif /* M68K_EMULATE_PREFETCH */
}

uint M68k::m68ki_read_imm_8(void)
{
	/* map read immediate 8 to read immediate 16 */
	return MASK_OUT_ABOVE_8(m68ki_read_imm_16());
}

uint M68k::m68ki_read_imm_32(void)
{

	#if M68K_EMULATE_PREFETCH
	uint temp_val;

	m68ki_set_fc(FLAG_S | FUNCTION_CODE_USER_PROGRAM); /* auto-disable (see m68kcpu.h) */
	m68ki_check_address_error(REG_PC, MODE_READ, FLAG_S | FUNCTION_CODE_USER_PROGRAM); /* auto-disable (see m68kcpu.h) */

	if(REG_PC != CPU_PREF_ADDR)
	{
		CPU_PREF_ADDR = REG_PC;
		CPU_PREF_DATA = m68k_read_immediate_16(ADDRESS_68K(CPU_PREF_ADDR));
	}
	temp_val = MASK_OUT_ABOVE_16(CPU_PREF_DATA);
	REG_PC += 2;
	CPU_PREF_ADDR = REG_PC;
	CPU_PREF_DATA = m68k_read_immediate_16(ADDRESS_68K(CPU_PREF_ADDR));

	temp_val = MASK_OUT_ABOVE_32((temp_val << 16) | MASK_OUT_ABOVE_16(CPU_PREF_DATA));
	REG_PC += 2;
	CPU_PREF_ADDR = REG_PC;
	CPU_PREF_DATA = m68k_read_immediate_16(ADDRESS_68K(CPU_PREF_ADDR));

	return temp_val;
	#else
	m68ki_set_fc(FLAG_S | FUNCTION_CODE_USER_PROGRAM); /* auto-disable (see m68kcpu.h) */
	m68ki_check_address_error(REG_PC, MODE_READ, FLAG_S | FUNCTION_CODE_USER_PROGRAM); /* auto-disable (see m68kcpu.h) */
	REG_PC += 4;
	return m68k_read_immediate_32(ADDRESS_68K(REG_PC-4));
	#endif /* M68K_EMULATE_PREFETCH */
}

uint M68k::m68ki_get_ea_pcdi(void)
{
	uint old_pc = REG_PC;
	m68ki_use_program_space(); /* auto-disable */
	return old_pc + MAKE_INT_16(m68ki_read_imm_16());
}


uint M68k::m68ki_get_ea_pcix(void)
{
	m68ki_use_program_space(); /* auto-disable */
	return m68ki_get_ea_ix(REG_PC);
}

/* Indexed addressing modes are encoded as follows:
	*
	* Base instruction format:
	* F E D C B A 9 8 7 6 | 5 4 3 | 2 1 0
	* x x x x x x x x x x | 1 1 0 | BASE REGISTER      (An)
	*
	* Base instruction format for destination EA in move instructions:
	* F E D C | B A 9    | 8 7 6 | 5 4 3 2 1 0
	* x x x x | BASE REG | 1 1 0 | X X X X X X       (An)
	*
	* Brief extension format:
	*  F  |  E D C   |  B  |  A 9  | 8 | 7 6 5 4 3 2 1 0
	* D/A | REGISTER | W/L | SCALE | 0 |  DISPLACEMENT
	*
	* Full extension format:
	*  F     E D C      B     A 9    8   7    6    5 4       3   2 1 0
	* D/A | REGISTER | W/L | SCALE | 1 | BS | IS | BD SIZE | 0 | I/IS
	* BASE DISPLACEMENT (0, 16, 32 bit)                (bd)
	* OUTER DISPLACEMENT (0, 16, 32 bit)               (od)
	*
	* D/A:     0 = Dn, 1 = An                          (Xn)
	* W/L:     0 = W (sign extend), 1 = L              (.SIZE)
	* SCALE:   00=1, 01=2, 10=4, 11=8                  (*SCALE)
	* BS:      0=add base reg, 1=suppress base reg     (An suppressed)
	* IS:      0=add index, 1=suppress index           (Xn suppressed)
	* BD SIZE: 00=reserved, 01=NULL, 10=Word, 11=Long  (size of bd)
	*
	* IS I/IS Operation
	* 0  000  No Memory Indirect
	* 0  001  indir prex with null outer
	* 0  010  indir prex with word outer
	* 0  011  indir prex with long outer
	* 0  100  reserved
	* 0  101  indir postx with null outer
	* 0  110  indir postx with word outer
	* 0  111  indir postx with long outer
	* 1  000  no memory indirect
	* 1  001  mem indir with null outer
	* 1  010  mem indir with word outer
	* 1  011  mem indir with long outer
	* 1  100-111  reserved
	*/
uint M68k::m68ki_get_ea_ix(uint An)
{
	/* An = base register */
	uint extension = m68ki_read_imm_16();
	uint Xn = 0;                        /* Index register */
	uint bd = 0;                        /* Base Displacement */
	uint od = 0;                        /* Outer Displacement */

	if(CPU_TYPE_IS_010_LESS(CPU_TYPE))
	{
		/* Calculate index */
		Xn = REG_DA[extension>>12];     /* Xn */
		if(!BIT_B(extension))           /* W/L */
			Xn = MAKE_INT_16(Xn);

		/* Add base register and displacement and return */
		return An + Xn + MAKE_INT_8(extension);
	}

	/* Brief extension format */
	if(!BIT_8(extension))
	{
		/* Calculate index */
		Xn = REG_DA[extension>>12];     /* Xn */
		if(!BIT_B(extension))           /* W/L */
			Xn = MAKE_INT_16(Xn);
		/* Add scale if proper CPU type */
		if(CPU_TYPE_IS_EC020_PLUS(CPU_TYPE))
			Xn <<= (extension>>9) & 3;  /* SCALE */

		/* Add base register and displacement and return */
		return An + Xn + MAKE_INT_8(extension);
	}

	/* Full extension format */

	USE_CYCLES(m68ki_ea_idx_cycle_table[extension&0x3f]);

	/* Check if base register is present */
	if(BIT_7(extension))                /* BS */
		An = 0;                         /* An */

	/* Check if index is present */
	if(!BIT_6(extension))               /* IS */
	{
		Xn = REG_DA[extension>>12];     /* Xn */
		if(!BIT_B(extension))           /* W/L */
			Xn = MAKE_INT_16(Xn);
		Xn <<= (extension>>9) & 3;      /* SCALE */
	}

	/* Check if base displacement is present */
	if(BIT_5(extension))                /* BD SIZE */
		bd = BIT_4(extension) ? m68ki_read_imm_32() : (uint32)MAKE_INT_16(m68ki_read_imm_16());

	/* If no indirect action, we are done */
	if(!(extension&7))                  /* No Memory Indirect */
		return An + bd + Xn;

	/* Check if outer displacement is present */
	if(BIT_1(extension))                /* I/IS:  od */
		od = BIT_0(extension) ? m68ki_read_imm_32() : (uint32)MAKE_INT_16(m68ki_read_imm_16());

	/* Postindex */
	if(BIT_2(extension))                /* I/IS:  0 = preindex, 1 = postindex */
		return m68ki_read_32(An + bd) + Xn + od;

	/* Preindex */
	return m68ki_read_32(An + bd + Xn) + od;
}

/* ---------------------------- Stack Functions --------------------------- */

/* Push/pull data from the stack */
void M68k::m68ki_push_16(uint value)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP - 2);
	m68ki_write_16(REG_SP, value);
}

void M68k::m68ki_push_32(uint value)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP - 4);
	m68ki_write_32(REG_SP, value);
}

uint M68k::m68ki_pull_16(void)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP + 2);
	return m68ki_read_16(REG_SP-2);
}

uint M68k::m68ki_pull_32(void)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP + 4);
	return m68ki_read_32(REG_SP-4);
}


/* Increment/decrement the stack as if doing a push/pull but
	* don't do any memory access.
	*/
void M68k::m68ki_fake_push_16(void)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP - 2);
}

void M68k::m68ki_fake_push_32(void)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP - 4);
}

void M68k::m68ki_fake_pull_16(void)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP + 2);
}

void M68k::m68ki_fake_pull_32(void)
{
	REG_SP = MASK_OUT_ABOVE_32(REG_SP + 4);
}

/* ----------------------------- Program Flow ----------------------------- */

/* Jump to a new program location or vector.
	* These functions will also call the pc_changed callback if it was enabled
	* in m68kconf.h.
	*/
void M68k::m68ki_jump(uint new_pc)
{
	REG_PC = new_pc;
	m68ki_pc_changed(REG_PC);
}

void M68k::m68ki_jump_vector(uint vector)
{
	REG_PC = (vector<<2) + REG_VBR;
	REG_PC = m68ki_read_data_32(REG_PC);
	m68ki_pc_changed(REG_PC);
}


/* Branch to a new memory location.
	* The 32-bit branch will call pc_changed if it was enabled in m68kconf.h.
	* So far I've found no problems with not calling pc_changed for 8 or 16
	* bit branches.
	*/
void M68k::m68ki_branch_8(uint offset)
{
	REG_PC += MAKE_INT_8(offset);
}

void M68k::m68ki_branch_16(uint offset)
{
	REG_PC += MAKE_INT_16(offset);
}

void M68k::m68ki_branch_32(uint offset)
{
	REG_PC += offset;
	m68ki_pc_changed(REG_PC);
}

/* ---------------------------- Status Register --------------------------- */

/* Set the S flag and change the active stack pointer.
	* Note that value MUST be 4 or 0.
	*/
void M68k::m68ki_set_s_flag(uint value)
{
	/* Backup the old stack pointer */
	REG_SP_BASE[FLAG_S | ((FLAG_S>>1) & FLAG_M)] = REG_SP;
	/* Set the S flag */
	FLAG_S = value;
	/* Set the new stack pointer */
	REG_SP = REG_SP_BASE[FLAG_S | ((FLAG_S>>1) & FLAG_M)];
}

/* Set the S and M flags and change the active stack pointer.
	* Note that value MUST be 0, 2, 4, or 6 (bit2 = S, bit1 = M).
	*/
void M68k::m68ki_set_sm_flag(uint value)
{
	/* Backup the old stack pointer */
	REG_SP_BASE[FLAG_S | ((FLAG_S>>1) & FLAG_M)] = REG_SP;
	/* Set the S and M flags */
	FLAG_S = value & SFLAG_SET;
	FLAG_M = value & MFLAG_SET;
	/* Set the new stack pointer */
	REG_SP = REG_SP_BASE[FLAG_S | ((FLAG_S>>1) & FLAG_M)];
}

/* Set the S and M flags.  Don't touch the stack pointer. */
void M68k::m68ki_set_sm_flag_nosp(uint value)
{
	/* Set the S and M flags */
	FLAG_S = value & SFLAG_SET;
	FLAG_M = value & MFLAG_SET;
}


/* Set the condition code register */
void M68k::m68ki_set_ccr(uint value)
{
	FLAG_X = BIT_4(value)  << 4;
	FLAG_N = BIT_3(value)  << 4;
	FLAG_Z = !BIT_2(value);
	FLAG_V = BIT_1(value)  << 6;
	FLAG_C = BIT_0(value)  << 8;
}

/* Set the status register but don't check for interrupts */
void M68k::m68ki_set_sr_noint(uint value)
{
	/* Mask out the "unimplemented" bits */
	value &= CPU_SR_MASK;

	/* Now set the status register */
	FLAG_T1 = BIT_F(value);
	FLAG_T0 = BIT_E(value);
	FLAG_INT_MASK = value & 0x0700;
	m68ki_set_ccr(value);
	m68ki_set_sm_flag((value >> 11) & 6);
}

/* Set the status register but don't check for interrupts nor
	* change the stack pointer
	*/
void M68k::m68ki_set_sr_noint_nosp(uint value)
{
	/* Mask out the "unimplemented" bits */
	value &= CPU_SR_MASK;

	/* Now set the status register */
	FLAG_T1 = BIT_F(value);
	FLAG_T0 = BIT_E(value);
	FLAG_INT_MASK = value & 0x0700;
	m68ki_set_ccr(value);
	m68ki_set_sm_flag_nosp((value >> 11) & 6);
}

/* Set the status register and check for interrupts */
void M68k::m68ki_set_sr(uint value)
{
	m68ki_set_sr_noint(value);
	m68ki_check_interrupts();
}

/* ------------------------- Exception Processing ------------------------- */

/* Initiate exception processing */
uint M68k::m68ki_init_exception(void)
{
	/* Save the old status register */
	uint sr = m68ki_get_sr();

	/* Turn off trace flag, clear pending traces */
	FLAG_T1 = FLAG_T0 = 0;
	m68ki_clear_trace();
	/* Enter supervisor mode */
	m68ki_set_s_flag(SFLAG_SET);

	return sr;
}

/* 3 word stack frame (68000 only) */
void M68k::m68ki_stack_frame_3word(uint pc, uint sr)
{
	m68ki_push_32(pc);
	m68ki_push_16(sr);
}

/* Format 0 stack frame.
	* This is the standard stack frame for 68010+.
	*/
void M68k::m68ki_stack_frame_0000(uint pc, uint sr, uint vector)
{
	(void)vector;
	m68ki_stack_frame_3word(pc, sr);
}

/* Bus error stack frame (68000 only).
	*/
void M68k::m68ki_stack_frame_buserr(uint sr)
{
	m68ki_push_32(REG_PC);
	m68ki_push_16(sr);
	m68ki_push_16(REG_IR);
	m68ki_push_32(m68ki_aerr_address);	/* access address */
	/* 0 0 0 0 0 0 0 0 0 0 0 R/W I/N FC
		* R/W  0 = write, 1 = read
		* I/N  0 = instruction, 1 = not
		* FC   3-bit function code
		*/
	m68ki_push_16(m68ki_aerr_write_mode | CPU_INSTR_MODE | m68ki_aerr_fc);
}

/* Used for Group 2 exceptions.
	* These stack a type 2 frame on the 020.
	*/
void M68k::m68ki_exception_trap(uint vector)
{
	uint sr = m68ki_init_exception();

	m68ki_stack_frame_0000(REG_PC, sr, vector);

	m68ki_jump_vector(vector);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[vector] - CYC_INSTRUCTION[REG_IR]);
}

/* Trap#n stacks a 0 frame but behaves like group2 otherwise */
void M68k::m68ki_exception_trapN(uint vector)
{
	uint t = REG_IR & 0xf;
	#if M68K_LOG_TRAP == M68K_OPT_ON
	M68K_DO_LOG((M68K_LOG_FILEHANDLE "%s at %08x: trap %01x (%s)\n",
				m68ki_cpu_names[CPU_TYPE], ADDRESS_68K(REG_PPC), t,
				m68ki_disassemble_quick(ADDRESS_68K(REG_PPC))));
	#endif
	if (m68ki_trap_callback(t))
	    return;

	uint sr = m68ki_init_exception();
	m68ki_stack_frame_0000(REG_PC, sr, vector);
	m68ki_jump_vector(vector);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[vector] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for trace mode */
void M68k::m68ki_exception_trace(void)
{
	uint sr = m68ki_init_exception();

	#if M68K_EMULATE_ADDRESS_ERROR == M68K_OPT_ON
	if(CPU_TYPE_IS_000(CPU_TYPE))
	{
		CPU_INSTR_MODE = INSTRUCTION_NO;
	}
	#endif /* M68K_EMULATE_ADDRESS_ERROR */
	m68ki_stack_frame_0000(REG_PC, sr, EXCEPTION_TRACE);

	m68ki_jump_vector(EXCEPTION_TRACE);

	/* Trace nullifies a STOP instruction */
	CPU_STOPPED &= ~STOP_LEVEL_STOP;

	/* Use up some clock cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_TRACE]);
}

/* Exception for privilege violation */
void M68k::m68ki_exception_privilege_violation(void)
{
	uint sr = m68ki_init_exception();

	#if M68K_EMULATE_ADDRESS_ERROR == M68K_OPT_ON
	if(CPU_TYPE_IS_000(CPU_TYPE))
	{
		CPU_INSTR_MODE = INSTRUCTION_NO;
	}
	#endif /* M68K_EMULATE_ADDRESS_ERROR */

	m68ki_stack_frame_0000(REG_PPC, sr, EXCEPTION_PRIVILEGE_VIOLATION);
	m68ki_jump_vector(EXCEPTION_PRIVILEGE_VIOLATION);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_PRIVILEGE_VIOLATION] - CYC_INSTRUCTION[REG_IR]);
}

//extern jmp_buf m68ki_bus_error_jmp_buf;
//#define m68ki_check_bus_error_trap() setjmp(m68ki_bus_error_jmp_buf)

/* Exception for bus error */
void M68k::m68ki_exception_bus_error(void)
{
#if 1
	assert(false);
#else
	int i;

	/* If we were processing a bus error, address error, or reset,
		* while writing the stack frame, this is a catastrophic failure.
		* Halt the CPU
		*/
	if(CPU_RUN_MODE == RUN_MODE_BERR_AERR_RESET_WSF)
	{
		m68k_read_memory_8(0x00ffff01);
		assert(false);
		CPU_STOPPED = STOP_LEVEL_HALT;
		return;
	}
	CPU_RUN_MODE = RUN_MODE_BERR_AERR_RESET_WSF;

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_BUS_ERROR] - CYC_INSTRUCTION[REG_IR]);

	for (i = 15; i >= 0; i--){
		REG_DA[i] = REG_DA_SAVE[i];
	}

	uint sr = m68ki_init_exception();

	m68ki_jump_vector(EXCEPTION_BUS_ERROR);

	CPU_RUN_MODE = RUN_MODE_BERR_AERR_RESET;

	longjmp(m68ki_bus_error_jmp_buf, 1);
#endif
}

/* Exception for A-Line instructions */
void M68k::m68ki_exception_1010(void)
{
	uint sr;
	#if M68K_LOG_1010_1111 == M68K_OPT_ON
	M68K_DO_LOG_EMU((M68K_LOG_FILEHANDLE "%s at %08x: called 1010 instruction %04x (%s)\n",
					m68ki_cpu_names[CPU_TYPE], ADDRESS_68K(REG_PPC), REG_IR,
					m68ki_disassemble_quick(ADDRESS_68K(REG_PPC))));
	#endif

	sr = m68ki_init_exception();
	m68ki_stack_frame_0000(REG_PPC, sr, EXCEPTION_1010);
	m68ki_jump_vector(EXCEPTION_1010);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_1010] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for F-Line instructions */
void M68k::m68ki_exception_1111(void)
{
	uint sr;

	#if M68K_LOG_1010_1111 == M68K_OPT_ON
	M68K_DO_LOG_EMU((M68K_LOG_FILEHANDLE "%s at %08x: called 1111 instruction %04x (%s)\n",
					m68ki_cpu_names[CPU_TYPE], ADDRESS_68K(REG_PPC), REG_IR,
					m68ki_disassemble_quick(ADDRESS_68K(REG_PPC))));
	#endif

	sr = m68ki_init_exception();
	m68ki_stack_frame_0000(REG_PPC, sr, EXCEPTION_1111);
	m68ki_jump_vector(EXCEPTION_1111);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_1111] - CYC_INSTRUCTION[REG_IR]);
}

#if M68K_ILLG_HAS_CALLBACK == M68K_OPT_SPECIFY_HANDLER
extern int m68ki_illg_callback(int);
#endif

#if M68K_TRAP_HAS_CALLBACK == M68K_OPT_SPECIFY_HANDLER
extern int m68ki_trap_callback(int);
#endif

/* Exception for illegal instructions */
void M68k::m68ki_exception_illegal(void)
{
	uint sr;

	M68K_DO_LOG((M68K_LOG_FILEHANDLE "%s at %08x: illegal instruction %04x (%s)\n",
				m68ki_cpu_names[CPU_TYPE], ADDRESS_68K(REG_PPC), REG_IR,
				m68ki_disassemble_quick(ADDRESS_68K(REG_PPC))));
	if (m68ki_illg_callback(REG_IR))
	    return;

	sr = m68ki_init_exception();

	#if M68K_EMULATE_ADDRESS_ERROR == M68K_OPT_ON
	if(CPU_TYPE_IS_000(CPU_TYPE))
	{
		CPU_INSTR_MODE = INSTRUCTION_NO;
	}
	#endif /* M68K_EMULATE_ADDRESS_ERROR */

	m68ki_stack_frame_0000(REG_PPC, sr, M68K_EXCEPTION_ILLEGAL_INSTRUCTION);
	m68ki_jump_vector(M68K_EXCEPTION_ILLEGAL_INSTRUCTION);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[M68K_EXCEPTION_ILLEGAL_INSTRUCTION] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for format errror in RTE */
void M68k::m68ki_exception_format_error(void)
{
	uint sr = m68ki_init_exception();
	m68ki_stack_frame_0000(REG_PC, sr, EXCEPTION_FORMAT_ERROR);
	m68ki_jump_vector(EXCEPTION_FORMAT_ERROR);

	/* Use up some clock cycles and undo the instruction's cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_FORMAT_ERROR] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for address error */
void M68k::m68ki_exception_address_error(void)
{
	uint sr = m68ki_init_exception();

	/* If we were processing a bus error, address error, or reset,
		* while writing the stack frame, this is a catastrophic failure.
		* Halt the CPU
		*/
	if(CPU_RUN_MODE == RUN_MODE_BERR_AERR_RESET_WSF)
	{
//		m68k_read_memory_8(0x00ffff01);
		CPU_STOPPED = STOP_LEVEL_HALT;
		return;
	}
	CPU_RUN_MODE = RUN_MODE_BERR_AERR_RESET_WSF;

	/* Note: This is implemented for 68000 only! */
	m68ki_stack_frame_buserr(sr);

	m68ki_jump_vector(EXCEPTION_ADDRESS_ERROR);

	CPU_RUN_MODE = RUN_MODE_BERR_AERR_RESET;

	/* Use up some clock cycles. Note that we don't need to undo the
		instruction's cycles here as we've longjmp:ed directly from the
		instruction handler without passing the part of the excecute loop
		that deducts instruction cycles */
	USE_CYCLES(CYC_EXCEPTION[EXCEPTION_ADDRESS_ERROR]);
}


/* Service an interrupt request and start exception processing */
void M68k::m68ki_exception_interrupt(uint int_level)
{
	uint vector;
	uint sr;
	uint new_pc;

	#if M68K_EMULATE_ADDRESS_ERROR == M68K_OPT_ON
	if(CPU_TYPE_IS_000(CPU_TYPE))
	{
		CPU_INSTR_MODE = INSTRUCTION_NO;
	}
	#endif /* M68K_EMULATE_ADDRESS_ERROR */

	/* Turn off the stopped state */
	CPU_STOPPED &= ~STOP_LEVEL_STOP;

	/* If we are halted, don't do anything */
	if(CPU_STOPPED)
		return;

	/* Acknowledge the interrupt */
	vector = m68ki_int_ack(int_level);

	/* Get the interrupt vector */
	if(vector == M68K_INT_ACK_AUTOVECTOR)
		/* Use the autovectors.  This is the most commonly used implementation */
		vector = EXCEPTION_INTERRUPT_AUTOVECTOR+int_level;
	else if(vector == M68K_INT_ACK_SPURIOUS)
		/* Called if no devices respond to the interrupt acknowledge */
		vector = EXCEPTION_SPURIOUS_INTERRUPT;
	else if(vector > 255)
	{
		M68K_DO_LOG_EMU((M68K_LOG_FILEHANDLE "%s at %08x: Interrupt acknowledge returned invalid vector $%x\n",
						m68ki_cpu_names[CPU_TYPE], ADDRESS_68K(REG_PC), vector));
		return;
	}

	/* Start exception processing */
	sr = m68ki_init_exception();

	/* Set the interrupt mask to the level of the one being serviced */
	FLAG_INT_MASK = int_level<<8;

	/* Get the new PC */
	new_pc = m68ki_read_data_32((vector<<2) + REG_VBR);

	/* If vector is uninitialized, call the uninitialized interrupt vector */
	if(new_pc == 0)
		new_pc = m68ki_read_data_32((EXCEPTION_UNINITIALIZED_INTERRUPT<<2) + REG_VBR);

	/* Generate a stack frame */
	m68ki_stack_frame_0000(REG_PC, sr, vector);

	m68ki_jump(new_pc);

	/* Defer cycle counting until later */
	USE_CYCLES(CYC_EXCEPTION[vector]);

	#if !M68K_EMULATE_INT_ACK
	/* Automatically clear IRQ if we are not using an acknowledge scheme */
	CPU_INT_LEVEL = 0;
	#endif /* M68K_EMULATE_INT_ACK */
}


/* ASG: Check for interrupts */
void M68k::m68ki_check_interrupts(void)
{
	if(m68ki_cpu.nmi_pending)
	{
		m68ki_cpu.nmi_pending = FALSE;
		m68ki_exception_interrupt(7);
	}
	else if(CPU_INT_LEVEL > FLAG_INT_MASK)
		m68ki_exception_interrupt(CPU_INT_LEVEL>>8);
}

m68ki_bitfield_t M68k::m68ki_make_bf(uint32 lo, uint8 hi, unsigned offset)
{
	m68ki_bitfield_t ret;
	ret.field = (lo << offset) | (hi >> (8 - offset));
	ret.lo = lo;
	ret.hi = hi;
	return ret;
}

m68ki_bitfield_t M68k::m68ki_load_bitfield(uint32 addr, unsigned offset, unsigned width)
{
	assert(offset < 8);

	/* Figure out how many bytes we need to load */
	unsigned bcount = (offset + width + 7) / 8;
	assert(bcount <= 5 && bcount > 0);

	if (bcount == 1) {
		uint8 lo = m68ki_read_8(addr);
		return m68ki_make_bf(((uint32)lo) << 24, 0, offset);
	}

	if (bcount < 4) {
		uint16 lo1 = m68ki_read_16(addr);

		if (bcount == 2)
			return m68ki_make_bf(((uint32)lo1) << 16, 0, offset);

		uint8 lo2 = m68ki_read_8(addr + 2);
		return m68ki_make_bf((((uint32)lo1) << 16) | (((uint32)lo2) << 8), 0, offset);
	}

	/* bcount = 4 or 5 */
	uint32 lo = m68ki_read_32(addr);

	if (bcount == 4)
		return m68ki_make_bf(lo, 0, offset);

	uint8 hi = m68ki_read_8(addr + 4);
	return m68ki_make_bf(lo, hi, offset);
}


/** val << shift but works when shift >= 32 */
uint32 M68k::lshift32_safe(uint32 val, unsigned shift)
{
	return shift < 32 ? (val << shift) : 0;
}

unsigned M68k::m68ki_bitfield_patch_offset(int32 offset)
{
	return ((uint32)offset) % 8;
}

uint32 M68k::m68ki_bitfield_patch_ea(uint32 ea, int32 offset)
{
	return ea + (offset >= 0 ? offset / 8 : -((7 - offset) / 8));
}

void M68k::m68ki_store_bitfield(uint32 addr, unsigned offset, unsigned width,uint32 res, m68ki_bitfield_t* bf)
{

	assert(offset < 8);

	/* Figure out how many bytes we need to store */
	unsigned bcount = (offset + width + 7) / 8;
	assert(bcount <= 5 && bcount > 0);

	/* Masks the bits in *field_lo which are outside the bitfield, thus we need to preserve them. */
	uint32 lomask = lshift32_safe(0xFFFFFFFF, 32 - offset);
	uint8 himask = (0xFF >> offset) & 0xFF;

	/* Rebuild lo & hi */
	uint32 lo = (bf->lo & lomask) | (res >> offset);
	uint8 hi = (bf->hi & himask) | ((res << (8 - offset) & 0xFF));

	if (bcount == 1) {
		m68ki_write_8(addr, (lo >> 24) & 0xFF);
		return;
	}

	if (bcount < 4) {
		m68ki_write_16(addr, (lo >> 16) & 0xFFFF);
		if (bcount == 2)
			return;

		m68ki_write_8(addr + 2, (lo >> 8) & 0xFF);
		return;
	}

	/* bcount = 4 or 5 */
	m68ki_write_32(addr, lo);

	if (bcount == 4)
		return;

	m68ki_write_8(addr + 4, hi);
}

/* Access the internals of the CPU */
unsigned int M68k::m68k_get_reg(m68k_register_t regnum)
{
	m68ki_cpu_core* cpu = &m68ki_cpu;
	switch(regnum)
	{
		case M68K_REG_D0:	return cpu->dar[0];
		case M68K_REG_D1:	return cpu->dar[1];
		case M68K_REG_D2:	return cpu->dar[2];
		case M68K_REG_D3:	return cpu->dar[3];
		case M68K_REG_D4:	return cpu->dar[4];
		case M68K_REG_D5:	return cpu->dar[5];
		case M68K_REG_D6:	return cpu->dar[6];
		case M68K_REG_D7:	return cpu->dar[7];
		case M68K_REG_A0:	return cpu->dar[8];
		case M68K_REG_A1:	return cpu->dar[9];
		case M68K_REG_A2:	return cpu->dar[10];
		case M68K_REG_A3:	return cpu->dar[11];
		case M68K_REG_A4:	return cpu->dar[12];
		case M68K_REG_A5:	return cpu->dar[13];
		case M68K_REG_A6:	return cpu->dar[14];
		case M68K_REG_A7:	return cpu->dar[15];
		case M68K_REG_PC:	return MASK_OUT_ABOVE_32(cpu->pc);
		case M68K_REG_SR:	return	cpu->t1_flag						|
			cpu->t0_flag						|
			(cpu->s_flag << 11)					|
			(cpu->m_flag << 11)					|
			cpu->int_mask						|
			((cpu->x_flag & XFLAG_SET) >> 4)	|
			((cpu->n_flag & NFLAG_SET) >> 4)	|
			((!cpu->not_z_flag) << 2)			|
			((cpu->v_flag & VFLAG_SET) >> 6)	|
			((cpu->c_flag & CFLAG_SET) >> 8);
		case M68K_REG_SP:	return cpu->dar[15];
		case M68K_REG_USP:	return cpu->s_flag ? cpu->sp[0] : cpu->dar[15];
		case M68K_REG_ISP:	return cpu->s_flag && !cpu->m_flag ? cpu->dar[15] : cpu->sp[4];
		case M68K_REG_MSP:	return cpu->s_flag && cpu->m_flag ? cpu->dar[15] : cpu->sp[6];
		case M68K_REG_SFC:	return cpu->sfc;
		case M68K_REG_DFC:	return cpu->dfc;
		case M68K_REG_VBR:	return cpu->vbr;
		case M68K_REG_CACR:	return cpu->cacr;
		case M68K_REG_CAAR:	return cpu->caar;
		case M68K_REG_PREF_ADDR:	return cpu->pref_addr;
		case M68K_REG_PREF_DATA:	return cpu->pref_data;
		case M68K_REG_PPC:	return MASK_OUT_ABOVE_32(cpu->ppc);
		case M68K_REG_IR:	return cpu->ir;
		case M68K_REG_CPU_TYPE:
			switch(cpu->cpu_type)
		{
			case CPU_TYPE_000:		return (unsigned int)M68K_CPU_TYPE_68000;
			case CPU_TYPE_010:		return (unsigned int)M68K_CPU_TYPE_68010;
			case CPU_TYPE_EC020:	return (unsigned int)M68K_CPU_TYPE_68EC020;
			case CPU_TYPE_020:		return (unsigned int)M68K_CPU_TYPE_68020;
			case CPU_TYPE_040:		return (unsigned int)M68K_CPU_TYPE_68040;
		}
		return M68K_CPU_TYPE_INVALID;
		default:			return 0;
	}
	return 0;
}

void M68k::m68k_set_reg(m68k_register_t regnum, unsigned int value)
{
	switch(regnum)
	{
		case M68K_REG_D0:	REG_D[0] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_D1:	REG_D[1] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_D2:	REG_D[2] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_D3:	REG_D[3] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_D4:	REG_D[4] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_D5:	REG_D[5] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_D6:	REG_D[6] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_D7:	REG_D[7] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_A0:	REG_A[0] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_A1:	REG_A[1] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_A2:	REG_A[2] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_A3:	REG_A[3] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_A4:	REG_A[4] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_A5:	REG_A[5] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_A6:	REG_A[6] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_A7:	REG_A[7] = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_PC:	m68ki_jump(MASK_OUT_ABOVE_32(value)); return;
		case M68K_REG_SR:	m68ki_set_sr_noint_nosp(value); return;
		case M68K_REG_SP:	REG_SP = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_USP:	if(FLAG_S)
			REG_USP = MASK_OUT_ABOVE_32(value);
			else
				REG_SP = MASK_OUT_ABOVE_32(value);
			return;
		case M68K_REG_ISP:	if(FLAG_S && !FLAG_M)
			REG_SP = MASK_OUT_ABOVE_32(value);
			else
				REG_ISP = MASK_OUT_ABOVE_32(value);
			return;
		case M68K_REG_MSP:	if(FLAG_S && FLAG_M)
			REG_SP = MASK_OUT_ABOVE_32(value);
			else
				REG_MSP = MASK_OUT_ABOVE_32(value);
			return;
		case M68K_REG_VBR:	REG_VBR = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_SFC:	REG_SFC = value & 7; return;
		case M68K_REG_DFC:	REG_DFC = value & 7; return;
		case M68K_REG_CACR:	REG_CACR = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_CAAR:	REG_CAAR = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_PPC:	REG_PPC = MASK_OUT_ABOVE_32(value); return;
		case M68K_REG_IR:	REG_IR = MASK_OUT_ABOVE_16(value); return;
		case M68K_REG_CPU_TYPE: m68k_set_cpu_type(value); return;
		default:			return;
	}
}

/* Set the CPU type. */
void M68k::m68k_set_cpu_type(unsigned int cpu_type)
{
	switch(cpu_type)
	{
		case M68K_CPU_TYPE_68000:
			CPU_TYPE         = CPU_TYPE_000;
			CPU_ADDRESS_MASK = 0x00ffffff;
			CPU_SR_MASK      = 0xa71f; /* T1 -- S  -- -- I2 I1 I0 -- -- -- X  N  Z  V  C  */
			CYC_INSTRUCTION  = m68ki_cycles[0];
			CYC_EXCEPTION    = m68ki_exception_cycle_table[0];
			CYC_BCC_NOTAKE_B = -2;
			CYC_BCC_NOTAKE_W = 2;
			CYC_DBCC_F_NOEXP = -2;
			CYC_DBCC_F_EXP   = 2;
			CYC_SCC_R_TRUE   = 2;
			CYC_MOVEM_W      = 2;
			CYC_MOVEM_L      = 3;
			CYC_SHIFT        = 1;
			CYC_RESET        = 132;
			HAS_PMMU	 = 0;
			return;
		case M68K_CPU_TYPE_SCC68070:
			m68k_set_cpu_type(M68K_CPU_TYPE_68010);
			CPU_ADDRESS_MASK = 0xffffffff;
			CPU_TYPE         = CPU_TYPE_SCC070;
			return;
		case M68K_CPU_TYPE_68010:
			CPU_TYPE         = CPU_TYPE_010;
			CPU_ADDRESS_MASK = 0x00ffffff;
			CPU_SR_MASK      = 0xa71f; /* T1 -- S  -- -- I2 I1 I0 -- -- -- X  N  Z  V  C  */
			CYC_INSTRUCTION  = m68ki_cycles[1];
			CYC_EXCEPTION    = m68ki_exception_cycle_table[1];
			CYC_BCC_NOTAKE_B = -4;
			CYC_BCC_NOTAKE_W = 0;
			CYC_DBCC_F_NOEXP = 0;
			CYC_DBCC_F_EXP   = 6;
			CYC_SCC_R_TRUE   = 0;
			CYC_MOVEM_W      = 2;
			CYC_MOVEM_L      = 3;
			CYC_SHIFT        = 1;
			CYC_RESET        = 130;
			HAS_PMMU	 = 0;
			return;
		case M68K_CPU_TYPE_68EC020:
			CPU_TYPE         = CPU_TYPE_EC020;
			CPU_ADDRESS_MASK = 0x00ffffff;
			CPU_SR_MASK      = 0xf71f; /* T1 T0 S  M  -- I2 I1 I0 -- -- -- X  N  Z  V  C  */
			CYC_INSTRUCTION  = m68ki_cycles[2];
			CYC_EXCEPTION    = m68ki_exception_cycle_table[2];
			CYC_BCC_NOTAKE_B = -2;
			CYC_BCC_NOTAKE_W = 0;
			CYC_DBCC_F_NOEXP = 0;
			CYC_DBCC_F_EXP   = 4;
			CYC_SCC_R_TRUE   = 0;
			CYC_MOVEM_W      = 2;
			CYC_MOVEM_L      = 2;
			CYC_SHIFT        = 0;
			CYC_RESET        = 518;
			HAS_PMMU	 = 0;
			return;
		case M68K_CPU_TYPE_68020:
			CPU_TYPE         = CPU_TYPE_020;
			CPU_ADDRESS_MASK = 0xffffffff;
			CPU_SR_MASK      = 0xf71f; /* T1 T0 S  M  -- I2 I1 I0 -- -- -- X  N  Z  V  C  */
			CYC_INSTRUCTION  = m68ki_cycles[2];
			CYC_EXCEPTION    = m68ki_exception_cycle_table[2];
			CYC_BCC_NOTAKE_B = -2;
			CYC_BCC_NOTAKE_W = 0;
			CYC_DBCC_F_NOEXP = 0;
			CYC_DBCC_F_EXP   = 4;
			CYC_SCC_R_TRUE   = 0;
			CYC_MOVEM_W      = 2;
			CYC_MOVEM_L      = 2;
			CYC_SHIFT        = 0;
			CYC_RESET        = 518;
			HAS_PMMU	 = 0;
			return;
		case M68K_CPU_TYPE_68030:
			CPU_TYPE         = CPU_TYPE_030;
			CPU_ADDRESS_MASK = 0xffffffff;
			CPU_SR_MASK      = 0xf71f; /* T1 T0 S  M  -- I2 I1 I0 -- -- -- X  N  Z  V  C  */
			CYC_INSTRUCTION  = m68ki_cycles[3];
			CYC_EXCEPTION    = m68ki_exception_cycle_table[3];
			CYC_BCC_NOTAKE_B = -2;
			CYC_BCC_NOTAKE_W = 0;
			CYC_DBCC_F_NOEXP = 0;
			CYC_DBCC_F_EXP   = 4;
			CYC_SCC_R_TRUE   = 0;
			CYC_MOVEM_W      = 2;
			CYC_MOVEM_L      = 2;
			CYC_SHIFT        = 0;
			CYC_RESET        = 518;
			HAS_PMMU	       = 1;
			return;
		case M68K_CPU_TYPE_68EC030:
			CPU_TYPE         = CPU_TYPE_EC030;
			CPU_ADDRESS_MASK = 0xffffffff;
			CPU_SR_MASK          = 0xf71f; /* T1 T0 S  M  -- I2 I1 I0 -- -- -- X  N  Z  V  C  */
			CYC_INSTRUCTION  = m68ki_cycles[3];
			CYC_EXCEPTION    = m68ki_exception_cycle_table[3];
			CYC_BCC_NOTAKE_B = -2;
			CYC_BCC_NOTAKE_W = 0;
			CYC_DBCC_F_NOEXP = 0;
			CYC_DBCC_F_EXP   = 4;
			CYC_SCC_R_TRUE   = 0;
			CYC_MOVEM_W      = 2;
			CYC_MOVEM_L      = 2;
			CYC_SHIFT        = 0;
			CYC_RESET        = 518;
			HAS_PMMU	       = 0;		/* EC030 lacks the PMMU and is effectively a die-shrink 68020 */
			return;
		case M68K_CPU_TYPE_68040:		// TODO: these values are not correct
			CPU_TYPE         = CPU_TYPE_040;
			CPU_ADDRESS_MASK = 0xffffffff;
			CPU_SR_MASK      = 0xf71f; /* T1 T0 S  M  -- I2 I1 I0 -- -- -- X  N  Z  V  C  */
			CYC_INSTRUCTION  = m68ki_cycles[4];
			CYC_EXCEPTION    = m68ki_exception_cycle_table[4];
			CYC_BCC_NOTAKE_B = -2;
			CYC_BCC_NOTAKE_W = 0;
			CYC_DBCC_F_NOEXP = 0;
			CYC_DBCC_F_EXP   = 4;
			CYC_SCC_R_TRUE   = 0;
			CYC_MOVEM_W      = 2;
			CYC_MOVEM_L      = 2;
			CYC_SHIFT        = 0;
			CYC_RESET        = 518;
			HAS_PMMU	 = 1;
			return;
		case M68K_CPU_TYPE_68EC040: // Just a 68040 without pmmu apparently...
			CPU_TYPE         = CPU_TYPE_EC040;
			CPU_ADDRESS_MASK = 0xffffffff;
			CPU_SR_MASK      = 0xf71f; /* T1 T0 S  M  -- I2 I1 I0 -- -- -- X  N  Z  V  C  */
			CYC_INSTRUCTION  = m68ki_cycles[4];
			CYC_EXCEPTION    = m68ki_exception_cycle_table[4];
			CYC_BCC_NOTAKE_B = -2;
			CYC_BCC_NOTAKE_W = 0;
			CYC_DBCC_F_NOEXP = 0;
			CYC_DBCC_F_EXP   = 4;
			CYC_SCC_R_TRUE   = 0;
			CYC_MOVEM_W      = 2;
			CYC_MOVEM_L      = 2;
			CYC_SHIFT        = 0;
			CYC_RESET        = 518;
			HAS_PMMU	 = 0;
			return;
		case M68K_CPU_TYPE_68LC040:
			CPU_TYPE         = CPU_TYPE_LC040;
			m68ki_cpu.sr_mask          = 0xf71f; /* T1 T0 S  M  -- I2 I1 I0 -- -- -- X  N  Z  V  C  */
			m68ki_cpu.cyc_instruction  = m68ki_cycles[4];
			m68ki_cpu.cyc_exception    = m68ki_exception_cycle_table[4];
			m68ki_cpu.cyc_bcc_notake_b = -2;
			m68ki_cpu.cyc_bcc_notake_w = 0;
			m68ki_cpu.cyc_dbcc_f_noexp = 0;
			m68ki_cpu.cyc_dbcc_f_exp   = 4;
			m68ki_cpu.cyc_scc_r_true   = 0;
			m68ki_cpu.cyc_movem_w      = 2;
			m68ki_cpu.cyc_movem_l      = 2;
			m68ki_cpu.cyc_shift        = 0;
			m68ki_cpu.cyc_reset        = 518;
			HAS_PMMU	       = 1;
			return;
	}
}

int M68k::m68k_cycles_run(void)
{
	return m68ki_initial_cycles - GET_CYCLES();
}

int M68k::m68k_cycles_remaining(void)
{
	return GET_CYCLES();
}

/* Change the timeslice */
void M68k::m68k_modify_timeslice(int cycles)
{
	m68ki_initial_cycles += cycles;
	ADD_CYCLES(cycles);
}


void M68k::m68k_end_timeslice(void)
{
	m68ki_initial_cycles -= GET_CYCLES();
	SET_CYCLES(0);
}


/* ASG: rewrote so that the int_level is a mask of the IPL0/IPL1/IPL2 bits */
/* KS: Modified so that IPL* bits match with mask positions in the SR
*     and cleaned out remenants of the interrupt controller.
*/
void M68k::m68k_set_irq(unsigned int int_level)
{
	uint old_level = CPU_INT_LEVEL;
	CPU_INT_LEVEL = int_level << 8;

	/* A transition from < 7 to 7 always interrupts (NMI) */
	/* Note: Level 7 can also level trigger like a normal IRQ */
	if(old_level != 0x0700 && CPU_INT_LEVEL == 0x0700)
		m68ki_cpu.nmi_pending = TRUE;
}

void M68k::m68k_set_virq(unsigned int level, unsigned int active)
{
	uint state = m68ki_cpu.virq_state;
	uint blevel;

	if(active)
		state |= 1 << level;
	else
		state &= ~(1 << level);
	m68ki_cpu.virq_state = state;

	for(blevel = 7; blevel > 0; blevel--)
		if(state & (1 << blevel))
			break;
	m68k_set_irq(blevel);
}

unsigned int M68k::m68k_get_virq(unsigned int level)
{
	return (m68ki_cpu.virq_state & (1 << level)) ? 1 : 0;
}

class AutoTableBuilder
{
public:
	AutoTableBuilder()
	{
		// we build the jmp table at exe init time
		m68ki_build_opcode_table();
	}
};
static AutoTableBuilder sAutoTableBuilder;

/* Trigger a Bus Error exception */
void M68k::m68k_pulse_bus_error(void)
{
	m68ki_exception_bus_error();
}

/* Pulse the RESET line on the CPU */
void M68k::m68k_pulse_reset(void)
{
	/* Disable the PMMU on reset */
	m68ki_cpu.pmmu_enabled = 0;

	/* Clear all stop levels and eat up all remaining cycles */
	CPU_STOPPED = 0;
	SET_CYCLES(0);

	CPU_RUN_MODE = RUN_MODE_BERR_AERR_RESET;
	CPU_INSTR_MODE = INSTRUCTION_YES;

	/* Turn off tracing */
	FLAG_T1 = FLAG_T0 = 0;
	m68ki_clear_trace();
	/* Interrupt mask to level 7 */
	FLAG_INT_MASK = 0x0700;
	CPU_INT_LEVEL = 0;
	m68ki_cpu.virq_state = 0;
	/* Reset VBR */
	REG_VBR = 0;
	/* Go to supervisor mode */
	m68ki_set_sm_flag(SFLAG_SET | MFLAG_CLEAR);

	/* Invalidate the prefetch queue */
	#if M68K_EMULATE_PREFETCH
	/* Set to arbitrary number since our first fetch is from 0 */
	CPU_PREF_ADDR = 0x1000;
	#endif /* M68K_EMULATE_PREFETCH */

	/* Read the initial stack pointer and program counter */
	m68ki_jump(0);
	REG_SP = m68ki_read_imm_32();
	REG_PC = m68ki_read_imm_32();
	m68ki_jump(REG_PC);

	CPU_RUN_MODE = RUN_MODE_NORMAL;

	RESET_CYCLES = CYC_EXCEPTION[EXCEPTION_RESET];
}

/* Pulse the HALT line on the CPU */
void M68k::m68k_pulse_halt(void)
{
	CPU_STOPPED |= STOP_LEVEL_HALT;
}
