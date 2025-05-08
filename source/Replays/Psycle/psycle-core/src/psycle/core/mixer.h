// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__MIXER__INCLUDED
#define PSYCLE__CORE__MIXER__INCLUDED
#pragma once

#include "machine.h"
#include "internalkeys.hpp"

namespace psycle { namespace core {

//////////////////////////////////////////////////////////////////////////
/// mixer machine.
class PSYCLE__CORE__DECL Mixer : public Machine {
	public:
		class InputChannel
		{
			public:
				InputChannel(){ Init(); }
				InputChannel(int sends){ Init(); sends_.resize(sends); }
				InputChannel(const InputChannel &in) { Copy(in); }
				inline void Init()
				{
					if (sends_.size() != 0) sends_.resize(0);
					volume_=1.0f;
					panning_=0.5f;
					drymix_=1.0f;
					mute_=false;
					dryonly_=false;
					wetonly_=false;
				}
				InputChannel& operator=(const InputChannel &in) { Copy(in); return *this; }
				void Copy(const InputChannel &in)
				{
					sends_.clear();
					for(unsigned int i=0; i<in.sends_.size(); ++i)
					{
						sends_.push_back(in.sends_[i]);
					}
					volume_ = in.volume_;
					panning_ = in.panning_;
					drymix_ = in.drymix_;
					mute_ = in.mute_;
					dryonly_ = in.dryonly_;
					wetonly_ = in.wetonly_;
				}
				inline float &Send(int i) { return sends_[i]; }
				inline const float &Send(int i) const { return sends_[i]; }
				inline float &Volume() { return volume_; }
				inline const float &Volume() const { return volume_; }
				inline float &Panning() { return panning_; }
				inline const float &Panning() const { return panning_; }
				inline float &DryMix() { return drymix_; }
				inline const float &DryMix() const { return drymix_; }
				inline bool &Mute() { return mute_; }
				inline const bool &Mute() const { return mute_; }
				inline bool &DryOnly() { return dryonly_; }
				inline const bool &DryOnly() const { return dryonly_; }
				inline bool &WetOnly() { return wetonly_; }
				inline const bool &WetOnly() const { return wetonly_; }

				void AddSend() { sends_.push_back(0); }
				void ResizeTo(int sends) { sends_.resize(sends); }
				void ExchangeSends(int send1,int send2)
				{
					float tmp = sends_[send1];
					sends_[send1] = sends_[send2];
					sends_[send2] = tmp;
				}

			protected:
				std::vector<float> sends_;
				float volume_;
				float panning_;
				float drymix_;
				bool mute_;
				bool dryonly_;
				bool wetonly_;
		};

		class MixerWire
		{
			public:
				MixerWire():machine_(-1),volume_(1.0f),normalize_(1.0f) {}
				MixerWire(int mac,float norm):machine_(mac),volume_(1.0f),normalize_(norm){}
				bool IsValid() const { return (machine_!=-1); }

				int machine_;
				float volume_;
				float normalize_;
		};

		class ReturnChannel
		{
			public:
				ReturnChannel(){Init();}
				ReturnChannel(int sends) { Init(); sends_.resize(sends); }
				ReturnChannel(const ReturnChannel &in) { Copy(in); }
				void Init()
				{
					if (sends_.size()!=0) sends_.resize(0);
					mastersend_=true;
					volume_=1.0f;
					panning_=0.5f;
					mute_=false;
				}
				ReturnChannel& operator=(const ReturnChannel& in) { Copy(in); return *this; }
				void Copy(const ReturnChannel& in)
				{
					wire_ = in.wire_;
					sends_.clear();
					for(unsigned int i=0; i<in.sends_.size(); ++i)
					{
						sends_.push_back(in.sends_[i]);
					}
					mastersend_ = in.mastersend_;
					volume_ = in.volume_;
					panning_ = in.panning_;
					mute_ = in.mute_;
				}
				inline void Send(int i,bool value) { sends_[i]= value; }
                inline bool Send(int i) const { return sends_[i]; }
				inline bool &MasterSend() { return mastersend_; }
				inline const bool &MasterSend() const{ return mastersend_; }
				inline float &Volume() { return volume_; }
				inline const float &Volume() const { return volume_; }
				inline float &Panning() { return panning_; }
				inline const float &Panning() const { return panning_; }
				inline bool &Mute() { return mute_; }
				inline const bool &Mute() const { return mute_; }
				inline MixerWire &Wire() { return wire_; }
				inline const MixerWire &Wire() const { return wire_; }

				bool IsValid() const { return wire_.IsValid(); }

				void AddSend() { sends_.push_back(false); }
				void ResizeTo(int sends) { sends_.resize(sends); }
				void ExchangeSends(int send1,int send2)
				{
					bool tmp = sends_[send1];
					sends_[send1] = sends_[send2];
					sends_[send2] = tmp;
				}

			protected:
				MixerWire wire_;
				std::vector<bool> sends_;
				bool mastersend_;
				float volume_;
				float panning_;
				bool mute_;
		};

		class MasterChannel
		{
			public:
				MasterChannel(){Init();}
				inline void Init()
				{
					volume_=1.0f; drywetmix_=0.5f; gain_=1.0f;
				}
				inline float &Volume() { return volume_; }
				inline const float &Volume() const { return volume_; }
				inline float &DryWetMix() { return drywetmix_; }
				inline const float &DryWetMix() const { return drywetmix_; }
				inline float &Gain() { return gain_; }
				inline const float &Gain() const { return gain_; }

			protected:
				float volume_;
				float drywetmix_;
				float gain_;
		};
		enum
		{
			chan1 = 0,
			chanmax = chan1+MAX_CONNECTIONS,
			return1 = chanmax,
			returnmax = return1+MAX_CONNECTIONS,
			send1 = returnmax,
			sendmax = send1+MAX_CONNECTIONS
		};

	protected:
		Mixer(MachineCallbacks* callbacks, id_type index); friend class InternalHost;
	public:
		virtual ~Mixer() throw();
		virtual void Init(void);
		virtual void Tick( int channel, const PatternEvent & pData );

		virtual void recursive_process(unsigned int frames);

		void FxSend(int numSamples, bool recurse = true);
		void Mix(int numSamples);
		virtual void InsertInputWire(Machine& srcMac, Wire::id_type dstWire,InPort::id_type dstType, float initialVol=1.0f);
		virtual bool MoveWireSourceTo(Machine& srcMac, InPort::id_type dsttype, Wire::id_type dstwire, OutPort::id_type srctype = OutPort::id_type(0));
		virtual void GetWireVolume(Wire::id_type wireIndex, float &value) const { value = GetWireVolume(wireIndex); }
		virtual float GetWireVolume(Wire::id_type wireIndex) const ;
		virtual void SetWireVolume(Wire::id_type wireIndex,float value);
		virtual Wire::id_type FindInputWire(Machine::id_type macIndex) const;
		virtual Wire::id_type GetFreeInputWire(InPort::id_type slotType=InPort::id_type(0)) const;
		virtual void ExchangeInputWires(Wire::id_type first,Wire::id_type second, InPort::id_type firstType= InPort::id_type(0), InPort::id_type secondType = InPort::id_type(0));
		virtual void NotifyNewSendtoMixer(Machine& caller,Machine& senderMac);
		virtual void DeleteInputWire(Wire::id_type wireIndex, InPort::id_type dstType);
		virtual void DeleteWires(CoreSong& song);
		virtual std::string GetAudioInputName(Wire::id_type wire) const;
		virtual std::string GetPortInputName(InPort::id_type port) const;
		virtual unsigned int GetInPorts() const { return 2; }
		virtual int GetAudioInputs() const{ return GetInPorts() * MAX_CONNECTIONS; }
		virtual const MachineKey& getMachineKey() const { return InternalKeys::mixer; }
		virtual std::string GetName() const { return _psName; }
		virtual int GetNumCols() const;
		virtual void GetParamName(int numparam,char *name) const;
		virtual void GetParamRange(int numparam, int &minval, int &maxval) const;
		virtual void GetParamValue(int numparam,char *parVal) const;
		virtual int GetParamValue(int numparam) const;
		virtual bool SetParameter(int numparam,int value);
		virtual bool LoadSpecificChunk(RiffFile * pFile, int version);
		virtual void SaveSpecificChunk(RiffFile * pFile) const;

		bool GetSoloState(int column) const { return column==solocolumn_; }
		void SetSoloState(int column,bool solo)
		{
			if (solo ==false && column== solocolumn_)
				solocolumn_=-1;
			else solocolumn_=column;
		}
		virtual float VuChan(Wire::id_type idx) const;
		virtual float VuSend(Wire::id_type idx) const;

		inline InputChannel & Channel(int i) { return inputs_[i]; }
		inline const InputChannel & Channel(int i) const { return inputs_[i]; }
		inline ReturnChannel & Return(int i) { return returns_[i]; }
		inline const ReturnChannel & Return(int i) const { return returns_[i]; }
		inline MixerWire & Send(int i) { return sends_[i]; }
		inline const MixerWire & Send(int i) const { return sends_[i]; }
		inline MasterChannel & Master() { return master_; }
        inline unsigned int numinputs() const { return inputs_.size(); }
        inline unsigned int numreturns() const { return returns_.size(); }
        inline unsigned int numsends() const { return sends_.size(); }
        inline bool ChannelValid(unsigned int i) const { assert (i<MAX_CONNECTIONS); return (i<numinputs() && _inputCon[i]); }
        inline bool ReturnValid(unsigned int i) const { assert (i<MAX_CONNECTIONS); return (i<numreturns() && Return(i).IsValid()); }
        inline bool SendValid(unsigned int i) const { assert (i<MAX_CONNECTIONS); return (i<numsends() && sends_[i].IsValid()); }

        void InsertChannel(unsigned int idx, InputChannel*input=0);
        void InsertReturn(unsigned int idx,ReturnChannel* retchan=0);
        void InsertReturn(unsigned int idx,MixerWire rwire)
		{
			InsertReturn(idx);
			Return(idx).Wire()=rwire;
		}
        void InsertSend(unsigned int idx, MixerWire swire);
        void DiscardChannel(unsigned int idx);
        void DiscardReturn(unsigned int idx);
        void DiscardSend(unsigned int idx);

		void ExchangeChans(int chann1,int chann2);
		void ExchangeReturns(int chann1,int chann2);
		void ExchangeSends(int send1,int send2);
		void ResizeTo(int inputs, int sends);

		void RecalcMaster();
		void RecalcReturn(int idx);
		void RecalcChannel(int idx);
		void RecalcSend(int chan,int send);

	protected:
		static std::string _psName;
		bool mixed;

		int solocolumn_;
		std::vector<InputChannel> inputs_;
		std::vector<ReturnChannel> returns_;
		std::vector<MixerWire> sends_;
		MasterChannel master_;

		// Arrays of precalculated volume values for the FxSend and Mix functions
		float _sendvolpl[MAX_CONNECTIONS][MAX_CONNECTIONS];
		float _sendvolpr[MAX_CONNECTIONS][MAX_CONNECTIONS];
		float mixvolpl[MAX_CONNECTIONS];
		float mixvolpr[MAX_CONNECTIONS];
		float mixvolretpl[MAX_CONNECTIONS][MAX_CONNECTIONS+1]; // +1 for master
		float mixvolretpr[MAX_CONNECTIONS][MAX_CONNECTIONS+1];

	///\name used by the multi-threaded scheduler
	///\{
		protected:
			/// tells the scheduler which machines to process before this one
			/*override*/ void sched_inputs(sched_deps&) const;
			/// tells the scheduler which machines may be processed after this one
			/*override*/ void sched_outputs(sched_deps&) const;
			/// called by the scheduler to ask for the actual processing of the machine
			/*override*/ bool sched_process(unsigned int frames);
	///\}
};

/// tweaks:
/// [0x]:
///  0 -> Master volume
///  1..C -> Input volumes
///  D -> master drywetmix.
///  E -> master gain.
///  F -> master pan.
/// [1x..Cx]:
///  0 -> input wet mix.
///  1..C -> input send amout to the send x.
///  D -> input drywetmix. ( 0 normal, 1 dryonly, 2 wetonly  3 mute)
///  E -> input gain.
///  F -> input panning.
/// [Dx]:
///  0 -> Solo channel.
///  1..C -> return grid. // the return grid array grid represents: bit0 -> mute, bit 1..12 routing to send. bit 13 -> route to master
/// [Ex]: 
///  1..C -> return volumes
/// [Fx]:
///  1..C -> return panning

}}
#endif
