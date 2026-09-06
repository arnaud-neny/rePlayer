/*--------------------------------------------------------------------
	Atari Audio Library v1.08
	Small & accurate ATARI-ST audio emulation
	Arnaud Carré aka Leonard/Oxygene
	@leonard_coder
--------------------------------------------------------------------*/
#pragma once
#include <stdint.h>
#include "AtariMachine.h"

static	const	int		kSubsongCountMax = 128;

class SndhFile
{
public:
	SndhFile();
	~SndhFile();

	struct SongInfo
	{
		int subsongCount;
		int defaultSubsong;
		int playerTickRate;
		const char* musicName;
		const char* musicAuthor;
		const char* ripper;
		const char* converter;
		const char* year;
		const void* rawBinaryPlayer;
		int rawBinaryPlayerSize;
	};

	bool	Load(const void* rawSndhFile, int sndhFileSize, uint32_t hostReplayRate);
	const 	SongInfo&	GetSongInfo() const { return m_songInfo; };
	uint32_t GetSubsongDurationSample(int subsongId) const;
	uint32_t GetSubsongDurationMs(int subsongId) const;
	void	Unload();
	
	bool	InitSubSong(int subSongId);

	// Main audio rendering function.
	// Compute the next "count" samples into "buffer" (mono, signed, 16bits samples)
	// by default the song will loop. If you want to stop at the perfect end, you can
	// use GetSubsongDurationInSample() upfront to get exact amount of samples.
	// (Note: Some old SNDH files does NOT have duration information, GetSubsongDurationInSample() will return 0)
	void	AudioRender(int16_t* buffer, uint32_t sampleCount);
	void	AudioRenderStereo(int16_t* buffer, uint32_t sampleCount, uint32_t* pVisualSamples);

	// Same as AudioRender but also fills pVisualSamples buffer with 1 32bits per sample
	// the 32bits contains vu meter values for 3 ym voices and STE DAC in form of 8888
	// Use it if you want to draw some per voice vu meter in a player
	void	AudioRenderWithVisualInfos(int16_t* buffer, uint32_t sampleCount, uint32_t* pVisualSamples);

	// Fast forward
	void 	FastForward(uint32_t sampleCount);


private:
	uint16_t		Read16(const char*);
	uint32_t		Read32(const char*);
	const char*	skipNTString(const char* r);
	void		AudioRenderInternal(int16_t* buffer, uint32_t count, uint32_t* pSampleViewInfo);
	bool IsValid() const { return m_songInfo.subsongCount > 0; }

	SongInfo m_songInfo;
	AtariMachine m_atariMachine;

	uint32_t	m_subSongLenInTick[kSubsongCountMax];
	uint32_t	m_samplePerTick;
	uint32_t	m_innerSamplePos;
	uint32_t m_hostReplayRate;
};
