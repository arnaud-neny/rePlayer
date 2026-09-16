/*--------------------------------------------------------------------
	Atari Audio Library v1.24
	Small & accurate ATARI-ST audio emulation
	Arnaud Carré aka Leonard/Oxygene
	@leonard_coder
--------------------------------------------------------------------*/
#pragma once
#include <stdint.h>
#include "AtariAudioRenderer.h"
#include "AtariMachine.h"

class SndhRenderer : public AtariAudioRenderer
{
public:
	// Read the base AtariAudioRenderer.h header for more details about API
	static SndhRenderer* Create(const void* sndhMemoryData, uint32_t sndhMemorySize, uint32_t hostReplayRate);
	const SongInfo&	GetSongInfo() const;
	uint32_t GetSubsongDurationSample(int subsongId) const;
	bool InitSubSong(int subSongId);
	void AudioRender(int16_t* buffer, uint32_t sampleCount);
	void AudioRenderStereo(int16_t* buffer, uint32_t sampleCount, uint32_t* pVisualSamples);
	void AudioRenderWithVisualInfos(int16_t* buffer, uint32_t sampleCount, uint32_t* pVisualSamples);
	void MuteVoices(uint32_t muteVoiceMask) { m_atariMachine.MuteVoices(muteVoiceMask); }

private:
	static	const	uint32_t	SNDH_UPLOAD_ADDR = 0x10002;		// some SNDH can't play below (ie SynthDream2) Also some driver crash if loaded at 64KiB bound ( metal planet by Floopy at 1:44 )

    // Private constructors prevent direct instantiation
    SndhRenderer();
    ~SndhRenderer();
    SndhRenderer(const SndhRenderer&) = delete;            // Prevent copy construction
    SndhRenderer& operator=(const SndhRenderer&) = delete; // Prevent copy assignment

	bool	Load(const void* rawSndhFile, uint32_t sndhFileSize, uint32_t hostReplayRate);
	void		AudioRenderInternal(int16_t* buffer, uint32_t count, uint32_t* pSampleViewInfo);

	AtariMachine m_atariMachine;
};

