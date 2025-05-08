#pragma once
#include <psycle/plugin_interface.hpp>
#include <windows.h>
// #pragma warning(push)
// 	#pragma warning(disable:4201) // nonstandard extension used : nameless struct/union
// 	#include <mmsystem.h>
// 	#pragma comment(lib, "winmm")
// #pragma warning(pop)
#include <stdio.h>
namespace psycle::plugins::y_midi {
#define YMIDI_VERSION "1.1"
int const IYMIDI_VERSION =0x0110;
#define MIDI_TRACKS 16				// Maximum tracks allowed.

//===================================================================
// TWO BYTE PARAMETER MIDI EVENTS
#define MIDI_NOTEOFF					0x80        // note-off
#define MIDI_NOTEON						0x90        // note-on
#define MIDI_AFTERTOUCH					0xa0        // aftertouch
#define MIDI_CCONTROLLER				0xb0        // continuous controller
#define MIDI_PITCHWHEEL					0xe0        // pitch wheel
//===================================================================
// ONE BYTE PARAMETER MIDI EVENTS
#define MIDI_PATCHCHANGE				0xc0        // patch change
#define MIDI_CHANPRESSURE				0xd0		// channel pressure
//===================================================================
#define MAX_MIDI_DEVICES				16

typedef				union
{
	struct split_mtype
	{
		unsigned char message;
		unsigned char byte1;
		unsigned char byte2;
		unsigned char byte3;
	} split;
	unsigned long value;
}MidiEvent;
// =========================================
// Midi channel class - will need 16 of them
// =========================================
class midichannel
{
public:
	midichannel();
	~midichannel();

	void Init(HMIDIOUT handle_in, const int channel);
	void Play(const int note, const int vol);
	void Stop(const int note);
	void SetPatch(const int patchnum);
	void StopMidi();
	void SendMidi(MidiEvent event);
	MidiEvent BuildEvent(const int eventtype, const int p1=0, const int p2=0);

private:

	int  Chan;
	HMIDIOUT handle;				// Midi device/channel handler.
};

struct MacParams {
	int portidx;
	int patch1;
	int patch2;
	int patch3;
	int patch4;
	int patch5;
	int patch6;
	int patch7;
	int patch8;
	int patch9;
	int patch10;
	int patch11;
	int patch12;
	int patch13;
	int patch14;
	int patch15;
	int patch16;
};

psycle::plugin_interface::CMachineParameter const prPort = {"Output Port selector","Output Port",0,MAX_MIDI_DEVICES,psycle::plugin_interface::MPF_STATE,0};
psycle::plugin_interface::CMachineParameter const prPatch1 = {"Program Channel 1","Program Channel 1",0,127,psycle::plugin_interface::MPF_STATE,0};
psycle::plugin_interface::CMachineParameter const prPatch2 = {"Program Channel 2","Program Channel 2",0,127,psycle::plugin_interface::MPF_STATE,0};
psycle::plugin_interface::CMachineParameter const prPatch3 = {"Program Channel 3","Program Channel 3",0,127,psycle::plugin_interface::MPF_STATE,0};
psycle::plugin_interface::CMachineParameter const prPatch4 = {"Program Channel 4","Program Channel 4",0,127,psycle::plugin_interface::MPF_STATE,0};
psycle::plugin_interface::CMachineParameter const prPatch5 = {"Program Channel 5","Program Channel 5",0,127,psycle::plugin_interface::MPF_STATE,0};
psycle::plugin_interface::CMachineParameter const prPatch6 = {"Program Channel 6","Program Channel 6",0,127,psycle::plugin_interface::MPF_STATE,0};
psycle::plugin_interface::CMachineParameter const prPatch7 = {"Program Channel 7","Program Channel 7",0,127,psycle::plugin_interface::MPF_STATE,0};
psycle::plugin_interface::CMachineParameter const prPatch8 = {"Program Channel 8","Program Channel 8",0,127,psycle::plugin_interface::MPF_STATE,0};
psycle::plugin_interface::CMachineParameter const prPatch9 = {"Program Channel 9","Program Channel 9",0,127,psycle::plugin_interface::MPF_STATE,0};
psycle::plugin_interface::CMachineParameter const prPatch10 = {"Program Channel 10","Program Channel 10",0,127,psycle::plugin_interface::MPF_STATE,0};
psycle::plugin_interface::CMachineParameter const prPatch11 = {"Program Channel 11","Program Channel 11",0,127,psycle::plugin_interface::MPF_STATE,0};
psycle::plugin_interface::CMachineParameter const prPatch12 = {"Program Channel 12","Program Channel 12",0,127,psycle::plugin_interface::MPF_STATE,0};
psycle::plugin_interface::CMachineParameter const prPatch13 = {"Program Channel 13","Program Channel 13",0,127,psycle::plugin_interface::MPF_STATE,0};
psycle::plugin_interface::CMachineParameter const prPatch14 = {"Program Channel 14","Program Channel 14",0,127,psycle::plugin_interface::MPF_STATE,0};
psycle::plugin_interface::CMachineParameter const prPatch15 = {"Program Channel 15","Program Channel 15",0,127,psycle::plugin_interface::MPF_STATE,0};
psycle::plugin_interface::CMachineParameter const prPatch16 = {"Program Channel 16","Program Channel 16",0,127,psycle::plugin_interface::MPF_STATE,0};

psycle::plugin_interface::CMachineParameter const *pParameters[] = 
{ 
	&prPort,
	&prPatch1,
	&prPatch2,
	&prPatch3,
	&prPatch4,
	&prPatch5,
	&prPatch6,
	&prPatch7,
	&prPatch8,
	&prPatch9,
	&prPatch10,
	&prPatch11,
	&prPatch12,
	&prPatch13,
	&prPatch14,
	&prPatch15,
	&prPatch16,
};

psycle::plugin_interface::CMachineInfo const MacInfo (
	psycle::plugin_interface::MI_VERSION,
	IYMIDI_VERSION,
	psycle::plugin_interface::GENERATOR,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	"YMidi - Midi Out "
		"v. " YMIDI_VERSION
		#if !defined NDEBUG
			" (debug)"
		#endif
		,
	"YMidi" YMIDI_VERSION,
	"YanniS and JosepMa on " __DATE__,
	"Command Help",
	1
);

// =====================================
// mi plugin class
// =====================================
class mi : public psycle::plugin_interface::CMachineInterface
{
public:
	mi();
	virtual ~mi();

	virtual void Init();
	virtual void SequencerTick();
	virtual void ParameterTweak(int par, int val);
	virtual void Work(float *psamplesleft, float* psamplesright, int numsamples, int tracks);
	virtual void Stop();
	virtual void MidiEvent(int channel, int midievent, int value);
	virtual bool DescribeValue(char* txt,int const param, int const value);
	virtual bool HostEvent(const int eventNr, int const val1, float const val2);
	virtual void Command();
	virtual void SeqTick(int channel, int note, int ins, int cmd, int val);

protected:
	void InitMidi();
	void FreeMidi();
	void GetMidiNames();
	inline midichannel& MidiChannel(int midich) { return numChannel[midich]; };
	inline midichannel& Channel(int ch) { return numChannel[numC[ch]]; };
	inline void assignChannel(int ch,int midich) { numC[ch]=midich; };
	void UpdatePatch(int midich, int patch);

private:
	midichannel numChannel[MIDI_TRACKS]; // List of MIDI_TRACKS (16 usually) which hold channel note information
	int numC[psycle::plugin_interface::MAX_TRACKS]; // Assignation from tracker track to midi channel.
	int notes[psycle::plugin_interface::MAX_TRACKS]; // Last note being played in this track.
	std::string outputNames[MAX_MIDI_DEVICES];
	MacParams pars;

	static int midiopencount[MAX_MIDI_DEVICES+1];
	static HMIDIOUT handles[MAX_MIDI_DEVICES+1];

};
}