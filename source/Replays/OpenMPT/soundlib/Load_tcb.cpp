/*
 * Load_tcb.cpp
 * ------------
 * Purpose: TCB Tracker loader
 * Notes  : Based on the manual scan available at https://files.scene.org/view/resources/gotpapers/manuals/tcb_tracker_1.0_manual_1990.pdf
 *          and a bit of messing about in TCB Tracker 1.0 and 1.1.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "Loaders.h"
#include "MODTools.h"

OPENMPT_NAMESPACE_BEGIN

struct TCBFileHeader
{
	char     magic[8];     // "AN COOL." (new) or "AN COOL!" (old - even TCB Tracker 1.0 cannot load these files)
	uint32be numPatterns;
	uint8    tempo;
	uint8    unused1;
	uint8    order[128];
	uint8    lastOrder;
	uint8    unused2;  // Supposed to be part of lastOrder but then it would have to be a little-endian word

	bool IsNewFormat() const
	{
		return magic[7] == '.';
	}

	bool IsValid() const
	{
		if(memcmp(magic, "AN COOL.", 8) && memcmp(magic, "AN COOL!", 8))
			return false;
		if(tempo > 15 || unused1 || lastOrder > 127 || unused2 || numPatterns > 128)
			return false;
		return true;
	}

	uint32 GetHeaderMinimumAdditionalSize() const
	{
		uint32 size = 16 * 8;  // Instrument names
		if(IsNewFormat())
			size += 2 + 32;  // Amiga flag + special values
		size += numPatterns * 512;
		return size;
	}
};

MPT_BINARY_STRUCT(TCBFileHeader, 144)


CSoundFile::ProbeResult CSoundFile::ProbeFileHeaderTCB(MemoryFileReader file, const uint64 *pfilesize)
{
	TCBFileHeader fileHeader;
	file.Rewind();
	if(!file.ReadStruct(fileHeader))
		return ProbeWantMoreData;
	if(!fileHeader.IsValid())
		return ProbeFailure;
	return ProbeAdditionalSize(file, pfilesize, fileHeader.GetHeaderMinimumAdditionalSize());
}


bool CSoundFile::ReadTCB(FileReader &file, ModLoadingFlags loadFlags)
{
	file.Rewind();
	TCBFileHeader fileHeader;
	if(!file.ReadStruct(fileHeader) || !fileHeader.IsValid())
		return false;
	if(!file.CanRead(fileHeader.GetHeaderMinimumAdditionalSize()))
		return false;
	if(loadFlags == onlyVerifyHeader)
		return true;

	InitializeGlobals(MOD_TYPE_MOD, 4);

	SetupMODPanning(true);
	Order().SetDefaultSpeed(16 - fileHeader.tempo);
	Order().SetDefaultTempoInt(125);
	ReadOrderFromArray(Order(), fileHeader.order, fileHeader.lastOrder + 1);
	m_nSamplePreAmp = 64;
	m_SongFlags.set(SONG_IMPORTED);
	m_playBehaviour.set(kApplyUpperPeriodLimit);

	const bool newFormat = fileHeader.IsNewFormat();
	bool useAmigaFreqs = false;
	std::array<char[8], 16> instrNames{};
	std::array<int16be, 16> specialValues{};
	if(newFormat)
	{
		uint16 amigaFreqs = file.ReadUint16BE();
		if(amigaFreqs > 1)
			return false;
		useAmigaFreqs = amigaFreqs != 0;
	}
	file.ReadStruct(instrNames);
	if(newFormat)
		file.ReadStruct(specialValues);

	m_nMinPeriod = useAmigaFreqs ? 113 * 4 : 92 * 4;
	m_nMaxPeriod = useAmigaFreqs ? 856 * 4 : 694 * 4;

	const PATTERNINDEX numPatterns = static_cast<PATTERNINDEX>(fileHeader.numPatterns);
	if(loadFlags & loadPatternData)
		Patterns.ResizeArray(numPatterns);
	const uint8 noteOffset = useAmigaFreqs ? 0 : 3;
	for(PATTERNINDEX pat = 0; pat < numPatterns; pat++)
	{
		if(!(loadFlags & loadPatternData) || !Patterns.Insert(pat, 64))
		{
			file.Skip(512);
			continue;
		}
		for(ModCommand &m : Patterns[pat])
		{
			const auto [note, instrEffect] = file.ReadArray<uint8, 2>();
			if(note >= 0x10 && note <= 0x3B)
			{
				m.note = static_cast<ModCommand::NOTE>(NOTE_MIDDLEC - 24 + (note >> 4) * 12 + (note & 0x0F) + noteOffset);
				m.instr = static_cast<ModCommand::INSTR>((instrEffect >> 4) + 1);
			} else if(note)
			{
				return false;
			}
			switch(instrEffect & 0x0F)
			{
			case 0x00:  // Nothing
			case 0x0E:  // Reserved
			case 0x0F:  // Reserved
				break;
			case 0x0B:  // Interrupt sample
			case 0x0C:  // Continue sample after interrupt
				m.SetVolumeCommand(VOLCMD_PLAYCONTROL, static_cast<ModCommand::VOL>(((specialValues[0x0B] == 2) ? 5 : 0) + ((instrEffect & 0x0F) - 0x0B)));
				break;
			case 0x0D:
				m.SetEffectCommand(CMD_PATTERNBREAK, 0);
				break;
			default:
				if(int value = specialValues[(instrEffect & 0x0F)]; value > 0)
					m.SetEffectCommand(CMD_PORTAMENTODOWN, mpt::saturate_cast<ModCommand::PARAM>((value + 16) / 32));
				else if(value < 0)
					m.SetEffectCommand(CMD_PORTAMENTOUP, mpt::saturate_cast<ModCommand::PARAM>((- value + 16) / 32));
			}
		}
	}

	const auto sampleStart = file.GetPosition();
	file.Skip(4);  // Size of remaining data
	
	FileReader sampleHeaders1 = file.ReadChunk(16 * 4);
	FileReader sampleHeaders2 = file.ReadChunk(16 * 8);
	SampleIO sampleIO(SampleIO::_8bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::unsignedPCM);
	m_nSamples = 16;
	for(SAMPLEINDEX smp = 1; smp <= 16; smp++)
	{
		ModSample &mptSample = Samples[smp];
		mptSample.Initialize(MOD_TYPE_MOD);
		mptSample.nVolume = std::min(sampleHeaders1.ReadUint8(), uint8(127)) * 2;
		sampleHeaders1.Skip(1);  // Empty value according to docs
		mptSample.nLoopStart = sampleHeaders1.ReadUint16BE();
		uint32 offset = sampleHeaders2.ReadUint32BE();
		mptSample.nLength = sampleHeaders2.ReadUint32BE();
		if(mptSample.nLoopStart && mptSample.nLoopStart < mptSample.nLength)
		{
			mptSample.nLoopEnd = mptSample.nLength;
			mptSample.nLoopStart = mptSample.nLength - mptSample.nLoopStart;
			mptSample.uFlags.set(CHN_LOOP);
		}
		if(!useAmigaFreqs)
			mptSample.nFineTune = 5 * 16;

		if((loadFlags & loadSampleData) && file.Seek(sampleStart + offset))
			sampleIO.ReadSample(mptSample, file);

		m_szNames[smp] = mpt::String::ReadBuf(mpt::String::spacePadded, instrNames[smp - 1]);
	}

	m_modFormat.formatName = newFormat ? UL_("TCB Tracker (New Format)") : UL_("TCB Tracker (Old Format)");
	m_modFormat.type = UL_("mod");
	m_modFormat.madeWithTracker = UL_("TCB Tracker");
	m_modFormat.charset = mpt::Charset::AtariST;

	return true;
}

OPENMPT_NAMESPACE_END