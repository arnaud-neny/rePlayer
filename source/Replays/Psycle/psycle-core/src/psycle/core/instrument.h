// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

///\interface psycle::core::Instrument

#ifndef PSYCLE__CORE__INSTRUMENT__INCLUDED
#define PSYCLE__CORE__INSTRUMENT__INCLUDED
#pragma once

#include <psycle/core/detail/project.hpp>
#include <universalis/stdlib/cstdint.hpp>
#include <psycle/helpers/filter.hpp>
#include <string>

namespace psycle { namespace core {

class RiffFile;

/// an instrument is a waveform with some extra features added around it.
class PSYCLE__CORE__DECL Instrument {
	public:
		//PSYCLE__STRONG_TYPEDEF(int, id_type);
		typedef int id_type;

		Instrument();
		~Instrument();
		void Reset();
		void DeleteLayer(void);
		void LoadFileChunk(RiffFile* pFile,int version,bool fullopen=true);
		void SaveFileChunk(RiffFile* pFile);
		bool Empty();

		///\name Loop stuff
		///\{
			bool _loop;
			int32_t _lines;
		///\}

		///\verbatim
		/// NNA values overview:
		///
		/// 0 = Note Cut      [Fast Release 'Default']
		/// 1 = Note Release  [Release Stage]
		/// 2 = Note Continue [No NNA]
		///\endverbatim
		unsigned char _NNA;

		///\name Instrument assignation to a specific sampler
		///\{
		int _locked_machine_index; //-1 means not locked
		bool _locked_to_machine;
		///\}

		
		///\name Amplitude Envelope overview:
		///\{
			/// Attack Time [in Samples at 44.1Khz]
			int32_t ENV_AT;
			/// Decay Time [in Samples at 44.1Khz]
			int32_t ENV_DT;
			/// Sustain Level [in %]
			int32_t ENV_SL;
			/// Release Time [in Samples at 44.1Khz]
			int32_t ENV_RT;
		///\}
		
		///\name Filter 
		///\{
			/// Attack Time [in Samples at 44.1Khz]
			int32_t ENV_F_AT;
			/// Decay Time [in Samples at 44.1Khz]
			int32_t ENV_F_DT;
			/// Sustain Level [0..128]
			int32_t ENV_F_SL;
			/// Release Time [in Samples at 44.1Khz]
			int32_t ENV_F_RT;

			/// Cutoff Frequency [0-127]
			int32_t ENV_F_CO;
			/// Resonance [0-127]
			int32_t ENV_F_RQ;
			/// EnvAmount [-128,128]
			int32_t ENV_F_EA;
			/// Filter Type [0-4]
			helpers::dsp::FilterType ENV_F_TP;
		///\}

		int32_t _pan;
		bool _RPAN;
		bool _RCUT;
		bool _RRES;

		char _sName[32];

		///\name wave stuff
		///\{
			uint32_t waveLength;
			uint16_t waveVolume;
			uint32_t waveLoopStart;
			uint32_t waveLoopEnd;
			int32_t waveTune;
			int32_t waveFinetune;
			bool waveLoopType;
			bool waveStereo;
			char waveName[32];
			int16_t *waveDataL;
			int16_t *waveDataR;

			// xml copy paste methods
			std::string toXml() const;
			void setName( const std::string & name );
			void createWavHeader( const std::string & name, const std::string & header );
			void setCompressedData( unsigned char* left, unsigned char* right );

			void getData( unsigned char* data, const std::string & dataStr);
		///\}
};

/// Instrument index of the wave preview.
Instrument::id_type const PREV_WAV_INS(255);

}}
#endif
