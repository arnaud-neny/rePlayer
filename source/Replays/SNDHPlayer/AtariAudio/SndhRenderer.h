/*--------------------------------------------------------------------
	Atari Audio Library v1.10
	Small & accurate ATARI-ST audio emulation
	Arnaud Carré aka Leonard/Oxygene
	@leonard_coder
--------------------------------------------------------------------*/
#pragma once
#include <stdint.h>
#include "AtariMachine.h"

class SndhRenderer
{
public:
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
		uint32_t rawBinaryPlayerSize;
	};

	// Create a SndhRenderer instance from a SNDH file data located in memory
	// The input SNDH data could be ICE! packed
	// hostReplayRate is the rate you want to render audio stream ( ie 44100 or 44.1Khz )
	// After create you can free sndhMemoryData if needed (SndhRenderer keep an internal copy of the required data)
	static SndhRenderer*	Create(const void* sndhMemoryData, uint32_t sndhMemorySize, uint32_t hostReplayRate);

	// Destroy SndhRenderer object
	static void Destroy(SndhRenderer* sr);

	// Get information about the SNDH (like song name, author, amount of subsong, etc.)
	const 	SongInfo&	GetSongInfo() const { return m_songInfo; };

	// Get a subsong duration in samples. 0 means there is no information about duration for this subsong
	uint32_t GetSubsongDurationSample(int subsongId) const;

	// Same as GetSubsongDurationSample, but returned value is in millisec
	uint32_t GetSubsongDurationMs(int subsongId) const;

	// Initialize music driver to play a sub-song. By convention, subsongId starts at 1 (not 0)
	// You must call InitSubSong before any call to AudioRender
	bool	InitSubSong(int subSongId);

	// Main audio rendering function.
	// Compute the next "count" samples into "buffer" (mono, signed, 16bits samples)
	// by default the song will loop. If you want to stop at the perfect end, you can
	// use GetSubsongDurationSample() upfront to get exact amount of samples.
	void	AudioRender(int16_t* buffer, uint32_t sampleCount);
	void	AudioRenderStereo(int16_t* buffer, uint32_t sampleCount, uint32_t* pVisualSamples);

	// Fast forward sampleCount in the song
	void 	FastForward(uint32_t sampleCount);


	//-------------------------------------------------------------------------
	// Additional functions for high level players
	//-------------------------------------------------------------------------

	// Same as AudioRender but also fills pVisualSamples buffer with 1 32bits per sample
	// the 32bits contains vu meter values for 3 ym voices and STE DAC in form of 8888
	// Use it if you want to draw some per voice vu meter in a player
	void	AudioRenderWithVisualInfos(int16_t* buffer, uint32_t sampleCount, uint32_t* pVisualSamples);

	// Set a mute mask to artifically mute some YM or STE dac voices
	// NOTE: InitSubSong always un-mute everything. So MuteVoices should be called after InitSubsong
	static const uint32_t kYMVoiceA = (1 << 0);
	static const uint32_t kYMVoiceB = (1 << 1);
	static const uint32_t kYMVoiceC = (1 << 2);
	static const uint32_t kSTEDac = (1 << 3);
	void MuteVoices(uint32_t muteVoiceMask) { m_atariMachine.MuteVoices(muteVoiceMask); }

private:
	static	const	int		kSubsongCountMax = 128;
	static	const	uint32_t	SNDH_UPLOAD_ADDR = 0x10002;		// some SNDH can't play below (ie SynthDream2) Also some driver crash if loaded at 64KiB bound ( metal planet by Floopy at 1:44 )

    // Private constructors prevent direct instantiation
    SndhRenderer();
    ~SndhRenderer();
    SndhRenderer(const SndhRenderer&) = delete;            // Prevent copy construction
    SndhRenderer& operator=(const SndhRenderer&) = delete; // Prevent copy assignment

	bool	Load(const void* rawSndhFile, uint32_t sndhFileSize, uint32_t hostReplayRate);
	uint16_t		Read16(const char*);
	uint32_t		Read32(const char*);
	const char*	skipNTString(const char* r);
	void		AudioRenderInternal(int16_t* buffer, uint32_t count, uint32_t* pSampleViewInfo);

	SongInfo m_songInfo;
	AtariMachine m_atariMachine;

	uint32_t	m_subSongLenInTick[kSubsongCountMax];
	uint32_t	m_samplePerTick;
	uint32_t	m_innerSamplePos;
	bool 		m_subsongInit;
	uint32_t 	m_hostReplayRate;
};

