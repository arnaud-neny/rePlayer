#include "y_midi.hpp"
#include "gmnames.h"
#include <sstream>
namespace psycle::plugins::y_midi {
int mi::midiopencount[MAX_MIDI_DEVICES+1]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
HMIDIOUT mi::handles[MAX_MIDI_DEVICES+1]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

//////////////////////////////////////////////////////////////////////////
// midichannel class
midichannel::midichannel()
:Chan(0)
,handle(0)
{
}

midichannel::~midichannel()
{
}

void midichannel::Init(HMIDIOUT handle_in, const int channel)
{
	handle = handle_in;
	Chan = channel;
}

void midichannel::Play(const int note, const int vol)
{
	SendMidi(BuildEvent(MIDI_NOTEON,note,vol));
}

void midichannel::Stop(const int note)
{
	SendMidi(BuildEvent(MIDI_NOTEOFF,note));
}

void midichannel::SetPatch(const int patchnum)
{
	SendMidi(BuildEvent(MIDI_PATCHCHANGE,patchnum));
}


MidiEvent midichannel::BuildEvent(const int eventtype, const int p1, const int p2)
{
	MidiEvent mevent;
	mevent.split.message= (eventtype&0xF0) | (Chan&0x0F);
	mevent.split.byte1= p1&0x7F;
	mevent.split.byte2= p2&0x7F;

	return mevent;
}

void midichannel::SendMidi(MidiEvent event)
{
	midiOutShortMsg(handle, event.value);
}
void midichannel::StopMidi()
{
	SendMidi(BuildEvent(MIDI_CCONTROLLER,0x7B)); // All Notes Off
}

//////////////////////////////////////////////////////////////////////////
//  mi machine class.

PSYCLE__PLUGIN__INSTANTIATOR("ymidi", mi, MacInfo)

mi::mi()
{
	int i;
	Vals=new int[MacInfo.numParameters];

	for(i=0;i<psycle::plugin_interface::MAX_TRACKS; i++)
	{
		numC[i]=0;
		notes[i]=-1;
	}
	pars.portidx = 0;
}

mi::~mi()
{
	FreeMidi();
	delete[] Vals;
}

void mi::Init()
{
	// Initialize your stuff here
	InitMidi();
}

bool mi::HostEvent(int const eventNr, int const val1, float const val2) {
	if (eventNr == psycle::plugin_interface::HE_NEEDS_AUX_COLUMN) {
		return true;
	}
	else {
		return false;
	}
}

void mi::Command()
{
	// Called when user presses editor button
	// Probably you want to show your custom window here
	// or an about button
	char buffer[256];

	sprintf(

			buffer,"%s%s%s%s",
			"Commands:\n",
			"C1xx - Set Program to xx\n",
			"C2xx or aux column - Set Midi Channel for this track\n\n",
			"CCxx or 0Cxx - Set Volume\n\n",
			"Use the Aux column to force a midi channel for this note\n"
			);

	pCB->MessBox(buffer,"·-=<YanniS> YMidi Midi Output v." YMIDI_VERSION "=-·",0);
}

void mi::SequencerTick()
{
// Called on each tick while sequencer is playing
}

void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks)
{
}

void mi::ParameterTweak(int par, int val)
{
	Vals[par]=val;
	switch(par)
	{
	case 0:
		if(pars.portidx != val){
			FreeMidi();
			pars.portidx = val;
			InitMidi();
		}
		break;
	case 1: numChannel[0].SetPatch(val); pars.patch1=val; break;
	case 2: numChannel[1].SetPatch(val); pars.patch2=val; break;
	case 3: numChannel[2].SetPatch(val); pars.patch3=val; break;
	case 4: numChannel[3].SetPatch(val); pars.patch4=val; break;
	case 5: numChannel[4].SetPatch(val); pars.patch5=val; break;
	case 6: numChannel[5].SetPatch(val); pars.patch6=val; break;
	case 7: numChannel[6].SetPatch(val); pars.patch7=val; break;
	case 8: numChannel[7].SetPatch(val); pars.patch8=val; break;
	case 9: numChannel[8].SetPatch(val); pars.patch9=val; break;
	case 10: numChannel[9].SetPatch(val); pars.patch10=val; break;
	case 11: numChannel[10].SetPatch(val); pars.patch11=val; break;
	case 12: numChannel[11].SetPatch(val); pars.patch12=val; break;
	case 13: numChannel[12].SetPatch(val); pars.patch13=val; break;
	case 14: numChannel[13].SetPatch(val); pars.patch14=val; break;
	case 15: numChannel[14].SetPatch(val); pars.patch15=val; break;
	case 16: numChannel[15].SetPatch(val); pars.patch16=val; break;
	default:break;
	}
}

void mi::Stop()
{
	midiOutReset(handles[pars.portidx]);
}

bool mi::DescribeValue(char* txt,int const param, int const value)
{
	switch(param)
	{
	case 0: {
		std::ostringstream out;
		
		if(value==0) out << "Default output (" << midiopencount[value] <<")";
		else if(value <= MAX_MIDI_DEVICES) out << outputNames[value-1] <<"(" << midiopencount[value] <<")";
		else out << "wrong index";
		strcpy(txt,out.str().c_str());
		return true;
		}
	case 1: strcpy(txt,GmNames[value]); return true;
	case 2: strcpy(txt,GmNames[value]); return true;
	case 3: strcpy(txt,GmNames[value]); return true;
	case 4: strcpy(txt,GmNames[value]); return true;
	case 5: strcpy(txt,GmNames[value]); return true;
	case 6: strcpy(txt,GmNames[value]); return true;
	case 7: strcpy(txt,GmNames[value]); return true;
	case 8: strcpy(txt,GmNames[value]); return true;
	case 9: strcpy(txt,GmNames[value]); return true;
	case 10: {
		switch(value) {
		case 0:strcpy(txt,GmDrumSets[0]);break;
		case 8:strcpy(txt,GmDrumSets[1]);break;
		case 16:strcpy(txt,GmDrumSets[2]);break;
		case 24:strcpy(txt,GmDrumSets[3]);break;
		case 25:strcpy(txt,GmDrumSets[4]);break;
		case 32:strcpy(txt,GmDrumSets[5]);break;
		case 40:strcpy(txt,GmDrumSets[6]);break;
		case 48:strcpy(txt,GmDrumSets[7]);break;
		case 56:strcpy(txt,GmDrumSets[8]);break;
		default:strcpy(txt,"-");break;
		}
		return true;
	 }
	case 11: strcpy(txt,GmNames[value]); return true;
	case 12: strcpy(txt,GmNames[value]); return true;
	case 13: strcpy(txt,GmNames[value]); return true;
	case 14: strcpy(txt,GmNames[value]); return true;
	case 15: strcpy(txt,GmNames[value]); return true;
	case 16: strcpy(txt,GmNames[value]); return true;
	default : return false;
	}
}

// Process each sequence tick if note on or note off is pressed.
void mi::SeqTick(int channel, int note, int ins, int cmd, int val)
{
	//\todo: Add synchronization code. Due to the way Psycle works, ticks
	// might not come each n samples so sending them to MIDI when received is
	// not correct.
	if ( cmd == 0xC2 ) assignChannel(channel,val&0x0F);
	else if (cmd == 0xC1)
	{
		Channel(channel).SetPatch(val&0x7F);
		UpdatePatch(numC[channel],val&0x7F);
	}

	if(note<=psycle::plugin_interface::NOTE_MAX) // Note on
	{
		if ( ins != 255 ) assignChannel(channel,ins&0x0F);
		if (notes[channel]!= -1) Channel(channel).Stop(notes[channel]);
		Channel(channel).Play(note, (cmd == 0xCC || cmd == 0x0C)?val>>1:0x64);
		notes[channel]=note;
	}
	else if (note==psycle::plugin_interface::NOTE_NOTEOFF && notes[channel]!= -1) // Note off
	{
		if ( ins != 255 ) assignChannel(channel,ins&0x0F);
		Channel(channel).Stop(notes[channel]);
		notes[channel]=-1;
	}
}
void mi::MidiEvent(int channel, int midievent, int value) {
	switch(midievent) {
		case MIDI_NOTEOFF: MidiChannel(channel).Stop(value>>8);break;
		case MIDI_NOTEON: MidiChannel(channel).Play(value>>8, value&0xFF);break;
		case MIDI_PATCHCHANGE: {
					MidiChannel(channel).SetPatch((value>>8)&0x7F);
					UpdatePatch(channel,(value>>8)&0x7F);
				  }
				  break;
		default:
			MidiChannel(channel).SendMidi(MidiChannel(channel).BuildEvent(midievent, value >> 8, value&0x7F));
		break;
	}
}
void mi::InitMidi()
{
	GetMidiNames();
	if(midiopencount[pars.portidx] < 0) midiopencount[pars.portidx]=0;
	if(midiopencount[pars.portidx] == 0) {
		if (!midiOutOpen(&handles[pars.portidx], (UINT)pars.portidx-1, 0, 0, CALLBACK_NULL) )
		{
			midiopencount[pars.portidx]++;
		}
	}
	for(int i=0;i<MIDI_TRACKS; i++)
	{
		numChannel[i].Init(handles[pars.portidx],i);
	}
}

void mi::GetMidiNames()
{
	// get the number of MIDI input devices
	int numDevs = midiOutGetNumDevs();
	if(numDevs > MAX_MIDI_DEVICES) numDevs = MAX_MIDI_DEVICES;
	
	int idx = 0;
	for(; idx < numDevs; idx++ )
	{
		std::ostringstream out;
		MIDIOUTCAPS moc;
		midiOutGetDevCaps( idx, &moc, sizeof( moc ) );
		if(moc.wTechnology == MOD_MIDIPORT) out << "[POR] ";
		else if(moc.wTechnology == MOD_SYNTH) out << "[SYN] ";
		else if(moc.wTechnology == MOD_SQSYNTH) out << "[SQU] ";
		else if(moc.wTechnology == MOD_FMSYNTH) out << "[FM] ";
		else if(moc.wTechnology == MOD_MAPPER) out << "[MAP] ";
		else if(moc.wTechnology == MOD_WAVETABLE) out << "[WAV] ";
		else if(moc.wTechnology == MOD_SWSYNTH) out << "[SOF] ";
		out << moc.szPname;
		outputNames[idx] = out.str();
	}
	for(; idx < MAX_MIDI_DEVICES; idx++) {
		outputNames[idx] = "";
	}
}

void mi::FreeMidi()
{
	midiopencount[pars.portidx]--;
	if (midiopencount[pars.portidx]==0) 
	{
		midiOutClose(handles[pars.portidx]);
	}
}
void mi::UpdatePatch(int channel,int patch)
{
	switch(channel)
	{
	case 0: pars.patch1=patch; Vals[1]=patch; break;
	case 1: pars.patch2=patch; Vals[2]=patch; break;
	case 2: pars.patch3=patch; Vals[3]=patch; break;
	case 3: pars.patch4=patch; Vals[4]=patch; break;
	case 4: pars.patch5=patch; Vals[5]=patch; break;
	case 5: pars.patch6=patch; Vals[6]=patch; break;
	case 6: pars.patch7=patch; Vals[7]=patch; break;
	case 7: pars.patch8=patch; Vals[8]=patch; break;
	case 8: pars.patch9=patch; Vals[9]=patch; break;
	case 9: pars.patch10=patch; Vals[10]=patch; break;
	case 10: pars.patch11=patch; Vals[11]=patch; break;
	case 11: pars.patch12=patch; Vals[12]=patch; break;
	case 12: pars.patch13=patch; Vals[13]=patch; break;
	case 13: pars.patch14=patch; Vals[14]=patch; break;
	case 14: pars.patch15=patch; Vals[15]=patch; break;
	case 15: pars.patch16=patch; Vals[16]=patch; break;
	default:break;
	}
}
}