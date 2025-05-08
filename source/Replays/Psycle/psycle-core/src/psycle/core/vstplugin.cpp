// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>

#ifndef DIVERSALIS__OS__MICROSOFT
	UNIVERSALIS__COMPILER__WARNING("VST plugin is currently only implemented on Microsoft Windows operating system.")
#else
	#include "vstplugin.h"

	#include "cpu_time_clock.hpp"
	#include "player.h"
	#include "playertimeinfo.h"
	#include "fileio.h"

	#include <psycle/helpers/dsp.hpp>
	#include <universalis/os/aligned_alloc.hpp>
	#include <sstream>

	#ifdef DIVERSALIS__OS__MICROSOFT
		#include <universalis/os/include_windows_without_crap.hpp>
	#else
		#include <dlfcn.h>
	#endif

	// win32 note: plugins produced by mingw and msvc are binary-incompatible due to c++ abi ("this" pointer and std calling convention)

	namespace psycle { namespace core { namespace vst {

	using namespace helpers;

	/**************************************************************************/
	// Plugin
	float plugin::junk[MAX_BUFFER_LENGTH];
	int plugin::pitchWheelCentre(8191);

	plugin::plugin(MachineCallbacks* callbacks, MachineKey key, Machine::id_type id, LoadedAEffect &loadstruct)
		:Machine(callbacks, id)
		,CEffect(loadstruct)
		,key_(key)
		,queue_size(0)
		,requiresRepl(0)
		,requiresProcess(0)
		,rangeInSemis(2)
	{
		SetAudioRange(1.0f);
		_nCols=0;

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
		///\todo: for now we only support stereo
		if(!IsGenerator()) {
			defineInputAsStereo();
		}
		defineOutputAsStereo();

		for(int i(0) ; i < MAX_TRACKS; ++i)
		{
			trackNote[i].key = 255; // No Note.
			trackNote[i].midichan = 0;
		}


		// Compatibility hacks
		{
			if(uniqueId() == 0x41446c45 ) //"sc-101"
			{
				requiresRepl = true;
			}
		}

		std::memset(junk, 0, MAX_BUFFER_LENGTH * sizeof(float));
		for(int i(2) ; i < vst::max_io ; ++i)
		{
			inputs[i]=junk;
			outputs[i]=junk;
		}
		inputs[0] = _pSamplesL;
		inputs[1] = _pSamplesR;
		try
		{
			if (WillProcessReplace())
			{
				_pOutSamplesL = _pOutSamplesR = junk;
				outputs[0] = inputs[0];
				outputs[1] = inputs[1];
			}
			else
			{
				universalis::os::aligned_memory_alloc(16, _pOutSamplesL, MAX_BUFFER_LENGTH);
				universalis::os::aligned_memory_alloc(16, _pOutSamplesR, MAX_BUFFER_LENGTH);
				dsp::Clear(_pOutSamplesL, MAX_BUFFER_LENGTH);
				dsp::Clear(_pOutSamplesR, MAX_BUFFER_LENGTH);
				outputs[0] = _pOutSamplesL;
				outputs[1] = _pOutSamplesR;
			}
		}PSYCLE__HOST__CATCH_ALL(crashclass);


		char temp[kVstMaxVendorStrLen];
		std::memset(temp, 0, sizeof temp);
		try {
			if(GetPlugCategory() != kPlugCategShell) {
				// GetEffectName is the better option to GetProductString.
				// To the few that they show different values in these,
				// synthedit plugins show only "SyntheditVST" in GetProductString()
				// and others like battery 1 or psp-nitro, don't have GetProductString(),
				// so it's almost a no-go.
				if(GetEffectName(temp) && temp[0]) _psName = temp;
				else if(GetProductString(temp) && temp[0]) _psName = temp;
				// This is a safe measure against some plugins that have noise at its output for some
				// unexplained reason ( example : mda piano.dll )
				recursive_process(MAX_BUFFER_LENGTH);
			}

			if(_psName.empty()) _psName = key.dllName();

			if(GetVendorString(temp) && temp[0]) _psAuthor = temp;
			else _psAuthor = "Unknown vendor";
			SetEditName(_psName);
		} PSYCLE__HOST__CATCH_ALL(crashclass);
	}

	plugin::~plugin( ) throw() {
		if(aEffect) {
			if(!WillProcessReplace()) {
				universalis::os::aligned_memory_dealloc(_pOutSamplesL);
				universalis::os::aligned_memory_dealloc(_pOutSamplesR);
			}
		}
	}


	void plugin::GetParamValue(int numparam, char * parval) const
	{
		try
		{
			if(numparam < numParams())
			{
				if(!DescribeValue(numparam, parval))
				{
					std::sprintf(parval,"%.0f",GetParameter(numparam) * CVSTHost::GetQuantization());
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

	bool plugin::DescribeValue(int parameter, char * psTxt) const
	{
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
		return false;
	}


	VstMidiEvent* plugin::reserveVstMidiEvent() {
		assert(queue_size>=0 && queue_size <= MAX_VST_EVENTS);
		if(queue_size >= MAX_VST_EVENTS) {
			//loggers::info("vst::plugin warning: event buffer full, midi message could not be sent to plugin");
			return NULL;
		}
		return &midievent[queue_size++];
	}

	VstMidiEvent* plugin::reserveVstMidiEventAtFront() {
		assert(queue_size>=0 && queue_size <= MAX_VST_EVENTS);
		if(queue_size >= MAX_VST_EVENTS) {
			//loggers::info("vst::plugin warning: event buffer full, midi message could not be sent to plugin");
			return NULL;
		}
		for(int i=queue_size; i > 0 ; --i) midievent[i] = midievent[i - 1];
		queue_size++;
		return &midievent[0];
	}


	bool plugin::AddMIDI(unsigned char data0, unsigned char data1, unsigned char data2,unsigned int sampleoffset)
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
		pevent->midiData[1] = data1;
		pevent->midiData[2] = data2;
		pevent->midiData[3] = 0;
		return true;
	}
	bool plugin::AddNoteOn(unsigned char channel, unsigned char key, unsigned char velocity, unsigned char midichannel,unsigned int sampleoffset, bool slide)
	{
		if(trackNote[channel].key != notetypes::empty && !slide)
			AddNoteOff(channel, trackNote[channel].key, true);

		if(AddMIDI(0x90 | midichannel /*Midi On*/, key, velocity,sampleoffset)) {
			note thisnote;
			thisnote.key = key;
			thisnote.midichan = midichannel;
			trackNote[channel] = thisnote;
			return true;
		}
		return false;
	}

	bool plugin::AddNoteOff(unsigned char channel, unsigned char midichannel, bool addatStart,unsigned int sampleoffset)
	{
		if(trackNote[channel].key == notetypes::empty)
			return false;
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
		pevent->midiData[0] = 0x80 | static_cast<unsigned char>(trackNote[channel].midichan); //midichannel; // Midi Off
		pevent->midiData[1] = trackNote[channel].key;
		pevent->midiData[2] = 0;
		pevent->midiData[3] = 0;

		note thisnote;
		thisnote.key = 255;
		thisnote.midichan = 0;
		trackNote[channel] = thisnote;
		return true;
	}
	void plugin::SendMidi()
	{
		assert(queue_size >= 0);
		assert(queue_size <= MAX_VST_EVENTS);


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

					#if 0
					// assert that the note sequence is well-formed,
					// which means, no note-offs happen without a
					// corresponding preceding note-on.
					switch(midievent[q].midiData[0]&0xf0) {
					case 0x90: // note-on
						note_checker_.note_on(midievent[q].midiData[1],
							midievent[q].midiData[0]&0x0f);
						break;
					case 0x80: // note-off
						note_checker_.note_off(midievent[q].midiData[1],
							midievent[q].midiData[0]&0x0f);
						break;
					}
					#endif
				#endif
				mevents.events[q] = (VstEvent*) &midievent[q];
			}
			//Finally Send the events.
			queue_size = 0;
			//WantsMidi(ProcessEvents(reinterpret_cast<VstEvents*>(&mevents)));
			ProcessEvents(reinterpret_cast<VstEvents*>(&mevents));
		}
	}
	void plugin::Tick(int channel, const PatternEvent & pData) {
		const int note = pData.note();
		int midiChannel;
		if (pData.instrument() == 0xFF) midiChannel = 0;
		else midiChannel = pData.instrument() & 0x0F;

		if(note == notetypes::tweak) // Tweak Command
		{
			const float value(((pData.command() * 256) + pData.parameter()) / 65535.0f);
			SetParameter(pData.instrument(), value);
			Player::singleton().Tweaker = true;
		}
		else if(note == notetypes::tweak_slide)
		{
			int i;
			if(TWSActive)
			{
				for(i = 0 ; i < MAX_TWS; ++i) if(TWSInst[i] == pData.instrument() && TWSDelta[i]) break;
				if(i == MAX_TWS) for(i = 0 ; i < MAX_TWS; ++i) if(!TWSDelta[i]) break;
			}
			else for(i = MAX_TWS - 1 ; i > 0 ; --i) TWSDelta[i] = 0;
			if(i < MAX_TWS)
			{
				TWSDestination[i] = ((pData.command() * 256) + pData.parameter()) / 65535.0f;
				TWSInst[i] = pData.instrument();
				TWSCurrent[i] = GetParameter(TWSInst[i]);
				TWSDelta[i] = ((TWSDestination[i] - TWSCurrent[i]) * TWEAK_SLIDE_SAMPLES) / callbacks->timeInfo().samplesPerTick();
				TWSSamples = 0;
				TWSActive = true;
			}
			else
			{
				// we have used all our slots, just send a twk
				const float value(((pData.command() * 256) + pData.parameter()) / 65535.0f);
				SetParameter(pData.instrument(), value);
			}
			Player::singleton().Tweaker = true;
		}
		else if(pData.note() == notetypes::midi_cc) // Mcm (MIDI CC) Command
		{
			AddMIDI(pData.instrument(), pData.command(), pData.parameter());
		}
		else
		{
			if(pData.command() == 0xC1) //set the pitch bend range
			{
				rangeInSemis = pData.parameter();
			}
			else if(pData.command() == 0xC2) //Panning
			{
				AddMIDI(0xB0 | midiChannel, 0x0A,pData.parameter()*0.5f);
			}

			if(note < notetypes::release) // Note on
			{
				int semisToSlide(0);

				if((pData.command() == 0x10) && ((pData.instrument() & 0xF0) == 0x80 || (pData.instrument() & 0xF0) == 0x90)) // _OLD_ MIDI Command
				{
					AddMIDI(pData.instrument(), note, pData.parameter());
				}
				else if((pData.command() & 0xF0) == 0xD0 || (pData.command() & 0xF0) == 0xE0) //semislide
				{
					if (NSCurrent[midiChannel] != pitchWheelCentre)
					{
						AddMIDI(0xE0 + midiChannel,LSB(pitchWheelCentre),MSB(pitchWheelCentre));
					}
					///\todo: sorry???
					currentSemi[midiChannel] = 0;

					if ((pData.command() & 0xF0) == 0xD0) //pitch slide down
					{
						semisToSlide = -(pData.command() & 0x0F);
						if (semisToSlide < (-rangeInSemis - currentSemi[midiChannel]))
							semisToSlide = (-rangeInSemis - currentSemi[midiChannel]);
					}
					else //pitch slide up
					{
						semisToSlide = pData.command() & 0x0F;
						if (semisToSlide > (rangeInSemis - currentSemi[midiChannel]))
							semisToSlide = (rangeInSemis - currentSemi[midiChannel]);
					}

					int speedToSlide = pData.parameter();

					NSCurrent[midiChannel] = pitchWheelCentre + (currentSemi[midiChannel] * (pitchWheelCentre / rangeInSemis));
					NSDestination[midiChannel] = NSCurrent[midiChannel] + ((pitchWheelCentre / rangeInSemis) * semisToSlide);
					NSDelta[midiChannel] = ((NSDestination[midiChannel] - NSCurrent[midiChannel]) * (speedToSlide*2)) / callbacks->timeInfo().samplesPerTick();
					NSSamples[midiChannel] = 0;
					NSActive[midiChannel] = true;
					currentSemi[midiChannel] = currentSemi[midiChannel] + semisToSlide;
					AddNoteOn(channel, note, 127, midiChannel);
				}
				else if((pData.command() == 0xC3) && (oldNote[midiChannel]!=-1))//slide to note
				{
					semisToSlide = note - oldNote[midiChannel];
					int speedToSlide = pData.parameter();
					NSDestination[midiChannel] = NSDestination[midiChannel] + ((pitchWheelCentre / rangeInSemis) * semisToSlide);
					NSTargetDistance[midiChannel] = (NSDestination[midiChannel] - NSCurrent[midiChannel]);
					NSDelta[midiChannel] = (NSTargetDistance[midiChannel] * (speedToSlide*2)) / callbacks->timeInfo().samplesPerTick();
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
					AddNoteOn(channel,note,(pData.command() == 0x0C)?pData.parameter()/2:127,midiChannel,0,(pData.machine()==0xFF));
				}
				if (((pData.command() & 0xF0) == 0xD0) || ((pData.command() & 0xF0) == 0xE0))
					oldNote[midiChannel] = note + semisToSlide;
				else
					oldNote[midiChannel] = note;
			}
			else if(note == notetypes::release) // Note Off.
			{
				AddNoteOff(channel, midiChannel);
				oldNote[midiChannel] = -1;
			}
			else if(pData.note() == notetypes::empty)
			{
				int semisToSlide(0);

				if (pData.command() == 0x10) // _OLD_ MIDI Command
				{
					AddMIDI(pData.instrument(),pData.parameter());
				}
				//else if (pData.command() == 0x0C) // channel volume.
				//{
					//AddMIDI(0xB0 | midiChannel,0x07,pData.parameter()*0.5f);
				//}
				else if (pData.command() == 0x0C) // channel aftertouch.
				{
					AddMIDI(0xD0 | midiChannel,pData.parameter()*0.5f);
				}
				else if(pData.command() == 0xC3) //slide to note . Used to change the speed.
				{
					int speedToSlide = pData.parameter();
					NSDelta[midiChannel] = (NSTargetDistance[midiChannel] * (speedToSlide*2)) / callbacks->timeInfo().samplesPerTick();
				}
				else if((pData.command() & 0xF0) == 0xD0 || (pData.command() & 0xF0) == 0xE0) //semislide
				{
					if (NSCurrent[midiChannel] != NSDestination[midiChannel])
					{
						AddMIDI(0xE0 | midiChannel,LSB(NSDestination[midiChannel]),MSB(NSDestination[midiChannel]));
					}

					if ((pData.command() & 0xF0) == 0xD0) //pitch slide down
					{
						semisToSlide = -(pData.command() & 0x0F);
						if (semisToSlide < (-rangeInSemis - currentSemi[midiChannel]))
							semisToSlide = (-rangeInSemis - currentSemi[midiChannel]);
					}
					else //pitch slide up
					{
						semisToSlide = pData.command() & 0x0F;
						if (semisToSlide > (rangeInSemis - currentSemi[midiChannel]))
							semisToSlide = (rangeInSemis - currentSemi[midiChannel]);
					}

					int speedToSlide = pData.parameter();

					NSCurrent[midiChannel] = pitchWheelCentre + (currentSemi[midiChannel] * (pitchWheelCentre / rangeInSemis));
					NSDestination[midiChannel] = NSCurrent[midiChannel] + ((pitchWheelCentre / rangeInSemis) * semisToSlide);
					NSDelta[midiChannel] = ((NSDestination[midiChannel] - NSCurrent[midiChannel]) * (speedToSlide*2)) / callbacks->timeInfo().samplesPerTick();
					NSSamples[midiChannel] = 0;
					NSActive[midiChannel] = true;
					currentSemi[midiChannel] = currentSemi[midiChannel] + semisToSlide;
				}
				if (((pData.command() & 0xF0) == 0xD0) || ((pData.command() & 0xF0) == 0xE0))
					oldNote[midiChannel] = oldNote[midiChannel] + semisToSlide;
			}
		}
	}

	void plugin::Stop()
	{
		for(int i(0) ; i < 16 ; ++i) AddMIDI(0xb0 + i, 0x7b); // All Notes Off
		for(int i(0) ; i < MAX_TRACKS ; ++i) AddNoteOff(i);
	}

	void plugin::PreWork(int numSamples,bool clear)
	{
		Machine::PreWork(numSamples,clear);
		if(!WillProcessReplace())
		{
			dsp::Clear(_pOutSamplesL, numSamples);
			dsp::Clear(_pOutSamplesR, numSamples);
		}

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
					AddMIDI(0xE0 + midiChannel,LSB(NSCurrent[midiChannel]),MSB(NSCurrent[midiChannel]),NSSamples[midiChannel]);
					NSSamples[midiChannel]+= std::min(TWEAK_SLIDE_SAMPLES, ns);
					ns-=TWEAK_SLIDE_SAMPLES;
				}
			}
		}
	}

	int plugin::GenerateAudioInTicks(int startSample, int numsamples)
	{
		//nanoseconds const t0(cpu_time_clock());
		if(!Mute())
		{
			if (IsGenerator() || (!Standby() && !Bypass()) || bCanBypass)
			{
				#if 0 // The following is now being done in the OnTimer() thread of the UI.
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
				#endif
				try
				{
					/*if (WantsMidi())*/ SendMidi();
				}
				catch(const std::exception &)
				{
					// o_O`
				}
				if(numInputs() == 1)
				{
					dsp::Add(inputs[1]+startSample,inputs[0]+startSample,numsamples,0.5f);
				}

				///\todo: Move all this messy retrigger code to somewhere else. (it is repeated in each machine subclass)
				// Store temporary pointers so that we can increase the address in the retrigger code
				float * tempinputs[vst::max_io];
				float * tempoutputs[vst::max_io];
				for(int i(0) ; i < vst::max_io; ++i)
				{
					tempinputs[i] = inputs[i]+startSample;
					tempoutputs[i] = outputs[i]+startSample;
				}
				int ns(numsamples);
				while(ns)
				{
					int nextevent;
					if(TWSActive) nextevent = TWSSamples; else nextevent = ns + 1;
					for(uint32_t i(0) ; i < callbacks->song().tracks() ; ++i)
					{
						if(TriggerDelay[i].command() && TriggerDelayCounter[i] < nextevent) nextevent = TriggerDelayCounter[i];
					}
					if(nextevent > ns)
					{
						if(TWSActive) TWSSamples -= ns;
						for(uint32_t i(0) ; i < callbacks->song().tracks(); ++i)
						{
							// come back to this
							if(TriggerDelay[i].command()) TriggerDelayCounter[i] -= ns;
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
							for(int i(0) ; i < vst::max_io ; ++i)
							{
								tempinputs[i]+=nextevent;
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
						for(uint32_t i(0) ; i < callbacks->song().tracks(); ++i)
						{
							// come back to this
							if(TriggerDelay[i].command() == commandtypes::NOTE_DELAY)
							{
								if(TriggerDelayCounter[i] == nextevent)
								{
									// do event
									Tick(i, TriggerDelay[i]);
									TriggerDelay[i].setCommand(0);
								}
								else TriggerDelayCounter[i] -= nextevent;
							}
							else if(TriggerDelay[i].command() == commandtypes::RETRIGGER)
							{
								if(TriggerDelayCounter[i] == nextevent)
								{
									// do event
									Tick(i, TriggerDelay[i]);
									TriggerDelayCounter[i] = (RetriggerRate[i] * callbacks->timeInfo().samplesPerTick()) / 256;
								}
								else TriggerDelayCounter[i] -= nextevent;
							}
							else if(TriggerDelay[i].command() == commandtypes::RETR_CONT)
							{
								if(TriggerDelayCounter[i] == nextevent)
								{
									// do event
									Tick(i, TriggerDelay[i]);
									TriggerDelayCounter[i] = (RetriggerRate[i] * callbacks->timeInfo().samplesPerTick()) / 256;
									int parameter(TriggerDelay[i].parameter() & 0x0f);
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
				try
				{
					if(numOutputs() == 1) dsp::Mov(outputs[0]+startSample,outputs[1]+startSample, numsamples);
				}
				catch(const std::exception &)
				{
					// o_O`
				}
				if (!WillProcessReplace())
				{
					// We need the output in _pSamples, so we invert the
					// pointers to avoid copying _pOutSamples into _pSamples
					#if 0
					float* const tempSamplesL = inputs[0];
					float* const tempSamplesR = inputs[1];
					_pSamplesL = inputs[0] = outputs[0];
					_pSamplesR = inputs[1] = outputs[1];
					_pOutSamplesL = outputs[0] = tempSamplesL;
					_pOutSamplesR = outputs[1] = tempSamplesR;
					#endif
					std::memcpy(inputs[0],outputs[0],numsamples*sizeof(float));
					std::memcpy(inputs[1],outputs[1],numsamples*sizeof(float));
				}
				UpdateVuAndStanbyFlag(numsamples);

				//nanoseconds const t1(cpu_time_clock());
				//accumulate_processing_time(t1 - t0);

				recursive_processed_ = true;
				return true;
			}
		}

		Machine::UpdateVuAndStanbyFlag(numsamples);

		//nanoseconds const t1(cpu_time_clock());
		//accumulate_processing_time(t1 - t0);

		recursive_processed_ = true;
		return false;
	}

	bool plugin::LoadSpecificChunk(RiffFile * pFile, int version)
	{
		uint32_t size;
		unsigned char _program;
		pFile->Read(size);
		if(size)
		{
			if(version > CURRENT_FILE_VERSION_MACD)
			{
				pFile->Skip(size);
				std::ostringstream s; s
					<< version << " > " << CURRENT_FILE_VERSION_MACD << std::endl
					<< "Data is from a newer format of psycle, it might be unsafe to load." << std::endl;
				//MessageBox(0, s.str().c_str(), "Loading Error", MB_OK | MB_ICONWARNING);
				return false;
			}
			uint32_t count;
			pFile->Read(_program);
			pFile->Read(count);
			size -= sizeof _program + sizeof count + sizeof(float) * count;
			if(!size)
			{
				BeginSetProgram();
				SetProgram(_program);
				for(uint32_t i(0) ; i < count ; ++i)
				{
					float temp;
					pFile->Read(temp);
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
					pFile->ReadArray(data, size); // Number of parameters
					SetChunk(data,size);
					delete data;
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

	void plugin::SaveSpecificChunk(RiffFile * pFile) const
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
				// can't do this! we have a thing called "autosave" and everything is stopped then.
				//MainsChanged(false);
				count=0;
				chunksize = GetChunk((void**)&pData);
				size+=chunksize;
				//MainsChanged(true);
			}
			else
			{
				size += sizeof(float) * count;
			}
			pFile->Write(size);
			_program = static_cast<unsigned char>(GetProgram());
			pFile->Write(_program);
			pFile->Write(count);

			if(b)
			{
				pFile->WriteArray(pData, chunksize);
			}
			else
			{
				for(UINT i(0); i < count; ++i)
				{
					float temp = GetParameter(i);
					pFile->Write(temp);
				}
			}
		} catch(...) {
		}
	}

	}}}
#endif
