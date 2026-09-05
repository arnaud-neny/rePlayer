/*--------------------------------------------------------------------
	Atari Audio Library v1.06
	Small & accurate ATARI-ST audio emulation
	Arnaud Carré aka Leonard/Oxygene
	@leonard_coder
--------------------------------------------------------------------*/
#pragma once
#include <stdint.h>
#include "AtariMachine.h"

static	const	int		kSubsongCountMax = 128;
static const	int kDefaultSongDuration = 60 * 3;

class SndhFile
{
public:
	SndhFile();
	~SndhFile();

	struct SubSongInfo
	{
		int subsongCount;
		int	playerTickCount;
		int playerTickRate;
		int samplePerTick;
		const char* musicName;
		const char* musicAuthor;
		const char* ripper;
		const char* converter;
		const char* year;
	};

	bool	Load(const void* rawSndhFile, int sndhFileSize, uint32_t hostReplayRate);
	void	Unload();
	bool	IsLoaded() const { return m_bLoaded; }
	
	int		GetSubsongCount() const;
	int		GetDefaultSubsong() const { return m_defaultSubSong; }
	bool	GetSubsongInfo(int subSongId, SubSongInfo& out) const;
	bool	InitSubSong(int subSongId);
	int 	FastForward(int framesToSkip);

	/*
	 * Main audio rendering function.
	 * Compute the next "count" samples into "buffer" (mono, signed, 16bits samples)
	 * returns the amount of samples output (could be less than count when music is out of frames)
	*/
	int		AudioRender(int16_t* buffer, int count);
	int		AudioRenderStereo(int16_t* buffer, int count, uint32_t* pVisualSamples);
	int		AudioNull(int count);

	/*
	* Same as AudioRender but also fills pVisualSamples buffer with 1 32bits per sample
	* the 32bits contains vu meter values for 3 ym voices and STE DAC in form of 8888
	* Use it if you want to draw some per voice vu meter in a player
	*/
	int		AudioRenderWithVisualInfos(int16_t* buffer, int count, uint32_t* pVisualSamples);

	const void*	GetRawData() const { return m_rawBuffer; }
	const int	GetRawDataSize() const { return m_rawSize; }
	void 	SetDefaultSongDuration(int durationInSec);


private:
	uint16_t		Read16(const char*);
	uint32_t		Read32(const char*);
	const char*	skipNTString(const char* r);
	int		AudioRenderInternal(int16_t* buffer, int count, uint32_t* pSampleViewInfo);

	bool	m_bLoaded;
	const char*	m_Title;
	const char*	m_Author;
	const char* m_Ripper;
	const char* m_Converter;
	const char*	m_sYear;
	const void*	m_rawBuffer;
	int		m_rawSize;

	int		m_defaultSubSong;
	uint32_t	m_subSongLenInTick[kSubsongCountMax];
	int		m_subSongCount;
	int		m_playerRate;

	int		m_samplePerTick;
	int		m_innerSamplePos;
	int		m_frame;
	int		m_frameCount;
	uint32_t m_hostReplayRate;
	int 	m_defaultSongDurationInSec;

	AtariMachine m_atariMachine;
};
