//----------------------------------------------------------
//
//	AtariAudio 1.26
//	Small & accurate ATARI-ST audio emulation
//	by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//
//----------------------------------------------------------
#include <assert.h>
#include <memory.h>
#include "Blitter.h"
#include "AtariMachine.h"


void	Blitter::Reset()
{
	memset(m_regs, 0, sizeof(m_regs));
}

uint8_t Blitter::Read8(int port)
{
	return r8(port);
}

uint16_t Blitter::Read16(int port)
{
	return r16(port);
}

void Blitter::Write8(int port, uint8_t data, AtariMachine& machine)
{
	w8(port, data);
	if (0x3c == port)
		Run(machine);
}

void Blitter::Write16(int port, uint16_t data, AtariMachine& machine)
{
	w16(port, data);
	if (0x3c == port)
		Run(machine);
}

uint16_t	Blitter::ProcessMemorySourceWord(AtariMachine& machine)
{

	uint16_t iWord;
	if (m_bNFSR && (1 == m_xCount))			// if fxsr and we are on the last word, don't do anything
	{
		iWord = m_currentMotif;
		m_srcAd -= m_xSrcInc;		// HACK!!! Je pige pas bien, seule magouille que j'ai trouvé (extacy demo)
	}
	else
	{
		iWord = machine.memRead16( m_srcAd );
		if (m_xCount > 1)
			m_srcAd += m_xSrcInc;
	}
	return iWord;
}

uint16_t	Blitter::ReadHOP(AtariMachine& machine)
{
	uint16_t iHOP = 0;
	
	static	const	int	s_iSourceRead[ 16 ] = { 0,1,1,1,
	1,0,1,1,
	1,1,0,1,
	1,1,1,0};

	const int iLogicalHop = r8(0x3b)&0xf;
	if ( s_iSourceRead[ iLogicalHop ] )
	{
		switch ( r8(0x3a) & 3)
		{
			case 0:	iHOP = 0xffff;													break;
			case 1:	iHOP = r16( m_halfToneLine<<1 );								break;
			case 2: iHOP = ProcessMemorySourceWord(machine);								break;
			case 3: iHOP = ProcessMemorySourceWord(machine) & r16( m_halfToneLine<<1 );	break;
		}
	}
	return iHOP;
}

void	Blitter::InternalFetch(AtariMachine& machine)
{
	m_currentMotif = (m_currentMotif<<16) | ReadHOP(machine);
}

void	Blitter::Run(AtariMachine& machine)
{

	if (0 == (r8( 0x3c ) & 0x80))		// busy bit
		return;

	int iLineCount = r16( 0x38 );

	if (iLineCount > 0)
	{
		m_currentMotif = 0;
		m_xCountReset = (0 == r16( 0x36 )) ? 65536 : r16( 0x36 );

		m_xSrcInc = int16_t(r16( 0x20 ) & (-2));
		m_ySrcInc = int16_t(r16( 0x22 ) & (-2));
		m_xDstInc = int16_t(r16( 0x2e ) & (-2));
		m_yDstInc = int16_t(r16( 0x30 ) & (-2));
		const int	iShift = r8( 0x3d ) & 0x0f;
		m_bFXSR = 0 != (r8( 0x3d ) & 0x80);
		m_bNFSR = 0 != (r8( 0x3d ) & 0x40);

		const int	iLogicalOp = r8( 0x3b ) & 0x0f;

		m_srcAd = ((uint32_t(r16(0x24)) << 16) | r16(0x26))&0x00fffffe;
		m_dstAd = ((uint32_t(r16(0x32)) << 16) | r16(0x34))&0x00fffffe;

		uint16_t iEndMask[ 3 ];
		iEndMask[ 0 ] = r16( 0x28 );
		iEndMask[ 1 ] = r16( 0x2a );
		iEndMask[ 2 ] = r16( 0x2c );

		m_halfToneLine = r8( 0x3c ) & 0x0f;
		assert( 0 == (r8(0x3c) & 0x20) );			// assume there is no smudge bit

		do
		{
			m_xCount = m_xCountReset;

			// first block
			if ( m_bFXSR )
			{	// FXSR
				InternalFetch(machine);
			}

			int iCurrentMaskId = 0;
			while (m_xCount >= 1)
			{
				InternalFetch(machine);
				uint16_t iHOP = m_currentMotif;
				if ( 1 != r8( 0x3a ))
					iHOP = (m_currentMotif >> iShift);

				uint16_t iOriginalValue = machine.memRead16( m_dstAd);

				uint16_t iHOPResult;
				switch (iLogicalOp)
				{
					case 0:		iHOPResult = 0;										break;
					case 1:		iHOPResult = iHOP & machine.memRead16( m_dstAd );			break;
					case 2:		iHOPResult = iHOP & (~machine.memRead16( m_dstAd ));		break;
					case 3:		iHOPResult = iHOP;									break;
					case 4:		iHOPResult = (~iHOP) & machine.memRead16( m_dstAd );		break;
					case 5:		iHOPResult = machine.memRead16( m_dstAd );				break;
					case 6:		iHOPResult = iHOP ^ machine.memRead16( m_dstAd );			break;
					case 7:		iHOPResult = iHOP | machine.memRead16( m_dstAd );			break;
					case 8:		iHOPResult = (~iHOP) & (~machine.memRead16( m_dstAd ));	break;
					case 9:		iHOPResult = (~iHOP) ^ machine.memRead16( m_dstAd );		break;
					case 10:	iHOPResult = ~machine.memRead16( m_dstAd );				break;
					case 11:	iHOPResult = iHOP | (~machine.memRead16( m_dstAd ));		break;
					case 12:	iHOPResult = ~iHOP;									break;
					case 13:	iHOPResult = (~iHOP) | machine.memRead16( m_dstAd );		break;
					case 14:	iHOPResult = (~iHOP) | (~machine.memRead16( m_dstAd ));	break;
					case 15:	iHOPResult = 0xffff;								break;
				}

				iHOPResult = (iHOPResult & iEndMask[ iCurrentMaskId ]) | (iOriginalValue & (~iEndMask[ iCurrentMaskId ]));

				machine.memWrite16( m_dstAd, iHOPResult );

				if (m_xCount > 1)
					m_dstAd += m_xDstInc;

				m_xCount--;
				if (0 == iCurrentMaskId)
					iCurrentMaskId = 1;		// middle mask
				if (1 == m_xCount)
					iCurrentMaskId = 2;		// end mask
			}

			// end of line
			m_srcAd += m_ySrcInc;
			m_dstAd += m_yDstInc;
			if (m_yDstInc >= 0)
				m_halfToneLine = (m_halfToneLine + 1) & 15;
			else
				m_halfToneLine = (m_halfToneLine - 1) & 15;
		}
		while (--iLineCount);

		w16(0x24, m_srcAd >> 16);
		w16(0x26, m_srcAd&0xfffe);
		w16(0x32, m_dstAd >> 16);
		w16(0x34, m_dstAd&0xfffe);
		w16(0x38, 0);
	}

	m_regs[0x3c] &= 0x7f;
}
