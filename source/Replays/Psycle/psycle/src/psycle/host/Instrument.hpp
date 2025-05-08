///\file
///\brief interface file for psycle::host::Instrument.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include "FileIO.hpp"
#include <psycle/host/XMinstrument.hpp>
#include <psycle/helpers/filter.hpp>
namespace psycle
{
	namespace host
	{
		/// an instrument is a waveform with some extra features added around it.
		class Instrument
		{
		public:
			Instrument();
			virtual ~Instrument();
			void Init();
			void LoadFileChunk(RiffFile* pFile,int version,SampleList& samples, int sampleIdx, bool fullopen=true);
			void LoadWaveSubChunk(RiffFile* pFile,SampleList& samples, int instrIdx, int pan, char * instrum_name, bool fullopen, int loadIdx);
			void SaveFileChunk(RiffFile* pFile);
			void SetVolEnvFromEnvelope(const XMInstrument::Envelope& penv, float ticktomillis);
			void SetFilterEnvFromEnvelope(const XMInstrument::Envelope& penv, float ticktomillis);
			void CopyXMInstrument(const XMInstrument& inst, float ticktomillis);
		protected:
			void SetEnvInternal(const XMInstrument::Envelope& penvin, float ticktomillis, int &attack, int& decay, int& sustain, int& release);
		public:
			///\name Loop stuff
			///\{
			bool _loop;
			int _lines;
			///\}

			///\verbatim
			/// NNA values overview:
			///
			/// 0 = Note Cut      [Fast Release 'Default']
			/// 1 = Note Release  [Release Stage]
			/// 2 = Note Continue [No NNA]
			///\endverbatim
			unsigned char _NNA;


			int sampler_to_use; // Sampler machine index for lockinst.
			bool _LOCKINST;	// Force this instrument number to change the selected machine to use a specific sampler when editing (i.e. when using the pc or midi keyboards, not the notes already existing in a pattern)

			///\name Amplitude Envelope overview:
			///\{
			/// Attack Time [in Samples at 44.1Khz, independently of the real samplerate]
			int ENV_AT;	
			/// Decay Time [in Samples at 44.1Khz, independently of the real samplerate]
			int ENV_DT;	
			/// Sustain Level [in %]
			int ENV_SL;	
			/// Release Time [in Samples at 44.1Khz, independently of the real samplerate]
			int ENV_RT;	
			///\}
			
			///\name Filter 
			///\{
			/// Attack Time [in Samples at 44.1Khz]
			int ENV_F_AT;	
			/// Decay Time [in Samples at 44.1Khz]
			int ENV_F_DT;	
			/// Sustain Level [0..128]
			int ENV_F_SL;	
			/// Release Time [in Samples at 44.1Khz]
			int ENV_F_RT;	

			/// Cutoff Frequency [0-127]
			int ENV_F_CO;	
			/// Resonance [0-127]
			int ENV_F_RQ;	
			/// EnvAmount [-128,128]
			int ENV_F_EA;	
			/// Filter Type. See psycle::helpers::dsp::FilterType. [0..6]
			dsp::FilterType ENV_F_TP;	
			///\}

			bool _RPAN;
			bool _RCUT;
			bool _RRES;
		};
	}
}
