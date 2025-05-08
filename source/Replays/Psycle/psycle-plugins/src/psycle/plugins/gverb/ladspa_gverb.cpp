//////////////////////////////////////////////////////////////////////
// LADSPA GVerb plugin for PSYCLE by Sartorius
//
//   Original
/*

GVerb algorithm designed and implemented by Juhana Sadeharju.
LADSPA implementation and GVerb speeds ups by Steve Harris.

Comments and suggestions should be mailed to Juhana Sadeharju
(kouhia at nic funet fi).

*/

#include <psycle/plugin_interface.hpp>
#include "gverb.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>
namespace psycle::plugins::gverb {
using namespace psycle::plugin_interface;

CMachineParameter const paraRoomSize = { "Room size", "Room size", 1, 300, MPF_STATE, 144 };
CMachineParameter const paraRevTime = { "Reverb time", "Reverb time", 10, 3000, MPF_STATE, 1800 };
CMachineParameter const paraDamping = { "Damping", "Damping", 0, 1000, MPF_STATE, 1000 };
CMachineParameter const paraBandwidth = { "Input bandwidth", "Input bandwidth", 0, 1000, MPF_STATE, 0 };
CMachineParameter const paraDry = { "Dry signal level", "Dry", -70000, 0, MPF_STATE, -70000 };
CMachineParameter const paraEarly = { "Early reflection level", "Early", -70000, 0, MPF_STATE, 0 };
CMachineParameter const paraTail = { "Tail level", "Tail level", -70000, 0, MPF_STATE, -17500 };
CMachineParameter const paraMonoStereo = { "Input", "Input", 0, 1, MPF_STATE, 0 };

CMachineParameter const *pParameters[] = {
	&paraRoomSize,
	&paraRevTime,
	&paraDamping,
	&paraBandwidth,
	&paraDry,
	&paraEarly,
	&paraTail,
	&paraMonoStereo
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0110,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	#ifndef NDEBUG
		"LADSPA GVerb (Debug build)",
	#else
		"LADSPA GVerb",
	#endif
	"GVerb",
	"Juhana Sadeharju/Steve Harris/Sartorius",
	"About",
	1
);

class mi : public CMachineInterface {
	public:
		mi();
		virtual ~mi();

		virtual void Init();
		virtual void SequencerTick();
		virtual void Work(float *psamplesleft, float *psamplesright, int numsamples, int tracks);
		virtual bool DescribeValue(char* txt, int const param, int const value);
		virtual void Command();
		virtual void ParameterTweak(int par, int val);
	private:
		gverb *gv_l, *gv_r;
		int samplerate;
};

PSYCLE__PLUGIN__INSTANTIATOR("ladspa-gverb", mi, MacInfo)

template<typename Real> inline Real DB_CO(Real g) {
	return g > Real(-90) ? std::pow(Real(10), g * Real(0.05)) : Real(0);
}

template<typename Real> inline Real CO_DB(Real v) {
	return Real(20) * std::log10(v);
}

mi::mi() {
	Vals = new int[MacInfo.numParameters];
}

mi::~mi() {
	delete[] Vals;
	delete gv_l;
	delete gv_r;
}

void mi::Init() {
	samplerate = pCB->GetSamplingRate();
	gv_l = new gverb(samplerate, 300.0f, 50.0f, 7.0f, 0.5f, 15.0f, 0.5f, 0.5f, 0.5f);
	gv_r = new gverb(samplerate, 300.0f, 50.0f, 7.0f, 0.5f, 15.0f, 0.5f, 0.5f, 0.5f);
}

void mi::SequencerTick() {
	if(samplerate != pCB->GetSamplingRate()) {
		samplerate = pCB->GetSamplingRate();

		delete gv_l;
		delete gv_r;
		
		gv_l = new gverb(samplerate, 300.0f, 50.0f, 7.0f, 0.5f, 15.0f, 0.5f, 0.5f, 0.5f);
		gv_r = new gverb(samplerate, 300.0f, 50.0f, 7.0f, 0.5f, 15.0f, 0.5f, 0.5f, 0.5f);

		gv_l->set_roomsize(Vals[0]);
		gv_r->set_roomsize(Vals[0]);

		gv_l->set_revtime(Vals[1] * .01f);
		gv_r->set_revtime(Vals[1] * .01f);

		gv_l->set_damping(Vals[2] * .001f);
		gv_r->set_damping(Vals[2] * .001f);

		gv_l->set_inputbandwidth(Vals[3] * .001f);
		gv_r->set_inputbandwidth(Vals[3] * .001f);

		gv_l->set_earlylevel(DB_CO(Vals[5] * .001f));
		gv_r->set_earlylevel(DB_CO(Vals[5] * .001f));

		gv_l->set_taillevel(DB_CO(Vals[6] * .001f));
		gv_r->set_taillevel(DB_CO(Vals[6] * .001f));
	}
}

void mi::Command() {
	pCB->MessBox("LADSPA GVerb","GVerb",0);
}

void mi::ParameterTweak(int par, int val) {
	Vals[par] = val;
	switch(par) {
		case 0:
			gv_l->set_roomsize(val);
			gv_r->set_roomsize(val);
			break;
		case 1:
			gv_l->set_revtime(val * .01f);
			gv_r->set_revtime(val * .01f);
			break;
		case 2:
			gv_l->set_damping(val * .001f);
			gv_r->set_damping(val * .001f);
			break;
		case 3:
			gv_l->set_inputbandwidth(val * .001f);
			gv_r->set_inputbandwidth(val * .001f);
			break;
		case 4: break;
		case 5:
			gv_l->set_earlylevel(DB_CO(val * .001f));
			gv_r->set_earlylevel(DB_CO(val * .001f));
			break;
		case 6:
			gv_l->set_taillevel(DB_CO(val * .001f));
			gv_r->set_taillevel(DB_CO(val * .001f));
			break;
		case 7:
			gv_l->flush();
			gv_r->flush();
			break;
		default:
			break;
	}
}

void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks) {
	float const dry = DB_CO(Vals[4] * .001f);
	
	float outl = 0;
	float outr = 0;

	if(Vals[7]) {
		do {
			float inl = *psamplesleft; // * 0.000030517578125f;
			float inr = *psamplesright; // * 0.000030517578125f;
			gv_l->process(inl, &outl, &outr);
			*psamplesleft = inl * dry + outl; //*32767.f;
			*psamplesright = inr*dry + outr; //*32767.f;
			gv_r->process(inr, &outl, &outr);
			*psamplesleft += outl; //*32767.f;
			*psamplesright += outr; //*32767.f;

			++psamplesleft;
			++psamplesright;
		} while(--numsamples);
	} else {
		do {
			float sm = (*psamplesleft + *psamplesright) * 0.5f; // * 0.000030517578125f;
			gv_l->process(sm, &outl, &outr);

			*psamplesleft = *psamplesleft * dry + outl; //*32767.f;
			*psamplesright = *psamplesright * dry + outr; //*32767.f;

			++psamplesleft;
			++psamplesright;
		} while(--numsamples);
	}
}

// Function that describes value on client's displaying
bool mi::DescribeValue(char* txt,int const param, int const value) {
	switch(param) {
		case 0:
			std::sprintf(txt, "%i m", value);
			return true;
		case 1:
			std::sprintf(txt, "%.2f s", value * .01f);
			return true;
		case 2: case 3:
			std::sprintf(txt, "%.01f", value * .001f);
			return true;
		case 4: case 5: case 6:
			std::sprintf(txt, "%i dB", value / 1000);
			return true;
		case 7:
			std::sprintf(txt, value ? "stereo" : "mono");
			return true;
		default:
			return false;
	}
}
}