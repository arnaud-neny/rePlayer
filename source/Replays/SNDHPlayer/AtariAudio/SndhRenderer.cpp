/*--------------------------------------------------------------------
	Atari Audio Library v1.09
	Small & accurate ATARI-ST audio emulation
	Arnaud Carré aka Leonard/Oxygene
	@leonard_coder
--------------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "SndhRenderer.h"
#include "external/ice_24.h"
#include "timedb.h"

SndhRenderer*	SndhRenderer::Create(const void* sndhMemoryData, uint32_t sndhMemorySize, uint32_t hostReplayRate)
{
	SndhRenderer* sr = new SndhRenderer();
	if ( sr->Load(sndhMemoryData, sndhMemorySize, hostReplayRate ))
		return sr;
	delete sr;
	return nullptr;
}

void SndhRenderer::Destroy(SndhRenderer* sr)
{
	delete sr;
}

SndhRenderer::SndhRenderer()
{
	static const char* sEmptyString = "";
	m_songInfo.rawBinaryPlayer = nullptr;
	memset(&m_songInfo, 0, sizeof(m_songInfo));
	m_hostReplayRate = 0;
	m_songInfo.musicName = sEmptyString;
	m_songInfo.musicAuthor = sEmptyString;
	m_songInfo.ripper = sEmptyString;
	m_songInfo.converter = sEmptyString;
	m_songInfo.year = sEmptyString;
	m_subsongInit = false;
}

SndhRenderer::~SndhRenderer()
{
	free((void*)m_songInfo.rawBinaryPlayer);
}

uint16_t	SndhRenderer::Read16(const char* r)
{
	const uint8_t* r8 = (const uint8_t*)r;
	uint16_t v = (r8[0] << 8) | (r8[1]);
	return v;
}

uint32_t	SndhRenderer::Read32(const char* r)
{
	uint32_t v = (Read16(r) << 16) | Read16(r + 2);
	return v;
}

const char* SndhRenderer::skipNTString(const char* r)
{
	r += strlen(r) + 1;
	return r;
}

bool	SndhRenderer::Load(const void* rawSndhFile, uint32_t sndhFileSize, uint32_t hostReplayRate)
{

	m_hostReplayRate = hostReplayRate;
	bool ret = false;
	SongInfo& si = m_songInfo;
	if (ice_24_header((unsigned char*)rawSndhFile))
	{
		si.rawBinaryPlayerSize = (uint32_t)ice_24_origsize((unsigned char*)rawSndhFile);
		si.rawBinaryPlayer = malloc(si.rawBinaryPlayerSize);
		long csize = ice_24_depack((unsigned char*)rawSndhFile, (unsigned char*)si.rawBinaryPlayer);
		if (si.rawBinaryPlayerSize != csize)
		{
			return false;
		}
	}
	else
	{
		si.rawBinaryPlayerSize = sndhFileSize;
		si.rawBinaryPlayer = malloc(si.rawBinaryPlayerSize);
		memcpy((void*)si.rawBinaryPlayer, rawSndhFile, sndhFileSize);
	}

	for (int i = 0; i < kSubsongCountMax; i++)
		m_subSongLenInTick[i] = 0;

	bool bFrms = false;
	const char* read8 = (const char*)si.rawBinaryPlayer;
	if (si.rawBinaryPlayerSize > 16)
	{
		if ((0x60 == read8[0]) && (0 == strncmp(read8 + 12, "SNDH", 4)))
		{
			int headerSize = Read16(read8 + 2) + 2; // suppose it's bra.w
			if (read8[1])
				headerSize = read8[1] + 2;			// but maybe it's bra.s
			const char* readEnd = read8 + headerSize;

			si.playerTickRate = 50;
			si.defaultSubsong = 1;
			si.subsongCount = 1;

			read8 += 16;
			while (read8 + 4 <= readEnd)
			{
				if (0 == strncmp(read8, "!#SN", 4))
				{
					assert(si.subsongCount > 0);
					read8 += 4 + si.subsongCount * 2;			// skip 2bytes per offset
				}
				if (0 == strncmp(read8, "!#", 2))
				{
					si.defaultSubsong = atoi(read8 + 2);
					read8 = skipNTString(read8+2);
				}
				else if (0 == strncmp(read8, "TITL", 4))
				{
					si.musicName = read8 + 4;
					read8 = skipNTString(read8 + 4);
				}
				else if (0 == strncmp(read8, "COMM", 4))
				{
					si.musicAuthor = read8 + 4;
					read8 = skipNTString(read8 + 4);
				}
				else if (0 == strncmp(read8, "RIPP", 4))
				{
					si.ripper = read8 + 4;
					read8 = skipNTString(read8 + 4);
				}
				else if (0 == strncmp(read8, "CONV", 4))
				{
					si.converter = read8 + 4;
					read8 = skipNTString(read8 + 4);
				}
				else if ((0 == strncmp(read8, "YEAR", 4)))
				{
					if ( read8[4] != 0)
						si.year = read8 + 4;	// many sndh files have "" as year string
					read8 = skipNTString(read8 + 4);
				}
				else if (0 == strncmp(read8, "##", 2))
				{
					char stemp[3];
					memcpy(stemp, read8 + 2, 2);
					stemp[2] = 0;
					si.subsongCount = atoi(stemp);
					if ((si.subsongCount <= 0) || (si.subsongCount > kSubsongCountMax))	// some SNDH files have broken ## tag
						si.subsongCount = 1;
					read8 += 4;
				}
				else if (0 == strncmp(read8, "TIME", 4))
				{
					assert(si.subsongCount > 0);
					read8 += 4;
					if (uintptr_t(read8) & 1)
						read8++;
					for (int i = 0; i < si.subsongCount; i++)
					{
						int lenInSec = Read16(read8);
						assert(si.playerTickRate > 0);
						m_subSongLenInTick[i] = lenInSec * si.playerTickRate;
						read8 += 2;
					}
				}
				else if (0 == strncmp(read8, "FRMS", 4))
				{
					assert(si.subsongCount > 0);
					read8 += 4;
					for (int i = 0; i < si.subsongCount; i++)
					{
						m_subSongLenInTick[i] = Read32(read8);
						read8 += 4;
					}
					bFrms = true;
				}
				else if (0 == strncmp(read8, "HDNS", 4))
				{
					break;
				}
				else if (	(0 == strncmp(read8, "TA", 2)) ||
							(0 == strncmp(read8, "TB", 2)) ||
							(0 == strncmp(read8, "TC", 2)) ||
							(0 == strncmp(read8, "TD", 2)) ||
							(0 == strncmp(read8, "!V", 2)))
				{
					si.playerTickRate = atoi(read8 + 2);
					read8 = skipNTString(read8 + 2);
				}
				else
				{
					read8++;
				}
			}

			if ((si.defaultSubsong > si.subsongCount) || (si.defaultSubsong < 1))
				si.defaultSubsong = 1;

			// if no new FRMS timing tag, try to search in timedb
			// (and eventually override any old TIME tag, that are often broken)
			if (!bFrms)
				timedbSearch(si.rawBinaryPlayer, si.rawBinaryPlayerSize, m_subSongLenInTick, kSubsongCountMax);

			ret = true;
		}
	}

	if (ret) m_samplePerTick = m_hostReplayRate / m_songInfo.playerTickRate;// rePlayer

	return ret;
}

bool	SndhRenderer::InitSubSong(int subSongId)
{
	bool ret = false;
	if ((subSongId >= 1) && (subSongId <= m_songInfo.subsongCount))
	{
		m_innerSamplePos = 0;
		m_atariMachine.Startup(m_hostReplayRate);
		if (m_atariMachine.Upload(m_songInfo.rawBinaryPlayer, SNDH_UPLOAD_ADDR, m_songInfo.rawBinaryPlayerSize))
		{
			ret = m_atariMachine.Jsr(SNDH_UPLOAD_ADDR, subSongId);
		}
	}
	m_subsongInit = ret;
	return ret;
}

void	SndhRenderer::AudioRenderInternal(int16_t* buffer, uint32_t count, uint32_t* pSampleViewInfo)
{
	if (!m_subsongInit)
		return;

	while (count > 0)
	{
		if (0 == m_innerSamplePos)
		{
			m_atariMachine.Jsr(SNDH_UPLOAD_ADDR + 8, 0);
			m_innerSamplePos = m_samplePerTick;
		}

		uint32_t todo = (m_innerSamplePos <= count) ? m_innerSamplePos : count;
		assert(m_innerSamplePos >= todo);

		if (buffer)
		{
			if (nullptr == pSampleViewInfo)
			{
				for (uint32_t s = 0; s < todo; s++)
					*buffer++ = m_atariMachine.ComputeNextSample();
			}
			else
			{
				for (uint32_t s = 0; s < todo; s++)
				{
					*buffer++ = m_atariMachine.ComputeNextSample();
					*pSampleViewInfo++ = m_atariMachine.ComputeCurrentVisualLevels();
				}
			}
		}
		else
		{
			// fast forward
			for (uint32_t s = 0; s < todo; s++)
				m_atariMachine.ComputeNextSample();
		}

		count -= todo;
		m_innerSamplePos -= todo;
	}
}

void SndhRenderer::AudioRender(int16_t* buffer, uint32_t count)
{
	AudioRenderInternal(buffer, count, nullptr);
}

void 	SndhRenderer::FastForward(uint32_t sampleCount)
{
	AudioRenderInternal(nullptr, sampleCount, nullptr);
}

void SndhRenderer::AudioRenderWithVisualInfos(int16_t* buffer, uint32_t count, uint32_t* pVisualSamples)
{
	AudioRenderInternal(buffer, count, pVisualSamples);
}

void	SndhRenderer::AudioRenderStereo(int16_t* buffer, uint32_t count, uint32_t* pSampleViewInfo)
{
	if (!m_subsongInit)
		return;

	while (count > 0)
	{
		if (0 == m_innerSamplePos)
		{
			m_atariMachine.Jsr(SNDH_UPLOAD_ADDR + 8, 0);
			m_innerSamplePos = m_samplePerTick;
		}

		uint32_t todo = (m_innerSamplePos <= count) ? m_innerSamplePos : count;
		assert(m_innerSamplePos >= todo);

		if (buffer)
		{
			if (nullptr == pSampleViewInfo)
			{
				for (uint32_t s = 0; s < todo; s++)
					m_atariMachine.ComputeNextSample(buffer);
			}
			else
			{
				for (uint32_t s = 0; s < todo; s++)
				{
					m_atariMachine.ComputeNextSample(buffer);
					*pSampleViewInfo++ = m_atariMachine.ComputeCurrentVisualLevels();
				}
			}
		}
		else
		{
			// fast forward
			for (uint32_t s = 0; s < todo; s++)
				m_atariMachine.ComputeNextSample();
		}

		count -= todo;
		m_innerSamplePos -= todo;
	}
}

uint32_t SndhRenderer::GetSubsongDurationSample(int subsongId) const
{
	if ((subsongId <= 0) || (subsongId > m_songInfo.subsongCount))
		return 0;

	return (m_subSongLenInTick[subsongId-1] * m_samplePerTick);	// by convention, SNDH subsong id starts at 1
}

uint32_t SndhRenderer::GetSubsongDurationMs(int subsongId) const
{
	uint32_t ms = 0;
	if (m_hostReplayRate > 0)
	{
		uint64_t tmp = (uint64_t(GetSubsongDurationSample(subsongId)) * 1000) / m_hostReplayRate;
		ms = uint32_t(tmp);
	}
	return ms;
}