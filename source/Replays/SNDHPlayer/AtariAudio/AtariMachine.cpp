/*--------------------------------------------------------------------
	Atari Audio Library v1.10
	Small & accurate ATARI-ST audio emulation
	Arnaud Carré aka Leonard/Oxygene
	@leonard_coder
--------------------------------------------------------------------*/
#include <stdlib.h>		// malloc & free
#include <string.h>		// memset & memcpy
#include <assert.h>
#include "AtariMachine.h"

#define D_DUMP_READ		0
#define D_DUMP_WRITE	0
static const uint32_t D_DUMP_READ_AD1 = 0xfffa00;
static const uint32_t D_DUMP_READ_AD2 = 0xfffaff;
static const uint32_t D_DUMP_WRITE_AD1 = 0xfffa00;
static const uint32_t D_DUMP_WRITE_AD2 = 0xfffaff;
#if (D_DUMP_READ|D_DUMP_WRITE)
#include <stdio.h>
#endif

#define	D_ADDR_BUS_MASK(a)	((a)&0x00ffffff)

static const uint32_t ivector[5] = { 0x134,0x120,0x114,0x110,0x13c };

unsigned int M68k_Read8(void* user, unsigned int address)
{
	AtariMachine* mch = (AtariMachine*)user;
	return mch->memRead8(address);
}

unsigned int M68k_Read16(void* user, unsigned int address)
{
	AtariMachine* mch = (AtariMachine*)user;
	return mch->memRead16(address);
}

unsigned int M68k_Read32(void* user, unsigned int address)
{
	AtariMachine* mch = (AtariMachine*)user;
	uint32_t v = mch->memRead16(address);
	v = (v << 16) | mch->memRead16(address + 2);
	return v;
}

void M68k_Write8(void* user, unsigned int address, unsigned int value)
{
	AtariMachine* mch = (AtariMachine*)user;
	mch->memWrite8(address, value);
}

void M68k_Write16(void* user, unsigned int address, unsigned int value)
{
	AtariMachine* mch = (AtariMachine*)user;
	mch->memWrite16(address, value);
}

void M68k_Write32(void* user, unsigned int address, unsigned int value)
{
	AtariMachine* mch = (AtariMachine*)user;
	mch->memWrite16(address, value>>16);
	mch->memWrite16(address+2, value&0xffff);
}

void M68k_Reset_Callback(void* user)
{
	AtariMachine* mch = (AtariMachine*)user;
	mch->ResetCb();
}

int M68k_Illegal_Callback(void* user, int opcode)
{
	(void)user;
	(void)opcode;
	return 1;
}

int M68k_TrapN_Callback(void* user, int n)
{
	AtariMachine* mch = (AtariMachine*)user;
	mch->TrapInstructionCallback(n);
	return 1;
}

unsigned int  AtariMachine::memRead8(unsigned int address)
{
	assert(0 == (address & 0xff000000));
	uint8_t r = ~0;
	if (address < RAM_SIZE)
		return m_RAM[address];
	if ((address >= 0xff8800) && (address < 0xff8900))
		r = m_ym2149.ReadPort(address & 255);
	else if (0xff8260 == address)
		r = 0;		// simulate Atari ST low res
	else if (0xff820a == address)
		r = 2;		// simulate Atari ST PAL (50Hz)
	else if ((address >= 0xfffa00) && (address < 0xfffa26))
		r = m_mfp.Read8(address - 0xfffa00);
	else if ((address >= 0xff8900) && (address < 0xff8926))
		r = m_steDac.Read8(address - 0xff8900);
#if D_DUMP_READ
	if ((address >= D_DUMP_READ_AD1) && (address <= D_DUMP_READ_AD2))
	{
		uint32_t pc = m68k_get_reg(nullptr, M68K_REG_PC);
		printf("$%06x: move.b $%06x,d0 ( =#$%02x )\n", pc, address, r);
	}
#endif
	return r;
}

unsigned int  AtariMachine::memRead16(unsigned int address)
{
	assert(0 == (address & 0xff000000));
	uint16_t r = ~0;
	if (address < RAM_SIZE - 1)
		return uint16_t((m_RAM[address] << 8) | (m_RAM[address + 1]));
	if ((address >= 0xff8800) && (address < 0xff8900))
		r = m_ym2149.ReadPort(address & 0xfe) << 8;
	else if ((address >= 0xfffa00) && (address < 0xfffa26))
		r = m_mfp.Read16(address - 0xfffa00);
	else if ((address >= 0xff8900) && (address < 0xff8926))
		r = m_steDac.Read16(address - 0xff8900);
#if D_DUMP_READ
	if ((address >= D_DUMP_READ_AD1) && (address <= D_DUMP_READ_AD2))
	{
		uint32_t pc = m68k_get_reg(nullptr, M68K_REG_PC);
		printf("$%06x: move.w $%06x,d0 ( =#$%04x )\n", pc, address, r);
	}
#endif
	return r;
}

void	AtariMachine::ResetCb(void)
{
	m_exitCode |= AtariMachine::ExitCode::kReset;
	m_cpu.m68k_end_timeslice();
}

void AtariMachine::memWrite8(unsigned int address, unsigned int value)
{
	assert(0 == (address & 0xff000000));
	if (address < RAM_SIZE)
	{
		m_RAM[address] = value;
		return;
	}
#if D_DUMP_WRITE
	if ((address >= D_DUMP_WRITE_AD1) && (address <= D_DUMP_WRITE_AD2))
	{
		uint32_t pc = m68k_get_reg(nullptr, M68K_REG_PC);
		printf("$%06x: move.b #$%02x,$%06x\n", pc, value, address);
	}
#endif
	if ((address >= 0xff8800) && (address < 0xff8900))
		m_ym2149.WritePort(address & 0xfe, (uint8_t)value);	// atari ym 8800 is also shadowed in 8801 and 8802 in 8803
	else if ((address >= 0xfffa00) && (address < 0xfffa26))
		m_mfp.Write8(address - 0xfffa00, uint8_t(value));
	else if ((address >= 0xff8900) && (address < 0xff8926))
		m_steDac.Write8(address - 0xff8900, uint8_t(value));
}

void AtariMachine::memWrite16(unsigned int address, unsigned int value)
{
	assert(0 == (address & 0xff000000));
	if (address < RAM_SIZE - 1)
	{
		m_RAM[address] = uint8_t(value >> 8);
		m_RAM[address + 1] = uint8_t(value);
		return;
	}
#if D_DUMP_WRITE
	if ((address >= D_DUMP_WRITE_AD1) && (address <= D_DUMP_WRITE_AD2))
	{
		uint32_t pc = m68k_get_reg(nullptr, M68K_REG_PC);
		printf("$%06x: move.w #$%04x,$%06x\n", pc, value, address);
	}
#endif
	if ((address >= 0xff8800) && (address < 0xff8900))
		m_ym2149.WritePort(address & 0xfe, uint8_t(value >> 8));
	else if ((address >= 0xfffa00) && (address < 0xfffa26))
		m_mfp.Write16(address - 0xfffa00, uint16_t(value));
	else if ((address >= 0xff8900) && (address < 0xff8926))
		m_steDac.Write16(address - 0xff8900, uint16_t(value));
}

AtariMachine::AtariMachine()
{
	m_RAM = (uint8_t*)malloc(RAM_SIZE);
}

AtariMachine::~AtariMachine()
{
	if (m_RAM)
	{
		free(m_RAM);
		m_RAM = nullptr;
	}
}

void	AtariMachine::Gemdos(int func, uint32_t a7)
{
	switch (func)
	{
	case 0x48:			// MALLOC
	{
		// very basic incremental allocator (required by Maxymizer player)
		int size = m_cpu.MemRead32(a7 + 2);
		m_cpu.m68k_set_reg(M68K_REG_D0, m_nextGemdosMallocAd);
		m_nextGemdosMallocAd = (m_nextGemdosMallocAd + size + 1)&(-2);
		assert(m_nextGemdosMallocAd <= RAM_SIZE);
	}
	break;
	case 0x30:			// system version
	{
		m_cpu.m68k_set_reg(M68K_REG_D0, 0x0000);	// 0.15 : TOS 1.04 & 1.06
	}
	break;

	default:
		assert(false);	// unsupported GEMDOS function
		break;
	}
}

void	AtariMachine::XbiosTimerSet(int ctrlPort, int dataPort, int enablePort, int bit, int mask, int ctrlValue, int dataValue)
{
	const uint8_t back = m_mfp.Read8(ctrlPort)&mask;
	m_mfp.Write8(ctrlPort, back | 0x0);
	m_mfp.Write8(dataPort, dataValue);
	m_mfp.Write8(ctrlPort, back | ctrlValue);
	// seems Atari BIOS always enable the timer (even when switching it off)
	m_mfp.Write8(enablePort, m_mfp.Read8(enablePort) | (1 << bit));
	m_mfp.Write8(enablePort+12, m_mfp.Read8(enablePort+12) | (1 << bit));
}

void	AtariMachine::XBios(int func, uint32_t a7)
{
	switch (func)
	{
	case 31:
	{
		uint16_t timer = m_cpu.MemRead16(a7 + 2);
		uint16_t ctrlWord = m_cpu.MemRead16(a7 + 4);
		uint16_t dataWord = m_cpu.MemRead16(a7 + 6);
		uint32_t vector = m_cpu.MemRead32(a7 + 8);
		if (timer < 4)
		{
			m_cpu.MemWrite32(ivector[timer], vector);
			switch (timer)
			{
			case 0:		// A
				XbiosTimerSet(0x19, 0x1f, 0x07, 5, 0x00, ctrlWord, dataWord);
				break;
			case 1:		// B
				XbiosTimerSet(0x1b, 0x21, 0x07, 0, 0x00, ctrlWord, dataWord);
				break;
			case 2:		// C
				XbiosTimerSet(0x1d, 0x23, 0x09, 5, 0x0f, (ctrlWord&0xf)<<4, dataWord);
				break;
			case 3:		// D
				XbiosTimerSet(0x1d, 0x25, 0x09, 4, 0xf0, ctrlWord&0xf, dataWord);
				break;
			default:
				assert(false);			// unknown timer!
				break;
			}
		}
	}
	break;
		case 38:
		{
			// XBios(38) -> execute callback code in supervisor
			// we just simulate a "jsr callback"
			uint32_t callbackAddr = m_cpu.MemRead32(a7 + 2);

			// push PC on stack (so future RTS will get back right after the TRAP)
			uint32_t pc = m_cpu.m68k_get_reg(M68K_REG_PC);
			a7 -= 4;
			m_cpu.MemWrite32(a7, pc);
			m_cpu.m68k_set_reg(M68K_REG_SP, a7);
			m_cpu.m68k_set_reg(M68K_REG_PC, callbackAddr);
		}
		break;
	default:
		assert(false);	// unsupported XBIOS function
		break;
	}
}

void	AtariMachine::TrapInstructionCallback(int v)
{
	int a7 = m_cpu.m68k_get_reg(M68K_REG_SP);
	int func = m_cpu.MemRead16(a7);

	switch (v)
	{
	case 1:
		Gemdos(func, a7);
		break;
	case 14:
		XBios(func, a7);
		break;
	default:
		assert(false);		// unsupported TRAP #n
		break;
	}
}

void	AtariMachine::Startup(uint32_t hostReplayRate)
{
	assert(m_RAM);
	memset(m_RAM, 0, RAM_SIZE);

	m_ym2149.Reset(hostReplayRate);
	m_mfp.Reset(hostReplayRate);
	m_steDac.Reset(hostReplayRate);
	m_nextGemdosMallocAd = GEMDOS_MALLOC_EMUL_BUFFER;
	MuteVoices(0);		// nothing is muted by default

	memset(&m_cpu, 0, sizeof(m_cpu));// rePlayer
	m_cpu.SetUserData(this);
	m_cpu.m68k_set_cpu_type(M68K_CPU_TYPE_68000);

	// setup some cookie jar for MaxyMizer player!
	m_cpu.MemWrite32(0x900, 0x5f534e44);	// '_SND'
	m_cpu.MemWrite32(0x904, 0x3);		// soundchip+STE DMA
	m_cpu.MemWrite32(0x908, 0x5f4d4348);	// '_MCH'
	m_cpu.MemWrite32(0x90c, 0x00010000);	// STE
	m_cpu.MemWrite32(0x910, 0);			// end
	m_cpu.MemWrite32(0x5a0, 0x900);		// cookie jar start

	m_cpu.MemWrite16(RESET_INSTRUCTION_ADDR, 0x4e70);			// 4e70=reset instruction
	m_cpu.MemWrite16(RTE_INSTRUCTION_ADDR, 0x4e73);			// 4e73=rte

	// some SNDH setup MFP registers with values from OS, with timer C running!
	// so by default, set the timer C handler to RTE, just in case
	m_cpu.MemWrite32(0x114, RTE_INSTRUCTION_ADDR);

}

bool	AtariMachine::Upload(const void* src, uint32_t addr, uint32_t size)
{
	if (addr + size > RAM_SIZE)
		return false;

	if ((nullptr == src) || (0 == size))
		return false;

	memcpy(m_RAM + addr, src, size);
	return true;
}

void	AtariMachine::ConfigureReturnByRts()
{
	m_cpu.MemWrite32(RAM_SIZE - 4, RESET_INSTRUCTION_ADDR);		// next RTS will go to RESET_INSTRUCTION_ADDR (reset)
	m_cpu.MemWrite32(0, RAM_SIZE-4);				// stack ptr at next reset on TOP of RAM
}

void	AtariMachine::ConfigureReturnByRte()
{
	m_cpu.MemWrite32(RAM_SIZE - 4, RESET_INSTRUCTION_ADDR);		// next RTE will go to RESET_INSTRUCTION_ADDR (reset)
	m_cpu.MemWrite16(RAM_SIZE - 6, 0x2300);					// SR=2300
	m_cpu.MemWrite32(0, RAM_SIZE - 6);				// stack ptr at next reset on TOP of RAM
}

bool	AtariMachine::JmpBinary(uint32_t pc, int timeOut50Hz)
{
	m_cpu.MemWrite32(0x14, RTE_INSTRUCTION_ADDR);		// DIV by ZERO excep jump at $500

	m_cpu.MemWrite32(4, pc);			// pc at next RESET
	m_cpu.m68k_pulse_reset();						// reset CPU & start execution at PC

	m_exitCode = 0;
	int cycles = 0;
	for (int t = 0; t < timeOut50Hz; t++)
	{
		cycles += m_cpu.Execute(512 * 313);				// 50hz frame
		if (m_exitCode)
			break;
	}
	return (kReset == m_exitCode);
}

bool	AtariMachine::Jsr(uint32_t addr, uint32_t d0)
{
	bool ret = false;
	// upload data in RAM
	ConfigureReturnByRts();
	m_cpu.m68k_set_reg(M68K_REG_D0, d0);
	ret = JmpBinary(addr, 50*10);		// timeout of 1sec for init
	return ret;
}

#define	k15toS8(a)	((((a*127)>>15)+63)^0x80)	// signed 8bits value for oscillators viewing display per voice
static const uint32_t	s_ViewVolTab[16*2] =
{
	k15toS8(152),k15toS8(181),k15toS8(215),k15toS8(255),
	k15toS8(304),k15toS8(362),k15toS8(430),k15toS8(511),
	k15toS8(608),k15toS8(724),k15toS8(861),k15toS8(1023),
	k15toS8(1217),k15toS8(1448),k15toS8(1722),k15toS8(2047),
	k15toS8(2435),k15toS8(2896),k15toS8(3444),k15toS8(4095),
	k15toS8(4870),k15toS8(5792),k15toS8(6888),k15toS8(8191),
	k15toS8(9741),k15toS8(11584),k15toS8(13776),k15toS8(16383),
	k15toS8(19483),k15toS8(23169),k15toS8(27553),k15toS8(32767)
};

uint32_t AtariMachine::ComputeCurrentVisualLevels() const
{
	const uint32_t ymVisual = m_ym2149.GetCurrentVisualLevels();
	const unsigned int indexA = (ymVisual >> 0) & 31;
	const unsigned int indexB = (ymVisual >> 5) & 31;
	const unsigned int indexC = (ymVisual >> 10) & 31;
	uint32_t visualLevels = (s_ViewVolTab[indexA] << 0) | (s_ViewVolTab[indexB] << 8) | (s_ViewVolTab[indexC] << 16);
	if ( 0 == (m_muteMask&(1<<3)))
		visualLevels |= (m_steDac.GetCurrentVisualLevel()<<24);
	return visualLevels;
}

void AtariMachine::MuteVoices(uint32_t muteMask)
{
	m_ym2149.MuteVoices(muteMask);
	m_muteMask = muteMask;
}

int16_t	AtariMachine::ComputeNextSample()
{
	int32_t level = m_ym2149.ComputeNextSample().sMono;
	int32_t steLevel = m_steDac.ComputeNextSample((const int8_t*)m_RAM, RAM_SIZE, m_mfp);
	if ( 0 == (m_muteMask&(1<<3)))
		level += steLevel;

	if (level > 32767)
		level = 32767;
	else if (level < -32768)
		level = -32768;

	int16_t out = (int16_t)level;

	// tick 4 Atari timers, maybe one of them is running
	for (int t = 0; t < 4+1; t++)
	{
		if (m_mfp.Tick(t))
		{
			uint32_t pc = m_cpu.MemRead32(ivector[t]);
			ConfigureReturnByRte();
			m_ym2149.InsideTimerIrq(true);
			JmpBinary(pc, 1);	// execute the timer code until RTE (probably SID or any other special fx code)
			m_ym2149.InsideTimerIrq(false);
		}
	}
	return out;
}

void AtariMachine::ComputeNextSample(int16_t*& buffer)
{
	auto level = m_ym2149.ComputeNextSample();
	int16_t steLevel = m_steDac.ComputeNextSample((const int8_t*)m_RAM, RAM_SIZE, m_mfp);
	if ( 0 == (m_muteMask&(1<<3)))
		level.sRight += steLevel;

	if (level.sRight > 32767)
		level.sRight = 32767;
	else if (level.sRight < -32768)
		level.sRight = -32768;

	*buffer++ = level.sLeft;
	*buffer++ = level.sRight;

	// tick 4 Atari timers, maybe one of them is running
	for (int t = 0; t < 4 + 1; t++)
	{
		if (m_mfp.Tick(t))
		{
			uint32_t pc = m_cpu.MemRead32(ivector[t]);
			ConfigureReturnByRte();
			m_ym2149.InsideTimerIrq(true);
			JmpBinary(pc, 1);	// execute the timer code until RTE (probably SID or any other special fx code)
			m_ym2149.InsideTimerIrq(false);
		}
	}
}
