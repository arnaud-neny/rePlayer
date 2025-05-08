///\file
///\brief interface file for psycle::host::vsthost
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include "Machine.hpp"
#include <seib-vsthost/CVSTHost.Seib.hpp>
#include <psycle/helpers/math/math.hpp>
#include <cstring>
/*
*<@JosepMa> Psycle's seib-VstHost is a reimplementation of HermannSeib's vsthost composed of two classes:
*<@JosepMa> CVstHost and CEffect.
*<@JosepMa> the former maps all the AudioMaster calls, provides a way to create CEffects, and helps
*           in getting time/position information.
*<@JosepMa> CEffect is a C++ wrapper for the AEffect class, for a host (AudioEffect is a C++ wrapper
*           for a plugin)
*<@JosepMa> as such it maps all the dispatch calls to functions with parameter validation, and helps
*           in the construction and destruction processes. Tries to help on other simpler tasks, and
*           in the handling of parameter windows (VstEffectWnd.cpp/.hpp)
*<@JosepMa> vst::Host and vst::Plugin are subclasses of the aforementioned classes, which both: extend
*           the functionality of the base classes, and adapts them to its usage inside psycle
*<@JosepMa> the host one doesn't provide much more (since the base class is good enough), and the
*           plugin one wraps the CEffect into a Machine class
*/
namespace psycle
{
	namespace host
	{
		namespace vst
		{
			// Dialog max ticks for parameters.
			const int MAX_VST_EVENTS = 128;

			// The real VstEvents in the SDK is defined as events[2], so  it cannot be used from a Host point of view.
			typedef struct VstEventsDynamicstruct
			{
				VstInt32 numEvents;		///< number of Events in array
				VstIntPtr reserved;		///< zero (Reserved for future use)
				VstEvent* events[MAX_VST_EVENTS];	///< event pointer array, variable size
			} VstEventsDynamic;
			extern const char* MIDI_CHAN_NAMES[16];


			class CVstEffectWnd;

			class Host : public seib::vst::CVSTHost
			{
			public:
				Host();
				virtual ~Host(){;}

				static VstInt32 quantizationVal() { return quantization; }

				///< Helper class for Machine Creation.
				virtual seib::vst::CEffect * CreateEffect(seib::vst::LoadedAEffect &loadstruct);
				virtual seib::vst::CEffect * CreateWrapper(AEffect *effect);
				virtual void Log(std::string message);

				///> Plugin gets Info from the host
				virtual bool OnGetProductString(char *text) { strcpy(text, "Psycle"); return true; }
				virtual VstInt32 OnGetHostVendorVersion() { return PSYCLE__VERSION__NUMBER; }
				virtual bool OnCanDo(seib::vst::CEffect &pEffect,const char *ptr);
				virtual VstInt32 OnGetHostLanguage() { return kVstLangEnglish; }
				virtual void CalcTimeInfo(VstInt32 lMask = -1);
				virtual VstInt32 DECLARE_VST_DEPRECATED(OnTempoAt)(seib::vst::CEffect &pEffect, VstInt32 pos);
				virtual VstInt32 DECLARE_VST_DEPRECATED(OnGetNumAutomatableParameters)(seib::vst::CEffect &pEffect) { return 0xFF; } // the size of the aux column.
				virtual VstInt32 OnGetAutomationState(seib::vst::CEffect &pEffect);
				virtual VstInt32 OnGetInputLatency(seib::vst::CEffect &pEffect);
				virtual VstInt32 OnGetOutputLatency(seib::vst::CEffect &pEffect);
				//\todo : how can this function be implemented? :o
				virtual VstInt32 OnGetCurrentProcessLevel(seib::vst::CEffect &pEffect) { return 0; }
				virtual bool OnWillProcessReplacing(seib::vst::CEffect &pEffect);

				///> Plugin sends actions to the host
				virtual bool OnProcessEvents(seib::vst::CEffect &pEffect, VstEvents* events) { return false; }

				
			};
			class Plugin : public Machine, public seib::vst::CEffect
			{
			protected:
				class note
				{
				public:
					unsigned char key;
					unsigned char midichan;
				};
				/// midi events queue, is sent to processEvents.
				note trackNote[MAX_TRACKS];
				VstMidiEvent midievent[MAX_VST_EVENTS];
				VstEventsDynamic mevents;
				int	queue_size;
				/// reserves space for a new midi event in the queue.
				/// \return midi event to be filled in, or null if queue is full.
				VstMidiEvent* reserveVstMidiEvent();
				VstMidiEvent* reserveVstMidiEventAtFront(); // ugly hack

				bool NSActive[16];
				int	NSSamples[16];
				int NSDelta[16];
				int NSDestination[16];
				int NSTargetDistance[16];
				int NSCurrent[16];
				static int pitchWheelCentre;
				int rangeInSemis;
				int currentSemi[16];
				int oldNote[16];

				std::vector<float *> inputs;
				std::vector<float *> outputs;
				std::vector<float *> tmpinputs;
				std::vector<float *> tmpoutputs;

				std::string _sDllName;
				std::string _sProductName;
				std::string _sVendorName;

				/// It needs to use Process
				bool requiresProcess;
				/// It needs to use ProcessReplacing
				bool requiresRepl;

			public:
				Plugin(seib::vst::LoadedAEffect &loadstruct);
				//this constructor is to be used with the old Song loading routine, in order to create an "empty" plugin.
				Plugin(AEffect *effect):seib::vst::CEffect(effect)
				{
					queue_size = 0;
					requiresRepl = 0;
					requiresProcess = 0;
					_nCols=0;
					_type=MACH_VST;
					InitializeSamplesVector();
				}
				virtual ~Plugin() { Free(); }
				void Free() throw(::std::exception) { if  (ploader)	Unload(); };

				// Machine overloaded functions
				//////////////////////////////////////////////////////////////////////////
				// Actions
				virtual void Init(){ Machine::Init();}
				virtual void PreWork(int numSamples,bool clear, bool measure_cpu_usage);
				virtual int GenerateAudioInTicks(int startSample,  int numSamples);
				virtual void NewLine() { Machine::NewLine(); }
				virtual void Tick(int track, PatternEntry * pData);
				virtual void Stop();
				virtual bool NeedsAuxColumn() { return _type == MACH_VST; }
				virtual const char* AuxColumnName(int idx) const { return MIDI_CHAN_NAMES[idx]; }
				virtual int NumAuxColumnIndexes() { return 16;}
				// old fileformat {
				virtual bool PreLoad(RiffFile * pFile, unsigned char &_program, int &_instance);
				virtual bool LoadFromMac(vst::Plugin *pMac);
				virtual bool LoadChunk(RiffFile* pFile);
				// }
				virtual bool IsShellMaster() {return (GetPlugCategory() == kPlugCategShell); }
				virtual int GetShellIdx() { return ( IsShellPlugin()) ? uniqueId() : 0;	}
				virtual int GetPluginCategory() { return GetPlugCategory(); }
				virtual bool LoadSpecificChunk(RiffFile* pFile, int version);
				virtual void SaveSpecificChunk(RiffFile * pFile);
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
				}
				bool AddMIDI(unsigned char data0, unsigned char data1 = 0, unsigned char data2 = 0, unsigned int sampleoffset=0);
				bool AddNoteOn(unsigned char channel, unsigned char key, unsigned char velocity, unsigned char midichannel = 0, unsigned int sampleoffset=0,bool slide=false);
				bool AddNoteOff(unsigned char channel, bool addatStart = false, unsigned int sampleoffset=0);
				inline void SendMidi();
				int LSB(int val)
				{				
					return val & 0x7f;
				}

				int MSB(int val)
				{				
					return (val & 0x3F80) >> 7;
				}
				// Properties
				//////////////////////////////////////////////////////////////////////////
				virtual void SetSampleRate(int sr) { try {Machine::SetSampleRate(sr); CEffect::SetSampleRate((float)sr); }catch(...){} }
				virtual const char * const GetDllName() const throw() { return _sDllName.c_str(); }
				virtual const char* const GetName(void) const { return _sProductName.c_str(); }
				inline const char * const GetVendorName() const throw() { return _sVendorName.c_str(); }
				virtual const uint32_t GetAPIVersion() { try {return GetVstVersion(); }catch(...){return 0;} }
				virtual const uint32_t GetPlugVersion() { try { return GetVendorVersion(); }catch(...){return 0;}}
				//
				virtual void GetParamRange(int numparam,int &minval, int &maxval) {	minval = 0; maxval = Host::quantizationVal(); }
				virtual int GetNumParams() { return numParams(); }
				virtual int GetParamType(int numparam) { return 2; }
				virtual void GetParamName(int numparam, char * parval)
				{
					try {
						if (numparam<numParams()) CEffect::GetParamName(numparam,parval);
					}catch(...){}
				}

				virtual void GetParamValue(int numparam, char * parval);
				virtual int GetParamValue(int numparam)
				{
					try
					{
						if(numparam < numParams())
							return math::round<int>(GetParameter(numparam) * Host::quantizationVal());
					}catch(...){}
					return 0;
				}
				virtual bool SetParameter(int numparam, int value)
				{
					try
					{
						if(numparam < numParams())
						{
							CEffect::SetParameter(numparam,float(value)/float(Host::quantizationVal()));
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
				virtual bool DescribeValue(int parameter, char * psTxt);
				virtual void SetCurrentProgram(int idx) {SetProgram(idx);};
				virtual int GetCurrentProgram() {
					return GetProgram()%128;
				};
				virtual void GetCurrentProgramName(char* val) {GetProgramName(val);};
				virtual void GetIndexProgramName(int bnkidx, int prgIdx, char* val){
					GetProgramNameIndexed(-1, bnkidx*128 + prgIdx, val);
				};
				virtual int GetNumPrograms(){ return numPrograms()<128?numPrograms():128;};
				virtual int GetTotalPrograms(){ return numPrograms();};
				virtual void SetCurrentBank(int idx) { SetProgram(idx*128+GetCurrentProgram());};
				virtual int GetCurrentBank() { try {return GetProgram()/128; } catch(...){return 0;}};
				virtual void GetCurrentBankName(char* val) {GetIndexBankName(GetCurrentBank(),val);};
				virtual void GetIndexBankName(int bnkidx, char* val){
					if(bnkidx < GetNumBanks())
						sprintf(val,"Internal %d", bnkidx+1);
					else
						val[0]='\0';
				};
				virtual int GetNumBanks(){ return (numPrograms()/128)+1;};
				virtual void OnPinChange(Wire & wire, Wire::Mapping const & newMapping)
				{
					try
					{
						bool state=bMainsState;
						if(state )MainsChanged(false);
						if(wire.GetSrcMachine()._macIndex == _macIndex) {
							OutputMapping(wire.GetMapping(),false);
							OutputMapping(newMapping,true);
						}
						else {
							InputMapping(wire.GetMapping(),false);
							InputMapping(newMapping,true);
						}
						if(state) MainsChanged(true);
					}catch(...){}
				}
				virtual void OnOutputConnected(Wire & wire, int outType, int outWire)
				{
					try
					{
						MainsChanged(false); OutputMapping(wire.GetMapping(),true); Machine::OnOutputConnected(wire, outType, outWire);  MainsChanged(true);
					}catch(...){}
				}
				virtual void OnInputConnected(Wire & wire)
				{
					try
					{
					    MainsChanged(false); InputMapping(wire.GetMapping(),true); Machine::OnInputConnected(wire);  MainsChanged(true);
					}catch(...){}
				}
				virtual void OnOutputDisconnected(Wire & wire)
				{
					try
					{
						MainsChanged(false); OutputMapping(wire.GetMapping(),false); Machine::OnOutputDisconnected(wire);  MainsChanged(true);
					}catch(...){}
				}
				virtual void OnInputDisconnected(Wire & wire)
				{ 
					try
					{
						MainsChanged(false); InputMapping(wire.GetMapping(),false); Machine::OnInputDisconnected(wire);  MainsChanged(true);
					}catch(...){}
				}
				virtual int GetNumInputPins() const { return (aEffect)?numInputs():Machine::GetNumInputPins();}
				virtual int GetNumOutputPins() const { return (aEffect)?numOutputs():Machine::GetNumOutputPins();}
				virtual std::string GetOutputPinName(int pin) const ;
				virtual std::string GetInputPinName(int pin) const;
				virtual float GetAudioRange() const { return 1.0f; }

				void InputMapping(Wire::Mapping const &mapping, bool enabled);
				void OutputMapping(Wire::Mapping const &mapping, bool enabled);
				virtual void change_buffer(std::vector<float*>& buf);

				// CEffect overloaded functions
				//////////////////////////////////////////////////////////////////////////
				virtual void crashed2(std::exception const & e) { Machine::crashed(e); }
				virtual bool WillProcessReplace() { return !requiresProcess && (CanProcessReplace() || requiresRepl); }
				/// IsIn/OutputConnected are called when the machine receives a mainschanged(on), so the correct way to work is
				/// doing an "off/on" when a connection changes.
				virtual bool DECLARE_VST_DEPRECATED(IsInputConnected)(VstInt32 input);
				virtual bool DECLARE_VST_DEPRECATED(IsOutputConnected)(VstInt32 output);
				// AEffect asks host about its input/outputspeakers.
				virtual VstSpeakerArrangement* OnHostInputSpeakerArrangement();
				virtual VstSpeakerArrangement* OnHostOutputSpeakerArrangement();
				// AEffect informs of changed IO. verify numins/outs, speakerarrangement and the likes.
				virtual bool OnIOChanged();

			};
		}
	}
}
