/*--------------------------------------------------------------------
	Atari Audio Library v1.22
	Small & accurate ATARI-ST audio emulation
	Arnaud Carré aka Leonard/Oxygene
	@leonard_coder
--------------------------------------------------------------------*/
#pragma once
#include <stdint.h>

class AtariAudioRenderer
{
public:
	enum class eFileType
	{
		eUnknown,
		eSndh,
		eYm,
	};

	struct SongInfo
	{
		int subsongCount;
		int defaultSubsong;
		int playerTickRate;
		uint32_t 	hostReplayRate;
		uint32_t ym2149Clock;
		eFileType fileType;
		const char* musicName;
		const char* musicAuthor;
		const char* ripper;
		const char* converter;
		const char* year;

		const void* rawBinaryData;
		uint32_t rawBinaryDataSize;
	};

	// Create a AtariAudioRenderer instance from a .SNDH or .YM file data located in memory
	// The input SNDH data could be ICE! packed and .YM could be LHA packed
	// hostReplayRate is the rate you want to render audio stream ( ie 48000 for 48Khz )
	// After create you can free fileMemoryData if needed (AtariAudioRenderer keep an internal copy of the required data)
	static AtariAudioRenderer* Create(const void* fileMemoryData, uint32_t fileMemorySize, uint32_t hostReplayRate);
	static void Destroy(AtariAudioRenderer* ar);
	
	// Get information about the SNDH (like song name, author, amount of subsong, etc.)
	const 	AtariAudioRenderer::SongInfo&	GetSongInfo() const { return m_songInfo; };

	// Get a subsong duration in samples. 0 means there is no information about duration for this subsong
	virtual uint32_t GetSubsongDurationSample(int subsongId) const = 0;

	// Initialize music driver to play a sub-song. By convention, subsongId starts at 1 (not 0)
	// You must call InitSubSong before any call to AudioRender
	virtual bool	InitSubSong(int subSongId) = 0;

	// Main audio rendering function.
	// Compute the next "count" samples into "buffer" (mono, signed, 16bits samples)
	// by default the song will loop. If you want to stop at the perfect end, you can
	// use GetSubsongDurationSample() upfront to get exact amount of samples.
	virtual void AudioRender(int16_t* buffer, uint32_t count) = 0;
	virtual void AudioRenderStereo(int16_t* buffer, uint32_t sampleCount, uint32_t* pVisualSamples) = 0;

	// Fast forward into the music. Doesn't output data, but perform full emulation
	void FastForward(uint32_t count) { AudioRender(nullptr, count); }

	// Helper time unit convert functions
	uint32_t SampleToMs(uint32_t sample) const;
	uint32_t MsToSample(uint32_t ms) const;

	//-------------------------------------------------------------------------
	// Additional functions for high level players
	//-------------------------------------------------------------------------

	// Same as AudioRender but also fills pVisualSamples buffer with 1 32bits per sample
	// the 32bits contains vu meter values for 3 ym voices and STE DAC in form of 8888
	// Use it if you want to draw some per voice vu meter in a player
	virtual void AudioRenderWithVisualInfos(int16_t* buffer, uint32_t sampleCount, uint32_t* pVisualSamples) = 0;

	// Set a mute mask to artifically mute some YM or STE dac voices
	// NOTE: InitSubSong always un-mute everything. So MuteVoices should be called after InitSubsong
	static const uint32_t kYMVoiceA = (1 << 0);
	static const uint32_t kYMVoiceB = (1 << 1);
	static const uint32_t kYMVoiceC = (1 << 2);
	static const uint32_t kSTEDac = (1 << 3);
	virtual void MuteVoices(uint32_t muteVoiceMask) = 0;

protected:
	static	const	int		kSubsongCountMax = 128;

	// Private constructors prevent direct instantiation
	virtual ~AtariAudioRenderer();
	AtariAudioRenderer();
    AtariAudioRenderer(const AtariAudioRenderer&) = delete;            // Prevent copy construction
    AtariAudioRenderer& operator=(const AtariAudioRenderer&) = delete; // Prevent copy assignment

	static eFileType QuickFileTypeCheck(const void* rawMemory, uint32_t rawSize);
	uint16_t ReadBE16(const char* r);
	uint32_t ReadBE32(const char* r);
	const char* AUskipNTString(const char* r);

	SongInfo m_songInfo;
	uint32_t	m_subSongLenInTick[kSubsongCountMax];
	uint32_t	m_samplePerTick;
	uint32_t	m_innerSamplePos;
	bool 		m_subsongInit;
};
