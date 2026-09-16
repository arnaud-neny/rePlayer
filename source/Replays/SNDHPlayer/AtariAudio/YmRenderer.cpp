/*--------------------------------------------------------------------
	Atari Audio Library v1.24
	Small & accurate ATARI-ST audio emulation
	Arnaud Carré aka Leonard/Oxygene
	@leonard_coder
--------------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "YmRenderer.h"
#include "external/lzh.h"
#include "ym2data.h"

YmRenderer*	YmRenderer::Create(const void* ymMemoryData, uint32_t ymMemorySize, uint32_t hostReplayRate)
{
	YmRenderer* yr = new YmRenderer();
	if ( yr->Load(ymMemoryData, ymMemorySize, hostReplayRate ))
		return yr;
	delete yr;
	return nullptr;
}

YmRenderer::YmRenderer()
{
	m_dataStream = nullptr;
	m_dataStreamStride = 0;
}

YmRenderer::~YmRenderer()
{
}

uint16_t YmRenderer::StreamBE16(const char** r)
{
	const uint8_t* r8 = (const uint8_t*)*r;
	uint16_t v = (r8[0] << 8) | (r8[1]);
	*r = (const char*)(r8+2);
	return v;
}

uint32_t YmRenderer::StreamBE32(const char** r)
{
	uint32_t v = StreamBE16(r);
	v = (v<<16) | StreamBE16(r);
	return v;
}

void YmRenderer::ConvertTo4Bits(void)
{
	// MadMax 4bits table ripped from Wings Of Death original Atari replayer :)
	static const uint8_t sMadMax4BitsTable[16] = { 0x0, 0x7, 0x9, 0xa, 0xb, 0xc, 0xc, 0xd, 0xd, 0xd, 0xe, 0xe, 0xe, 0xf, 0xf, 0xf };
	for (int s = 0; s < m_sampleCount; s++)
	{
		YmSample& smp = m_samples[s];
		uint8_t* w8 = (uint8_t *)smp.data;	// we can overwrite, we're using our own copy of the data
		for (uint32_t i = 0; i < smp.len; i++)
		{
			uint8_t v = (smp.data[i])>>4;
			w8[i] = sMadMax4BitsTable[v&15];
		}
	}
}

bool YmRenderer::Load(const void* rawYmFile, uint32_t ymFileSize, uint32_t hostReplayRate)
{

	bool ret = false;
	m_innerSamplePos = 0;
	m_samplePerTick = 0;
	m_songDurationSample = 0;
	m_sampleCount = 0;
	m_tick = 0;

	SongInfo& si = m_songInfo;
	si.hostReplayRate = hostReplayRate;

	if (LzhDepacker::IsLzhPacked(rawYmFile, ymFileSize))
	{
		LzhDepacker lzd;
		m_songInfo.rawBinaryData = lzd.Unpack(rawYmFile, ymFileSize, m_songInfo.rawBinaryDataSize);
		if (nullptr == m_songInfo.rawBinaryData)
			return false;
	}
	else
	{
		si.rawBinaryDataSize = ymFileSize;
		si.rawBinaryData = malloc(si.rawBinaryDataSize);
		memcpy((void*)si.rawBinaryData, rawYmFile, ymFileSize);
	}

	uint32_t ymClock = Ym2149c::kDefaultAtariYmClock;

	const char* r8 = (const char*)si.rawBinaryData;
	const eYmType sign = eYmType(ReadBE32(r8));
	switch (sign)
	{
		case eYmType::eYM2a:
		case eYmType::eYM3a:
		case eYmType::eYM3b:
		{
			m_dataStreamStride = 14;
			m_flags = 1;		// interleaved
			m_subSongLenInTick[0] = (si.rawBinaryDataSize-4) / m_dataStreamStride; // -4 for header
			si.playerTickRate = 50;
			m_songLoopTick = 0;
			m_ymType = sign;
			m_dataStream = (const uint8_t *)r8 + 4;
			if (eYmType::eYM3b == m_ymType)
			{
				const uint32_t* pr = (const uint32_t *)(r8 + si.rawBinaryDataSize - 4);
				m_songLoopTick = *pr;
			}
			m_samplePerTick = si.hostReplayRate / si.playerTickRate;
			m_songDurationSample = m_subSongLenInTick[0] * m_samplePerTick;
			ret = true;
		}
		break;
		case eYmType::eYM5a://'YM5!':		// Extended YM2149 format, all machines.
		case eYmType::eYM6a://'YM6!':		// Extended YM2149 format, all machines.
		{
			if (0 == strncmp(r8 + 4, "LeOnArD!", 8))
			{
				r8 += 12;
				m_subSongLenInTick[0] = StreamBE32(&r8);
				m_flags = StreamBE32(&r8);
				m_sampleCount = StreamBE16(&r8);
				ymClock = StreamBE32(&r8);
				si.playerTickRate = StreamBE16(&r8);
				assert(si.playerTickRate > 0);
				m_songLoopTick = StreamBE32(&r8);
				int skip = StreamBE16(&r8);
				r8 += skip;
				if (m_sampleCount <= kYmMaxSamples)
				{
					if (m_sampleCount > 0)
					{
						for (int s = 0; s < m_sampleCount; s++)
						{
							YmSample& smp = m_samples[s];
							smp.len = StreamBE32(&r8);
							smp.data = (const uint8_t*)r8;
							r8 += smp.len;
						}
						if (0 == (m_flags&kYm4BitsSample))
							ConvertTo4Bits();
					}

					si.musicName = r8;
					r8 = SkipNTString(r8);
					si.musicAuthor = r8;
					r8 = SkipNTString(r8);
					si.converter = r8;
					r8 = SkipNTString(r8);

					m_dataStream = (const uint8_t *)r8;
					m_dataStreamStride = 16;

					m_ymType = sign;
					m_samplePerTick = si.hostReplayRate / si.playerTickRate;
					m_songDurationSample = m_subSongLenInTick[0] * m_samplePerTick;
					ret = true;
				}
			}
		}
		break;
		case eYmType::eMIX1:	// 'MIX1'
		{
			m_songInfo.playerTickRate = 50;
			r8 += 12;
			uint32_t tmp = StreamBE32(&r8);
			m_flags = 0;		// MIX score data is not interleaved
			if (tmp & 1)
				m_flags |= kYmSignedSample;

			si.playerTickRate = 50;

			StreamBE32(&r8);			// skip total sample bank size
			m_mixPatternCount = StreamBE32(&r8);
			m_dataStream = (const uint8_t *)r8;
			uint64_t duration = 0;
			for (int i = 0; i < m_mixPatternCount; i++)
			{
				StreamBE32(&r8);	// skip sample start
				uint32_t len = StreamBE32(&r8);
				uint32_t mixRepeat = StreamBE16(&r8);
				if (mixRepeat > 16)
					mixRepeat = 16;
				uint32_t replayRate = StreamBE16(&r8);
				assert(replayRate > 0);
				duration += (uint64_t(len * mixRepeat) * m_songInfo.hostReplayRate) / replayRate;
			}
			m_songDurationSample = uint32_t(duration);
			si.musicName = r8;
			r8 = SkipNTString(r8);
			si.musicAuthor = r8;
			r8 = SkipNTString(r8);
			si.converter = r8;
			r8 = SkipNTString(r8);
			m_mixBank = (const int8_t*)r8;	// mix bank is considered as signed
			m_mixFrac = 0;
			m_ymType = sign;
			m_mixPatternPos = -1;		// on purpose start at -1 so following FetchNext will fetch the first row
			FetchNextDigimixBlock();
			m_mixSamplePos = 0;
			m_samplePerTick = si.hostReplayRate / si.playerTickRate;
			ret = true;
		}
		break;
		case eYmType::eYMT1:
		case eYmType::eYMT2:
		{
			r8 += 12;
			m_ymType = sign;
			m_trkVoiceCount = StreamBE16(&r8);
			if (m_trkVoiceCount <= kYmMaxTrackerVoices)
			{
				si.playerTickRate = StreamBE16(&r8);
				m_subSongLenInTick[0] = StreamBE32(&r8);
				m_songLoopTick = StreamBE32(&r8);
				m_sampleCount = StreamBE16(&r8);
				m_flags = StreamBE32(&r8);
				si.musicName = r8;
				r8 = SkipNTString(r8);
				si.musicAuthor = r8;
				r8 = SkipNTString(r8);
				si.converter = r8;
				r8 = SkipNTString(r8);
				if (m_sampleCount <= kYmMaxSamples)
				{
					if (m_sampleCount > 0)
					{
						for (int s = 0; s < m_sampleCount; s++)
						{
							YmSample& smp = m_samples[s];
							smp.len = StreamBE16(&r8);
							smp.repPos = 0;
							if (eYmType::eYMT2 == m_ymType)
							{
								smp.repPos = smp.len - StreamBE16(&r8);
								if (smp.repPos >= smp.len)
									smp.repPos = 0;
								StreamBE16(&r8);	// skip useless "flags"
							}
							smp.data = (const uint8_t *)r8;
							r8 += smp.len;
						}
						assert(0 == (m_flags & kYm4BitsSample));		// YMT is not supposed to have 4bits samples input data
					}
					m_trkFreqShift = 0;
					if (eYmType::eYMT2 == m_ymType)
						m_trkFreqShift = (m_flags >> 28) & 15;

					m_dataStream = (const uint8_t *)r8;
					m_dataStreamStride = m_trkVoiceCount*4;	// 4 bytes per voice in YMT music score
					m_samplePerTick = si.hostReplayRate / si.playerTickRate;
					m_songDurationSample = m_subSongLenInTick[0] * m_samplePerTick;
					memset(m_trkVoices, 0, sizeof(m_trkVoices));
					ret = true;
				}
			}
		}
		break;
		default:
			break;
	}

	if (ret)
	{
		assert(ymClock > 0);
		assert(m_songInfo.playerTickRate > 0);

		if (m_songLoopTick >= m_subSongLenInTick[0])
			m_songLoopTick = 0;

		m_mixSignXor = (m_flags & kYmSignedSample) ? 0x00 : 0x80;
		si.ym2149Clock = ymClock;
		si.subsongCount = 1;
		si.defaultSubsong = 1;
		si.fileType = eFileType::eYm;
		si.fileFormat = GetFileFormatString();
	}

	return ret;
}

uint32_t YmRenderer::GetSubsongDurationSample(int subsongId) const
{
	if ((subsongId <= 0) || (subsongId > m_songInfo.subsongCount))
		return 0;

	assert(1 == subsongId);
	return m_songDurationSample;
}

bool YmRenderer::InitSubSong(int subSongId)
{
	bool ret = false;
	if ((subSongId >= 1) && (subSongId <= m_songInfo.subsongCount))
	{
		m_innerSamplePos = 0;
		m_tick = 0;

		m_ym2149.Reset(m_songInfo.hostReplayRate, m_songInfo.ym2149Clock);
		m_mfp.Reset(m_songInfo.hostReplayRate);
		MuteVoices(0);		// nothing is muted by default

		// enable timer A & B for potential 2 YM fx
		m_mfp.Write8(0x07, (1 << 5) | (1 << 0));
		m_mfp.Write8(0x13, (1 << 5) | (1 << 0));
		SetTimer(0, 0, 0);
		SetTimer(1, 0, 0);
		ret = true;
	}
	return ret;
}

uint32_t YmRenderer::ComputeCurrentVisualLevels()
{
	if ((eYmType::eMIX1 == m_ymType) ||
		(eYmType::eYMT1 == m_ymType) ||
		(eYmType::eYMT2 == m_ymType))
	{
		int8_t v = m_mixLastSample >> 1;
		return uint32_t(v)<<24;
	}
	return m_ym2149.ComputeCurrentVisualLevels();
}

Ym2149c::Levels YmRenderer::ComputeNextYmTrackerSample()
{
	int32_t out[3] = { 0 };
	for (int v = 0; v < m_trkVoiceCount; v++)
	{
		YmTrackerVoice& voice = m_trkVoices[v];
		if ( voice.running )
		{
			assert(voice.sampleId < uint32_t(m_sampleCount));
			const YmSample& smp = m_samples[voice.sampleId];
			int data = int(int8_t(smp.data[voice.samplePos] ^ 0x80));
			out[0] += (data * voice.volume)<<(6-6);	// 6 bits because of MUL volume
			out[1 + (v & 1)] += (data * voice.volume)<<(6-6);	// 6 bits because of MUL volume

			voice.innerClock += voice.replayRate;
			while (voice.innerClock >= m_songInfo.hostReplayRate)	// most of the time it won't loop, but some tunes could imply greater sampling rate than hostReplayRate!
			{
				voice.samplePos++;
				if (voice.samplePos >= smp.len)
				{
					voice.samplePos = smp.repPos;
					if (!voice.loop)
						voice.running = false;
				}
				voice.innerClock -= m_songInfo.hostReplayRate;
			}
		}
	}

	for (int i = 0; i < 3; ++i)
	{
		if (out[i] > 32767)
			out[i] = 32767;
		else if (out[i] < -32768)
			out[i] = -32768;
	}

	m_mixLastSample = int8_t((out[0] >> 8) & m_muteSteMask);

	return { .sLevels = { int16_t(out[0]) & m_muteSteMask, int16_t(out[1]) & m_muteSteMask, int16_t(out[2]) & m_muteSteMask } };
}

void YmRenderer::FetchNextDigimixBlock()
{
	m_mixPatternPos++;
	if (m_mixPatternPos >= m_mixPatternCount)
		m_mixPatternPos = 0;

	const char* r8 = (const char*)(m_dataStream + m_mixPatternPos * 12);
	m_mixBankOffset = ReadBE32(r8 + 0);
	m_mixSampleLen = ReadBE32(r8 + 4);
	m_mixCurrentRepeat = ReadBE16(r8 + 8);
	m_mixReplayRate = ReadBE16(r8 + 10);
}

int16_t YmRenderer::ComputeNextYmMixSample()
{

	m_mixLastSample = (m_mixBank[m_mixBankOffset + m_mixSamplePos] ^ m_mixSignXor);
	m_mixLastSample &= int8_t(m_muteSteMask);

	m_mixFrac += m_mixReplayRate;
	if (m_mixFrac >= m_songInfo.hostReplayRate)
	{
		m_mixSamplePos++;
		if (m_mixSamplePos >= m_mixSampleLen)
		{
			m_mixSamplePos = 0;
			m_mixCurrentRepeat--;
			if (m_mixCurrentRepeat <= 0)
				FetchNextDigimixBlock();
		}
		m_mixFrac -= m_songInfo.hostReplayRate;
	}
	return int16_t(m_mixLastSample) << 7;
}

Ym2149c::Levels YmRenderer::ComputeNextSample()
{
	Ym2149c::Levels out;
	if ((eYmType::eYMT1 == m_ymType) || (eYmType::eYMT2 == m_ymType))
	{
		out = ComputeNextYmTrackerSample();
	}
	else if (eYmType::eMIX1 == m_ymType)
	{
		out.sLevels[0] = out.sLevels[1] = out.sLevels[2] = ComputeNextYmMixSample();
	}
	else
	{
		out = m_ym2149.ComputeNextSample();

		// tick 2 Atari timers, maybe one of them is running
		for (int t = 0; t < 2; t++)
		{
			if (m_mfp.Tick(t))
			{
				YmFx& fx = m_ymFx[t];
				if (eYmFxType::eSid == fx.type)
				{
					fx.fxPhase++;
					const uint8_t r = (fx.fxPhase & 1)?fx.sidVol : 0;
					YmWrite(fx.ymVoice + 8, r);
				}
				else if (eYmFxType::eSyncBuzzer == fx.type)
				{
					YmWrite(13, fx.syncBuzzShape);
				}
				else if (eYmFxType::eDigidrum == fx.type)
				{
					if (fx.fxPhase < fx.sampleLen)
					{
						YmWrite(fx.ymVoice + 8, fx.sample[fx.fxPhase]);
						fx.fxPhase++;
					}
					else
					{
						// end digidrum, switch off
						SetTimer(t, 0, 0);
						fx.type = eYmFxType::eNone;
					}
				}
			}
		}
	}
	return out;
}

void YmRenderer::AudioRender(int16_t* buffer, uint32_t count)
{
	AudioRenderInternal(buffer, count, nullptr);
}

void YmRenderer::AudioRenderWithVisualInfos(int16_t* buffer, uint32_t sampleCount, uint32_t* pVisualSamples)
{
	AudioRenderInternal(buffer, sampleCount, pVisualSamples);
}

void YmRenderer::AudioRenderStereo(int16_t* buffer, uint32_t count, uint32_t* pSampleViewInfo)
{
	while (count > 0)
	{
		if (0 == m_innerSamplePos)
		{
			PlayerTick();
			m_innerSamplePos = m_samplePerTick;
		}

		uint32_t todo = count;
		todo = (m_innerSamplePos <= count) ? m_innerSamplePos : count;
		assert(m_innerSamplePos >= todo);

		if (buffer)
		{
			if (nullptr == pSampleViewInfo)
			{
				for (uint32_t s = 0; s < todo; s++)
				{
					auto l = ComputeNextSample();
					*buffer++ = l.sLeft;
					*buffer++ = l.sRight;
				}
			}
			else
			{
				for (uint32_t s = 0; s < todo; s++)
				{
					auto l = ComputeNextSample();
					*buffer++ = l.sLeft;
					*buffer++ = l.sRight;
					*pSampleViewInfo++ = ComputeCurrentVisualLevels();
				}
			}
		}
		else
		{
			// fast forward
			for (uint32_t s = 0; s < todo; s++)
				ComputeNextSample();
		}

		count -= todo;
		m_innerSamplePos -= todo;
	}
}

void YmRenderer::YmWrite(int reg, uint8_t d)
{
	m_ym2149.WritePort(0, reg);	// select reg
	m_ym2149.WritePort(2, d);	// write data
}

void YmRenderer::SetTimer(int slot, int prediv, int count)
{
	// drive Atari timer A or B for YM fx
	m_mfp.Write8((0 == slot) ? 0x19 : 0x1b, prediv);
	m_mfp.Write8((0 == slot) ? 0x1f : 0x21, count);
}

uint32_t YmRenderer::YmFxDecode(int fxSlot, int regCode, int regPrediv, int regCount)
{
	uint32_t skipMask = 0;

	YmFx& fx = m_ymFx[fxSlot];

	int code = ReadInterleaved(regCode)&0xf0;
	int prediv = (ReadInterleaved(regPrediv) >> 5) & 7;
	int count = ReadInterleaved(regCount);

	if (eYmType::eYM5a == m_ymType)
	{
		// ym5 fx, convert data into ym6 
		code &= 0x30;
		if (code)
		{
			static const uint8_t sFxCode[2] = {0x0, 0x40}; // ym5 SID & ym5 digidrum
			code |= sFxCode[fxSlot];
		}
	}

	if (code & 0x30)
	{
		fx.ymVoice = ((code&0x30)>>4)-1;
		switch (code & 0xc0)
		{
			case 0x00:		// SID
				fx.type = eYmFxType::eSid;
				fx.sidVol = ReadInterleaved(fx.ymVoice + 8) & 15;
				skipMask = 1 << (fx.ymVoice + 8);
				SetTimer(fxSlot, prediv, count);
				break;

			case 0x40:		// DigiDrum
				{
					int drumId = ReadInterleaved(fx.ymVoice + 8) & 31;
					if ((drumId >= 0) && (drumId < m_sampleCount))
					{
						fx.type = eYmFxType::eDigidrum;
						fx.sample = m_samples[drumId].data;
						fx.sampleLen = m_samples[drumId].len;
						skipMask = 1 << (fx.ymVoice + 8);
						fx.fxPhase = 0;
						SetTimer(fxSlot, prediv, count);
					}
				}
				break;

			case 0xc0:		// Sync-Buzzer.
				fx.type = eYmFxType::eSyncBuzzer;
				fx.syncBuzzShape = ReadInterleaved(fx.ymVoice + 8) & 15;
				skipMask = 1 << (fx.ymVoice + 8);
				SetTimer(fxSlot, prediv, count);
				break;

			default:
				assert(false);
				break;
		}
	}
	else
	{
		// no fx, if a fx was running, switch off timer (except for digidrum, they stop by themself)
		if (fx.type != eYmFxType::eDigidrum)
		{
			SetTimer(fxSlot, 0, 0);
			fx.type = eYmFxType::eNone;
		}
	}

	if (fx.type == eYmFxType::eDigidrum)
		skipMask |= 1 << (fx.ymVoice + 8);

	return skipMask;
}

uint8_t YmRenderer::ReadInterleaved(int reg) const
{
	if (m_flags&kYmInterleaved)
		return m_dataStream[m_subSongLenInTick[0]*reg + m_tick];	// stream interleaved

	return m_dataStream[m_tick * m_dataStreamStride + reg];
}

void YmRenderer::Ym2DriverTick()
{

	uint32_t skipMask = 0;
	uint8_t r13 = ReadInterleaved(13);
	if (r13 != 0xff)
	{
		YmWrite(11,ReadInterleaved(11));
		YmWrite(12,0);
		YmWrite(13, 10);
	}

	YmFx& fx = m_ymFx[0];
	uint8_t r10 = ReadInterleaved(10);
	if (r10 & 0x80)
	{
		r10 &= 0x7f;
		uint8_t r12 = ReadInterleaved(12);
		if ((r12) && (r10 < 40))
		{
			// digidrum
			fx.type = eYmFxType::eDigidrum;
			fx.fxPhase = 0;
			fx.sample = ym2Bank+ym2BankOffsets[r10];
			fx.sampleLen = ym2BankLens[r10];
			SetTimer(0, 1, r12);
			fx.ymVoice = 2;
		}
	}

	if (fx.type == eYmFxType::eDigidrum)
	{
		YmWrite(7, ReadInterleaved(7)|0x24);
		skipMask |= (1 << 7)|(1 << (fx.ymVoice + 8));
	}

	// send data to YM
	for (int r = 0; r <= 10; r++)
	{
		if (0 == (skipMask&(1 << r)))
			YmWrite(r, ReadInterleaved(r));
	}
}

void YmRenderer::Ym356DriverTick()
{
	uint32_t skipMask = 0;
	if ((eYmType::eYM5a == m_ymType) || (eYmType::eYM6a == m_ymType))
	{
		skipMask |= YmFxDecode(0, 1, 6, 14);
		skipMask |= YmFxDecode(1, 3, 8, 15);
	}

	// very specific to ym format: if digidrm is running, switch off noise+tone on the voice
	uint8_t r7 = ReadInterleaved(7);
	for (int fx = 0; fx < 2; fx++)
	{
		if (m_ymFx[fx].type == eYmFxType::eDigidrum)
			r7 |= ((1 << 0) | (1 << 3)) << m_ymFx[fx].ymVoice;
	}
	YmWrite(7, r7);
	skipMask |= (1 << 7);

	// some YM files have one frame delay between enabling env and setting env period.
	// Set a very long period to avoid high pich env a single player tick frame
	int envPer = (ReadInterleaved(12) << 8) | ReadInterleaved(11);
	if (0 == envPer)
	{
		YmWrite(11, 0xff);
		YmWrite(12, 0xff);
		skipMask |= (1 << 11) | (1 << 12);
	}

	// send data to YM
	for (int r = 0; r <= 12; r++)
	{
		if (0 == (skipMask&(1 << r)))
			YmWrite(r, ReadInterleaved(r));
	}
	uint8_t r13 = ReadInterleaved(13);
	if (r13 != 0xff)
		YmWrite(13, r13);
}

void YmRenderer::YmTrackerDriverTick()
{
	for (int v = 0; v < m_trkVoiceCount; v++)
	{
		YmTrackerVoice& voice = m_trkVoices[v];
		uint8_t b0 = ReadInterleaved(v * 4 + 0);
		uint8_t b1 = ReadInterleaved(v * 4 + 1);
		voice.replayRate = (uint16_t(ReadInterleaved(v * 4 + 2)) << 8) | ReadInterleaved(v * 4 + 3);
		if (voice.replayRate)
		{
			voice.replayRate = voice.replayRate << m_trkFreqShift;
			voice.loop = (b1 & 0x40) != 0;
			voice.volume = ((b1 & 63)*64)/63;	// convert to full range [0..64]
			if (0xff != b0)
			{
				// note on
				voice.sampleId = b0;
				voice.samplePos = 0;
				voice.running = true;
				voice.innerClock = 0;
			}
		}
		else
			voice.running = false;
	}
}

void YmRenderer::PlayerTick()
{

	switch (m_ymType)
	{
		case eYmType::eYM2a:
			Ym2DriverTick();
			break;
		case eYmType::eYM3a:
		case eYmType::eYM3b:
		case eYmType::eYM5a:
		case eYmType::eYM6a:
			Ym356DriverTick();
			break;
		case eYmType::eMIX1:	// on purpose no call, there is no player tick for YM "digimix"
			break;
		case eYmType::eYMT1:
		case eYmType::eYMT2:
			YmTrackerDriverTick();
		default:
			break;
	}

	// next ym music frame
	m_tick++;
	if (m_tick >= m_subSongLenInTick[0])
		m_tick = m_songLoopTick;
}

void	YmRenderer::AudioRenderInternal(int16_t* buffer, uint32_t count, uint32_t* pSampleViewInfo)
{
	while (count > 0)
	{
		if (0 == m_innerSamplePos)
		{
			PlayerTick();
			m_innerSamplePos = m_samplePerTick;
		}

		uint32_t todo = count;
		todo = (m_innerSamplePos <= count) ? m_innerSamplePos : count;
		assert(m_innerSamplePos >= todo);

		if (buffer)
		{
			if (nullptr == pSampleViewInfo)
			{
				for (uint32_t s = 0; s < todo; s++)
					*buffer++ = ComputeNextSample().sMono;
			}
			else
			{
				for (uint32_t s = 0; s < todo; s++)
				{
					*buffer++ = ComputeNextSample().sMono;
					*pSampleViewInfo++ = ComputeCurrentVisualLevels();
				}
			}
		}
		else
		{
			// fast forward
			for (uint32_t s = 0; s < todo; s++)
				ComputeNextSample();
		}

		count -= todo;
		m_innerSamplePos -= todo;
	}
}

void YmRenderer::MuteVoices(uint32_t muteVoiceMask)
{
	m_muteSteMask = (muteVoiceMask & (1<<3)) ? 0 : -1;
	m_ym2149.MuteVoices(muteVoiceMask);
}

const char* YmRenderer::GetFileFormatString() const
{
	switch (m_ymType)
	{
		case eYmType::eYM2a: return "YM 2";
		case eYmType::eYM3a: return "YM 3a";
		case eYmType::eYM3b: return "YM 3b";
		case eYmType::eYM4a: return "YM 4";
		case eYmType::eYM5a: return "YM 5";
		case eYmType::eYM6a: return "YM 6";
		case eYmType::eMIX1: return "YM Digimix";
		case eYmType::eYMT1: return "YM Tracker 1";
		case eYmType::eYMT2: return "YM Tracker 2";
		default:
			break;
	}
	return "";
}
