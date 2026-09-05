/*--------------------------------------------------------------------
	Atari Audio Library v1.06
	Small & accurate ATARI-ST audio emulation
	Arnaud Carré aka Leonard/Oxygene
	@leonard_coder
--------------------------------------------------------------------*/
#pragma once
#include <stdint.h>

class Ym2149c
{
public:
	union Levels
	{
		uint64_t value = 0;
		struct
		{
			uint16_t uMono;
			uint16_t uLeft;
			uint16_t uRight;
		};
		struct
		{
			int16_t sMono;
			int16_t sLeft;
			int16_t sRight;
		};
		uint16_t uLevels[3];
		int16_t sLevels[3];
	};


	void	Reset(uint32_t hostReplayRate, uint32_t ymClock = 2000000);
	void	WritePort(uint8_t port, uint8_t value);
	uint8_t ReadPort(uint8_t port) const;
	Levels	ComputeNextSample();
	void	InsideTimerIrq(bool inside);

	uint16_t GetCurrentVisualLevels() const { return m_currentVisualLevels; } // only used for some player visual, contains 3 YM voices volume and STE DAC in 8888 format

private:
	void	WriteReg(int reg, uint8_t value);
	uint16_t Tick();

	static const uint32_t kDcAdjustHistoryBit = 11;	// 2048 values (~20ms at 44Khz)

	Levels	dcAdjust(Levels v);

	int			m_selectedReg;
	const uint8_t* m_pCurrentEnv;
	uint32_t	m_ymClockOneEighth;
	uint32_t	m_resamplingDividor;
	uint32_t	m_hostReplayRate;
	uint32_t	m_toneCounter[3];
	uint32_t	m_tonePeriod[3];
	uint32_t	m_toneEdges;

	uint32_t	m_envCounter;
	int			m_envPos;
	uint32_t	m_envPeriod;
	uint32_t	m_noiseCounter;
	uint32_t	m_noisePeriod;
	uint32_t	m_toneMask;
	uint32_t	m_noiseMask;
	uint32_t	m_noiseRndRack;
	uint32_t	m_currentNoiseMask;
	uint16_t 	m_currentVisualLevels;
	struct
	{
		uint16_t		buffer[1 << kDcAdjustHistoryBit];
		unsigned int	pos;
		uint32_t		sum;
	}			m_dcAdjust[3];
	uint8_t		m_regs[14];
	uint32_t	m_currentLevel;
	uint32_t	m_innerCycle;
	uint32_t 	m_noiseHalf;
	bool		m_insideTimerIrq;
	bool		m_edgeNeedReset[3];
};
