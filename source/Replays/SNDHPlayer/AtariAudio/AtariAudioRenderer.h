//----------------------------------------------------------
//
//	AtariAudio 1.26
//	Small & accurate ATARI-ST audio emulation
//	by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//
//----------------------------------------------------------
#pragma once
#include <stdint.h>

class AtariAudioRenderer
{
public:

	static const uint32_t kDefaultAtariYmClock = 2000000;
	static const uint32_t kAtariSTECpuClock = 8021247;

	enum class eFileType
	{
		eUnknown,
		eSndh,
		eYm,
	};

	struct SongInfo
	{
		int subsongCount;					// Number of subsongs (.sndh can have several, .ym has 1)
		int defaultSubsong;					// Default subsong when multiple are present
		int playerTickRate;					// Original music driver tick rate (typically 50Hz)
		uint32_t 	hostReplayRate;			// Emulation output sample rate (typically 48kHz)
		uint32_t ym2149Clock;				// YM2149 audio chip clock (typically 2MHz for Atari ST)
		eFileType fileType;					// File type (.sndh or .ym)
		const char* musicName;				// Music name or empty string ""
		const char* musicAuthor;			// Music author or empty string ""
		const char* ripper;					// Music ripper or empty string ""
		const char* converter;				// Music converter or empty string ""
		const char* year;					// Music year or empty string ""
		const char* fileFormat;				// Detailed file format string ("SNDH", "YM 5", etc.)
		const void* rawBinaryData;			// Raw unpacked music file data
		uint32_t rawBinaryDataSize;			// Raw unpacked music file data size (in bytes)
	};

	// Creates an AtariAudioRenderer instance from .sndh or .ym file data in memory.
	// hostReplayRate is the target output sample rate (e.g., 48000 for 48kHz).
	// defaultYm2149Clock is only used for legacy file formats that omit clock info (YM2 or YM3).
	// The input .sndh file may be ICE! packed, and .ym may be LZH packed.
	// After creation, fileMemoryData can be freed (AtariAudioRenderer retains an internal copy).
	static AtariAudioRenderer* Create(const void* fileMemoryData, uint32_t fileMemorySize, uint32_t hostReplayRate, uint32_t defaultYm2149Clock = kDefaultAtariYmClock);
	static void Destroy(AtariAudioRenderer* ar);
	
	// Get information about the loaded song (title, author, subsong count, etc.)
	const 	AtariAudioRenderer::SongInfo&	GetSongInfo() const { return m_songInfo; };

	// Get subsong duration in samples. Returns 0 if duration info is unavailable for this subsong.
	virtual uint32_t GetSubsongDurationSample(int subsongId) const = 0;

	// Initialize the music driver for a specific subsong (starts at 1 by convention, not 0).
	// Must be called before any AudioRender call, even for single subsong files (.ym).
	virtual bool	InitSubSong(int subSongId) = 0;

	// Main audio rendering function.
	// Renders the next "count" samples into "buffer" (mono, signed 16-bit samples).
	// Songs loop by default. To stop at the exact duration, call GetSubsongDurationSample() in advance.
	// Rendering can be broken into smaller blocks by calling AudioRender iteratively with smaller sample counts.
	virtual void AudioRender(int16_t* buffer, uint32_t count) = 0;
	virtual void AudioRenderStereo(int16_t* buffer, uint32_t sampleCount, uint32_t* pVisualSamples) = 0;

	// Fast forward through the music. Performs full emulation without generating audio output.
	void FastForward(uint32_t count) { AudioRender(nullptr, count); }

	// Time unit conversion helpers
	uint32_t SampleToMs(uint32_t sample) const;
	uint32_t MsToSample(uint32_t ms) const;

	//-------------------------------------------------------------------------
	// Additional functions for high-level players
	//-------------------------------------------------------------------------

	// Same as AudioRender, but also populates pVisualSamples with one 32bit value per sample.
	// The 32bit value contains VU meter levels for 3 YM channels and the STE DAC (8 bits each, 8888 format).
	// Use this to display per channel VU meters in a music player interface.
	virtual void AudioRenderWithVisualInfos(int16_t* buffer, uint32_t sampleCount, uint32_t* pVisualSamples) = 0;

	// Sets a mask to artificially mute specific YM or STE DAC channels.
	// NOTE: InitSubSong always unmutes all channels. Call MuteVoices after InitSubSong.
	static const uint32_t kYMVoiceA = (1 << 0);
	static const uint32_t kYMVoiceB = (1 << 1);
	static const uint32_t kYMVoiceC = (1 << 2);
	static const uint32_t kSTEDac = (1 << 3);
	virtual void MuteVoices(uint32_t muteVoiceMask) = 0;

protected:
	static	const	int		kSubsongCountMax = 128;

	// Prevent direct instantiation
	virtual ~AtariAudioRenderer();
	AtariAudioRenderer();
    AtariAudioRenderer(const AtariAudioRenderer&) = delete;
    AtariAudioRenderer& operator=(const AtariAudioRenderer&) = delete;
	uint32_t ComputeHostSamplePerTick(uint32_t hostReplayRate, int playerTickRate) const;

	static eFileType QuickFileTypeCheck(const void* rawMemory, uint32_t rawSize);
	uint16_t ReadBE16(const char* r);
	uint32_t ReadBE32(const char* r);
	const char* SkipNTString(const char* r);

	SongInfo m_songInfo;
	uint32_t	m_subSongLenInTick[kSubsongCountMax];
	uint32_t	m_samplePerTick;
	uint32_t	m_innerSamplePos;
	bool 		m_subsongInit;
};
