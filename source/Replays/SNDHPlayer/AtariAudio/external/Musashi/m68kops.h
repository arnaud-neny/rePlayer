#ifndef M68KOPS__HEADER
#define M68KOPS__HEADER

/* ======================================================================== */
/* ============================ OPCODE HANDLERS =========================== */
/* ======================================================================== */


#include "m68kcpu.h"

/* Build the opcode handler table */
void m68ki_build_opcode_table(void);

extern unsigned char m68ki_cycles[][0x10000];

/* Helper to load a bitfield from EA */
typedef struct {
	uint32 field;

	/** When dealing with bitfields to/from memory,
		* we need to support offsets not aligned to a byte boundary.
		* Since the maximum width of a bitfield is 32, that means accessing at most
		* 5 bytes.
		* When writing back the result, we need to leave the bits outside the accessed
		* bitfield alone. To do that, we track the 5 bytes we read here
		*/
	uint32 lo;
	uint8 hi;

} m68ki_bitfield_t;

class M68k
{
public:

	void SetUserData(void* data) { m_user = data; }
	int Execute(int cycles);

	uint8 MemRead8(uint32 ad) { return m68ki_read_8(ad); }
	uint16 MemRead16(uint32 ad) { return m68ki_read_16(ad); }
	uint32 MemRead32(uint32 ad) { return m68ki_read_32(ad); }
	void MemWrite8(uint32 ad, uint8 v) { m68ki_write_8(ad, v); }
	void MemWrite16(uint32 ad, uint16 v) { m68ki_write_16(ad, v); }
	void MemWrite32(uint32 ad, uint32 v) { m68ki_write_32(ad, v); }

	m68ki_cpu_core m68ki_cpu;
	int  m68ki_initial_cycles;
	int  m68ki_remaining_cycles = 0;                     /* Number of clocks remaining */
	uint m68ki_tracing = 0;
	uint m68ki_address_space;

	uint    m68ki_aerr_address;
	uint    m68ki_aerr_write_mode;
	uint    m68ki_aerr_fc;

private:
	void*	m_user = nullptr;

public:
	uint m68ki_read_imm_8(void);
	uint m68ki_read_imm_16(void);
	uint m68ki_read_imm_32(void);
	uint m68ki_get_ea_pcdi(void);
	uint m68ki_get_ea_pcix(void);
	uint m68ki_get_ea_ix(uint An);

	/* Fetch operands */
	uint OPER_AY_AI_8(void)  {uint ea = EA_AY_AI_8();  return m68ki_read_8(ea); }
	uint OPER_AY_AI_16(void) {uint ea = EA_AY_AI_16(); return m68ki_read_16(ea);}
	uint OPER_AY_AI_32(void) {uint ea = EA_AY_AI_32(); return m68ki_read_32(ea);}
	uint OPER_AY_PI_8(void)  {uint ea = EA_AY_PI_8();  return m68ki_read_8(ea); }
	uint OPER_AY_PI_16(void) {uint ea = EA_AY_PI_16(); return m68ki_read_16(ea);}
	uint OPER_AY_PI_32(void) {uint ea = EA_AY_PI_32(); return m68ki_read_32(ea);}
	uint OPER_AY_PD_8(void)  {uint ea = EA_AY_PD_8();  return m68ki_read_8(ea); }
	uint OPER_AY_PD_16(void) {uint ea = EA_AY_PD_16(); return m68ki_read_16(ea);}
	uint OPER_AY_PD_32(void) {uint ea = EA_AY_PD_32(); return m68ki_read_32(ea);}
	uint OPER_AY_DI_8(void)  {uint ea = EA_AY_DI_8();  return m68ki_read_8(ea); }
	uint OPER_AY_DI_16(void) {uint ea = EA_AY_DI_16(); return m68ki_read_16(ea);}
	uint OPER_AY_DI_32(void) {uint ea = EA_AY_DI_32(); return m68ki_read_32(ea);}
	uint OPER_AY_IX_8(void)  {uint ea = EA_AY_IX_8();  return m68ki_read_8(ea); }
	uint OPER_AY_IX_16(void) {uint ea = EA_AY_IX_16(); return m68ki_read_16(ea);}
	uint OPER_AY_IX_32(void) {uint ea = EA_AY_IX_32(); return m68ki_read_32(ea);}
		 
	uint OPER_AX_AI_8(void)  {uint ea = EA_AX_AI_8();  return m68ki_read_8(ea); }
	uint OPER_AX_AI_16(void) {uint ea = EA_AX_AI_16(); return m68ki_read_16(ea);}
	uint OPER_AX_AI_32(void) {uint ea = EA_AX_AI_32(); return m68ki_read_32(ea);}
	uint OPER_AX_PI_8(void)  {uint ea = EA_AX_PI_8();  return m68ki_read_8(ea); }
	uint OPER_AX_PI_16(void) {uint ea = EA_AX_PI_16(); return m68ki_read_16(ea);}
	uint OPER_AX_PI_32(void) {uint ea = EA_AX_PI_32(); return m68ki_read_32(ea);}
	uint OPER_AX_PD_8(void)  {uint ea = EA_AX_PD_8();  return m68ki_read_8(ea); }
	uint OPER_AX_PD_16(void) {uint ea = EA_AX_PD_16(); return m68ki_read_16(ea);}
	uint OPER_AX_PD_32(void) {uint ea = EA_AX_PD_32(); return m68ki_read_32(ea);}
	uint OPER_AX_DI_8(void)  {uint ea = EA_AX_DI_8();  return m68ki_read_8(ea); }
	uint OPER_AX_DI_16(void) {uint ea = EA_AX_DI_16(); return m68ki_read_16(ea);}
	uint OPER_AX_DI_32(void) {uint ea = EA_AX_DI_32(); return m68ki_read_32(ea);}
	uint OPER_AX_IX_8(void)  {uint ea = EA_AX_IX_8();  return m68ki_read_8(ea); }
	uint OPER_AX_IX_16(void) {uint ea = EA_AX_IX_16(); return m68ki_read_16(ea);}
	uint OPER_AX_IX_32(void) {uint ea = EA_AX_IX_32(); return m68ki_read_32(ea);}
		 
	uint OPER_A7_PI_8(void)  {uint ea = EA_A7_PI_8();  return m68ki_read_8(ea); }
	uint OPER_A7_PD_8(void)  {uint ea = EA_A7_PD_8();  return m68ki_read_8(ea); }
		 
	uint OPER_AW_8(void)     {uint ea = EA_AW_8();     return m68ki_read_8(ea); }
	uint OPER_AW_16(void)    {uint ea = EA_AW_16();    return m68ki_read_16(ea);}
	uint OPER_AW_32(void)    {uint ea = EA_AW_32();    return m68ki_read_32(ea);}
	uint OPER_AL_8(void)     {uint ea = EA_AL_8();     return m68ki_read_8(ea); }
	uint OPER_AL_16(void)    {uint ea = EA_AL_16();    return m68ki_read_16(ea);}
	uint OPER_AL_32(void)    {uint ea = EA_AL_32();    return m68ki_read_32(ea);}
	uint OPER_PCDI_8(void)   {uint ea = EA_PCDI_8();   return m68ki_read_pcrel_8(ea); }
	uint OPER_PCDI_16(void)  {uint ea = EA_PCDI_16();  return m68ki_read_pcrel_16(ea);}
	uint OPER_PCDI_32(void)  {uint ea = EA_PCDI_32();  return m68ki_read_pcrel_32(ea);}
	uint OPER_PCIX_8(void)   {uint ea = EA_PCIX_8();   return m68ki_read_pcrel_8(ea); }
	uint OPER_PCIX_16(void)  {uint ea = EA_PCIX_16();  return m68ki_read_pcrel_16(ea);}
	uint OPER_PCIX_32(void)  {uint ea = EA_PCIX_32();  return m68ki_read_pcrel_32(ea);}

	void m68ki_push_16(uint value);
	void m68ki_push_32(uint value);
	uint m68ki_pull_16(void);
	uint m68ki_pull_32(void);
	void m68ki_fake_push_16(void);
	void m68ki_fake_push_32(void);
	void m68ki_fake_pull_16(void);
	void m68ki_fake_pull_32(void);

	void m68ki_jump(uint new_pc);
	void m68ki_jump_vector(uint vector);

	void m68ki_branch_8(uint offset);
	void m68ki_branch_16(uint offset);
	void m68ki_branch_32(uint offset);
	void m68ki_set_s_flag(uint value);
	void m68ki_set_sm_flag(uint value);
	void m68ki_set_sm_flag_nosp(uint value);
	void m68ki_set_ccr(uint value);
	void m68ki_set_sr_noint(uint value);
	void m68ki_set_sr_noint_nosp(uint value);
	void m68ki_set_sr(uint value);

	uint m68ki_init_exception(void);
	void m68ki_stack_frame_3word(uint pc, uint sr);
	void m68ki_stack_frame_0000(uint pc, uint sr, uint vector);
	void m68ki_stack_frame_buserr(uint sr);

	/* Used for Group 2 exceptions.
	* These stack a type 2 frame on the 020.
	*/
	void m68ki_exception_trap(uint vector);
	void m68ki_exception_trapN(uint vector);
	void m68ki_exception_trace(void);
	void m68ki_exception_privilege_violation(void);
	void m68ki_exception_bus_error(void);
	void m68ki_exception_1010(void);
	void m68ki_exception_1111(void);
	void m68ki_exception_illegal(void);
	void m68ki_exception_format_error(void);
	void m68ki_exception_address_error(void);
	void m68ki_exception_interrupt(uint int_level);
	void m68ki_check_interrupts(void);

	unsigned int m68k_get_reg(m68k_register_t regnum);
	void m68k_set_reg(m68k_register_t regnum, unsigned int value);
	void m68k_set_cpu_type(unsigned int cpu_type);

	m68ki_bitfield_t m68ki_make_bf(uint32 lo, uint8 hi, unsigned offset);
	m68ki_bitfield_t m68ki_load_bitfield(uint32 addr, unsigned offset, unsigned width);
	uint32 lshift32_safe(uint32 val, unsigned shift);
	unsigned m68ki_bitfield_patch_offset(int32 offset);
	uint32 m68ki_bitfield_patch_ea(uint32 ea, int32 offset);
	void m68ki_store_bitfield(uint32 addr, unsigned offset, unsigned width, uint32 res, m68ki_bitfield_t* bf);

	int m68k_cycles_run(void);
	int m68k_cycles_remaining(void);
	void m68k_modify_timeslice(int cycles);
	void m68k_end_timeslice(void);
	void m68k_set_irq(unsigned int int_level);
	void m68k_set_virq(unsigned int level, unsigned int active);
	unsigned int m68k_get_virq(unsigned int level);
	void m68k_pulse_bus_error(void);
	void m68k_pulse_reset(void);
	void m68k_pulse_halt(void);

	#include "m68k_func.inc"
};

extern void (M68k::*m68ki_instruction_jump_table[0x10000])(void); /* opcode handler jump table */

/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */

#endif /* M68KOPS__HEADER */


