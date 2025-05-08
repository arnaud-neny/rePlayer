// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__INTERNAL_MACHINES__INCLUDED
#define PSYCLE__CORE__INTERNAL_MACHINES__INCLUDED
#pragma once

#include "machine.h"
#include "internalkeys.hpp"

namespace psycle { namespace core {

/****************************************************************************************************/
/// dummy machine.
class PSYCLE__CORE__DECL Dummy : public Machine {
	protected:
		Dummy(MachineCallbacks* callbacks, Machine::id_type index); friend class InternalHost;
	public:
		virtual ~Dummy() throw();
		/// Loader for psycle fileformat version 2.
		void CopyFrom(Machine* mac);
		virtual bool LoadSpecificChunk(RiffFile*,int version);
		virtual int GenerateAudio(int numSamples);
		virtual  const MachineKey& getMachineKey() const { return InternalKeys::dummy; }
		virtual std::string GetName() const { return _psName; }
		virtual bool isGenerator() const { return generator; }
		void setGenerator(bool gen) { generator = gen; }
	protected:
		static std::string _psName;
		bool generator;
};

/****************************************************************************************************/
/// note duplicator machine.
class PSYCLE__CORE__DECL DuplicatorMac : public Machine {
	protected:
		DuplicatorMac(MachineCallbacks* callbacks, Machine::id_type index); friend class InternalHost;
	public:
		virtual ~DuplicatorMac() throw();
		virtual void Init(void);
		virtual void Tick( int channel, const PatternEvent & pData );
		virtual void Stop();
		virtual void PreWork(int numSamples, bool clear = true);
		virtual int GenerateAudio( int numSamples );
		virtual  const MachineKey& getMachineKey() const { return InternalKeys::duplicator; }
		virtual std::string GetName() const { return _psName; }
		virtual void GetParamName(int numparam,char *name) const;
		virtual void GetParamRange(int numparam,int &minval,int &maxval) const;
		virtual void GetParamValue(int numparam,char *parVal) const;
		virtual int GetParamValue(int numparam) const;
		virtual bool SetParameter(int numparam,int value);
		virtual bool LoadSpecificChunk(RiffFile * pFile, int version);
		virtual void SaveSpecificChunk(RiffFile * pFile) const;

	protected:
		static const int NUM_MACHINES=8;
		void AllocateVoice(int channel, int machine);
		void DeallocateVoice(int channel, int machine);
		int16_t macOutput[NUM_MACHINES];
		int16_t noteOffset[NUM_MACHINES];
		static std::string _psName;
		bool bisTicking;
		// returns the allocated channel of the machine, for the channel (duplicator's channel) of this tick.
		int allocatedchans[MAX_TRACKS][NUM_MACHINES];
		// indicates if the channel of the specified machine is in use or not
		bool availablechans[MAX_MACHINES][MAX_TRACKS];
};

/****************************************************************************************************/
/// master machine.
class PSYCLE__CORE__DECL Master : public Machine {
	protected:
		Master(MachineCallbacks* callbacks, Machine::id_type index); friend class InternalHost;
	public:
		virtual ~Master() throw();
		virtual void Init(void);
		virtual void Stop();
		virtual void Tick(int channel, const PatternEvent & data );
		virtual int GenerateAudio( int numSamples );
		virtual const MachineKey& getMachineKey() const { return InternalKeys::master; }
		virtual std::string GetName() const { return _psName; }
		/// Loader for psycle fileformat version 2.
		virtual bool LoadPsy2FileFormat(RiffFile* pFile);
		virtual bool LoadSpecificChunk(RiffFile * pFile, int version);
		virtual void SaveSpecificChunk(RiffFile * pFile) const;

		/// this is for the VstHost
		double sampleCount;
		bool _clip;
		bool decreaseOnClip;
		float* _pMasterSamples;
		int peaktime;
		float currentpeak;
		float _lMax;
		float _rMax;
		int _outDry;
		bool vuupdated;
	protected:
		static std::string _psName;
};

class PSYCLE__CORE__DECL AudioRecorder : public Machine
{
	protected:
		AudioRecorder(MachineCallbacks* callbacks, Machine::id_type index); friend class InternalHost;
	public:
		virtual ~AudioRecorder() throw();
		virtual void Init(void);
		virtual bool LoadSpecificChunk(RiffFile * pFile, int version);
		virtual void SaveSpecificChunk(RiffFile * pFile) const;
        virtual void PreWork(int numSamples,bool /*clear*/) { Machine::PreWork(numSamples,false); }
		virtual int GenerateAudio(int numSamples);
		virtual const MachineKey& getMachineKey() const { return InternalKeys::audioinput; }
		virtual std::string GetName() const { return _psName; }

		virtual void ChangePort(int newport);
		virtual void setGainVol(float gainvol) { _gainvol = gainvol; }
		virtual float GainVol() const { return _gainvol; }
		virtual int CaptureIdx() const { return _captureidx; }
	
	private:
		static std::string _psName;

		int _captureidx;
		bool _initialized;
		float _gainvol;
		float* pleftorig;
		float* prightorig;
};

/****************************************************************************************************/
/// LFO machine
class PSYCLE__CORE__DECL LFO : public Machine {
	protected:
		LFO(MachineCallbacks* callbacks, Machine::id_type index); friend class InternalHost;
	public:
		virtual ~LFO() throw();
		virtual void Init(void);
		virtual void Tick( int channel, const PatternEvent & pData );
		virtual void PreWork(int numSamples, bool clear = true);
		virtual int GenerateAudio( int numSamples );
		virtual const MachineKey& getMachineKey() const { return InternalKeys::lfo; }
		virtual std::string GetName() const { return _psName; }
		virtual void GetParamName(int numparam,char *name) const;
		virtual void GetParamRange(int numparam,int &minval,int &maxval) const;
		virtual void GetParamValue(int numparam,char *parVal) const;
		virtual int GetParamValue(int numparam) const;
		virtual bool SetParameter(int numparam,int value);
		virtual bool LoadSpecificChunk(RiffFile * pFile, int version);
		virtual void SaveSpecificChunk(RiffFile * pFile) const;


		///\name constants
		///\{
			int const static LFO_SIZE = 2048;
			int const static MAX_PHASE = LFO_SIZE;
			int const static MAX_SPEED = 100000;
			int const static MAX_DEPTH = 100;
			int const static NUM_CHANS = 6;
		///\}

		struct lfo_types {
			enum lfo_type {
				sine, tri, saw, /*sawdown,*/ square, ///< depth ranges from -100% to 100% -- inverse saw is redundant
				num_lfos
			};
		};

		struct prms {
			/// our parameter indices
			enum prm {
				wave, speed,
				mac0,   mac1,   mac2,   mac3,   mac4,   mac5,
				prm0,   prm1,   prm2,   prm3,   prm4,   prm5,
				level0, level1, level2, level3, level4, level5,
				phase0, phase1, phase2, phase3, phase4, phase5,
				num_params
			};
		};

	protected:
		/// fills the lfo table based on the value of waveform
		virtual void FillTable();
		/// initializes data to start modulating the param given by 'which'
		virtual void ParamStart(int which);
		/// resets a parameter that's no longer being modulated
		virtual void ParamEnd(int which);

		// parameter settings
	
		int16_t waveform;
		int32_t lSpeed;
		int16_t macOutput[NUM_CHANS];
		int16_t paramOutput[NUM_CHANS];
		int32_t phase[NUM_CHANS];
		int32_t level[NUM_CHANS];

		// internal state vars
	
		/// position in our lfo
		float lfoPos;
		/// our lfo
		float waveTable[LFO_SIZE];
		/// value of knob when last seen-- used to compensate for outside changes
		int16_t prevVal[NUM_CHANS];
		/// where knob should be at lfo==0
		int32_t centerVal[NUM_CHANS];

		static std::string _psName;
		bool bisTicking;
};

}}
#endif
