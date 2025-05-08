#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include "Machine.hpp"
namespace psycle
{
	namespace host
	{    
//////////////////////////////////////////////////////////////////////////
/// dummy machine.
class Dummy : public Machine
{
public:
	Dummy(int index);
	Dummy(Machine *mac);
	virtual int GenerateAudio(int numSamples, bool measure_cpu_usage);
	virtual float GetAudioRange() const { return 32768.0f; }
	virtual const char* const GetName(void) const { return _psName; }
	virtual bool LoadSpecificChunk(RiffFile* pFile, int version);
	virtual const char * const GetDllName() const throw() { return dllName.c_str(); }

	/// Marks that the Dummy was in fact a VST plugin that couldn't be loaded
	bool wasVST;
	/// Indicates the dll name of the plugin that could not be loaded
	std::string dllName;
protected:
	static char * _psName;
};
//////////////////////////////////////////////////////////////////////////
/// Duplicator machine.
class MultiMachine: public Machine
{
public:
	MultiMachine();
	MultiMachine(int index, int nums);
	virtual ~MultiMachine();
	virtual void Init();
	virtual void Tick( int channel,PatternEntry* pData);
	virtual void Stop();

protected:
	int NUMMACHINES;
	void AllocateVoice(int channel, int machine);
	void DeallocateVoice(int channel, int machine);
	virtual void CustomTick(int channel,int i, PatternEntry& pData) = 0;
	virtual bool playsTrack(const int track) const;
	std::vector<short> macOutput;
	std::vector<short> noteOffset;
	bool bisTicking;
	// returns the allocated channel of the machine, for the channel (duplicator's channel) of this tick.
	int allocatedchans[MAX_TRACKS][MAX_VIRTUALINSTS]; //Not Using MAX_MACHINES because now there are the Virtual instruments
	// indicates if the channel of the specified machine is in use or not
	bool availablechans[MAX_VIRTUALINSTS][MAX_TRACKS];//Not Using MAX_MACHINES because now there are the Virtual instruments
};

class DuplicatorMac : public MultiMachine
{
public:
	DuplicatorMac();
	DuplicatorMac(int index);
	virtual void Init(void);
	virtual void NewLine();
	virtual void CustomTick(int channel,int i, PatternEntry& pData);
	virtual int GenerateAudio(int numSamples, bool measure_cpu_usage);
	virtual bool playsTrack(const int track) const;
	virtual float GetAudioRange() const { return 32768.0f; }
	virtual const char* const GetName(void) const { return _psName; }
	virtual int GetParamType(int numparam) { return 2; }
	virtual void GetParamName(int numparam,char *name);
	virtual void GetParamRange(int numparam, int &minval, int &maxval);
	virtual void GetParamValue(int numparam,char *parVal);
	virtual int GetParamValue(int numparam);
	virtual bool SetParameter(int numparam,int value);
	virtual bool LoadSpecificChunk(RiffFile * pFile, int version);
	virtual void SaveSpecificChunk(RiffFile * pFile);

protected:
	static char* _psName;
};
//////////////////////////////////////////////////////////////////////////
/// Duplicator machine.
class DuplicatorMac2 : public MultiMachine
{
public:
	DuplicatorMac2();
	DuplicatorMac2(int index);
	virtual void CustomTick(int channel,int i, PatternEntry& pData);
	virtual int GenerateAudio(int numSamples, bool measure_cpu_usage);
	virtual bool playsTrack(const int track) const;
	virtual float GetAudioRange() const { return 32768.0f; }
	virtual const char* const GetName(void) const { return _psName; }
	virtual int GetParamType(int numparam) { return 2; }
	virtual void GetParamName(int numparam,char *name);
	virtual void GetParamRange(int numparam, int &minval, int &maxval);
	virtual void GetParamValue(int numparam,char *parVal);
	virtual int GetParamValue(int numparam);
	virtual bool SetParameter(int numparam,int value);
	virtual bool LoadSpecificChunk(RiffFile * pFile, int version);
	virtual void SaveSpecificChunk(RiffFile * pFile);
	virtual void Tick(int channel,PatternEntry* pData);

protected:
	std::vector<short> lowKey;
	std::vector<short> highKey;	
	static char* _psName;
};

/*
//////////////////////////////////////////////////////////////////////////
/// Drum Matrix Machine 
class DrumsMatrix : public MultiMachine
{
public:
	DrumsMatrix();
	DrumsMatrix(int index);
	virtual void Init(void);
	virtual void NewLine();
	virtual void Tick( int channel,PatternEntry* pData);
	virtual void Stop();
	virtual void Work(int numSamples);
	virtual float GetAudioRange() const { return 32768.0f; }
	virtual const char* const GetName(void) const { return _psName; }
	virtual int GetParamType(int numparam) { return 2; }
	virtual void GetParamName(int numparam,char *name);
	virtual void GetParamRange(int numparam, int &minval, int &maxval);
	virtual void GetParamValue(int numparam,char *parVal);
	virtual int GetParamValue(int numparam);
	virtual bool SetParameter(int numparam,int value);
	virtual bool LoadSpecificChunk(RiffFile * pFile, int version);
	virtual void SaveSpecificChunk(RiffFile * pFile);

protected:
	static char* _psName;
};
*/
//////////////////////////////////////////////////////////////////////////
/// Audio Recorder
class AudioRecorder : public Machine
{
public:
	AudioRecorder();
	AudioRecorder(int index);
	virtual ~AudioRecorder();
	virtual void PreWork(int numSamples,bool clear, bool measure_cpu_usage) { Machine::PreWork(numSamples,false,measure_cpu_usage); }
	virtual void Init(void);
	virtual int GenerateAudio(int numSamples, bool measure_cpu_usage);
	virtual float GetAudioRange() const { return 32768.0f; }
	virtual const char* const GetName(void) const { return _psName; }
	virtual bool LoadSpecificChunk(RiffFile * pFile, int version);
	virtual void SaveSpecificChunk(RiffFile * pFile);

	virtual void ChangePort(int newport);

	static char* _psName;

	char drivername[32];
	int _captureidx;
	bool _initialized;
	float _gainvol;
	float* pleftorig;
	float* prightorig;
	
};

//////////////////////////////////////////////////////////////////////////
/// mixer machine.
class Mixer : public Machine
{
public:
	class InputChannel
	{
	public:
		InputChannel(){ Init(); }
		InputChannel(int sends){ Init(); sendVols_.resize(sends); }
		InputChannel(const InputChannel &in) { Copy(in); }
		inline void Init()
		{
			if (sendVols_.size() != 0) sendVols_.resize(0);
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
			sendVols_.clear();
			for(unsigned int i=0; i<in.sendVols_.size(); ++i)
			{
				sendVols_.push_back(in.sendVols_[i]);
			}
			volume_ = in.volume_;
			panning_ = in.panning_;
			drymix_ = in.drymix_;
			mute_ = in.mute_;
			dryonly_ = in.dryonly_;
			wetonly_ = in.wetonly_;
		}
		inline float &SendVol(int i) { return sendVols_[i]; }
		inline const float &SendVol(int i) const { return sendVols_[i]; }
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

		void ResizeTo(int sends) { sendVols_.resize(sends); }
		void ExchangeSends(int send1,int send2)
		{
			float tmp = sendVols_[send1];
			sendVols_[send1] = sendVols_[send2];
			sendVols_[send2] = tmp;
		}

	protected:
		std::vector<float> sendVols_;
		float volume_;
		float panning_;
		float drymix_;
		bool mute_;
		bool dryonly_;
		bool wetonly_;
	};

	class ReturnChannel
	{
	public:
		ReturnChannel():wire_(NULL) {Init();}
		ReturnChannel(const ReturnChannel &in):wire_(in.wire_) { Copy(in); }
		ReturnChannel(Machine * const dstMac):wire_(dstMac) {Init();}
		ReturnChannel(Machine * const dstMac, int sends):wire_(dstMac) { Init(); sendsTo_.resize(sends); }
		void Init()
		{
			if (sendsTo_.size()!=0) sendsTo_.resize(0);
			mastersend_=true;
			volume_=1.0f;
			panning_=0.5f;
			mute_=false;
		}
		ReturnChannel& operator=(const ReturnChannel& in) { Copy(in); return *this; }
		void Copy(const ReturnChannel& in)
		{
			wire_ = in.wire_;
			sendsTo_.clear();
			for(unsigned int i=0; i<in.sendsTo_.size(); ++i)
			{
				sendsTo_.push_back(in.sendsTo_[i]);
			}
			mastersend_ = in.mastersend_;
			volume_ = in.volume_;
			panning_ = in.panning_;
			mute_ = in.mute_;
		}
		inline void SendsTo(int i,bool value) { sendsTo_[i]= value; }
		inline const bool SendsTo(int i) const { return sendsTo_[i]; }
		inline bool &MasterSend() { return mastersend_; }
		inline const bool &MasterSend() const{ return mastersend_; }
		inline float &Volume() { return volume_; }
		inline const float &Volume() const { return volume_; }
		inline float &Panning() { return panning_; }
		inline const float &Panning() const { return panning_; }
		inline bool &Mute() { return mute_; }
		inline const bool &Mute() const { return mute_; }
		inline Wire &GetWire() { return wire_; }
		inline const Wire &GetWire() const { return wire_; }
		
		bool IsValid() const { return wire_.Enabled(); }

		void ResizeTo(int sends) { sendsTo_.resize(sends); }
		void ExchangeSends(int send1,int send2)
		{
			bool tmp = sendsTo_[send1];
			sendsTo_[send1] = sendsTo_[send2];
			sendsTo_[send2] = tmp;
		}

	protected:
		Wire wire_;
		std::vector<bool> sendsTo_;
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
		inline float &DryWetMix() { return drywetmix_; }
		inline float &Gain() { return gain_; }

	protected:
		float volume_;
		float drywetmix_;
		float gain_;
	};

	enum 
	{
		chan1=0,
		chanmax = chan1+MAX_CONNECTIONS,
		return1 = chanmax,
		returnmax = return1+MAX_CONNECTIONS,
		send1 = returnmax,
		sendmax = send1+MAX_CONNECTIONS
	};
	Mixer();
	Mixer(int index);
	virtual ~Mixer() throw();
	virtual void Init(void);
	virtual void Tick( int channel,PatternEntry* pData);
	virtual void recursive_process(unsigned int frames, bool measure_cpu_usage);
	///\name used by the multi-threaded scheduler
	///\{
			/// tells the scheduler which machines to process before this one
			/*override*/ void sched_inputs(sched_deps&) const;
			/// tells the scheduler which machines may be processed after this one
			/*override*/ void sched_outputs(sched_deps&) const;
			/// called by the scheduler to ask for the actual processing of the machine
			/*override*/ bool sched_process(unsigned int frames, bool measure_cpu_usage);
			void inline InitialWorkState() {
				mixed=true;
				sched_returns_processed_curr=0;
				sched_returns_processed_prev=0;
			}
	///\}
	protected:
	void FxSend(int numSamples, bool recurse, bool measure_cpu_usage);
	void MixInToSend(int inIdx,int outIdx, int numSamples);
	void MixReturnToSend(int inIdx,int outIdx, int numSamples, float wirevol);
	void Mix(int numSamples);
public:
	virtual void GetWireVolume(int wireIndex, float &value){ value = GetWireVolume(wireIndex); }
	virtual float GetWireVolume(int wireIndex);
	virtual void SetWireVolume(int wireIndex,float value);
	virtual void OnWireVolChanged(Wire & wire);
	virtual void OnPinChange(Wire & wire, Wire::Mapping const & newMapping);
	virtual void OnOutputDisconnected(Wire & wire);
	virtual void OnOutputConnected(Wire & wire, int outType, int outWire);
	virtual void OnInputConnected(Wire & wire);
	virtual int FindInputWire(const Wire* pWireToFind) const;
	virtual int FindOutputWire(const Wire* pWireToFind) const;
	virtual int FindInputWire(int macIndex) const;
	virtual int FindOutputWire(int macIndex) const;
	virtual void NotifyNewSendtoMixer(Machine & callerMac, Machine & senderMac);
	virtual int GetFreeInputWire(int slottype=0) const;
	virtual int GetInputSlotTypes() const { return 2; }
	virtual void OnInputDisconnected(Wire & wire);
	virtual void DeleteWires();
	virtual float GetAudioRange() const { return 32768.0f; }
	std::string GetAudioInputName(int port);
	virtual const char* const GetName(void) const { return _psName; }
	virtual int GetNumCols();
	virtual int GetParamType(int numparam) { return 2; }
	virtual void GetParamName(int numparam,char *name);
	virtual void GetParamRange(int numparam, int &minval, int &maxval);
	virtual void GetParamValue(int numparam,char *parVal);
	virtual int GetParamValue(int numparam);
	virtual bool SetParameter(int numparam,int value);
	virtual bool LoadSpecificChunk(RiffFile * pFile, int version);
	virtual bool LoadWireMapping(RiffFile* pFile, int version);
	virtual void SaveSpecificChunk(RiffFile * pFile);
	virtual bool SaveWireMapping(RiffFile* pFile);
	virtual void PostLoad(Machine** _pMachine);

	bool IsSoloColumn() { return solocolumn_ != -1;}
	bool IsSoloChannel() { return solocolumn_ >= 0 && solocolumn_ < MAX_CONNECTIONS;}
	bool IsSoloReturn() { return solocolumn_ >= MAX_CONNECTIONS;}
	bool GetSoloState(int column) { return column==solocolumn_; }
	void SetSoloState(int column,bool solo)
	{
		if (solo ==false && column== solocolumn_)
			solocolumn_=-1;
		else solocolumn_=column;
	}
	float VuChan(int idx);
	float VuSend(int idx);

	inline InputChannel & Channel(int i) { return inputs_[i]; }
	inline const InputChannel & Channel(int i) const { return inputs_[i]; }
	inline ReturnChannel & Return(int i) { return returns_[i]; }
	inline const ReturnChannel & Return(int i) const { return returns_[i]; }
	inline Wire & Send(int i) { return *sends_[i]; }
	inline const Wire & Send(int i) const { return *sends_[i]; }
	inline MasterChannel & Master() { return master_; }
	inline int numinputs() const { return static_cast<int>(inputs_.size()); }
	inline int numreturns() const { return static_cast<int>(returns_.size()); }
	inline int numsends() const { return static_cast<int>(sends_.size()); }
	inline bool ChannelValid(int i) const { assert (i<MAX_CONNECTIONS); return (i<numinputs() && inWires[i].Enabled()); }
	inline bool ReturnValid(int i) const { assert (i<MAX_CONNECTIONS); return (i<numreturns() && Return(i).IsValid()); }
	inline bool SendValid(int i) const { assert (i<MAX_CONNECTIONS); return (i<numsends() && sends_[i] != NULL && sends_[i]->Enabled()); }

	void InsertChannel(int idx,InputChannel* input=0);
	void InsertReturn(int idx,ReturnChannel* retchan=0);
	void InsertSend(int idx,Wire* swire);
	void DiscardChannel(int idx, int idxdeleting=-1);
	void DiscardReturn(int idx, int idxdeleting=-1);
	void DiscardSend(int idx, int idxdeleting=-1);

	void ExchangeChans(int chann1,int chann2);
	void ExchangeReturns(int chann1,int chann2);
	void ResizeTo(int inputs, int sends);

	void RecalcMaster();
	void RecalcReturn(int idx);
	void RecalcChannel(int idx);
	void RecalcSend(int chan,int send);

	void RecalcInMapping(int inIdx, int sendIdx, const Wire::Mapping& inMap, const Wire::Mapping& sendMap);
	void RecalcRetMapping(int retIdx, int sendIdx, const Wire::Mapping& retMap, const Wire::Mapping& sendMap);


protected:
	static char* _psName;
	/// helper variable for scheduled processing: returns can be routed to other returns
	int sched_returns_processed_curr;
	int sched_returns_processed_prev;
	bool mixed;

	int solocolumn_;
	std::vector<InputChannel> inputs_;
	std::vector<ReturnChannel> returns_;
	std::vector<Wire*> sends_;
	MasterChannel master_;
	std::vector<LegacyWire> legacyReturn_;
	std::vector<LegacyWire> legacySend_;

	// Arrays of precalculated values for FxSend and Mix functions
	// _sendvol = vol from ins to sends
	// _mixvol = vol from ins to master
	// mixvolret = vol from returns to sends (if redirected)
	// last mixvolret = vol from returns to master.
	float _sendvolpl[MAX_CONNECTIONS][MAX_CONNECTIONS];
	float _sendvolpr[MAX_CONNECTIONS][MAX_CONNECTIONS];
	float mixvolpl[MAX_CONNECTIONS];
	float mixvolpr[MAX_CONNECTIONS];
	float mixvolretpl[MAX_CONNECTIONS][MAX_CONNECTIONS+1]; // +1 is Master
	float mixvolretpr[MAX_CONNECTIONS][MAX_CONNECTIONS+1];
	Wire::Mapping sendMapping[2*MAX_CONNECTIONS][MAX_CONNECTIONS];//ins and returns to sends.

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
	}
}
