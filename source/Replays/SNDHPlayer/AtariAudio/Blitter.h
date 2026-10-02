//----------------------------------------------------------
//
//	AtariAudio 1.26
//	Small & accurate ATARI-ST audio emulation
//	by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//
//----------------------------------------------------------
#pragma once
#include <stdint.h>

class AtariMachine;

class Blitter
{
public:

	void		Reset();
	void		Run(AtariMachine& machine);

	uint8_t		Read8(int port);
	uint16_t	Read16(int port);
	void		Write8(int port, uint8_t data, AtariMachine& machine);		// Write a 8bits value to blitter
	void		Write16(int port, uint16_t data, AtariMachine& machine);	// Write a 16bits value to blitter

private:
	uint16_t	ProcessMemorySourceWord(AtariMachine& machine);
	uint8_t r8(int r) const { return m_regs[r]; }
	uint16_t r16(int r) const { return (uint16_t(m_regs[r]) << 8) | m_regs[r + 1]; }
	void w8(int r, uint8_t v) { m_regs[r] = v; }
	void w16(int r, uint16_t v) { m_regs[r] = uint8_t(v >> 8); m_regs[r + 1] = uint8_t(v); }

	void		InternalFetch(AtariMachine& machine);
	uint16_t ReadHOP(AtariMachine& machine);

	uint32_t	m_currentMotif;
	int			m_xCountReset;
	int			m_xCount;
	uint32_t	m_srcAd;
	uint32_t	m_dstAd;

	int16_t		m_xSrcInc;
	int16_t		m_ySrcInc;
	int16_t		m_xDstInc;
	int16_t		m_yDstInc;

	int			m_halfToneLine;

	bool		m_bFXSR;
	bool		m_bNFSR;

	uint8_t m_regs[256];
};
