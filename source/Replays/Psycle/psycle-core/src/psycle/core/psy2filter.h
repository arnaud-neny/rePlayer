// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__PSY2FILTER__INCLUDED
#define PSYCLE__CORE__PSY2FILTER__INCLUDED
#pragma once

#include "psyfilterbase.h"

#include <vector>

namespace psycle { namespace core {

class RiffFile;
class PatternCategory;
class SequenceLine;
class Machine;
class PatternEvent;

namespace convert_internal_machines {
	class Converter;
}

class PSYCLE__CORE__DECL Psy2Filter : public PsyFilterBase {
	protected:
		//Note: convert_internal_machines uses its own enum.
		enum machineclass_t {
			MACH_MASTER = 0,
			MACH_SINE = 1, 
			MACH_DIST = 2,
			MACH_SAMPLER = 3,
			MACH_DELAY = 4,
			MACH_2PFILTER = 5,
			MACH_GAIN = 6, 
			MACH_FLANGER = 7, 
			MACH_PLUGIN = 8,
			MACH_VST = 9,
			MACH_VSTFX = 10,
			MACH_SCOPE = 11,
			MACH_DUMMY = 255
		};

		class VSTLoader {
			public:
				bool valid;
				char dllName[128];
				int numpars;
				float * pars;
		};

		class PatternEntry {
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
			Psy2Filter();
		private:
			Psy2Filter( Psy2Filter const & );
			Psy2Filter& operator=(Psy2Filter const &);
		public:
			static Psy2Filter* getInstance();
	///\}

	public:
		/*override*/ int version() const { return 2; }
		/*override*/ std::string filePostfix() const { return "psy"; }
		/*override*/ bool testFormat(const std::string & fileName);
		/*override*/ bool load(const std::string & fileName, CoreSong& song);
		/*override*/ bool save(const std::string & /*fileName*/, const CoreSong& /*song*/) {  /* so saving for legacy file format */ return false; }


	protected:
		virtual bool LoadINFO(RiffFile* file,CoreSong& song);
		virtual bool LoadSNGI(RiffFile* file,CoreSong& song);
		virtual bool LoadSEQD(RiffFile* file,CoreSong& song);
		virtual bool LoadPATD(RiffFile* file,CoreSong& song,int index);
		virtual bool LoadINSD(RiffFile* file,CoreSong& song);
		virtual bool LoadWAVD(RiffFile* file,CoreSong& song);
		virtual bool PreLoadVSTs(RiffFile* file,CoreSong& song);
		virtual bool LoadMACD(RiffFile* file,CoreSong& song,convert_internal_machines::Converter& converter);
		virtual bool TidyUp(RiffFile* file,CoreSong& song,convert_internal_machines::Converter& converter);

	protected:
		static std::string const FILE_FOURCC;
		/// PSY2-fileformat Constants
		static int const PSY2_MAX_TRACKS;
		static int const PSY2_MAX_WAVES;
		static int const PSY2_MAX_INSTRUMENTS;
		static int const PSY2_MAX_PLUGINS;

	private:
		std::vector<int> seqList;
		PatternCategory* singleCat;
		SequenceLine* singleLine;
		Machine* pMac[128];
		bool _machineActive[128];
		unsigned char busMachine[64];
		unsigned char busEffect[64];
		VSTLoader vstL[256];
		float volMatrix[128][12];

		//inter-loading value
		int linesPerBeat;

		//todo: psy2filtermfc, restore these two values.
		unsigned char octave;
		int instSelected; //-> map to auxcolselected

		void prepareSequence(CoreSong& song);
		PatternEvent convertEntry( unsigned char * data ) const;
};

}}
#endif
