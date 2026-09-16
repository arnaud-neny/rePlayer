/*--------------------------------------------------------------------
	Atari Audio Library v1.23
	Small & accurate ATARI-ST audio emulation
	Arnaud Carré aka Leonard/Oxygene
	@leonard_coder
--------------------------------------------------------------------*/
#pragma once
#include <stdint.h>
#include "AtariAudioRenderer.h"
#include "ym2149c.h"
#include "Mk68901.h"

class YmRenderer : public AtariAudioRenderer
{
public:
	// Read the base AtariAudioRenderer.h header for more details about API
	static YmRenderer* Create(const void* sndhMemoryData, uint32_t sndhMemorySize, uint32_t hostReplayRate);
	const SongInfo&	GetSongInfo() const;
	uint32_t GetSubsongDurationSample(int subsongId) const;
	bool InitSubSong(int subSongId);
	void AudioRender(int16_t* buffer, uint32_t sampleCount);
	void AudioRenderStereo(int16_t* buffer, uint32_t sampleCount, uint32_t* pVisualSamples);
	void AudioRenderWithVisualInfos(int16_t* buffer, uint32_t sampleCount, uint32_t* pVisualSamples);
	void MuteVoices(uint32_t muteVoiceMask);

private:

    // Private constructors prevent direct instantiation
    YmRenderer();
    ~YmRenderer();
    YmRenderer(const YmRenderer&) = delete;            // Prevent copy construction
    YmRenderer& operator=(const YmRenderer&) = delete; // Prevent copy assignment

	enum class eYmType
	{
		eUnknown = 0,
		eYM2a = ('Y' << 24) | ('M' << 16) | ('2' << 8) | ('!'),	//'YM2!'
		eYM3a = ('Y' << 24) | ('M' << 16) | ('3' << 8) | ('!'),	//'YM3!'
		eYM3b = ('Y' << 24) | ('M' << 16) | ('3' << 8) | ('b'),	//'YM3b'
		eYM4a = ('Y' << 24) | ('M' << 16) | ('4' << 8) | ('!'),	//'YM4!'
		eYM5a = ('Y' << 24) | ('M' << 16) | ('5' << 8) | ('!'),	//'YM5!'
		eYM6a = ('Y' << 24) | ('M' << 16) | ('6' << 8) | ('!'),	//'YM6!'
		eMIX1 = ('M' << 24) | ('I' << 16) | ('X' << 8) | ('1'),	//'MIX1'
		eYMT1 = ('Y' << 24) | ('M' << 16) | ('T' << 8) | ('1'),	//'YMT1'
		eYMT2 = ('Y' << 24) | ('M' << 16) | ('T' << 8) | ('2'),	//'YMT2'
	};

	enum class eYmFxType
	{
		eNone,
		eSid,
		eSinSid,
		eSyncBuzzer,
		eDigidrum,
	};

	struct YmFx
	{
		eYmFxType type = eYmFxType::eNone;
		const uint8_t* sample;
		int ymVoice;
			uint32_t fxPhase;
		uint32_t sampleLen;
		uint8_t sidVol;
		uint8_t syncBuzzShape;
	};


	void PlayerTick();
	void Ym2DriverTick();
	void Ym356DriverTick();
	void YmTrackerDriverTick();
	void SetTimer(int slot, int prediv, int count);
	uint32_t YmFxDecode(int fxSlot, int regCode, int regPrediv, int regCount);
	Ym2149c::Levels ComputeNextSample(void);
	bool	Load(const void* rawYmFile, uint32_t ymFileSize, uint32_t hostReplayRate);
	void		AudioRenderInternal(int16_t* buffer, uint32_t count, uint32_t* pSampleViewInfo);
	uint8_t ReadInterleaved(int reg) const;
	void YmWrite(int reg, uint8_t d);
	void ConvertTo4Bits(void);
	uint32_t ComputeCurrentVisualLevels();

	Ym2149c::Levels ComputeNextYmTrackerSample();
	int16_t ComputeNextYmMixSample();
	void FetchNextDigimixBlock();

	uint16_t StreamBE16(const char** r);
	uint32_t StreamBE32(const char** r);
	const char* GetFileFormatString() const;

	Ym2149c m_ym2149;
	Mk68901 m_mfp;

	YmFx m_ymFx[2];
	uint32_t m_tick;
	uint32_t m_songLoopTick;
	uint32_t m_flags;
	eYmType m_ymType;
	const uint8_t* m_dataStream;
	int m_dataStreamStride;

	static const int kYmInterleaved = 1<<0;
	static const int kYmSignedSample = 1<<1;
	static const int kYm4BitsSample = 1<<2;

	static const int kYmMaxSamples = 64;
	struct YmSample
	{
		const uint8_t* data;
		uint32_t len;
		uint32_t repPos;
	};

	static const int kYmMaxTrackerVoices = 8;
	struct YmTrackerVoice
	{
		uint32_t sampleId;
		uint32_t samplePos;
		uint32_t replayRate;
		uint32_t innerClock;
		uint32_t volume;
		bool loop;
		bool running;
	};

	uint32_t m_songDurationSample;
	const int8_t* m_mixBank;
	uint32_t m_mixFrac;
	int m_mixPatternPos;
	int m_mixPatternCount;
	int m_mixCurrentRepeat;
	uint32_t m_mixBankOffset;
	uint32_t m_mixSamplePos;
	uint32_t m_mixSampleLen;
	uint32_t m_mixReplayRate;
	uint8_t m_mixSignXor;
	int8_t m_mixLastSample;
	int m_trkFreqShift;
	int m_trkVoiceCount;
	YmTrackerVoice m_trkVoices[kYmMaxTrackerVoices];
	int m_sampleCount;
	YmSample m_samples[kYmMaxSamples];


};

