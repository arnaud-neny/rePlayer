// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__XM_INSTRUMENT__INCLUDED
#define PSYCLE__CORE__XM_INSTRUMENT__INCLUDED
#pragma once

#include "fileio.h"
#include "constants.h"

#include <psycle/helpers/filter.hpp>

#include <utility>
#include <string>
#include <cstring>
#include <cassert>

namespace psycle { namespace core {

class PSYCLE__CORE__DECL XMInstrument {
public:
	/// Size of the Instrument's note mapping.
	static const int NOTE_MAP_SIZE = 120; // C-0 .. B-9
	/// A Note pair (note number=first, and sample number=second)
	typedef std::pair<unsigned char,unsigned char> NotePair;

	/// When a new note comes to play in a channel, and there is still one playing in it,
	/// do this on the currently playing note:
	struct NewNoteAction {
		enum Type {
			STOP = 0,///< [Note Cut] (This one actually does a very fast fadeout)
			CONTINUE = 1, ///< [Ignore]
			NOTEOFF = 2, ///< [Note off]
			FADEOUT = 3 ///< [Note fade]
		};
	};

	/// In some cases, the default NNA is not adequate. This option lets choose one type of element
	/// that, if it is equal than the currently playing, will apply the DCAction intead.
	/// A common example is using NNA NOTEOFF, DCT_NOTE and DCA_STOP
	struct DCType {
		enum Type {
			DCT_NONE = 0,
			DCT_NOTE,
			DCT_SAMPLE,
			DCT_INSTRUMENT
		};
	};
/*
	Using NewNoteAction instead so that we can convert easily from DCA to NNA.
	struct DCAction {
		enum Type {
			DCA_STOP = 0,
			DCA_NOTEOFF,
			DCA_FADEOUT
		};
	};
*/

//////////////////////////////////////////////////////////////////////////
//  XMInstrument::WaveData Class declaration

	class PSYCLE__CORE__DECL WaveData {
	public:
		static const uint32_t WAVEVERSION = 0x00000001;
		/// Wave Loop Types
		struct LoopType {
			enum Type {
				DO_NOT = 0, ///< Do Nothing
				NORMAL = 1, ///< normal Start --> End ,Start --> End ...
				BIDI = 2 ///< bidirectional Start --> End, End --> Start ...
			};
		};

		/// Wave Form Types
		struct WaveForms {
			enum Type {
				SINUS = 0,
				SQUARE = 1,
				SAWUP = 2,
				SAWDOWN = 3,
				RANDOM = 4
			};
		};

		/// Constructor
		WaveData() : m_WaveLength(0), m_pWaveDataL(0), m_pWaveDataR(0) {Init();}
		WaveData(const WaveData& data) {
			m_pWaveDataL=NULL;
			m_pWaveDataL=NULL;
			operator=(data);
		}
		/// Destructor
		virtual ~WaveData(){
			DeleteWaveData();
		}

		// Object Functions
		void Init();
		void DeleteWaveData();

		void AllocWaveData(const int iLen,const bool bStereo);
			void ConvertToMono();
			void ConvertToStereo();
			void InsertAt(uint32_t insertPos, const WaveData& wave);
			void ModifyAt(uint32_t modifyPos, const WaveData& wave);
			void DeleteAt(uint32_t deletePos, uint32_t length);
			void Mix(const WaveData& waveIn, float buf1Vol=1.0f, float buf2Vol=1.0f);
            void Fade(unsigned int fadeStart, unsigned int fadeEnd, float startVol, float endVol);
            void Amplify(unsigned int ampStart, unsigned int ampEnd, float vol);
            void Silence(unsigned int silStart, unsigned int silEnd);

		int Load(RiffFile& riffFile);
		void Save(RiffFile& riffFile) const;

		/// Wave Data Copy Operator
		WaveData& operator= (const WaveData& source)
		{
			Init();
			m_WaveName = source.m_WaveName;
			m_WaveLength = source.m_WaveLength;
			m_WaveGlobVolume = source.m_WaveGlobVolume;
			m_WaveDefVolume = source.m_WaveDefVolume;
			m_WaveLoopStart = source.m_WaveLoopStart;
			m_WaveLoopEnd = source.m_WaveLoopEnd;
			m_WaveLoopType = source.m_WaveLoopType;
			m_WaveSusLoopStart = source.m_WaveSusLoopStart;
			m_WaveSusLoopEnd = source.m_WaveSusLoopEnd;
			m_WaveSusLoopType = source.m_WaveSusLoopType;
			m_WaveSampleRate = source.m_WaveSampleRate;
			m_WaveTune = source.m_WaveTune;
			m_WaveFineTune = source.m_WaveFineTune;
			m_WaveStereo = source.m_WaveStereo;
			m_PanEnabled = source.m_PanEnabled;
			m_PanFactor = source.m_PanFactor;
			m_Surround = source.m_Surround;
			m_VibratoAttack = source.m_VibratoAttack;
			m_VibratoSpeed = source.m_VibratoSpeed;
			m_VibratoDepth = source.m_VibratoDepth;
			m_VibratoType = source.m_VibratoType;

			AllocWaveData(source.m_WaveLength,source.m_WaveStereo);

			std::memcpy(m_pWaveDataL, source.m_pWaveDataL, source.m_WaveLength * sizeof *m_pWaveDataL);
			if(source.m_WaveStereo)
				std::memcpy(m_pWaveDataR, source.m_pWaveDataR, source.m_WaveLength * sizeof *m_pWaveDataR);
			return *this;
		}


		// Properties

		const std::string & WaveName() const { return m_WaveName;}
		void WaveName(const std::string& newname){ m_WaveName = newname;}

		uint32_t WaveLength() const { return m_WaveLength;}

		float WaveGlobVolume() const { return m_WaveGlobVolume;}
		void WaveGlobVolume(const float value) {m_WaveGlobVolume = value;}
		uint16_t WaveVolume() const { return m_WaveDefVolume;}
		void WaveVolume(const uint16_t value){m_WaveDefVolume = value;}

		/// Default position for panning ( 0..1 ) 0left 1 right.
		float PanFactor() const { return m_PanFactor;}
		void PanFactor(const float value){m_PanFactor = value;}
		bool PanEnabled() const { return m_PanEnabled;}
		void PanEnabled(const bool pan){ m_PanEnabled=pan;}
		bool IsSurround() const { return m_Surround;}
		void IsSurround(const bool surround){ m_Surround=surround;}

		uint32_t WaveLoopStart() const { return m_WaveLoopStart;}
		void WaveLoopStart(const uint32_t value){m_WaveLoopStart = value;}
		uint32_t WaveLoopEnd() const { return m_WaveLoopEnd;}
		void WaveLoopEnd(const uint32_t value){m_WaveLoopEnd = value;}
		LoopType::Type WaveLoopType() const { return m_WaveLoopType;}
		void WaveLoopType(const LoopType::Type value){ m_WaveLoopType = value;}

		uint32_t WaveSusLoopStart() const { return m_WaveSusLoopStart;}
		void WaveSusLoopStart(const uint32_t value){m_WaveSusLoopStart = value;}
		uint32_t WaveSusLoopEnd() const { return m_WaveSusLoopEnd;}
		void WaveSusLoopEnd(const uint32_t value){m_WaveSusLoopEnd = value;}
		LoopType::Type WaveSusLoopType() const { return m_WaveSusLoopType;}
		void WaveSusLoopType(const LoopType::Type value){ m_WaveSusLoopType = value;}

		int16_t WaveTune() const {return m_WaveTune;}
		void WaveTune(const int16_t value){m_WaveTune = value;}
		int16_t WaveFineTune() const {return m_WaveFineTune;}
		void WaveFineTune(const int16_t value){m_WaveFineTune = value;}
		uint32_t WaveSampleRate() const {return m_WaveSampleRate;}
		void WaveSampleRate(const uint32_t value);

		bool IsWaveStereo() const { return m_WaveStereo;}
		void IsWaveStereo(const bool value){ m_WaveStereo = value;}

		uint8_t VibratoType() const {return m_VibratoType;}
		uint8_t VibratoSpeed() const {return m_VibratoSpeed;}
		uint8_t VibratoDepth() const {return m_VibratoDepth;}
		uint8_t VibratoAttack() const {return m_VibratoAttack;}

		void VibratoType(const uint8_t value){m_VibratoType = value ;}
		void VibratoSpeed(const uint8_t value){m_VibratoSpeed = value ;}
		void VibratoDepth(const uint8_t value){m_VibratoDepth = value ;}
		void VibratoAttack(const uint8_t value){m_VibratoAttack = value ;}

		bool IsAutoVibrato() const {return m_VibratoDepth && m_VibratoSpeed;}

        const int16_t * pWaveDataL() const { return m_pWaveDataL;}
        const int16_t * pWaveDataR() const { return m_pWaveDataR;}

	private:

		std::string m_WaveName;
		/// Wave length in Samples.
		uint32_t m_WaveLength;
		/// Difference between Glob volume and defVolume is that defVolume determines
		/// the volume if no volume is specified in the pattern, while globVolume is
		/// an attenuator for all notes of this sample.
		float m_WaveGlobVolume; // range ( 0..1 ) 
		uint16_t m_WaveDefVolume;
		uint32_t m_WaveLoopStart;
		uint32_t m_WaveLoopEnd;
		LoopType::Type m_WaveLoopType;
		uint32_t m_WaveSusLoopStart;
		uint32_t m_WaveSusLoopEnd;
		LoopType::Type m_WaveSusLoopType;
		uint32_t m_WaveSampleRate;
		int16_t m_WaveTune;
		/// [ -100 .. 100] full range = -/+ 1 seminote
		int16_t m_WaveFineTune;
		bool m_WaveStereo;
		int16_t *m_pWaveDataL;
		int16_t *m_pWaveDataR;
		bool m_PanEnabled;
		/// Default position for panning ( 0..1 ) 0left 1 right.
		float m_PanFactor;
		bool m_Surround;
		uint8_t m_VibratoAttack;
		uint8_t m_VibratoSpeed;
		uint8_t m_VibratoDepth;
		uint8_t m_VibratoType;

	};// WaveData


//////////////////////////////////////////////////////////////////////////
//  XMInstrument::Envelope Class declaration

	class PSYCLE__CORE__DECL Envelope {
	public:
		/// Invalid point. Used to indicate that sustain/normal loop is disabled.
		static const unsigned int INVALID = 0xFFFFFFFF;

		/// ValueType is a float value from  0 to 1.0  (or -1.0 1.0, or whatever else) which can be used as a multiplier.
		typedef float ValueType;

		/// The meaning of the first value (int), is time, and the unit depends on the context.
        typedef std::pair<unsigned int,ValueType> PointValue;

		/// ?
		typedef std::vector< PointValue > Points;

		/// constructor
		explicit Envelope() {
			Init();
		}

		/// copy Constructor
		Envelope(const Envelope& other)
		{
			operator=(other);
		}
		virtual ~Envelope(){}

		// Object Functions.
			void Init();

		/// Gets the time at which the pointIndex point is located.
        inline unsigned int GetTime(const unsigned int pointIndex) const
		{
            if(pointIndex < NumOfPoints()) return m_Points[pointIndex].first;
			return INVALID;
		}
		/// Sets a new time for an existing pointIndex point.
		inline int SetTime(const unsigned int pointIndex,const int pointTime)
		{
            assert(pointIndex < NumOfPoints());
			m_Points[pointIndex].first = pointTime;
			return SetTimeAndValue(pointIndex,pointTime,m_Points[pointIndex].second);
		}
		/// Gets the value of the pointIndex point.
		inline ValueType GetValue(const unsigned int pointIndex) const
		{
            assert(pointIndex < NumOfPoints());
			return m_Points[pointIndex].second;
		}
		/// Sets the value pointVal to pointIndex point.
		inline void SetValue(const unsigned int pointIndex,const ValueType pointVal)
		{
            assert(pointIndex < NumOfPoints());
			m_Points[pointIndex].second = pointVal;
		}
		/// Appends a new point at the end of the array.
		/// Note: Be sure that the pointTime is the highest of the points, or use "Insert" instead.
		void Append(const int pointTime,const ValueType pointVal)
		{
			PointValue _value;
			_value.first = pointTime;
			_value.second = pointVal;
			m_Points.push_back(_value);
		}

		/// Helper to set a new time for an existing index.
        int SetTimeAndValue(const unsigned int pointIndex,const unsigned int pointTime,const ValueType pointVal);

		/// Inserts a new point to the points Array.
        unsigned int Insert(const unsigned int pointIndex, const ValueType pointVal);

		/// Removes a point from the points Array.
		void Delete(const unsigned int pointIndex);

		/// Clears the points Array
		void Clear() { m_Points.clear(); }

		void Load(RiffFile& riffFile,const uint32_t version);
		void Save(RiffFile& riffFile,const uint32_t version) const;

		/// overloaded copy function
		Envelope& operator=(const Envelope& other)
		{
			if(this == &other) return *this;

			m_Enabled = other.m_Enabled;
			m_Carry = other.m_Carry;

			m_Points = other.m_Points;

			m_LoopStart = other.m_LoopStart;
			m_LoopEnd = other.m_LoopEnd;
			m_SustainBegin = other.m_SustainBegin;
			m_SustainEnd = other.m_SustainEnd;

			return *this;
		}

		// Properties
		/// Set or Get the point Index for Sustain and Loop.
        inline unsigned int SustainBegin() const { return m_SustainBegin;}
		/// value has to be an existing point!
		inline void SustainBegin(const unsigned int value){m_SustainBegin = value;}

        inline unsigned int SustainEnd() const { return m_SustainEnd;}
		/// value has to be an existing point!
		inline void SustainEnd(const unsigned int value){m_SustainEnd = value;}

        inline unsigned int LoopStart() const {return m_LoopStart;}
		/// value has to be an existing point!
		inline void LoopStart(const unsigned int value){m_LoopStart = value;}

        inline unsigned int LoopEnd() const {return m_LoopEnd;}
		/// value has to be an existing point!
		inline void LoopEnd(const unsigned int value){m_LoopEnd = value;}

		inline unsigned int NumOfPoints() const { return static_cast<unsigned int>(m_Points.size());}

		//// If the envelope IsEnabled, it is used and triggered. Else, it is not.
		inline bool IsEnabled() const { return m_Enabled;}
		inline void IsEnabled(const bool value){ m_Enabled = value;}

		/// if IsCarry() and a new note enters, the envelope position is set to
		/// that of the previous note *on the same channel*
		inline bool IsCarry() const { return m_Carry;}
		inline void IsCarry(const bool value){ m_Carry = value;}

	private:
		/// Envelope is enabled or disabled
		bool m_Enabled;
		/// ????
		bool m_Carry;
		/// Array of Points of the envelope.
		/// first : time at which to set the value. Unit can be different things depending on the context.
		/// second : 0 .. 1.0f . (or -1.0 1.0 or whatever else) Use it as a multiplier.
		Points m_Points;
		/// Loop Start Point
        unsigned int m_LoopStart;
		/// Loop End Point
        unsigned int m_LoopEnd;
		/// Sustain Start Point
        unsigned int m_SustainBegin;
		/// Sustain End Point
        unsigned int m_SustainEnd;
	};// class Envelope


//////////////////////////////////////////////////////////////////////////
//  XMInstrument Class declaration
	XMInstrument();
	virtual ~XMInstrument();

	void Init();
	void SetDefaultNoteMap();

	int Load(RiffFile& riffFile);
	void Save(RiffFile& riffFile) const;

	XMInstrument & operator= (const XMInstrument & other)
	{
		m_bEnabled = other.m_bEnabled;

		m_Name = other.m_Name;

		m_Lines = other.m_Lines;

		// Volume
		m_AmpEnvelope = other.m_AmpEnvelope;
		m_GlobVol = other.m_GlobVol;
		m_VolumeFadeSpeed = other.m_VolumeFadeSpeed;

		// Paninng
		m_PanEnvelope = other.m_PanEnvelope;
		m_InitPan = other.m_InitPan;
		m_Surround = other.m_Surround;
		m_PanEnabled=other.m_PanEnabled;
		m_NoteModPanCenter=other.m_NoteModPanCenter;
		m_NoteModPanSep=other.m_NoteModPanSep;

		// Pitch/Filter Envelope
		m_PitchEnvelope = other.m_PitchEnvelope;
		m_FilterEnvelope = other.m_FilterEnvelope;
		m_FilterCutoff = other.m_FilterCutoff;
		m_FilterResonance = other.m_FilterResonance;
		m_FilterEnvAmount = other.m_FilterEnvAmount;
		m_FilterType = other.m_FilterType;

		m_RandomVolume = other.m_RandomVolume;
		m_RandomPanning = other.m_RandomPanning;
		m_RandomCutoff = other.m_RandomCutoff;
		m_RandomResonance = other.m_RandomResonance;

		m_NNA = other.m_NNA;
		m_DCT = other.m_DCT;
		m_DCA = other.m_DCA;
		for(int i=0;i<NOTE_MAP_SIZE;i++)
		{
			m_AssignNoteToSample[i]=other.m_AssignNoteToSample[i];
		}
		return *this;
	}

	// Properties

	/// IsEnabled() is used on Saving, to not store unused instruments
	bool IsEnabled() const { return m_bEnabled;}
	void IsEnabled(const bool value){ m_bEnabled = value;}

	const std::string& Name() const {return m_Name;}
	void Name(const std::string& name) { m_Name= name; }

	uint16_t Lines() const { return m_Lines;}
	void Lines(const uint16_t value){ m_Lines = value;}

	Envelope& AmpEnvelope() { return m_AmpEnvelope;}
	Envelope& PanEnvelope() {return m_PanEnvelope;}
	Envelope& FilterEnvelope() { return m_FilterEnvelope;}
	Envelope& PitchEnvelope() {return m_PitchEnvelope;}

	const Envelope& AmpEnvelope() const { return m_AmpEnvelope;}
	const Envelope& PanEnvelope() const {return m_PanEnvelope;}
	const Envelope& FilterEnvelope() const { return m_FilterEnvelope;}
	const Envelope& PitchEnvelope() const {return m_PitchEnvelope;}

	float GlobVol() const { return m_GlobVol;}
	void GlobVol(const float value){m_GlobVol = value;}
	float VolumeFadeSpeed() const { return m_VolumeFadeSpeed;}
	void VolumeFadeSpeed(const float value){ m_VolumeFadeSpeed = value;}

	/// Default position for panning ( 0..1 ) 0left 1 right.
	float Pan() const { return m_InitPan;}
	void Pan(const float pan) { m_InitPan = pan;}
	bool PanEnabled() const { return m_PanEnabled;}
	void PanEnabled(const bool pan) { m_PanEnabled = pan;}
	bool IsSurround() const { return m_Surround;}
	void IsSurround(const bool surround) { m_Surround = surround;}
	uint8_t NoteModPanCenter() const { return m_NoteModPanCenter;}
	void NoteModPanCenter(const uint8_t pan) { m_NoteModPanCenter = pan;}
	int8_t NoteModPanSep() const { return m_NoteModPanSep;}
	void NoteModPanSep(const int8_t pan) { m_NoteModPanSep = pan;}

	uint8_t FilterCutoff() const { return m_FilterCutoff;}
	void FilterCutoff(const uint8_t value){m_FilterCutoff = value;}
	uint8_t FilterResonance() const { return m_FilterResonance;}
	void FilterResonance(const uint8_t value){m_FilterResonance = value;}
	int16_t FilterEnvAmount() const { return m_FilterEnvAmount;}
	void FilterEnvAmount(const int16_t value){ m_FilterEnvAmount = value;}
	psycle::helpers::dsp::FilterType FilterType() const { return m_FilterType;}
	void FilterType(const psycle::helpers::dsp::FilterType value){ m_FilterType = value;}

	float RandomVolume() const {return  m_RandomVolume;}
	void RandomVolume(const float value){m_RandomVolume = value;}
	float RandomPanning() const {return  m_RandomPanning;}
	void RandomPanning(const float value){m_RandomPanning = value;}
	float RandomCutoff() const {return m_RandomCutoff;}
	void RandomCutoff(const float value){m_RandomCutoff = value;}
	float RandomResonance() const {return m_RandomResonance;}
	void RandomResonance(const float value){m_RandomResonance = value;}

	NewNoteAction::Type NNA() const { return m_NNA;}
	void NNA(const NewNoteAction::Type value){ m_NNA = value;}
	DCType::Type DCT() const { return m_DCT;}
	void DCT(const DCType::Type value){ m_DCT = value;}
	NewNoteAction::Type DCA() const { return m_DCA;}
	void DCA(const NewNoteAction::Type value){ m_DCA = value;}

	const NotePair& NoteToSample(const int note) const {return m_AssignNoteToSample[note];}
	void NoteToSample(const int note,const NotePair npair){m_AssignNoteToSample[note] = npair;}

private:
	bool m_bEnabled;

	std::string m_Name;

	/// If m_Lines > 0 use samplelen/(tickduration*m_Lines) to determine the wave speed instead of the note.
	uint16_t m_Lines;

	/// envelope range = [0.0f..1.0f]
	Envelope m_AmpEnvelope;
	/// envelope range = [-1.0f..1.0f]
	Envelope m_PanEnvelope;
	/// envelope range = [-1.0f..1.0f]
	Envelope m_PitchEnvelope;
	/// envelope range = [0.0f..1.0f]
	Envelope m_FilterEnvelope;

	/// [0..1.0f] Global volume affecting all samples of the instrument.
	float m_GlobVol;
	/// [0..1.0f] Fadeout speed. Decreasing amount for each tracker tick.
	float m_VolumeFadeSpeed;

	// Paninng

	bool m_PanEnabled;
	/// Initial panFactor (if enabled) [-1..1]
	float m_InitPan;
	bool m_Surround;
	/// Note number for center pan position
	uint8_t m_NoteModPanCenter;
	/// -32..32. 1/256th of panFactor change per seminote.
	int8_t m_NoteModPanSep;

	/// Cutoff Frequency [0..127]
	uint8_t m_FilterCutoff;
	/// Resonance [0..127]
	uint8_t m_FilterResonance;
	/// EnvAmount [-128..128]
	int16_t m_FilterEnvAmount;
	/// Filter Type. See psycle::helpers::dsp::FilterType. [0..6]
	psycle::helpers::dsp::FilterType m_FilterType;

	// Randomness. Applies on new notes.

	/// Random Volume % [ 0.0 -> No randomize. 1.0 = randomize full scale.]
	float m_RandomVolume;
	/// Random Panning (same)
	float m_RandomPanning;
	/// Random CutOff (same)
	float m_RandomCutoff;
	/// Random Resonance (same)
	float m_RandomResonance;

	/// Action to take on the playing voice when any new note comes in the same channel.
	NewNoteAction::Type m_NNA;
	/// Check to do when a new event comes in the channel.
	DCType::Type m_DCT;
	/// Action to take on the playing voice when the action defined by m_DCT comes in the same channel 
	/// (like the same note value).
	NewNoteAction::Type m_DCA;

	/// Table of mapped notes to samples
	/// (note number=first, sample number=second)
	///\todo Could it be interesting to map other things like volume,panning, cutoff...?
	NotePair m_AssignNoteToSample[NOTE_MAP_SIZE];
};

class PSYCLE__CORE__DECL SampleList{
	public:
		SampleList(){m_waves.resize(0);}
		virtual ~SampleList(){Clear();}
		inline unsigned int AddSample(const XMInstrument::WaveData &wave)
		{
			XMInstrument::WaveData* wavecopy = new XMInstrument::WaveData(wave);
			m_waves.push_back(wavecopy);
			return size()-1;
		}
        inline void SetSample(const XMInstrument::WaveData &wave,unsigned int pos)
		{
			XMInstrument::WaveData* wavecopy = new XMInstrument::WaveData(wave);
			if (pos>=m_waves.size()) {
				size_t val = m_waves.size();
				m_waves.resize(pos+1);
				for (size_t i=val;i<=pos;i++) {
					m_waves[i]=NULL;
				}
			}
			else if(m_waves[pos] != NULL) { delete m_waves[pos]; }
			m_waves[pos]=wavecopy;
		}
        inline const XMInstrument::WaveData &operator[](unsigned int pos) const
		{
			assert(pos<m_waves.size());
			assert(m_waves[pos]!=NULL);
			return *m_waves[pos];
		}
        inline XMInstrument::WaveData &get(unsigned int pos)
		{
			assert(pos<m_waves.size());
			assert(m_waves[pos]!=NULL);
			return *m_waves[pos];
		}
        inline void RemoveAt(unsigned int pos)
		{
			if(pos < m_waves.size()) {
				if(m_waves[pos] != NULL) {delete m_waves[pos]; m_waves[pos]=NULL;}
                for(size_t i=m_waves.size()-1 ; m_waves[i]==NULL ; i--){
					m_waves.pop_back();
				}
			}
		}
		inline void ExchangeSamples(int pos1, int pos2)
		{
			XMInstrument::WaveData* wave = m_waves[pos1];
			m_waves[pos1]=m_waves[pos2];
			m_waves[pos2]=wave;
		}

        inline bool IsEnabled(unsigned int pos) const { return pos < m_waves.size() && m_waves[pos] != NULL && m_waves[pos]->WaveLength() > 0; }
		unsigned int size() const { return static_cast<unsigned int>(m_waves.size()); }
		void Clear() {
			const size_t val = m_waves.size();
			for (size_t i=0;i<val;i++) {
				if (m_waves[i] != NULL) {
					delete m_waves[i];
					m_waves[i]=NULL;
				}
			}
			m_waves.clear();
		}
	private:
		std::vector<XMInstrument::WaveData*> m_waves;
};

class PSYCLE__CORE__DECL InstrumentList {
	public:
		InstrumentList(){m_inst.resize(0);}
		virtual ~InstrumentList(){Clear();}
		inline unsigned int AddIns(const XMInstrument &ins)
		{
			XMInstrument* inscopy = new XMInstrument(ins);
			m_inst.push_back(inscopy);
			return size()-1;
		}
        inline void SetInst(const XMInstrument &inst,unsigned int pos)
		{
			XMInstrument* inscopy = new XMInstrument(inst);
			if (pos>=m_inst.size()) {
				size_t val = m_inst.size();
				m_inst.resize(pos+1);
				for (size_t i=val;i<pos;i++) {
					m_inst[i]=NULL;
				}
			}
			else if(m_inst[pos] != NULL) { delete m_inst[pos]; }
			m_inst[pos]=inscopy;
		}
        inline const XMInstrument &operator[](unsigned int pos) const {
			assert(pos<m_inst.size());
			assert(m_inst[pos]!=NULL);
			return *m_inst[pos];
		}
        inline XMInstrument &get(unsigned int pos) {
			assert(pos<m_inst.size());
			assert(m_inst[pos]!=NULL);
			return *m_inst[pos];
		}
        inline void RemoveAt(unsigned int pos)
		{
			if(pos < m_inst.size()) {
				if(m_inst[pos] != NULL) {delete m_inst[pos]; m_inst[pos]=NULL;}
                for(size_t i=m_inst.size()-1;m_inst[i]==NULL;i--){
					m_inst.pop_back();
				}
			}
		}
		inline void ExchangeInstruments(int pos1, int pos2)
		{
			XMInstrument* instr = m_inst[pos1];
			m_inst[pos1]=m_inst[pos2];
			m_inst[pos2]=instr;
		}
        inline bool IsEnabled(unsigned int pos) const { return pos < m_inst.size() && m_inst[pos] != NULL && m_inst[pos]->IsEnabled(); }
		inline unsigned int size() const { return static_cast<unsigned int>(m_inst.size()); }
		void Clear() { 
			const size_t val = m_inst.size();
            for (unsigned int i=0;i<val;i++) {
				if (m_inst[i] != NULL) {
					delete m_inst[i];
					m_inst[i]=NULL;
				}
			}
			m_inst.clear();
		}
	private:
		std::vector<XMInstrument*> m_inst;
};

}}
#endif
