// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__PSY3FILTER__INCLUDED
#define PSYCLE__CORE__PSY3FILTER__INCLUDED
#pragma once

#include "psyfilterbase.h"

#include <vector>

namespace psycle { namespace core {

class Machine;
class RiffFile;
class PatternCategory;
class SequenceLine;
class PatternEvent;

class PSYCLE__CORE__DECL Psy3Filter : public PsyFilterBase
{
	protected:
		typedef enum MachineClass
		{
			MACH_MASTER = 0,
			MACH_SAMPLER = 3,
			MACH_PLUGIN = 8,
			MACH_VST = 9,
			MACH_VSTFX = 10,
			MACH_XMSAMPLER = 12,
			MACH_DUPLICATOR = 13,
			MACH_MIXER = 14,
			MACH_AUDIOINPUT = 15,
			MACH_DUMMY = 255
		} machineclass_t;

		class PatternEntry
		{
		public:
			inline PatternEntry()
				:
				_note(255),
				_inst(255),
				_mach(255),
				_cmd(0),
				_parameter(0)
			{}

			uint8_t _note;
			uint8_t _inst;
			uint8_t _mach;
			uint8_t _cmd;
			uint8_t _parameter;
		};

	///\name Singleton Pattern
	///\{
	protected:
		Psy3Filter();
	private:
		Psy3Filter(Psy3Filter const &);
		Psy3Filter& operator=(Psy3Filter const&);
	public:
		static Psy3Filter* getInstance();
	///\}

	protected:
		/*override*/ int version() const { return 3; }
		/*override*/ std::string filePostfix() const { return "psy"; }
		/*override*/ bool testFormat(const std::string & fileName);
		/*override*/ bool load(const std::string & fileName, CoreSong& song);
		/*override*/ bool save(const std::string & /*fileName*/, const CoreSong& /*song*/) {  /* so saving for legacy file format */ return false; }


	protected:
		virtual int LoadSONGv0(RiffFile* file,CoreSong& song);
		virtual bool LoadINFOv0(RiffFile* file,CoreSong& song,int minorversion);
		virtual bool LoadSNGIv0(RiffFile* file,CoreSong& song,int minorversion);
		virtual bool LoadSEQDv0(RiffFile* file,CoreSong& song,int minorversion);
		virtual bool LoadPATDv0(RiffFile* file,CoreSong& song,int minorversion);
		virtual Machine* LoadMACDv0(RiffFile* file,CoreSong& song,int minorversion);
		virtual bool LoadINSDv0(RiffFile* file,CoreSong& song,int minorversion);
		virtual bool LoadEINSv1(RiffFile* file,CoreSong& song,int minorversion, uint32_t size);

	protected:
		static std::string const FILE_FOURCC;
		static uint32_t const VERSION_INFO;
		static uint32_t const VERSION_SNGI;
		static uint32_t const VERSION_SEQD;
		static uint32_t const VERSION_PATD;
		static uint32_t const VERSION_MACD;
		static uint32_t const VERSION_INSD;
		static uint32_t const VERSION_WAVE;

		static uint32_t const FILE_VERSION;

	private:
		void RestoreMixerSendFlags(CoreSong& song);

		std::vector<int> seqList;
		PatternCategory* singleCat;
		SequenceLine* singleLine;

		//inter-loading value.
		int linesPerBeat;

		//todo: psy3filtermfc, restore these values.
		unsigned char octave;
		int seqBus;
		int instSelected;
		int midiSelected;
		int auxcolSelected;
		int machineSoloed;
		int trackSoloed;
};

}}
#endif
