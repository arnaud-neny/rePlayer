#include <psycle/plugin_interface.hpp>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <cstdio>

namespace psycle::plugins::bexphase {

using namespace psycle::plugin_interface;

const int fftlen = 2048;
const int iir = 6;
const int inbuflen = fftlen * iir * 2;
float optimal[iir+1] = { 0, 0.50f, 0.40f, 0.32f, 0.30f, 0.28f, 0.24f };

CMachineParameter const paraFreq = {"Phase Shift", "Phase Shift", 1, 512, MPF_STATE, 256 };
CMachineParameter const paraDry = {"Dry / Wet", "Dry / Wet", 0, 512, MPF_STATE, 390 };
CMachineParameter const paraRefresh = {"Lfo/Freq Speed", "Lfo/Freq Speed", 1, 32, MPF_STATE, 8 };
CMachineParameter const paraMode = {"Mode", "Mode", 1, iir, MPF_STATE, 1 };
CMachineParameter const paraAmount = {"LFO / Freq", "LFO / Freq", 0, 512, MPF_STATE, 384 };
CMachineParameter const paraDiff = {"Differentiator", "Differentiator", 0, 512, MPF_STATE, 0 };

CMachineParameter const *pParameters[] = {
	&paraFreq,
	&paraDry,
	&paraRefresh,
	&paraMode,
	&paraAmount,
	&paraDiff,
};

enum {
	pFreq = 0,
	pDry,
	pRefresh,
	pMode,
	pAmount,
	pDiff
};

CMachineInfo const bexphase_info (
	MI_VERSION,
	0x0120,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	"DocBexter'S PhaZaR"
		#ifndef NDEBUG
			" (debug build)"
		#endif
		,
	"BexPhase!",
	"Simon Bucher",
	"About",
	3
);


class bexphase : public CMachineInterface {
	public:
		bexphase();
		virtual ~bexphase();

		virtual void Init();
		virtual void SequencerTick();
		virtual void Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks);
		virtual bool DescribeValue(char* txt,int const param, int const value);
		virtual void Command();
		virtual void ParameterTweak(int par, int val);
		virtual void Stop();

	private:
		float inbufr[inbuflen], inbufl[inbuflen];
		float buffer[fftlen], altbuffer[fftlen];
		long int inpoint, counter;
		float dry, wet, freq, shift, amntfreq, amntlfo;
		int buflen, shiftcount, last_dir;
		float dlay_r, dlay_l, shift_r, shift_l, diff, undiff;
};

PSYCLE__PLUGIN__INSTANTIATOR("bexphase", bexphase, bexphase_info)

bexphase::bexphase() {
	Vals = new int[bexphase_info.numParameters];
	Vals[pMode]=1;
	memset(inbufr, 0, sizeof(float)* inbuflen);
	memset(inbufl, 0, sizeof(float)* inbuflen);
	memset(buffer, 0, sizeof(float)* fftlen);
	memset(altbuffer, 0, sizeof(float)* fftlen);
	inpoint = 0; counter = 0;
	dry = optimal[0]; wet = 1 - dry;
	freq = 1; amntfreq = 0.75f; amntlfo = 0.25f;
	shift = 0; shiftcount = 0;
	buflen = 0; last_dir = 0; dlay_r = 0; dlay_l = 0;
	shift_r = 0; shift_l = 0; diff = 0; undiff = 1;
}

bexphase::~bexphase() {
	delete[] Vals;
}
	
void bexphase::Init() {
};

void bexphase::SequencerTick() {
	#if 0
	if(++counter >= Vals[pRefresh]) {
		int y;
		if ( (inpoint - fftlen) < 0 ) y = inbuflen - ( fftlen - inpoint ); else y = inpoint - fftlen;
		for (int x = 0; x < fftlen; x++ ) { if ( (y+x) == inbuflen ) y = x * (-1); buffer[x] = inbufr[x+y]; }
		rfftw_one( forward, buffer, altbuffer );
		y = 1;
		for (int x = fftlen/64; x < fftlen/2; x++ )
		{
			if ( altbuffer[x] < 0 ) buffer[x] = altbuffer[x] * (-1); else buffer[x] = altbuffer[x];
			if ( buffer[x] > buffer[y] ) y = x;
		}
		shift_r = ( fftlen / (float)y ) * amntfreq;
		if ( last_dir == 0 ) shift_r += (fftlen/(3+iir)) * amntlfo;
		shift_r -= dlay_r; shift_r /= buflen;
		
		if ( (inpoint - fftlen) < 0 ) y = inbuflen - ( fftlen - inpoint ); else y = inpoint - fftlen;
		for (int x = 0; x < fftlen; x++ ) { if ( (y+x) == inbuflen ) y = x * (-1); buffer[x] = inbufl[x+y]; }
		rfftw_one( forward, buffer, altbuffer );
		y = 1;
		for (int x = fftlen/64; x < fftlen/2; x++ )
		{
			if ( altbuffer[x] < 0 ) buffer[x] = altbuffer[x] * (-1); else buffer[x] = altbuffer[x];
			if ( buffer[x] > buffer[y] ) y = x;
		}
		shift_l = ( fftlen / (float)y ) * amntfreq;
		if ( last_dir == 0 ) { shift_l += (fftlen/(3+iir)) * amntlfo; last_dir = 1; }
		else last_dir = 0;
		shift_l -= dlay_l; shift_l /= buflen;

		counter = 0;
	}
	#endif
}

void bexphase::Command() {
	pCB->MessBox("original author: docbexter <docbexter@web.de> ; maintained by psycledelics", ";)", 0);
}

void bexphase::ParameterTweak(int par, int val) {
	switch(par) {
		case pFreq:
			shift = val / 443.396f;
			shift *= shift;
			shift = ((shift / float(Vals[pMode])) - freq) / 256.0f;
			shiftcount = 256;
		break;
		case pDry:
			if(val < 384) {
				optimal[0] = val / 384.0f;
				wet = optimal[0] * optimal[Vals[pMode]];
			}
			else if(val < 448) wet = optimal[Vals[pMode]];
			else wet = 1;
			dry = 1 - wet;
		break;
		case pRefresh:
			buflen = val * pCB->GetTickLength();
			counter = val;
			last_dir = 1;
		break;
		case pMode:
			if (shiftcount > 0 ) {
				shift = Vals[pFreq] / 443.396f;
				shift *= shift;
				shift = ((shift / float(val)) - freq) / (float)shiftcount;
			}
			else freq *= Vals[pMode] / float(val);
			wet = optimal[0] * optimal[Vals[pMode]];
			dry = 1 - wet;
		break;
		case pAmount:
			amntfreq = val / 512.0f;
			amntlfo = 1 - amntfreq;
		break;
		case pDiff:
			diff = val / 512.0f;
			undiff = 1 - diff;
		break;
		default: ;
	}
	Vals[par] = val;
}

void bexphase::Stop()  {
	shift_r = 0;
	shift_l = 0;
	shift = 0;
}

void bexphase::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks) {
	do {
		inbufr[inpoint] = *psamplesright; 
		inbufl[inpoint] = *psamplesleft;

		if ( shiftcount != 0 ) { freq += shift; --shiftcount; }
		float help_r = (dlay_r+=shift_r) * freq; 
		float help_l = (dlay_l+=shift_l) * freq;

		for ( int x = 1; x <= Vals[pMode]; x++ ) {
			int y = inpoint - (int)(help_r * x);
			int z = inpoint - (int)(help_l * x);
			if ( y < 0 ) y += inbuflen;
			if ( z < 0 ) z += inbuflen;
			*psamplesright = (*psamplesright * dry) + (inbufr[y] * wet);
			*psamplesleft = (*psamplesleft * dry) + (inbufl[z] * wet);
		}

		*psamplesright = ((inbufr[inpoint] - *psamplesright)*diff)+(*psamplesright*undiff);
		++psamplesright;
		*psamplesleft = ((inbufl[inpoint] - *psamplesleft)*diff)+(*psamplesleft*undiff);
		++psamplesleft;
		if ( ++inpoint == inbuflen ) inpoint = 0;
	}
	while(--numsamples);
}

bool bexphase::DescribeValue(char* txt,int const param, int const value) {
	switch(param) {
		case pFreq:
			sprintf( txt,"%.02f deg",freq * 90.0f );
			return true;
		case pDry:
			if(value < 384) sprintf(txt, "%.02f%%", optimal[0]*100.0f);
			else if(value < 448) strcpy( txt, "100% Effect");
			else strcpy( txt, "100% Wet");
			return true;
		case pRefresh:
			sprintf( txt, "Tick x%d" , value);
			return true;
		case pMode:
			switch( value ) {
				case 1 : strcpy( txt,"Single Mode"); break;
				case 2 : strcpy( txt,"Double Mode"); break;
				case 3 : strcpy( txt,"Triple Mode"); break;
				case 4 : strcpy( txt,"Quad Mode"); break;
				default:
					sprintf( txt,"%dx Mode", value);
			}
			return true;
		case pAmount:
			sprintf( txt, "%.02f / %.02f", amntlfo, amntfreq );
			return true;
		case pDiff:
			sprintf( txt,"%.00f %%", value / 5.12);
			return true;
		default:
			return false;
	}
}
}