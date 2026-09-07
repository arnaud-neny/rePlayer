/*--------------------------------------------------------------------
	Atari Audio Library v1.09
	Small & accurate ATARI-ST audio emulation
	Arnaud Carré aka Leonard/Oxygene
	@leonard_coder
--------------------------------------------------------------------*/
#pragma once
#include <stdint.h>
#include "ym2149c.h"
#include "Mk68901.h"
#include "SteDac.h"


class AtariMachine
{
public:
	AtariMachine();
	~AtariMachine();

	enum ExitCode
	{
		kCrash = (1 << 0),
		kReset = (1 << 1),
	};

	void		Startup(uint32_t hostReplayRate);
	bool		Upload(const void* src, uint32_t addr, uint32_t size);
	bool		Jsr(uint32_t addr, uint32_t d0);
	int16_t		ComputeNextSample();
	void		ComputeNextSample(int16_t*& buffer);
	uint32_t	ComputeCurrentVisualLevels() const;

	unsigned int	memRead8(unsigned int address);
	unsigned int	memRead16(unsigned int address);
	void			memWrite8(unsigned int address, unsigned int value);
	void			memWrite16(unsigned int address, unsigned int value);
	void			TrapInstructionCallback(int v);
	void			ResetCb(void);
	void 			MuteVoices(uint32_t muteMask);

private:
	static	const	uint32_t	RAM_SIZE = 4*1024*1024;
	static	const	uint32_t	RTE_INSTRUCTION_ADDR = 0x500;
	static	const	uint32_t	RESET_INSTRUCTION_ADDR = 0x502;
	static	const	uint32_t	GEMDOS_MALLOC_EMUL_BUFFER = RAM_SIZE-0x100000;

	void		ConfigureReturnByRts();
	void		ConfigureReturnByRte();
	bool		JmpBinary(uint32_t pc, int timeOut50Hz);
	void		Gemdos(int func, uint32_t a7);
	void		XBios(int func, uint32_t a7);
	void		XbiosTimerSet(int ctrlPort, int dataPort, int enablePort, int bit, int mask, int ctrlValue, int dataValue);

	uint8_t*	m_RAM;
	int			m_exitCode;
	uint32_t	m_nextGemdosMallocAd;
	uint32_t 	m_muteMask;
	Ym2149c		m_ym2149;
	Mk68901		m_mfp;
	SteDac		m_steDac;
};
