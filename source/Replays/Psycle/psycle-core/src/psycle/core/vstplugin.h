// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net
/*
**<@JosepMa> The so-called seib host (which is mine, but based on his), is composed of two classes:
*<@JosepMa> CVstHost and CEffect.
*<@JosepMa> The former maps all the AudioMaster calls, provides a way to create CEffects, and helps
*           in getting time/position information.
*<@JosepMa> CEffect is a C++ wrapper of the AEffect class for a host. (AudioEffect is a C++ wrapper
*           for a plugin)
*<@JosepMa> As such it maps all the dispatch calls to functions with parameter validation, and helps
*           in the construction and destruction processes. Tries to help on other simpler tasks, and
*           in the handling of parameter windows (VstEffectWnd.cpp/.hpp)
*<@JosepMa> vst::AudioMaster and vst::plugin are subclasses of the previously mentioned classes,
*           which extend the functionality of the base classes, and adapts them to its usage
*           inside psycle.
*<@JosepMa> The host one doesn't provide much more (since the base class is good enough), and the
*           plugin one wraps the CEffect into a Machine class
*/

#ifndef PSYCLE__CORE__VSTPLUGIN__INCLUDED
#define PSYCLE__CORE__VSTPLUGIN__INCLUDED

#include <psycle/core/detail/project.hpp>

#ifdef DIVERSALIS__OS__MICROSOFT
	#include "machine.h"
	#include <seib/vst/CVSTHost.Seib.hpp>
	#include <psycle/helpers/math.hpp>

	namespace psycle { namespace core { namespace vst {

	using namespace seib::vst;
	using namespace helpers::math;

	// Maximum number of Audio Input/outputs
	// \todo : this shouldn't be a static value. Host should ask the plugin and the array get created dynamically.
	const int max_io = 16;
	const int MAX_VST_EVENTS = 128;

	// The real VstEvents in the SDK is defined as events[2], so  it cannot be used from a Host point of view.
	typedef struct VstEventsDynamicstruct
	{
		/// number of Events in array
		VstInt32 numEvents;
		/// zero (Reserved for future use)
		VstIntPtr reserved;
		/// event pointer array, variable size
		VstEvent* events[MAX_VST_EVENTS];
	} VstEventsDynamic;

	class AudioMaster;
	class CVstEffectWnd;


	class plugin : public Machine, public CEffect
	{
	friend class AudioMaster;
	protected:
		plugin(MachineCallbacks*, MachineKey, Machine::id_type, LoadedAEffect &loadstruct);
		//this constructor is to be used with the old Song loading routine, in order to create an "empty" plugin.
		plugin(MachineCallbacks* callbacks, Machine::id_type id, AEffect *effect)
			: CEffect(effect)
			, Machine(callbacks,id)
		{
			queue_size = 0;
			requiresRepl = 0;
			requiresProcess = 0;
			_nCols=0;
			_pOutSamplesL = 0;
			_pOutSamplesR = 0;
		}
	public:
		virtual ~plugin() throw();
		virtual void Init(){ Machine::Init();}
		//todo: Reimplement these calls with GenerateAudio
		virtual void PreWork(int numSamples,bool clear=true);
		virtual int GenerateAudioInTicks(int startSample, int numsamples);
		virtual void Tick() {}
		virtual void Tick(int /*channel*/, const PatternEvent &);

		virtual void Stop();

		virtual bool LoadSpecificChunk(RiffFile* pFile, int version);
		virtual void SaveSpecificChunk(RiffFile * pFile) const;
		// old fileformat {
		virtual bool PreLoadPsy2FileFormat(RiffFile * pFile, unsigned char &_program, int &_instance);
		virtual bool LoadFromMac(vst::plugin *pMac, unsigned char program, int numPars, float* pars);
		virtual bool LoadChunkPsy2FileFormat(RiffFile* pFile);
		// }
		virtual void InsertInputWire(Machine& srcMac, Wire::id_type dstWire,InPort::id_type dstType, float initialVol=1.0f)
		{
			try
			{
				MainsChanged(false); ConnectInput(0,true); ConnectInput(1,true); Machine::InsertInputWire(srcMac, dstWire, dstType, initialVol);  MainsChanged(true);
			}catch(...){}
		}
		virtual void InsertOutputWire(Machine& dstMac, Wire::id_type wireIndex, OutPort::id_type srctype )
		{
			try
			{
				MainsChanged(false); ConnectOutput(0,true); ConnectOutput(1,true); Machine::InsertOutputWire(dstMac, wireIndex, srctype);  MainsChanged(true);
			}catch(...){}
		}
		virtual void DeleteInputWire(Wire::id_type wireIndex, InPort::id_type dstType)
		{
			try
			{
				MainsChanged(false); ConnectInput(0,false); ConnectInput(1,false); Machine::DeleteInputWire(wireIndex, dstType);  MainsChanged(true);
			}catch(...){}
		}
		virtual void DeleteOutputWire(Wire::id_type wireIndex, OutPort::id_type srctype)
		{
			try
			{
				MainsChanged(false); ConnectOutput(0,false); ConnectOutput(1,false); Machine::DeleteOutputWire(wireIndex, srctype);  MainsChanged(true);
			}catch(...){}
		}
		// Properties
		//////////////////////////////////////////////////////////////////////////
		inline virtual std::string GetDllName() const { return key_.dllName(); }
		virtual const MachineKey& getMachineKey() const { return key_; }
		virtual std::string GetName() const { return _psName; }
		virtual std::string GetVendorName() const {
			char temp[kVstMaxVendorStrLen];
			memset(temp,0,sizeof(temp));
			if(GetVendorString(temp) && temp[0]) return temp;
			else return "Unknown vendor";
		}
		virtual const uint32_t GetAPIVersion() const { return GetVstVersion(); }
		virtual const uint32_t GetVersion() const { return GetVendorVersion(); }
		virtual bool IsGenerator() const { try { return IsSynth(); }PSYCLE__HOST__CATCH_ALL(crashclass); return false; }
		virtual bool IsShellMaster() const { try { return (GetPlugCategory() == kPlugCategShell); }PSYCLE__HOST__CATCH_ALL(crashclass); return 0; }
		virtual int GetShellIdx() const { try { return ( IsShellPlugin()) ? uniqueId() : 0; }PSYCLE__HOST__CATCH_ALL(crashclass); return 0; }
		virtual int GetPluginCategory() const { try { return GetPlugCategory(); }PSYCLE__HOST__CATCH_ALL(crashclass); return 0; }
		virtual void SetSampleRate(int sr) { Machine::SetSampleRate(sr); CEffect::SetSampleRate((float)sr); }
		virtual bool Bypass(void) const { return Machine::Bypass(); }
		virtual void Bypass(bool e)
		{
			Machine::Bypass(e);
			if (aEffect)
			{
				try
				{
					if (!bCanBypass) MainsChanged(!e);
					SetBypass(e);
				}catch(...){}
			}
		}
		virtual bool Standby() const { return Machine::Standby(); }
		virtual void Standby(bool e)
		{
			Machine::Standby(e);
			if (aEffect && !IsGenerator())
			{
				// some plugins ( psp vintage warmer ) might not like to change the state too
				// frequently, or might have a delay which makes fast switching unusable.
				// This is why this is commented out until another solution is found.
				//if (!bCanBypass) MainsChanged(!e);
				try
				{
					SetBypass(e);
				}catch(...){}
			}
		}
		virtual void GetParamRange(int numparam,int &minval, int &maxval) const { minval = 0; maxval = CVSTHost::GetQuantization(); }
		virtual int GetNumParams() const { return numParams(); }
		virtual void GetParamName(int numparam, char * parval) const { if (numparam<numParams()) CEffect::GetParamName(numparam,parval); }
		virtual void GetParamValue(int numparam, char * parval) const;
		virtual bool DescribeValue(int parameter, char * psTxt) const;
		virtual int GetParamValue(int numparam) const
		{
			try
			{
				if(numparam < numParams())
					return round<int>(GetParameter(numparam) * CVSTHost::GetQuantization());
			}catch(...){}
			return 0;
		}
		virtual bool SetParameter(int numparam, int value)
		{
			try
			{
				if(numparam < numParams())
				{
					CEffect::SetParameter(numparam,float(value)/float(CVSTHost::GetQuantization()));
					return true;
				}
			}catch(...){}
			return false;
		}
		virtual void SetParameter(int numparam, float value)
		{
			try
			{
				if(numparam < numParams())CEffect::SetParameter(numparam,value);
			}catch(...){}
		}


		// CEffect overloaded functions
		//////////////////////////////////////////////////////////////////////////
		virtual void EnterCritical() {;}
		virtual void LeaveCritical() {;}
		virtual void crashed2(const std::exception & e) const { /*Machine::crashed(e);*/ }
		virtual bool WillProcessReplace() const { return !requiresProcess && (CanProcessReplace() || requiresRepl); }
		/// IsIn/OutputConnected are called when the machine receives a mainschanged(on), so the correct way to work is
		/// doing an "off/on" when a connection changes.
		virtual bool DECLARE_VST_DEPRECATED(IsInputConnected)(int input) const { return ((input < 2)&& (_connectedInputs!=0)); }
		virtual bool DECLARE_VST_DEPRECATED(IsOutputConnected)(int output) const { return ((output < 2) && (_connectedOutputs!=0)); }
		// AEffect asks host about its input/outputspeakers.
		virtual VstSpeakerArrangement* OnHostInputSpeakerArrangement() const { return 0; }
		virtual VstSpeakerArrangement* OnHostOutputSpeakerArrangement() const { return 0; }
		// AEffect informs of changed IO. verify numins/outs, speakerarrangement and the likes.
		virtual bool OnIOChanged() { return false; }
	/*
		virtual void SetEditWnd(CEffectWnd* wnd)
		{
			CEffect::SetEditWnd(wnd);
			editorWindow = reinterpret_cast<CVstEffectWnd*>(wnd);
		}
	*/

	private:
		/// reserves space for a new midi event in the queue.
		/// \return midi event to be filled in, or null if queue is full.
		VstMidiEvent* reserveVstMidiEvent();
		VstMidiEvent* reserveVstMidiEventAtFront(); // ugly hack
		bool AddMIDI(unsigned char data0, unsigned char data1 = 0, unsigned char data2 = 0, unsigned int sampleoffset=0);
		bool AddNoteOn(unsigned char channel, unsigned char key, unsigned char velocity, unsigned char midichannel = 0, unsigned int sampleoffset=0,bool slide=false);
		bool AddNoteOff(unsigned char channel, unsigned char midichannel = 0, bool addatStart = false, unsigned int sampleoffset=0);
		inline void SendMidi();
		int LSB(int val)
		{
			return val & 0x7f;
		}

		int MSB(int val)
		{
			return (val & 0x3F80) >> 7;
		}

		char _psShortName[16];
		std::string _psAuthor;
		std::string _psName;
		MachineKey key_;
		/// It needs to use Process
		bool requiresProcess;
		/// It needs to use ProcessReplacing
		bool requiresRepl;

		class note
		{
		public:
			unsigned char key;
			unsigned char midichan;
		};
		/// midi events queue, is sent to processEvents.
		note trackNote[MAX_TRACKS];
		VstEventsDynamic mevents;
		VstMidiEvent midievent[MAX_VST_EVENTS];
		int queue_size;
		bool NSActive[16];
		int NSSamples[16];
		int NSDelta[16];
		int NSDestination[16];
		int NSTargetDistance[16];
		int NSCurrent[16];
		static int pitchWheelCentre;
		int rangeInSemis;
		int currentSemi[16];
		int oldNote[16];

		float * inputs[max_io];
		float * outputs[max_io];
		float * _pOutSamplesL;
		float * _pOutSamplesR;
		// Junk is a safe buffer for vst plugins that would want more buffers than
		// supplied.
		static float junk[MAX_BUFFER_LENGTH];

	};
	}}}
#endif
#endif

