/*--------------------------------------------------------------------
	Atari Audio Library v1.23
	Small & accurate ATARI-ST audio emulation
	Arnaud Carré aka Leonard/Oxygene
	@leonard_coder
	--------------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "AtariAudioRenderer.h"
#include "SndhRenderer.h"
#include "YmRenderer.h"
#include "external/lzh.h"
#include "external/ice_24.h"

AtariAudioRenderer* AtariAudioRenderer::Create(const void* fileMemoryData, uint32_t fileMemorySize, uint32_t hostReplayRate)
{
	const eFileType t = QuickFileTypeCheck(fileMemoryData, fileMemorySize);
	if ( eFileType::eSndh == t )
		return SndhRenderer::Create(fileMemoryData, fileMemorySize, hostReplayRate);
	if ( eFileType::eYm == t )
		return YmRenderer::Create(fileMemoryData, fileMemorySize, hostReplayRate);
	return nullptr;
}

void AtariAudioRenderer::Destroy(AtariAudioRenderer* ar)
{
	delete ar;
}

AtariAudioRenderer::AtariAudioRenderer()
{
	static const char* sEmptyString = "";
	memset(&m_songInfo, 0, sizeof(m_songInfo));
	m_songInfo.musicName = sEmptyString;
	m_songInfo.musicAuthor = sEmptyString;
	m_songInfo.ripper = sEmptyString;
	m_songInfo.converter = sEmptyString;
	m_songInfo.year = sEmptyString;
	m_songInfo.fileFormat = sEmptyString;
}

AtariAudioRenderer::~AtariAudioRenderer()
{
	free((void*)m_songInfo.rawBinaryData);
}

AtariAudioRenderer::eFileType AtariAudioRenderer::QuickFileTypeCheck(const void* rawMemory, uint32_t rawSize)
{
	if (rawSize > 16)
	{
		// check packed file
		if (LzhDepacker::IsLzhPacked(rawMemory, rawSize))
			return eFileType::eYm;

		if (ice_24_header((unsigned char*)rawMemory))
			return eFileType::eSndh;

		// check unpacked input file
		if (0 == strncmp(((const char*)rawMemory) + 4, "LeOnArD!", 8))
			return eFileType::eYm;

		const char* read8 = (const char*)rawMemory;
		static const char* sSigns[] =
		{
			"YM2!","YM3!","YM3b","YM5!","YM6!","MIX1","YMT1","YMT2",
			nullptr
		};
		const char** pr = sSigns;
		while (*pr)
		{
			if ( 0 == strncmp(*pr, read8, 4))
				return eFileType::eYm;
			pr++;
		}

		if ((0x60 == read8[0]) && (0 == strncmp(read8 + 12, "SNDH", 4)))
			return eFileType::eSndh;
	}
	return eFileType::eUnknown;
}

uint32_t AtariAudioRenderer::SampleToMs(uint32_t sample) const
{
	if (0 == m_songInfo.hostReplayRate)
		return 0;

	return uint32_t(((uint64_t(sample) * 1000) / m_songInfo.hostReplayRate));
}

uint32_t AtariAudioRenderer::MsToSample(uint32_t ms) const
{
	return uint32_t(((uint64_t(ms) * m_songInfo.hostReplayRate) / 1000));
}

uint16_t	AtariAudioRenderer::ReadBE16(const char* r)
{
	const uint8_t* r8 = (const uint8_t*)r;
	uint16_t v = (r8[0] << 8) | (r8[1]);
	return v;
}

uint32_t	AtariAudioRenderer::ReadBE32(const char* r)
{
	uint32_t v = (ReadBE16(r) << 16) | ReadBE16(r + 2);
	return v;
}

const char* AtariAudioRenderer::SkipNTString(const char* r)
{
	r += strlen(r) + 1;
	return r;
}
