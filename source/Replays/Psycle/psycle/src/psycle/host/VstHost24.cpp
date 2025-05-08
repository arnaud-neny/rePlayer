///\file
///\brief implementation file for psycle::host::Machine

#include <psycle/host/detail/project.private.hpp>
#include "VstHost24.hpp"
#include "Global.hpp"
#include "Player.hpp"
#include "Zap.hpp"

///\todo: these are required by the GetIn/OutLatency() functions. They should instead ask the player.
#include "Configuration.hpp"
#include "AudioDriver.hpp"

//This is so wrong. It's needed because of the loop inside the code for retrigger that it's in the Work() function.
#include "Song.hpp"

#include <universalis/os/loggers.hpp>
using namespace universalis::os;

///\todo: When inserting a note in a pattern (editing), set the correct samplePos and ppqPos corresponding to the place the note is being put.
//        (LiveSlice is a good example of what happens if it isn't correct)

#include <universalis/os/aligned_alloc.hpp>
#include "cpu_time_clock.hpp"

namespace psycle
{
	namespace host
	{
		namespace vst
		{
			int Plugin::pitchWheelCentre(8191);
			const char* MIDI_CHAN_NAMES[16] = {
				"MIDI Channel 01", "MIDI Channel 02","MIDI Channel 03","MIDI Channel 04",
				"MIDI Channel 05","MIDI Channel 06","MIDI Channel 07","MIDI Channel 08",
				"MIDI Channel 09","MIDI Channel 10","MIDI Channel 11","MIDI Channel 12",
				"MIDI Channel 13","MIDI Channel 14","MIDI Channel 15","MIDI Channel 16"
			};
			Host::Host()
			{
				quantization = 0xFFFF; SetBlockSize(STREAM_SIZE); 
				SetTimeSignature(4,4);
				vstTimeInfo.smpteFrameRate = kVstSmpte25fps;
			}
			seib::vst::CEffect * Host::CreateEffect(seib::vst::LoadedAEffect &loadstruct) { return new Plugin(loadstruct); }
			seib::vst::CEffect * Host::CreateWrapper(AEffect *effect) { return new Plugin(effect); }
			bool Host::OnWillProcessReplacing(seib::vst::CEffect &pEffect) { return static_cast<Plugin*>(&pEffect)->WillProcessReplace(); }

			void Host::CalcTimeInfo(VstInt32 lMask)
			{
				///\todo: cycleactive and recording to a "Start()" function.
				// automationwriting and automationreading.
				//
				/*
				kVstTransportCycleActive	= 1 << 2,
				kVstTransportRecording		= 1 << 3,

				kVstAutomationWriting		= 1 << 6,
				kVstAutomationReading		= 1 << 7,
				*/

				//kVstCyclePosValid			= 1 << 12,	// start and end
				//	cyclestart // locator positions in quarter notes.
				//	cycleend   // locator positions in quarter notes.

				//overriding this to better reflect the real state when bpm changes.
				if (lMask&(kVstPpqPosValid|kVstBarsValid|kVstClockValid)) {
					//ppqpos and barstart are automatically calculated.
					//samplestoNextClock, how many samples from the current position to the next clock, (24ppq precision, i.e. 1/24 beat ) (actually, to the nearest. previous-> negative value)
					if(lMask & kVstClockValid)
					{
						//Should be "round" instead of cast+1, but this is good enough.
						const double ppqclockpos = static_cast<VstInt32>(vstTimeInfo.ppqPos*24.0)+1;					// Get the next clock in 24ppqs
						const double sampleclockpos = ppqclockpos * 60.L * vstTimeInfo.sampleRate / vstTimeInfo.tempo;	// convert to samples
						vstTimeInfo.samplesToNextClock = sampleclockpos - vstTimeInfo.samplePos;									// get the difference.
						vstTimeInfo.flags |= kVstClockValid;
					}
				}
				lMask&=~(kVstPpqPosValid|kVstBarsValid|kVstClockValid);
				CVSTHost::CalcTimeInfo(lMask);
			}


			bool Host::OnCanDo(seib::vst::CEffect &pEffect, const char *ptr)
			{
				using namespace seib::vst::HostCanDos;
				bool value =  CVSTHost::OnCanDo(pEffect,ptr);
				if (value) return value;
				else if (
					//||	(!strcmp(ptr, canDoReceiveVstEvents))	// "receiveVstEvents",
					//||	(!strcmp(ptr, canDoReceiveVstMidiEvent ))// "receiveVstMidiEvent",
					//||	(!strcmp(ptr, "receiveVstTimeInfo" ))// DEPRECATED

					(!strcmp(ptr, canDoReportConnectionChanges )) // "reportConnectionChanges",
					||	(!strcmp(ptr, canDoAcceptIOChanges ))	// "acceptIOChanges",
					||	(!strcmp(ptr, canDoSizeWindow ))		// "sizeWindow",

					//||	(!strcmp(ptr, canDoAsyncProcessing ))	// DEPRECATED
					//||	(!strcmp(ptr, canDoOffline ))			// "offline",
					//||	(!strcmp(ptr, "supportShell" ))		// DEPRECATED
					//||	(!strcmp(ptr, canDoEditFile ))			// "editFile",
					//||	(!strcmp(ptr, canDoSendVstMidiEventFlagIsRealtime ))
					)
					return true;
				return false;                           /* per default, no.                  */
			}

			VstInt32 Host::DECLARE_VST_DEPRECATED(OnTempoAt)(seib::vst::CEffect &pEffect, VstInt32 pos)
			{
				//\todo: return the real tempo in the future, not always the current one
				// pos in Sample frames, return bpm* 10000
				return vstTimeInfo.tempo * 10000;
			}
			VstInt32 Host::OnGetOutputLatency(seib::vst::CEffect &pEffect)
			{
				AudioDriver* pdriver = Global::configuration()._pOutputDriver;
				return pdriver->GetOutputLatencySamples();
			}
			VstInt32 Host::OnGetInputLatency(seib::vst::CEffect &pEffect)
			{
				AudioDriver* pdriver = Global::configuration()._pOutputDriver;
				return pdriver->GetInputLatencySamples();
			}
			void Host::Log(std::string message)
			{
				loggers::information()(message);
			}

			///\todo: Get information about this function
			VstInt32 Host::OnGetAutomationState(seib::vst::CEffect &pEffect) { return kVstAutomationUnsupported; }

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

			Plugin::Plugin(seib::vst::LoadedAEffect &loadstruct)
				:seib::vst::CEffect(loadstruct)
				,queue_size(0)
				,requiresRepl(0)
				,requiresProcess(0)
				,rangeInSemis(2)
			{
				for (int midiChannel=0; midiChannel<16; midiChannel++)
				{
					NSActive[midiChannel] = false;
				    NSCurrent[midiChannel] = pitchWheelCentre;
					NSDestination[midiChannel] = pitchWheelCentre;
					NSTargetDistance[midiChannel] = 0;
					NSSamples[midiChannel] = 0;
					NSDelta[midiChannel] = 0;
					oldNote[midiChannel] = -1;
				}

				_nCols=0;
				if ( IsSynth())
				{
					_mode=MACHMODE_GENERATOR; _type=MACH_VST;
				}
				else 
				{
					_mode=MACHMODE_FX; _type=MACH_VSTFX;
				}
				// Compatibility hacks
				{
					if(uniqueId() == 0x41446c45 ) //"sc-101"
					{
						requiresRepl = true;
					}
				}
				
				if(WillProcessReplace()) {
					//Ensure at least two outputs (Patch for GetRMSVol and WireDlg Scope)
					VstInt32 maxval = std::max(std::max(numInputs(),numOutputs()),2);
					InitializeSamplesVector(maxval);
					inputs.resize(numInputs());
					for(VstInt32 i(0);i<numInputs();i++)	{
						inputs[i]=samplesV[i];
					}
					outputs.resize(numOutputs());
					for(VstInt32 i(0);i<numOutputs();i++) {
						outputs[i]=samplesV[i];
					}
				}
				else {
					//Ensure at least two outputs (Patch for GetRMSVol and WireDlg Scope)
					VstInt32 maxval = std::max(std::max(numInputs(),numOutputs()),2);
					//Note: Since in GenerateAudioInTicks we will swap the buffers, we need
					//to have the same amount on both sides. That's the reason of the *2 and using
					//themax as the size.
					InitializeSamplesVector(maxval*2);
					VstInt32 i=0;
					inputs.resize(maxval);
					for(;i<maxval;i++)	{
						inputs[i]=samplesV[i];
					}
					outputs.resize(maxval);
					for(VstInt32 j(0);j<maxval;j++, i++) {
						outputs[j]=samplesV[i];
					}
				}

				for(int i(0) ; i < MAX_TRACKS; ++i)
				{
					trackNote[i].key = 255; // No Note.
					trackNote[i].midichan = 0;
				}
				_sDllName= (char*)(loadstruct.pluginloader->sFileName);
				char temp[kVstMaxVendorStrLen+1];
				memset(temp,0,sizeof(temp));
				if ( GetPlugCategory() != kPlugCategShell )
				{
					// GetEffectName is the better option to GetProductString.
					// To the few that they show different values in these,
					// synthedit plugins show only "SyntheditVST" in GetProductString()
					// and others like battery 1 or psp-nitro, don't have GetProductString(),
					// so it's almost a no-go.
					std::stringstream ss;
					if (GetEffectName(temp) && temp[0]) { ss <<temp; temp[0]='\0'; }
					if (GetProductString(temp) && temp[0]) {
						if (ss.str().empty()) {	ss << temp; }
						else if(strcmp(ss.str().c_str(),temp)) {	ss<< " (" << temp << ")"; }
						temp[0]='\0';
					}
					if (ss.str().empty())
					{
						std::string temp;
						std::string::size_type pos;
						pos = _sDllName.rfind('\\');
						if(pos==std::string::npos)
							temp=_sDllName;
						else
							temp=_sDllName.substr(pos+1);
						ss << temp.substr(0,temp.rfind('.'));
					}
					_sProductName= ss.str();
					// This is a safety measure against some plugins that have noise at its output for some
					// unexplained reason ( example : mda piano.dll )
					GenerateAudioInTicks(0,STREAM_SIZE);
				}
				else
				{
					std::string temp;
					std::string::size_type pos;
					pos = _sDllName.rfind('\\');
					if(pos==std::string::npos)
						temp=_sDllName;
					else
						temp=_sDllName.substr(pos+1);
					_sProductName=temp.substr(0,temp.rfind('.'));
				}
				if(GetVendorString(temp) && temp[0]) _sVendorName = temp;
				else _sVendorName = "Unknown vendor";
				std::strncpy(_editName,_sProductName.c_str(),sizeof(_editName)-1);
				_editName[sizeof(_editName)-1]='\0';
			}


			void Plugin::change_buffer(std::vector<float*>& buf) {
				if(WillProcessReplace()) {
					Machine::change_buffer(buf);
					for(VstInt32 i(0);i<numInputs();i++)	{
						inputs[i]=samplesV[i];
					}
					outputs.resize(numOutputs());
					for(VstInt32 i(0);i<numOutputs();i++) {
						outputs[i]=samplesV[i];
					}				
				}
				else {
					UNIVERSALIS__COMPILER__WARNING("NOT IMPLEMENTED FOR PROCESS_ACCUMULATE");
				}
			}

			bool Plugin::OnIOChanged() {
				//TODO: IOChanged does not only reflect input pin changes.
				// it also (usually?) means changing latency, but Psycle does not support initialDelay.
				CExclusiveLock crit = Global::player().GetLockObject();
				//Avoid possible deadlocks
				if(crit.Lock(200)) {
					MainsChanged(false);
					VstInt32 numIns = numInputs();
					VstInt32 numOuts = numOutputs();
					VstSpeakerArrangement* SAip = 0;
					VstSpeakerArrangement* SAop = 0;
					if (GetSpeakerArrangement(&SAip,&SAop)) {
						numIns = SAip->numChannels;
						numOuts = SAop->numChannels;
					}
					if(WillProcessReplace()) {
						//Ensure at least two outputs (Patch for GetRMSVol and WireDlg Scope)
						VstInt32 maxval = std::max(std::max(numIns,numOuts),2);
						InitializeSamplesVector(maxval);
						inputs.resize(numIns);
						for(VstInt32 i(0);i<numIns;i++)	{
							inputs[i]=samplesV[i];
						}
						outputs.resize(numOuts);
						for(VstInt32 i(0);i<numOuts;i++) {
							outputs[i]=samplesV[i];
						}
					}
					else {
						//Ensure at least two outputs (Patch for GetRMSVol and WireDlg Scope)
						VstInt32 maxval = std::max(std::max(numIns,numOuts),2);
						//Note: Since in GenerateAudioInTicks we will swap the buffers, we need
						//to have the same amount on both sides. That's the reason of the *2 and using
						//maxval as the size in both vectors.
						InitializeSamplesVector(maxval*2);
						inputs.resize(maxval);
						for(VstInt32 i=0;i<maxval;i++)	{
							inputs[i]=samplesV[i];
						}
						outputs.resize(maxval);
						for(VstInt32 i(0);i<maxval;i++) {
							outputs[i]=samplesV[i+maxval];
						}
					}
					//validate pin mappings of wires already connected
					for(int i(0);i< MAX_CONNECTIONS;i++) {
						if(inWires[i].Enabled()) {
							bool changed =false;
							Wire::Mapping map = inWires[i].GetMapping();
							Wire::Mapping::iterator ite = map.begin();
							while (ite != map.end()) {
								if (ite->second >= numIns) {
									ite = map.erase(ite);
									changed=true;
								}
								else {
									ite++;
								}
							}
							if(changed) {
								inWires[i].ChangeMapping(map);
							}
						}
						if(outWires[i] && outWires[i]->Enabled()) {
							bool changed =false;
							Wire::Mapping map = outWires[i]->GetMapping();
							Wire::Mapping::iterator ite = map.begin();
							while (ite != map.end()) {
								if (ite->first>= numOuts) {
									ite = map.erase(ite);
									changed=true;
								}
								else {
									ite++;
								}
							}
							if(changed) {
								outWires[i]->ChangeMapping(map);
							}
						}
					}
					MainsChanged(true);
					return true;
				}
				return false;
			}
			void Plugin::GetParamValue(int numparam, char * parval)
			{
				try
				{
					if(numparam < numParams())
					{
						if(!DescribeValue(numparam, parval))
						{
							std::sprintf(parval,"%.0f",GetParameter(numparam) * Host::quantizationVal());
						}
					}
					else std::strcpy(parval,"Out of Range");
				}
				catch(const std::exception &)
				{
					// [bohan]
					// exception blocked here for now,
					// but we really should do something...
					//throw;
					std::strcpy(parval, "fucked up");
				}
			}

			bool Plugin::DescribeValue(int parameter, char * psTxt)
			{
				try {
					if(parameter >= 0 && parameter < numParams())
					{
						char par_display[kVstMaxProgNameLen+1]={0}; 
						char par_label[kVstMaxProgNameLen+1]={0};
						GetParamDisplay(parameter,par_display);
						GetParamLabel(parameter,par_label);
						std::sprintf(psTxt, "%s(%s)", par_display, par_label);
						return true;
					}
					else std::sprintf(psTxt, "Invalid NumParams Value");
				}
				catch(...){}
				return false;
			}
			// AEffect asks host about its input/outputspeakers.
			VstSpeakerArrangement* Plugin::OnHostInputSpeakerArrangement()
			{
				//TODO: Implement a per-plugin channel setup (i.e. not per wire)
				return NULL;
			}
			VstSpeakerArrangement* Plugin::OnHostOutputSpeakerArrangement()
			{
				//TODO: Implement a per-plugin channel setup (i.e. not per wire)
				return NULL;
			}
			/// IsIn/OutputConnected are called when the machine receives a mainschanged(on), so the correct way to work is
			/// doing an "off/on" when a connection changes.
			bool Plugin::DECLARE_VST_DEPRECATED(IsInputConnected)(VstInt32 input)
			{
				for (int i(0);i<inWires.size();i++) {
					if(inWires[i].Enabled()) {
						const Wire::Mapping& map = inWires[i].GetMapping();
						for(int j(0);j<map.size();j++) {
							if ( map[j].second == input) {
								return true;
							}
						}
					}
				}
				return false;
			} 
			bool Plugin::DECLARE_VST_DEPRECATED(IsOutputConnected)(VstInt32 output)
			{
				for (int i(0);i<outWires.size();i++) {
					if(outWires[i] && outWires[i]->Enabled()) {
						const Wire::Mapping& map = outWires[i]->GetMapping();
						for(int j(0);j<map.size();j++) {
							if ( map[j].first == output) {
								return true;
							}
						}
					}
				}
				return false;
			}
			void Plugin::InputMapping(Wire::Mapping const &mapping, bool enabled)
			{
				for(int i(0); i < mapping.size();i++) {
					ConnectInput(mapping[i].second, enabled);
				}
			}

			void Plugin::OutputMapping(Wire::Mapping const &mapping, bool enabled)
			{
				for(int i(0); i < mapping.size();i++) {
					ConnectOutput(mapping[i].first, enabled);
				}
			}

			std::string Plugin::GetOutputPinName(int pin) const {
				VstPinProperties pinprop;
				ZeroMemory(&pinprop, sizeof(VstPinProperties));
				if (GetOutputProperties(pin,&pinprop) ) {
					if(pinprop.label[0] != '\0') {
						return pinprop.label;
					}
					else if (pinprop.shortLabel[0] != '\0' ) {
						return pinprop.shortLabel;
					}
				}
				VstSpeakerArrangement *sai, *sao;
				if(GetSpeakerArrangement(&sai,&sao)) {
					return GetNameFromSpeakerArrangement(*sao, pin);
				}
				return (pin%2)?"Right":"Left";

			}
			std::string Plugin::GetInputPinName(int pin) const {
				VstPinProperties pinprop;
				ZeroMemory(&pinprop, sizeof(VstPinProperties));
				if(GetInputProperties(pin,&pinprop)) {
					if(pinprop.label[0] != '\0') {
						return pinprop.label;
					}
					else if (pinprop.shortLabel[0] != '\0' ) {
						return pinprop.shortLabel;
					}
				}
				VstSpeakerArrangement *sai, *sao;
				if(GetSpeakerArrangement(&sai,&sao)) {
					return GetNameFromSpeakerArrangement(*sai,pin);
				}
				return (pin%2)?"Right":"Left";
			}

			bool Plugin::LoadSpecificChunk(RiffFile * pFile, int version)
			{
				try {
					UINT size;
					unsigned char _program;
					pFile->Read(&size, sizeof size );
					if(size)
					{
						UINT count;
						pFile->Read(&_program, sizeof _program);
						pFile->Read(&count, sizeof count);
						size -= sizeof _program + sizeof count + sizeof(float) * count;
						if(!size)
						{
							BeginSetProgram();
							SetProgram(_program);
							for(UINT i(0) ; i < count ; ++i)
							{
								float temp;
								pFile->Read(&temp, sizeof temp);
								SetParameter(i, temp);
							}
							EndSetProgram();
						}
						else
						{
							BeginSetProgram();
							SetProgram(_program);
							EndSetProgram();
							pFile->Skip(sizeof(float) *count);
							if(ProgramIsChunk())
							{
								char * data(new char[size]);
								pFile->Read(data, size); // Number of parameters
								SetChunk(data,size);
								zapArray(data);
							}
							else
							{
								// there is a data chunk, but this machine does not want one.
								pFile->Skip(size);
								return false;
							}
						}
					}
					return true;
				}
				catch(...){return false;}
			}

			void Plugin::SaveSpecificChunk(RiffFile * pFile) 
			{
				try {
					UINT count(numParams());
					unsigned char _program=0;
					UINT size(sizeof _program + sizeof count);
					UINT chunksize(0);
					char * pData(0);
					bool b = ProgramIsChunk();
					if(b)
					{
						count=0;
						chunksize = GetChunk((void**)&pData);
						size+=chunksize;
					}
					else
					{
						 size+=sizeof(float) * count;
					}
					pFile->Write(&size, sizeof size);
					_program = static_cast<unsigned char>(GetProgram());
					pFile->Write(&_program, sizeof _program);
					pFile->Write(&count, sizeof count);

					if(b)
					{
						pFile->Write(pData, chunksize);
					}
					else
					{
						for(UINT i(0); i < count; ++i)
						{
							float temp = GetParameter(i);
							pFile->Write(&temp, sizeof temp);
						}
					}
				} catch(...) {
				}
			}

			VstMidiEvent* Plugin::reserveVstMidiEvent() {
				assert(queue_size>=0 && queue_size <= MAX_VST_EVENTS);
				if(queue_size >= MAX_VST_EVENTS) {
					loggers::information()("vst::Plugin warning: event buffer full, midi message could not be sent to plugin");
					return NULL;
				}
				return &midievent[queue_size++];
			}

			VstMidiEvent* Plugin::reserveVstMidiEventAtFront() {
				assert(queue_size>=0 && queue_size <= MAX_VST_EVENTS);
				if(queue_size >= MAX_VST_EVENTS) {
					loggers::information()("vst::Plugin warning: event buffer full, midi message could not be sent to plugin");
					return NULL;
				}
				for(int i=queue_size; i > 0 ; --i) midievent[i] = midievent[i - 1];
				queue_size++;
				return &midievent[0];
			}


			bool Plugin::AddMIDI(unsigned char data0, unsigned char data1, unsigned char data2,unsigned int sampleoffset)
			{
				VstMidiEvent * pevent(reserveVstMidiEvent());
				if(!pevent) return false;
				pevent->type = kVstMidiType;
				pevent->byteSize = 24;
				pevent->deltaFrames = sampleoffset;
				pevent->flags = 0;
				pevent->detune = 0;
				pevent->noteLength = 0;
				pevent->noteOffset = 0;
				pevent->reserved1 = 0;
				pevent->reserved2 = 0;
				pevent->noteOffVelocity = 0;
				pevent->midiData[0] = data0;
				pevent->midiData[1] = data1 & 0x7f;
				pevent->midiData[2] = data2 & 0x7f;
				pevent->midiData[3] = 0;
				return true;
			}
			bool Plugin::AddNoteOn(unsigned char channel, unsigned char key, unsigned char velocity, unsigned char midichannel,unsigned int sampleoffset, bool slide)
			{
				if(trackNote[channel].key != notecommands::empty && !slide) {
					AddNoteOff(channel, true);
				}

				if(AddMIDI(0x90 | midichannel /*Midi On*/, key, velocity,sampleoffset)) {
					if(trackNote[channel].key != notecommands::empty && slide) {
						// Noteoff previous glide note after the new note, but before overwriting the trackNote array.
						AddNoteOff(channel);
					}
					note thisnote;
					thisnote.key = key;
					thisnote.midichan = midichannel;
					trackNote[channel] = thisnote;
					return true;
				}
				return false;
			}

			bool Plugin::AddNoteOff(unsigned char channel, bool addatStart, unsigned int sampleoffset)
			{
				if(trackNote[channel].key == notecommands::empty) {
					return false;
				}
				VstMidiEvent * pevent;
				if( addatStart)
				{
					// PATCH:
					// When a new note enters, it adds a note-off for the previous note playing in
					// the track (this is ok). But if you have like: A-4 C-5 and in the next line
					// C-5 E-5 , you will only hear E-5.
					// Solution: Move the NoteOffs at the beginning.
					pevent = reserveVstMidiEventAtFront();
				}
				else 
				{
					pevent = reserveVstMidiEvent();
				}
				if(!pevent)
					return false;
				note thenote = trackNote[channel];
				pevent->type = kVstMidiType;
				pevent->byteSize = 24;
				pevent->deltaFrames = sampleoffset;
				pevent->flags = 0;
				pevent->detune = 0;
				pevent->noteLength = 0;
				pevent->noteOffset = 0;
				pevent->reserved1 = 0;
				pevent->reserved2 = 0;
				pevent->noteOffVelocity = 0;
				pevent->midiData[0] = 0x80 | thenote.midichan; //midichannel; // Midi Off
				pevent->midiData[1] = thenote.key;
				pevent->midiData[2] = 0;
				pevent->midiData[3] = 0;

				trackNote[channel].key = 255;
				trackNote[channel].midichan = 0;
				return true;
			}
			void Plugin::SendMidi()
			{
				assert(queue_size >= 0);
				assert(queue_size <= MAX_VST_EVENTS);

				try {
					if(queue_size > 0)
					{
						// Prepare MIDI events and free queue dispatching all events
						mevents.numEvents = queue_size;
						mevents.reserved = 0;
						for(int q(0) ; q < queue_size ; ++q) {
	#ifndef NDEBUG

							// assert that events are sent in order.
							// although the standard doesn't require this,
							// many synths rely on this.
							if(q>0) {
								assert(midievent[q-1].deltaFrames <= 
									midievent[q].deltaFrames);
							}
	#endif

							mevents.events[q] = reinterpret_cast<VstEvent*>(&midievent[q]);
						}
						//Finally Send the events.
						queue_size = 0;
	//					WantsMidi(ProcessEvents(reinterpret_cast<VstEvents*>(&mevents)));
						ProcessEvents(reinterpret_cast<VstEvents*>(&mevents));
					}
				}
				catch(...){}
			}
			void Plugin::PreWork(int numSamples,bool clear, bool measure_cpu_usage)
			{
				Machine::PreWork(numSamples,clear, measure_cpu_usage);
				cpu_time_clock::time_point t0;
				if(measure_cpu_usage) t0 = cpu_time_clock::now();

				for (int midiChannel=0; midiChannel<16; midiChannel++)
				{
					if(NSActive[midiChannel])
					{
						NSSamples[midiChannel]=NSSamples[midiChannel]%TWEAK_SLIDE_SAMPLES;
						int ns = numSamples + NSSamples[midiChannel] - TWEAK_SLIDE_SAMPLES;
						while(ns >= 0)
						{
							NSCurrent[midiChannel] += NSDelta[midiChannel];
							if(
								(NSDelta[midiChannel] > 0 && NSCurrent[midiChannel] >= NSDestination[midiChannel]) ||
								(NSDelta[midiChannel] < 0 && NSCurrent[midiChannel] <= NSDestination[midiChannel]))
							{
								NSCurrent[midiChannel] = NSDestination[midiChannel];
								NSDelta[midiChannel] = 0;
								NSActive[midiChannel] = false;
							}
							AddMIDI(0xE0 | midiChannel,LSB(NSCurrent[midiChannel]),MSB(NSCurrent[midiChannel]),NSSamples[midiChannel]);
							 
							NSSamples[midiChannel]+=std::min(TWEAK_SLIDE_SAMPLES,ns);
							ns-=TWEAK_SLIDE_SAMPLES;
						}
					}
				}
				if(measure_cpu_usage) {
					cpu_time_clock::time_point const t1(cpu_time_clock::now());
					Global::song().accumulate_routing_time(t1 - t0);
				}
			}
			void Plugin::Tick(int channel, PatternEntry * pData)
			{
				const int note = pData->_note;
				int midiChannel;
				if (pData->_inst == 0xFF) midiChannel = 0;
				else midiChannel = pData->_inst & 0x0F;

				if(note == notecommands::tweak || note == notecommands::tweakeffect) // Tweak Command
				{
					const float value(((pData->_cmd * 256) + pData->_parameter) / 65535.0f);
					int param = translate_param(pData->_inst);
					SetParameter(param, value);
				}
				else if(note == notecommands::tweakslide)
				{
					int param = translate_param(pData->_inst);
					int i;
					if(TWSActive)
					{
						for(i = 0 ; i < MAX_TWS; ++i) if(TWSInst[i] == param && TWSDelta[i]) break;
						if(i == MAX_TWS) for(i = 0 ; i < MAX_TWS; ++i) if(!TWSDelta[i]) break;
					}
					else for(i = MAX_TWS - 1 ; i > 0 ; --i) TWSDelta[i] = 0;
					if(i < MAX_TWS)
					{
						TWSDestination[i] = ((pData->_cmd * 256) + pData->_parameter) / 65535.0f;
						TWSInst[i] = param;
						TWSCurrent[i] = GetParameter(TWSInst[i]);
						TWSDelta[i] = ((TWSDestination[i] - TWSCurrent[i]) * TWEAK_SLIDE_SAMPLES) / Global::player().SamplesPerRow();
						TWSSamples = 0;
						TWSActive = true;
					}
					else
					{
						// we have used all our slots, just send a twk
						const float value(((pData->_cmd * 256) + pData->_parameter) / 65535.0f);
						int param = translate_param(pData->_inst);
						SetParameter(param, value);
					}
				}
				else if(pData->_note == notecommands::midicc && pData->_inst >= 0x80 && pData->_inst < 0xFF) // Mcm (MIDI CC) Command
				{
					AddMIDI(pData->_inst, pData->_cmd, pData->_parameter);
				}
				else
				{
					if(pData->_note==notecommands::midicc) {
						pData->_note=notecommands::empty;
						pData->_inst=0xFF;
					}
					if(pData->_cmd == 0xC1) //set the pitch bend range
					{
						rangeInSemis = pData->_parameter;
					}
					else if(pData->_cmd == 0xC2) //Panning
					{
						AddMIDI(0xB0 | midiChannel, 0x0A,pData->_parameter>>1);
					}

					if(note < notecommands::release) // Note on
					{
						int semisToSlide(0);

						if((pData->_cmd == 0x10) && ((pData->_inst & 0xF0) == 0x80 || (pData->_inst & 0xF0) == 0x90)) // _OLD_ MIDI Command
						{
							AddMIDI(pData->_inst, note, pData->_parameter);
						}
						else if((pData->_cmd & 0xF0) == 0xD0 || (pData->_cmd & 0xF0) == 0xE0) //semislide
						{
							if (NSCurrent[midiChannel] != pitchWheelCentre)
							{
								AddMIDI(0xE0 | midiChannel,LSB(pitchWheelCentre),MSB(pitchWheelCentre));
							}
							///\todo: sorry???
							currentSemi[midiChannel] = 0;

							if ((pData->_cmd & 0xF0) == 0xD0) //pitch slide down
							{
								semisToSlide = -(pData->_cmd & 0x0F);
								if (semisToSlide < (-rangeInSemis - currentSemi[midiChannel]))
									semisToSlide = (-rangeInSemis - currentSemi[midiChannel]);
							}
							else							  //pitch slide up
							{
								semisToSlide = pData->_cmd & 0x0F;
								if (semisToSlide > (rangeInSemis - currentSemi[midiChannel]))
									semisToSlide = (rangeInSemis - currentSemi[midiChannel]);
							}

							int speedToSlide = pData->_parameter;
							
							NSCurrent[midiChannel] = pitchWheelCentre + (currentSemi[midiChannel] * (pitchWheelCentre / rangeInSemis));
							NSDestination[midiChannel] = NSCurrent[midiChannel] + ((pitchWheelCentre / rangeInSemis) * semisToSlide);						
							NSDelta[midiChannel] = ((NSDestination[midiChannel] - NSCurrent[midiChannel]) * (speedToSlide*2)) / Global::player().SamplesPerRow();
							NSSamples[midiChannel] = 0;
							NSActive[midiChannel] = true;
							currentSemi[midiChannel] = currentSemi[midiChannel] + semisToSlide;
							AddNoteOn(channel, note, 127, midiChannel);
						}
						else if((pData->_cmd == 0xC3) && (oldNote[midiChannel]!=-1))//slide to note
						{
							semisToSlide = note - oldNote[midiChannel];
							int speedToSlide = pData->_parameter;
							NSDestination[midiChannel] = NSDestination[midiChannel] + ((pitchWheelCentre / rangeInSemis) * semisToSlide);
							NSTargetDistance[midiChannel] = (NSDestination[midiChannel] - NSCurrent[midiChannel]);
							NSDelta[midiChannel] = (NSTargetDistance[midiChannel] * (speedToSlide*2)) / Global::player().SamplesPerRow();
							NSSamples[midiChannel] = 0;
							NSActive[midiChannel] = true;
						}
						else //basic note on
						{
							if (NSCurrent[midiChannel] != pitchWheelCentre)
							{
								NSActive[midiChannel] = false;
								AddMIDI(0xE0 | midiChannel,LSB(pitchWheelCentre),MSB(pitchWheelCentre));
								NSCurrent[midiChannel] = pitchWheelCentre;
								NSDestination[midiChannel] = NSCurrent[midiChannel];
								currentSemi[midiChannel] = 0;
							}
							//AddMIDI(0xB0 | midiChannel,0x07,127); // channel volume. Reset it for the new note.
							AddNoteOn(channel,note,(pData->_cmd == 0x0C)?pData->_parameter/2:127,midiChannel,0,(pData->_mach==0xFF));
						}
						if (((pData->_cmd & 0xF0) == 0xD0) || ((pData->_cmd & 0xF0) == 0xE0))
							oldNote[midiChannel] = note + semisToSlide;								
						else
							oldNote[midiChannel] = note;
					}
					else if(note == notecommands::release) // Note Off. 
					{
						AddNoteOff(channel);
						oldNote[midiChannel] = -1;
					}
					else if(pData->_note == notecommands::empty)
					{
						int semisToSlide(0);

						if (pData->_cmd == 0x10) // _OLD_ MIDI Command
						{
							AddMIDI(pData->_inst,pData->_parameter);
						}
//						else if (pData->_cmd == 0x0C) // channel volume.
//						{
						//	AddMIDI(0xB0 | midiChannel,0x07,pData->_parameter>>1);
//						}
						else if (pData->_cmd == 0x0C) // channel aftertouch.
						{
							AddMIDI(0xD0 | midiChannel,pData->_parameter>>1);
						}
						else if(pData->_cmd == 0xC3) //slide to note . Used to change the speed.
						{
							int speedToSlide = pData->_parameter;
							NSDelta[midiChannel] = (NSTargetDistance[midiChannel] * (speedToSlide*2)) / Global::player().SamplesPerRow();
						}
						else if((pData->_cmd & 0xF0) == 0xD0 || (pData->_cmd & 0xF0) == 0xE0) //semislide
						{
							if (NSCurrent[midiChannel] != NSDestination[midiChannel])
							{
								AddMIDI(0xE0 | midiChannel,LSB(NSDestination[midiChannel]),MSB(NSDestination[midiChannel]));
							}

							if ((pData->_cmd & 0xF0) == 0xD0) //pitch slide down
							{
								semisToSlide = -(pData->_cmd & 0x0F);
								if (semisToSlide < (-rangeInSemis - currentSemi[midiChannel]))
									semisToSlide = (-rangeInSemis - currentSemi[midiChannel]);
							}
							else							  //pitch slide up
							{
								semisToSlide = pData->_cmd & 0x0F;
								if (semisToSlide > (rangeInSemis - currentSemi[midiChannel]))
									semisToSlide = (rangeInSemis - currentSemi[midiChannel]);
							}

							int speedToSlide = pData->_parameter;
							
							NSCurrent[midiChannel] = pitchWheelCentre + (currentSemi[midiChannel] * (pitchWheelCentre / rangeInSemis));
							NSDestination[midiChannel] = NSCurrent[midiChannel] + ((pitchWheelCentre / rangeInSemis) * semisToSlide);
							NSDelta[midiChannel] = ((NSDestination[midiChannel] - NSCurrent[midiChannel]) * (speedToSlide*2)) / Global::player().SamplesPerRow();
							NSSamples[midiChannel] = 0;
							NSActive[midiChannel] = true;
							currentSemi[midiChannel] = currentSemi[midiChannel] + semisToSlide;
							oldNote[midiChannel] = oldNote[midiChannel] + semisToSlide;								
						}
					}
				}
			}

			void Plugin::Stop()
			{
				for(int track(0) ; track < MAX_TRACKS ; ++track) AddNoteOff(track); //note off any known note.
				for(int chan(0) ; chan < 16 ; ++chan) AddMIDI(0xB0 | chan, 0x7B); // Send All Notes Off
				for(int chan(0) ; chan < 16 ; ++chan) AddMIDI(0xB0 | chan, 0x78); // Send All Sounds Off
			}

			int Plugin::GenerateAudioInTicks(int /*startSample*/,  int numSamples)
			{
				if(_mode == MACHMODE_GENERATOR){
					if (!_mute) Standby(false);
					else Standby(true);
				}
				if(!_mute)
				{
					if (_mode == MACHMODE_GENERATOR || (!Standby() && !Bypass()) || bCanBypass)
					{
/*						The following is now being done in the OnTimer() thread of the UI.
						if(bNeedIdle) 
						{
							try
							{
								Idle();
							}
							catch (...)
							{
								// o_O`
							}
						}
*/
						try
						{
							/*if (WantsMidi())*/ SendMidi();
						}
						catch(const std::exception &)
						{
							// o_O`
						}
						///\todo: Move all this messy retrigger code to somewhere else. (it is repeated in each machine subclass)
						// Store temporary pointers so that we can increase the address in the retrigger code
						float ** tempinputs;
						float ** tempoutputs;
						if (inputs.size() > 0) {
							tmpinputs = inputs;
							tempinputs = &tmpinputs[0];
						}
						else {
							tempinputs = NULL;
						}
						if (outputs.size() > 0) {
							tmpoutputs = outputs;
							tempoutputs = &tmpoutputs[0];
						}
						else {
							tempoutputs = NULL;
						}
						
						int ns(numSamples);
						while(ns)
						{
							int nextevent;
							if(TWSActive) nextevent = TWSSamples; else nextevent = ns + 1;
							for(int i(0) ; i < Global::song().SONGTRACKS ; ++i)
							{
								if(TriggerDelay[i]._cmd && TriggerDelayCounter[i] < nextevent) nextevent = TriggerDelayCounter[i];
							}
							if(nextevent > ns)
							{
								if(TWSActive) TWSSamples -= ns;
								for(int i(0) ; i < Global::song().SONGTRACKS; ++i)
								{
									// come back to this
									if(TriggerDelay[i]._cmd) TriggerDelayCounter[i] -= ns;
								}
								try
								{
									if(WillProcessReplace())
										ProcessReplacing(tempinputs, tempoutputs, ns);
									else
										Process(tempinputs, tempoutputs, ns);
								}
								catch(const std::exception &)
								{
									// o_O`
								}
								ns = 0;
							}
							else
							{
								if(nextevent)
								{
									ns -= nextevent;
									try
									{
										if(WillProcessReplace())
											ProcessReplacing(tempinputs, tempoutputs, nextevent);
										else
											Process(tempinputs, tempoutputs, nextevent);
									}
									catch(const std::exception &)
									{
										// o_O`
									}
									for(int i(0) ; i < numInputs() ; ++i)
									{
										tempinputs[i]+=nextevent;
									}
									for(int i(0) ; i < numOutputs() ; ++i)
									{
										tempoutputs[i]+=nextevent;
									}
								}
								if(TWSActive)
								{
									if(TWSSamples == nextevent)
									{
										int activecount = 0;
										TWSSamples = TWEAK_SLIDE_SAMPLES;
										for(int i(0) ; i < MAX_TWS; ++i)
										{
											if(TWSDelta[i])
											{
												TWSCurrent[i] += TWSDelta[i];
												if(
													(TWSDelta[i] > 0 && TWSCurrent[i] >= TWSDestination[i]) ||
													(TWSDelta[i] < 0 && TWSCurrent[i] <= TWSDestination[i]))
												{
													TWSCurrent[i] = TWSDestination[i];
													TWSDelta[i] = 0;
												}
												else ++activecount;
												SetParameter(TWSInst[i],TWSCurrent[i]);
											}
										}
										if(activecount == 0) TWSActive = false;
									}
								}
								for(int i(0) ; i < Global::song().SONGTRACKS; ++i)
								{
									// come back to this
									if(TriggerDelay[i]._cmd == PatternCmd::NOTE_DELAY)
									{
										if(TriggerDelayCounter[i] == nextevent)
										{
											// do event
											Tick(i, &TriggerDelay[i]);
											TriggerDelay[i]._cmd = 0;
										}
										else TriggerDelayCounter[i] -= nextevent;
									}
									else if(TriggerDelay[i]._cmd == PatternCmd::RETRIGGER)
									{
										if(TriggerDelayCounter[i] == nextevent)
										{
											// do event
											Tick(i, &TriggerDelay[i]);
											TriggerDelayCounter[i] = (RetriggerRate[i] * Global::player().SamplesPerRow()) / 256;
										}
										else TriggerDelayCounter[i] -= nextevent;
									}
									else if(TriggerDelay[i]._cmd == PatternCmd::RETR_CONT)
									{
										if(TriggerDelayCounter[i] == nextevent)
										{
											// do event
											Tick(i, &TriggerDelay[i]);
											TriggerDelayCounter[i] = (RetriggerRate[i] * Global::player().SamplesPerRow()) / 256;
											int parameter(TriggerDelay[i]._parameter & 0x0f);
											if(parameter < 9) RetriggerRate[i] += 4 * parameter;
											else
											{
												RetriggerRate[i] -= 2 * (16 - parameter);
												if(RetriggerRate[i] < 16) RetriggerRate[i] = 16;
											}
										}
										else TriggerDelayCounter[i] -= nextevent;
									}
								}
							}
						}
						if (!WillProcessReplace())
						{
							// We need the output in samplesV, so we invert the
							// pointers to avoid copying the content of outputs into inputs.
							VstInt32 maxval = std::max(std::max(numInputs(),numOutputs()),2);
							VstInt32 out=maxval;
							for(VstInt32 in=0; in<maxval;in++, out++) {
								float* const tempSamples = samplesV[in];
								samplesV[in] = (inputs[in] = samplesV[out]); 
								samplesV[out] = (outputs[in] = tempSamples);
							}
						}
						UpdateVuAndStanbyFlag(numSamples);
					}
				}
				return numSamples;
			}



			/// old file format vomit, don't look at it.
			///////////////////////////////////////////////
			bool Plugin::PreLoad(RiffFile * pFile, unsigned char &_program, int &_instance)
			{
				Machine::Init();

				pFile->Read(&_editName, 16);	//Remove when changing the fileformat.
				_editName[15]='\0';

				legacyWires.resize(MAX_CONNECTIONS);
				for(int i = 0; i < MAX_CONNECTIONS; i++) {
					pFile->Read(&legacyWires[i]._inputMachine,sizeof(legacyWires[i]._inputMachine));	// Incoming connections Machine number
				}
				for(int i = 0; i < MAX_CONNECTIONS; i++) {
					pFile->Read(&legacyWires[i]._outputMachine,sizeof(legacyWires[i]._outputMachine));	// Outgoing connections Machine number
				}
				for(int i = 0; i < MAX_CONNECTIONS; i++) {
					pFile->Read(&legacyWires[i]._inputConVol,sizeof(legacyWires[i]._inputConVol));	// Incoming connections Machine vol
					legacyWires[i]._wireMultiplier = 1.0f;
				}
				for(int i = 0; i < MAX_CONNECTIONS; i++) {
					pFile->Read(&legacyWires[i]._connection,sizeof(legacyWires[i]._connection));      // Outgoing connections activated
				}
				for(int i = 0; i < MAX_CONNECTIONS; i++) {
					pFile->Read(&legacyWires[i]._inputCon,sizeof(legacyWires[i]._inputCon));		// Incoming connections activated
				}
				pFile->Read(&_connectionPoint[0], sizeof(_connectionPoint));
				pFile->Skip(2*sizeof(int)); // numInputs and numOutputs

				pFile->Read(&_panning, sizeof(_panning));
				Machine::SetPan(_panning);
				pFile->Skip(109);

				bool old;
				pFile->Read(&old, sizeof old); // old format
				pFile->Read(&_instance, sizeof _instance); // ovst.instance
				if(old)
				{
					char mch;
					pFile->Read(&mch, sizeof mch);
					_program = 0;
				}
				else
				{
					pFile->Read(&_program, sizeof _program);
				}
				return true;
			}
			bool Plugin::LoadFromMac(vst::Plugin *pMac)
			{
				Machine::Init();
				strcpy(_editName,pMac->_editName);
				legacyWires.resize(MAX_CONNECTIONS);
				for(int i=0; i < MAX_CONNECTIONS; i++) {
					memcpy(&legacyWires[i],&pMac->legacyWires[i],sizeof(LegacyWire));
				}				
				memcpy(_connectionPoint,pMac->_connectionPoint,sizeof(_connectionPoint));
				Machine::SetPan(pMac->_panning);
				return true;
			}
			// Load for Old Psycle fileformat
			bool Plugin::LoadChunk(RiffFile * pFile)
			{
				bool b;
				try
				{
					b = ProgramIsChunk();
				}
				catch(const std::exception &)
				{
					b = false;
				}
				if(!b) return false;
				// read chunk size
				VstInt32 chunk_size;
				pFile->Read(&chunk_size, sizeof chunk_size);
				// read chunk data
				char * chunk(new char[chunk_size]);
				pFile->Read(chunk, chunk_size);
				try
				{
					SetChunk(chunk,chunk_size);
				}
				catch(const std::exception &)
				{
					// [bohan] hmm, so, data just gets lost?
					zapArray(chunk);
					return false;
				}
				zapArray(chunk);
				return true;
			}
		}
	}
}
