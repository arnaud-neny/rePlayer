/*--------------------------------------------------------------------
	Atari Audio Library v1.06
	Small & accurate ATARI-ST audio emulation
	by Arnaud Carré aka Leonard/Oxygene
	@leonard_coder
--------------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "SndhFile.h"
#include "external/ice_24.h"
#include "timedb.h"


SndhFile::SndhFile()
{
	m_rawBuffer = nullptr;
	Unload();
}

SndhFile::~SndhFile()
{
	Unload();
}

void	SndhFile::Unload()
{
	free((void*)m_rawBuffer);
	m_bLoaded = false;
	m_rawBuffer = nullptr;
	m_Title = nullptr;
	m_Author = nullptr;
	m_Ripper = nullptr;
	m_Converter = nullptr;
	m_sYear = nullptr;
	m_rawSize = 0;
	m_playerRate = 0;
	m_subSongCount = -1;
	m_defaultSongDurationInSec = kDefaultSongDuration;
}

uint16_t	SndhFile::Read16(const char* r)
{
	assert(m_rawBuffer);
	assert(r+2 <= (const char*)m_rawBuffer+m_rawSize);
	const uint8_t* r8 = (const uint8_t*)r;
	uint16_t v = (r8[0] << 8) | (r8[1]);
	return v;
}

uint32_t	SndhFile::Read32(const char* r)
{
	assert(m_rawBuffer);
	assert(r+4 <= (const char*)m_rawBuffer+m_rawSize);
	uint32_t v = (Read16(r) << 16) | Read16(r + 2);
	return v;
}

const char* SndhFile::skipNTString(const char* r)
{
	r += strlen(r) + 1;
	return r;
}

bool	SndhFile::Load(const void* rawSndhFile, int sndhFileSize, uint32_t hostReplayRate)
{

	Unload();
	m_hostReplayRate = hostReplayRate;
	bool ret = false;
	if (ice_24_header((unsigned char*)rawSndhFile))
	{
		m_rawSize = (int)ice_24_origsize((unsigned char*)rawSndhFile);
		m_rawBuffer = malloc(m_rawSize);
		long csize = ice_24_depack((unsigned char*)rawSndhFile, (unsigned char*)m_rawBuffer);
		if (m_rawSize != csize)
		{
			Unload();
			return false;
		}
	}
	else
	{
		m_rawSize = sndhFileSize;
		m_rawBuffer = malloc(m_rawSize);
		memcpy((void*)m_rawBuffer, rawSndhFile, sndhFileSize);
	}

	for (int i = 0; i < kSubsongCountMax; i++)
		m_subSongLenInTick[i] = 0;

	bool bFrms = false;
	const char* read8 = (const char*)m_rawBuffer;
	if (m_rawSize > 16)
	{
		if ((0x60 == read8[0]) && (0 == strncmp(read8 + 12, "SNDH", 4)))
		{
			int headerSize = Read16(read8 + 2) + 2; // suppose it's bra.w
			if (read8[1])
				headerSize = read8[1] + 2;			// but maybe it's bra.s
			const char* readEnd = read8 + headerSize;

			m_playerRate = 50;
			m_defaultSubSong = 1;
			m_subSongCount = 1;

			read8 += 16;
			while (read8 + 4 <= readEnd)
			{
				if (0 == strncmp(read8, "!#SN", 4))
				{
					assert(m_subSongCount > 0);
					read8 += 4 + m_subSongCount * 2;			// skip 2bytes per offset
				}
				if (0 == strncmp(read8, "!#", 2))
				{
					m_defaultSubSong = atoi(read8 + 2);
					read8 = skipNTString(read8+2);
				}
				else if (0 == strncmp(read8, "TITL", 4))
				{
					m_Title = read8 + 4;
					read8 = skipNTString(read8 + 4);
				}
				else if (0 == strncmp(read8, "COMM", 4))
				{
					m_Author = read8 + 4;
					read8 = skipNTString(read8 + 4);
				}
				else if (0 == strncmp(read8, "RIPP", 4))
				{
					m_Ripper = read8 + 4;
					read8 = skipNTString(read8 + 4);
				}
				else if (0 == strncmp(read8, "CONV", 4))
				{
					m_Converter = read8 + 4;
					read8 = skipNTString(read8 + 4);
				}
				else if ((0 == strncmp(read8, "YEAR", 4)))
				{
					if ( read8[4] != 0)
						m_sYear = read8 + 4;	// many sndh files have "" as year string
					read8 = skipNTString(read8 + 4);
				}
				else if (0 == strncmp(read8, "##", 2))
				{
					char stemp[3];
					memcpy(stemp, read8 + 2, 2);
					stemp[2] = 0;
					m_subSongCount = atoi(stemp);
					if ((m_subSongCount <= 0) || (m_subSongCount > kSubsongCountMax))	// some SNDH files have broken ## tag
						m_subSongCount = 1;
					read8 += 4;
				}
				else if (0 == strncmp(read8, "TIME", 4))
				{
					assert(m_subSongCount > 0);
					read8 += 4;
					if (uintptr_t(read8) & 1)
						read8++;
					for (int i = 0; i < m_subSongCount; i++)
					{
						int lenInSec = Read16(read8);
						assert(m_playerRate > 0);
						m_subSongLenInTick[i] = lenInSec * m_playerRate;
						read8 += 2;
					}
				}
				else if (0 == strncmp(read8, "FRMS", 4))
				{
					assert(m_subSongCount > 0);
					read8 += 4;
					for (int i = 0; i < m_subSongCount; i++)
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
					m_playerRate = atoi(read8 + 2);
					read8 = skipNTString(read8 + 2);
				}
				else
				{
					read8++;
				}
			}

			if ((m_defaultSubSong > m_subSongCount) ||
				(m_defaultSubSong < 1))
				m_defaultSubSong = 1;

			// if no new FRMS timing tag, try to search in timedb
			// (and eventually override any old TIME tag, that are often broken)
			if (!bFrms)
				timedbSearch(m_rawBuffer, m_rawSize, m_subSongLenInTick, kSubsongCountMax);

			ret = true;
		}
	}

	if (!ret)
		Unload();

	m_bLoaded = ret;
	return ret;
}

int	SndhFile::GetSubsongCount() const
{
	if (!m_bLoaded)
		return 0;
	return m_subSongCount;
}

bool	SndhFile::GetSubsongInfo(int subSongId, SubSongInfo& out) const
{
	if (!m_bLoaded)
		return false;
	if ((subSongId <= 0) || (subSongId > m_subSongCount))
		return false;

	out.playerTickCount = m_subSongLenInTick[subSongId - 1];
	if (out.playerTickCount <= 0)
		out.playerTickCount = m_defaultSongDurationInSec * m_playerRate;
	out.playerTickRate = m_playerRate;
	out.samplePerTick = m_hostReplayRate / m_playerRate;
	out.musicName = m_Title;
	out.musicAuthor = m_Author;
	out.ripper = m_Ripper;
	out.converter = m_Converter;
	out.year = m_sYear;

	out.subsongCount = m_subSongCount;
	return true;
}

bool	SndhFile::InitSubSong(int subSongId)
{
	bool ret = false;
	SubSongInfo info;
	if (!GetSubsongInfo(subSongId, info))
		return false;
	m_samplePerTick = m_hostReplayRate / m_playerRate;
	m_innerSamplePos = 0;
	m_frame = 0;
	m_frameCount = info.playerTickCount;
	m_atariMachine.Startup(m_hostReplayRate);
	if (m_atariMachine.Upload(m_rawBuffer, SNDH_UPLOAD_ADDR, m_rawSize))
	{
		ret = m_atariMachine.Jsr(SNDH_UPLOAD_ADDR, subSongId);
	}
	return ret;
}

int	SndhFile::AudioRenderInternal(int16_t* buffer, int count, uint32_t* pSampleViewInfo)
{
	int outsize = 0;
	if (m_frame < m_frameCount)
	{
		while (count > 0)
		{
			int todo = (m_innerSamplePos <= count) ? m_innerSamplePos : count;

			if (nullptr == pSampleViewInfo)
			{
				for (int s = 0; s < todo; s++)
					*buffer++ = m_atariMachine.ComputeNextSample();
			}
			else
			{
				for (int s = 0; s < todo; s++)
				{
					*buffer++ = m_atariMachine.ComputeNextSample();
					*pSampleViewInfo++ = m_atariMachine.ComputeCurrentVisualLevels();
				}
			}

			count -= todo;
			outsize += todo;
			m_innerSamplePos -= todo;

			if (m_innerSamplePos <= 0)
			{
				m_atariMachine.Jsr(SNDH_UPLOAD_ADDR + 8, 0);
				m_innerSamplePos = m_samplePerTick;
// rePlayer				m_frame++;
				if (m_frame >= m_frameCount)
					break;
			}
		}
	}
	return outsize;
}

int SndhFile::AudioRender(int16_t* buffer, int count)
{
	return AudioRenderInternal(buffer, count, nullptr);
}

int SndhFile::AudioRenderWithVisualInfos(int16_t* buffer, int count, uint32_t* pVisualSamples)
{
	return AudioRenderInternal(buffer, count, pVisualSamples);
}

int	SndhFile::AudioRenderStereo(int16_t* buffer, int count, uint32_t* pVisualSamples)
{
	int outsize = 0;
	if (m_frame < m_frameCount)
	{
		while (count > 0)
		{
			int todo = (m_innerSamplePos <= count) ? m_innerSamplePos : count;

			if (nullptr == pVisualSamples)
			{
				for (int s = 0; s < todo; s++)
					m_atariMachine.ComputeNextSample(buffer);
			}
			else
			{
				for (int s = 0; s < todo; s++)
				{
					m_atariMachine.ComputeNextSample(buffer);
					*pVisualSamples++ = m_atariMachine.ComputeCurrentVisualLevels();
				}
			}

			count -= todo;
			outsize += todo;
			m_innerSamplePos -= todo;

			if (m_innerSamplePos <= 0)
			{
				m_atariMachine.Jsr(SNDH_UPLOAD_ADDR + 8, 0);
				m_innerSamplePos = m_samplePerTick;
// rePlayer				m_frame++;
				if (m_frame >= m_frameCount)
					break;
			}
		}
	}
	return outsize;
}

int	SndhFile::AudioNull(int count)
{
	int outsize = 0;
	if (m_frame < m_frameCount)
	{
		while (count > 0)
		{
			int todo = (m_innerSamplePos <= count) ? m_innerSamplePos : count;

			for (int s = 0; s < todo; s++)
				m_atariMachine.ComputeNextSample();

			count -= todo;
			outsize += todo;
			m_innerSamplePos -= todo;

			if (m_innerSamplePos <= 0)
			{
				m_atariMachine.Jsr(SNDH_UPLOAD_ADDR + 8, 0);
				m_innerSamplePos = m_samplePerTick;
// rePlayer				m_frame++;
				if (m_frame >= m_frameCount)
					break;
			}
		}
	}
	return outsize;
}

int SndhFile::FastForward(int framesToSkip)
{
	if (framesToSkip <= 0)
		return 0;

	if (framesToSkip + m_frame >= m_frameCount)
		framesToSkip = m_frameCount - m_frame;

	for (int i = 0; i < framesToSkip; i++)
	{
		m_atariMachine.Jsr(SNDH_UPLOAD_ADDR + 8, 0);
		m_innerSamplePos = m_samplePerTick;
		m_frame++;
		assert(m_frame <= m_frameCount);
	}
	return framesToSkip;
}