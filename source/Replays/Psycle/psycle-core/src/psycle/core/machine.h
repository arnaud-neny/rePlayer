// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

///\interface psycle::core::Machine

#ifndef PSYCLE__CORE__MACHINE__INCLUDED
#define PSYCLE__CORE__MACHINE__INCLUDED
#pragma once

#include "constants.h"
#include "commands.h"
#include "patternevent.h"
#include "playertimeinfo.h"
#include "machinekey.hpp"
#include "cpu_time_clock.hpp"

#include <universalis/os/loggers.hpp>
#include <universalis/stdlib/cstdint.hpp>
#include <universalis/stdlib/chrono.hpp>
#include <cassert>
#include <deque>
#include <map>
#include <list>
#include <stdexcept>

namespace psycle { namespace core {

class RiffFile;

///\todo FIXME: stole these from analzyer.h just to fix compile error.
const int MAX_SCOPE_BANDS = 128;
const int SCOPE_BUF_SIZE  = 4096;
const int SCOPE_SPEC_SAMPLES = 256;

class Machine; // forward declaration
class CoreSong; // forward declaration

class PSYCLE__CORE__DECL AudioBuffer {
	public: 
		AudioBuffer(int numChannels, int numSamples);
		~AudioBuffer();
		void Clear();
		float* getBuffer() { return buffer_; }
		int getNumChannels() {  return numchannels_; }
		int getNumSamples() {  return numsamples_; }
	private:
		float* buffer_;
		int numchannels_;
		int numsamples_;
};

class AudioPort;

// A wire is what interconnects two AudioPorts. Appart from being the graphically representable element,
// the wire is also responsible of volume changes and even pin reassignation (convert 5.1 to stereo, etc.. not yet)
class PSYCLE__CORE__DECL Wire {
	public:
		typedef int32_t id_type;

		Wire(): volume(1.0f), pan(), multiplier(1.0f), rvol(1.0f), lvol(1.0f), index(), senderport(), receiverport() {}
		virtual ~Wire() {
			if(senderport) Disconnect(senderport);
			if(receiverport) Disconnect(receiverport);
		}
		virtual void Connect(AudioPort *senderp,AudioPort *receiverp);
		virtual void ChangeSource(AudioPort* newsource);
		virtual AudioPort* getSource(){return senderport;}
		virtual void ChangeDestination(AudioPort* newdest);
		virtual void CollectData(int numSamples);
		virtual AudioPort* getDestination(){return receiverport;}
		virtual void SetVolume(float newvol);
		// Range of pan is -1.0f for top left and 1.0f for top right. 0.0f is both channels full scale.
		virtual void SetPan(float newpan);
		virtual inline int GetIndex() const { return index; }
		virtual inline void SetIndex(int idx) { index = idx; }
		virtual inline AudioBuffer* GetBuffer() { return intermediatebuffer; }
		virtual inline void setBuffer(AudioBuffer* buffer) { intermediatebuffer=buffer; }
	protected:
		virtual void Disconnect(AudioPort* port);
		virtual inline float RVol() { return rvol; }
		virtual inline float LVol() { return lvol; }
		float volume;
		float pan;
		float multiplier;
		float rvol;
		float lvol;
		int index;
		AudioPort *senderport;
		AudioPort *receiverport;
		AudioBuffer *intermediatebuffer;
};

// Class which allows the setup (as in shape) of the machines' connectors.
// An Audio port is synonym of a multiplexed channel. In other words, it is an element that defines
// the characteristics of one or more individual inputs that are used in conjunction.
// From this definition, we could have one Stereo Audio Port (one channel, two inputs or outputs),
// a 5.1 Port, or several Stereo Ports (in the case of a mixer table), between others..
// Note that several wires can be connected to the same AudioPort (automatic mixing in the case of an input port).
class PSYCLE__CORE__DECL AudioPort {
	public:
		///\todo: Port creation, assign buffers to it (passed via ctor? they might be shared/pooled). 
		///\todo: Also, Multiple buffers or a packed buffer (left/right/left/right...)?
		AudioPort(Machine & parent, int arrangement, std::string const & name) : parent_(parent), arrangement_(arrangement), name_(name) {}
		virtual ~AudioPort() {}
		virtual void CollectData(int /*numSamples*/) {}
		virtual void Connected(Wire * wire);
		virtual void Disconnected(Wire * wire);
		virtual inline Wire* GetWire(unsigned int index) { assert(index<wires_.size()); return wires_[index]; }
		virtual inline int NumberOfWires() { return static_cast<int>(wires_.size()); }
		virtual inline int Arrangement() throw() { return arrangement_; }
		virtual inline Machine & GetMachine() throw() { return parent_; }
		///\todo : should change arrangement/name be allowed? (Mutating Port?)
		virtual inline void ChangeArrangement(int arrangement) { this->arrangement_ = arrangement; }
		virtual inline std::string const & Name() const throw() { return name_; }
		virtual inline void ChangeName(std::string const & name) { this->name_ = name; }
		virtual inline AudioBuffer* GetBuffer() { return audiobuffer_; } 
		virtual inline void setBuffer(AudioBuffer* buffer) { audiobuffer_ = buffer; }
	protected:
		Machine & parent_;
		int arrangement_;
		std::string name_;
		typedef std::vector<Wire*> wires_type;
		wires_type wires_;
		AudioBuffer *audiobuffer_;
};

class PSYCLE__CORE__DECL InPort : public AudioPort {
	public:
        typedef uint32_t id_type;
		InPort(Machine & parent, int arrangement, std::string const & name) : AudioPort(parent, arrangement, name) {}
		virtual ~InPort(){};
		virtual void CollectData(int numSamples);
};

class PSYCLE__CORE__DECL OutPort : public AudioPort {
	public:
        typedef uint32_t id_type;
		OutPort(Machine & parent, int arrangement, std::string const & name) : AudioPort(parent, arrangement, name) {}
		virtual ~OutPort() {}
		virtual void CollectData(int numSamples);
};

// Usage of the AudioPorts and Wire classes:
//
// the class Machine has zero or more InPorts, as well as zero or more OutPorts.
// Each AudioPort has an AudioBuffer associated. The scheduler supplies these buffers.
// To connect the AudioPorts, there's a Wire, which connects one AudioPort to another AudioPort
// There can be several Wires to/from the same AudioPort (either input or output), but not two connecting
// the same pair of AudioPorts, nor a wire from and to the same AudioPort.
//
// The scheduler maintains and cleans the AudioBuffers, as well as calling the appropiate machine.
// 
// If the machine has InPorts, then it first cycles through them calling CollectData() on them.
// This call, then, cycles through each of the wires it has connected, calling CollectData() aswell.
// At last, the Wire gets the content of the AudioBuffers of the OutPort, and processes it to change volume
// or remap/mixdown the discrete channels of the buffer.
// When ready, the InPort proceeds to mix the buffer from the Wire into its own buffer, in order to have it
// ready for its machine. 
// When all Wires of all InPorts are processed, of if the machine doesn't have InPorts, the Machine generates
// its audio over the AudioBuffer of its OutPorts and returns to the scheduler.
//
// Optimizations:
// A Wire needs its own buffer just when both the OutPort and the InPort have more than one Wire,
// An InPort needs its own buffer just when it has more than one Wire connected to it. (In order to mix them)
// 
// This means that the buffer set for the OutPort, for the Wire and for the InPort may be the same, and if the
// Wire doesn't need to modify it, the InPort can already process the data that the OutPort has provided
// In fact, this is the usual case.
//
/*
class Machine {
	// It is possible, but not necessary to have more than one port, so no need for the vector in that case.
	std::vector<InPort> inPorts;
	std::vector<OutPort> outPorts;

	void ProcessAudio(int numSamples) {
		for(int i(0); i < inPorts.size(); ++i) inPorts.CollectData(numSamples);
		GenerateAudio();
	}
}
*/

class WorkEvent {
	public:
		WorkEvent() {}
		
		WorkEvent(double beatOffset, int track, PatternEvent const & patternEvent)
			: offset_(beatOffset), track_(track), event_(patternEvent) {}

	///\name beat
	///\{
		public:
			double beatOffset() const throw() { return offset_; }
			void changeposition(double beatOffset) throw() { offset_ = beatOffset; }
		private:
			double offset_;
	///\}
	
	///\name track
	///\{
		public:
			int track() const throw() { return track_; }
		private:
			int track_;
	///\}

	///\name event
	///\{
		public:
			PatternEvent const & event() const throw() { return event_; }
		private:
			PatternEvent event_;
	///\}
};

class Song;

class MachineCallbacks {
	public:
		virtual ~MachineCallbacks() {}
		virtual const PlayerTimeInfo & timeInfo() const = 0;
		virtual PlayerTimeInfo &timeInfo() = 0;
		virtual bool autoStopMachines() const = 0;
		virtual const CoreSong & song() const = 0;
		virtual CoreSong & song() = 0;
};

/// Base class for "Machines", the audio producing elements.
class PSYCLE__CORE__DECL Machine {
	friend class CoreSong; friend class Psy2Filter; friend class Player;


	//////////////////////////////////////////////////////////////////////////
	// Draft for a new Machine Specification.
	// A machine is created using a MachineFactory.
	// To tell the factory which machine it needs to generate, a MachineKey is passed to it.
	// The factory creates the instance, loads any library (.dll/.so) that could be needed, and does
	// an initialization (like calling Reset()) on it, so the machine becomes operational.
	// Use Reset() to reinitialize the machine status and recall all default values for the parameters.
	// Use StandBy(bool) to set or unset the machine to a stopped state. ("suspend" in vst terminology).
	// "GenerateAudio()" will still be called so that the machine can update its state, but will return with no data
	// This function can be used as an "audio-only" reset. (Panic button)
	// Bypass(bool) un/sets the Bypass flag, and calls to StandBy() accordingly.
	// GenerateAudio() Call it to start the processing of input buffers and generate the output.
	// AddEvent(timestampedEvent)
	// MasterChanged(changetype)
	// SaveState(ofstream)
	// LoadState(ifstream)
	// several "Get" for Information (name, params...)
	// several "Set" for Information (name, params...)
	// Automation... calling, or being called? ( calling automata.work() or automata calling machine.work())
	// Use the concept of "Ports" to define inputs/outputs.
	//
	// bool IsBypass()
	// bool IsStandBy()
	//////////////////////////////////////////////////////////////////////////


	///\name machine's numeric identifier. It is required for pattern events<->machine association, gui, and obviusly, in file load/save.
	///\{
		public:
			///\todo should be unsigned but some functions return negative values to signal errors instead of throwing an exception
			typedef int32_t id_type;
			id_type id() const throw() { return id_; }
		private:
			id_type id_;
			void id(id_type id) { id_ = id; } 
	///\}

	public:
		virtual const MachineKey& getMachineKey() const = 0;

	///\name ctor/dtor
	///\{
		protected:
			Machine(MachineCallbacks* callbacks, Machine::id_type id);
		public:
			virtual ~Machine();
			virtual void CloneFrom(Machine& src);
	///\}
	
	protected:
		MachineCallbacks * callbacks;

	public:
		virtual void Init();
			
		virtual void PreWork(int numSamples, bool clear = true);
		virtual int GenerateAudio(int numsamples);
			
		virtual void AddEvent(double offset, int track, const PatternEvent & event);
		virtual void Tick() {}
		virtual void Tick(int /*channel*/, const PatternEvent &) {}
		virtual void PostNewLine() {};
		virtual void Stop() { playCol.clear(); playColIndex =0; }

	protected:
		virtual int GenerateAudioInTicks(int startSample, int numsamples);
		virtual void reallocateRemainingEvents(double beatOffset);
		std::deque<WorkEvent> workEvents;
		std::map<int /* track */, int /* channel */> playCol;
		int playColIndex;

	///\name (de)serialization
	///\{
		public:
			bool LoadFileChunk(RiffFile * pFile, int version);
			void SaveFileChunk(RiffFile * pFile) const;
			virtual bool LoadSpecificChunk(RiffFile * pFile, int version);
			virtual void SaveSpecificChunk(RiffFile * pFile) const;
			/// Loader for psycle fileformat version 2.
			virtual bool LoadPsy2FileFormat(RiffFile * pFile);
	///\}

	///\name connections ... ports
	///\{
		public:
			// ConnectTo enables the possibility to interconnect two machines. They connection goes from the instanced to the one passed by parameter.
			// Note that ConnectTo() does not do verifications on the connections. You should use Song's function for that.
			virtual Wire::id_type ConnectTo(Machine & dstMac, InPort::id_type dsttype = InPort::id_type(0), OutPort::id_type srctype = OutPort::id_type(0), float volume = 1.0f);
			virtual bool MoveWireDestTo(Machine& dstMac, OutPort::id_type srctype, Wire::id_type srcwire, InPort::id_type dsttype = InPort::id_type(0));
			virtual bool MoveWireSourceTo(Machine& srcMac, InPort::id_type dsttype, Wire::id_type dstwire, OutPort::id_type srctype = OutPort::id_type(0));
		protected:
			virtual bool MoveWireDestTo(Machine& dstMac, InPort::id_type srctype, Wire::id_type srcwire, OutPort::id_type dsttype, Machine& oldDest);
			virtual bool MoveWireSourceTo(Machine& srcMac, InPort::id_type dsttype, Wire::id_type dstwire, OutPort::id_type srctype, Machine& oldSrc);
		public:
			virtual bool Disconnect(Machine & dst);
			virtual void DeleteWires(CoreSong& song);

		///\warning: This should be protected, but the class Mixer needs to call them. Do not use them from Song or "Outside World".
		public:
			// Set/replace/delete wires.
			virtual void InsertInputWire(Machine& srcMac, Wire::id_type dstWire,InPort::id_type dstType, float initialVol=1.0f);
			virtual void InsertOutputWire(Machine& dstMac, Wire::id_type wireIndex, OutPort::id_type srctype );
			virtual void DeleteInputWire(Wire::id_type wireIndex, InPort::id_type dstType);
			virtual void DeleteOutputWire(Wire::id_type wireIndex, OutPort::id_type srctype);
			virtual void NotifyNewSendtoMixer(Machine & callerMac,Machine & senderMac);
			// The parameter song is specifically for the psy3filter loader, since at loading time, the song in callbacks is not the one being loaded.
			virtual void SetMixerSendFlag(CoreSong* song=0);
			virtual void ClearMixerSendFlag();
	///\}

	///\name connections ... wires
	///\{
		public:
			//virtual void InitWireVolume(type_type, Wire::id_type, float value);
			virtual void ExchangeInputWires(Wire::id_type first,Wire::id_type second, InPort::id_type firstType= InPort::id_type(0), InPort::id_type secondType = InPort::id_type(0));
			virtual void ExchangeOutputWires(Wire::id_type first,Wire::id_type second, OutPort::id_type firstType = OutPort::id_type(0), OutPort::id_type secondType = OutPort::id_type(0));
			virtual Wire::id_type FindInputWire(id_type) const;
			virtual Wire::id_type FindOutputWire(id_type) const;
		
			virtual Wire::id_type GetFreeInputWire(InPort::id_type slotType=InPort::id_type(0)) const;
			virtual Wire::id_type GetFreeOutputWire(OutPort::id_type slottype=OutPort::id_type(0)) const;
	///\}

	///\name states used by the schedulers
	///\{
		public:
			virtual bool Bypass() const { return _bypass; }
			virtual void Bypass(bool e) { _bypass = e; }
		public:///\todo private:
			bool _bypass;

		public:
			virtual bool Standby() const { return _standby; }
			virtual void Standby(bool e) { _standby = e; }
		public:///\todo private:
			bool _standby;

		public:
			bool Mute() const { return _mute; }
			void Mute(bool e) { _mute = e; }
		public:///\todo private:
			bool _mute;

	///\name used by the single-threaded, recursive scheduler
	///\{
		public:
			/// virtual because the mixer machine has its own implementation
			virtual void recursive_process(unsigned int frames);
			void recursive_process_deps(unsigned int frames, bool mix = true);
		public:///\todo private:
			/// guard to avoid feedback loops
			bool recursive_is_processing_;
			bool recursive_processed_;
	///\}

	///\name used by the multi-threaded scheduler
	///\{
		protected:
			/// The multi-threaded scheduler cannot use recursive_processed_ because it's not thread-synchronised.
			/// So, we define another boolean that's modified only by the multi-threaded scheduler,
			/// with proper thread synchronisations.
			/// The multi-threaded scheduler doesn't use recursive_processed_ nor recursive_is_processing_.
			bool sched_processed_;
			typedef std::list<Machine const*> sched_deps;
			/// tells the scheduler which machines to process before this one
			virtual void sched_inputs(sched_deps&) const;
			/// tells the scheduler which machines may be processed after this one
			virtual void sched_outputs(sched_deps&) const;
			/// called by the scheduler to ask for the actual processing of the machine
			virtual bool sched_process(unsigned int frames);
	///\}

	///\name cpu time usage measurement
	///\{
public: void reset_time_measurement() throw() { accumulated_processing_time_ = *new universalis::stdlib::chrono::nanoseconds(0);processing_count_ = 0; }

		public:  cpu_time_clock::duration accumulated_processing_time() const throw() { return accumulated_processing_time_; }
		private: cpu_time_clock::duration accumulated_processing_time_;
		protected: void accumulate_processing_time(cpu_time_clock::duration d) throw() {
				if(loggers::warning() && d.count() < 0) {
					std::ostringstream s;
					s << "time went backward by: " << chrono::nanoseconds(d).count() * 1e-9 << 's';
					loggers::warning()(s.str(), UNIVERSALIS__COMPILER__LOCATION);
				} else accumulated_processing_time_ += d;
			}

		public:    uint64_t processing_count() const throw() { return processing_count_; }
		protected: uint64_t processing_count_;
	///\}

	///\name crash handling
	///\{
		public:
			/// This function should be called when an exception was thrown from the machine.
			/// This will mark the machine as crashed, i.e. crashed() will return true,
			/// and it will be disabled.
			///\param e the exception that occured, converted to a std::exception if needed.
			void crashed(std::exception const & e) throw();
		public:
			/// Tells wether this machine has crashed.
			bool crashed() const throw() { return crashed_; }
		private:
			bool crashed_;
	///\}

	protected:
		void UpdateVuAndStanbyFlag(int numSamples);

	public:
		virtual void SetSampleRate(int /*hertz*/) {
			#if defined PSYCLE__CONFIGURATION__RMS_VUS
			// todo broken ..  'rms' : undeclared identifier
				/*rms.count = 0;
				rms.AccumLeft = 0.;
				rms.AccumRight = 0.;
				rms.previousLeft = 0.;
				rms.previousRight = 0.;*/
			#endif
		}

	///\name audio range
	///\{
		public:
			///\todo doc
			virtual float GetAudioRange() const { return audio_range_; }
		protected:
			void SetAudioRange(float audio_range) { audio_range_ = audio_range; }
		private:
			float audio_range_;
	///\}

	///\name ports
	///\{
		public:
			void defineInputAsStereo(int numports=1);
			void defineOutputAsStereo(int numports=1);
			bool acceptsConnections() const { return numInPorts>0; }
			bool emitsConnections() const { return numOutPorts>0; }

			virtual unsigned int GetInPorts() const { return numInPorts; }
			virtual unsigned int GetOutPorts() const { return numOutPorts; }
			virtual AudioPort& GetInPort(InPort::id_type i) const { assert(i<numInPorts); return inports[i]; }
			virtual AudioPort& GetOutPort(OutPort::id_type i) const  { assert(i<numOutPorts); return inports[i]; }
			virtual std::string GetPortInputName(InPort::id_type /*port*/) const { std::string rettxt = "Stereo Input"; return rettxt; }
			virtual std::string GetPortOutputName(OutPort::id_type /*port*/)  const { std::string rettxt = "Stereo Output"; return rettxt; }

			virtual int GetAudioInputs() const { return MAX_CONNECTIONS; }
			virtual int GetAudioOutputs() const { return MAX_CONNECTIONS; }


			// Subclass tells, if the component is a generator in opposite to an effect
			virtual bool IsGenerator() const;

		protected:
            uint numInPorts;
            uint numOutPorts;
			InPort *inports;
			OutPort *outports;
			/// this machine is used by a send/return mixer. (Some things cannot be done on these machines)
			bool _isMixerSend;
	///\}

	///\name input ports legacy mode.
	///\{
		public:
			/// number of Incoming connections
			int32_t _connectedInputs;
			/// Incoming connections Machine numbers
			///\todo hardcoded limits and wastes
			Machine::id_type _inputMachines[MAX_CONNECTIONS];
			/// Incoming connections activated
			///\todo hardcoded limits and wastes
			bool _inputCon[MAX_CONNECTIONS];
			/// Incoming connections Machine volumes
			///\todo hardcoded limits and wastes
			float _inputConVol[MAX_CONNECTIONS];
			/// Value to multiply _inputConVol[] to have a 0.0...1.0 range
			///\todo hardcoded limits and wastes
			float _wireMultiplier[MAX_CONNECTIONS];
	///\}

	///\name output ports legacy mode.
	///\{
		public:
			/// number of Outgoing connections
			int32_t _connectedOutputs;
			/// Outgoing connections Machine numbers
			///\todo hardcoded limits and wastes
			Machine::id_type _outputMachines[MAX_CONNECTIONS];
			/// Outgoing connections activated
			///\todo hardcoded limits and wastes
			bool _connection[MAX_CONNECTIONS];
	///\}

	///\name Audio data buffers. This will go to Wires, and they will be owned by Player.
	///\{
		public:///\todo private:
			/// left data
			float *_pSamplesL;
			/// right data
			float *_pSamplesR;
	///\}


	///\name amplification of the signal in connections/wires
	///\{
		public:
			virtual void GetWireVolume(Wire::id_type wire, float & result) const { result = _inputConVol[wire] * _wireMultiplier[wire]; }
			virtual float GetWireVolume(int wireIndex) const { return _inputConVol[wireIndex] * _wireMultiplier[wireIndex]; }
			virtual void SetWireVolume(Wire::id_type wire, float value) { _inputConVol[wire] = value / _wireMultiplier[wire]; }
			virtual bool GetDestWireVolume(id_type src, Wire::id_type, float & result) const;
			virtual bool SetDestWireVolume(id_type src, Wire::id_type, float value);
	///\}

	public:
		///\todo 3 dimensional?
		virtual void SetPan(int newpan);
		int32_t Pan() const { return _panning; }
		float lVol() const { return _lVol; }
		float rVol() const { return _rVol; }

	protected:
		/// numerical value of panning.
		int32_t _panning;
		/// left chan volume
		float _lVol;
		/// right chan volume
		float _rVol;

	///\name name
	///\{
		public:
			virtual std::string GetDllName() const { return ""; }
			virtual std::string GetName() const = 0;

		public:
			virtual std::string const & GetEditName() const { return editName_; }
			virtual void SetEditName(std::string const & editName) { editName_ = editName; }
		private:
			std::string  editName_;
	///\}

	///\name parameters
	///\{
		public:
			virtual int GetNumCols() const { return _nCols; }
			virtual int GetNumParams() const { return _numPars; }
			virtual void GetParamName(int /*numparam*/, char * name) const { name[0] = '\0'; }
			virtual void GetParamRange(int /*numparam*/, int & minval, int & maxval) const { minval = 0; maxval = 0; }
			virtual void GetParamValue(int /*numparam*/, char * parval) const { parval[0] = '\0'; }
			virtual int GetParamValue(int /*numparam*/) const { return 0; }
			virtual bool SetParameter(int /*numparam*/, int /*value*/) { return false; }
		protected:
			int _numPars;
			int _nCols;
	///\}

	///\name more misplaced gui stuff
	///\{
		public:
			virtual int  GetPosX() const { return _x; }
			virtual void SetPosX(int x) {_x = x;}
			virtual int  GetPosY() const { return _y; }
			virtual void SetPosY(int y) {_y = y;}
		private:
			int _x;
			int _y;
	///\}


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///\todo below are unencapsulated data members
	public:

		///\name signal measurements, perhaps can be considered misplaced gui stuff
		///\{
			/// output peak level for DSP
			float _volumeCounter;
			/// output peak level for display
			int _volumeDisplay;
			/// output peak level for display
			int _volumeMaxDisplay;
			/// output peak level for display
			int _volumeMaxCounterLife;

			///\todo: This is for the Wire dialog, which shows the audio data in different ways. It should be done directly by the scope, not inside PreWork.
			int _scopePrevNumSamples;
			int _scopeBufferIndex;
			float *_pScopeBufferL;
			float *_pScopeBufferR;
		///\}

		///\ various player-related states
		///\{
			///\todo: Do these via events
			// System to allow pattern effects Retrig, Retrig Continue and Arpeggio.
			PatternEvent TriggerDelay[MAX_TRACKS];
			int TriggerDelayCounter[MAX_TRACKS];
			int RetriggerRate[MAX_TRACKS];
			int ArpeggioCount[MAX_TRACKS];
			// System to make Tweak Slides.
			bool TWSActive;
			int TWSInst[MAX_TWS];
			int TWSSamples;
			float TWSDelta[MAX_TWS];
			float TWSCurrent[MAX_TWS];
			float TWSDestination[MAX_TWS];
		///\}
};

}}
#endif
