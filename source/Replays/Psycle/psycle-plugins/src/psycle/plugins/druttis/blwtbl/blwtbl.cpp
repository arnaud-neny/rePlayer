// blwtbl.cpp
// druttis@darkface.pp.se

#define DRUTTIS__BAND_LIMITED_WAVES_TABLES__SOURCE
#include "blwtbl.h"
#include "../dsp/DspMath.h"

namespace psycle::plugins::druttis {

//Max sample rate = 96Khz
int const MAX_FREQUENCY = 48000 * 2;

//////////////////////////////////////////////////////////////////////
//
//				Increment to Frequency factor
//
//////////////////////////////////////////////////////////////////////
float incr2freq = 0.0f;

//////////////////////////////////////////////////////////////////////
// Simple data
namespace {
	// sine period used in the calculation of bandlimited waves.
	float *psinetable;
	// sampling rate for which the bandlimited waves and fm/pm tables are calculated.
	int samplingrate;
	// amount of partial waves for the bandlimited waves for this samplerate.
	int totalpartials;
	// Translation frequency (Hz) -> partial index for the bandlimited waves.
	// preverse[0] returns always index 0.
	int *preverse[2];
	//Amount of variance per frequency [0..1].
	float *pfmtable;
	//Amount of variance per frequency [0..WAVESIZE];
	float *ppmtable;
}

//////////////////////////////////////////////////////////////////////
// BlankFunc
void BlankFunc(float *pbuf, int len, int partial) {
	for (int i = 0; i < len; i++)
	{
		pbuf[i] += 0.0f;
	}
}

//////////////////////////////////////////////////////////////////////
// SineFunc
void SineFunc(float *pbuf, int len, int partial) {
	for (int i = 0; i < len; i++)
	{
		pbuf[i] += (float) sin((double) i * PI2 / (double) len);
	}
}

//////////////////////////////////////////////////////////////////////
// BLTriangleFunc
void BLTriangleFunc(float *pbuf, int len, int partial) {
	if (partial & 1)
	{
		int p = WAVESIZE >> 2; // cos phase
		float m = 1.0f / (float) (partial * partial);
		for (int i = 0; i < len; i++)
		{
			pbuf[i] += m * psinetable[(i * partial + p) & WAVEMASK];
		}
	}
}

//////////////////////////////////////////////////////////////////////
// BLSquareFunc
void BLSquareFunc(float *pbuf, int len, int partial) {
	if (partial & 1)
	{
		float m = 1.0f / (float) (partial);
		for (int i = 0; i < len; i++)
		{
			pbuf[i] += m * psinetable[(i * partial) & WAVEMASK];
		}
	}
}

//////////////////////////////////////////////////////////////////////
// BLSawtoothFunc
void BLSawtoothFunc(float *pbuf, int len, int partial) {
	float m = 1.0f / (float) (partial);
	for (int i = 0; i < len; i++)
	{
		pbuf[i] += m * psinetable[(i * partial) & WAVEMASK];
	}
}

//////////////////////////////////////////////////////////////////////
// BLParabolaFunc
void BLParabolaFunc(float *pbuf, int len, int partial) {
	int p = WAVESIZE >> 2; // cos phase
	float m = 1.0f / (float) (partial * partial);
	m *= 4.0f * ((partial & 1) == 0 ? -1.0f : 1.0f);
	for (int i = 0; i < len; i++)
	{
		pbuf[i] += m * psinetable[(i * partial + p) & WAVEMASK];
	}
}

//////////////////////////////////////////////////////////////////////
// TriangleFunc
void TriangleFunc(float *pbuf, int len, int partial) {
	float t;
	for (int i = 0; i < len; i++)
	{
		t = (float) i / (float) len;
		if (t < 0.25f)
		{
			pbuf[i] += t * 4.0f;
		}
		else if (t < 0.75f)
		{
			pbuf[i] += (0.5f - t) * 4.0f;
		}
		else
		{
			pbuf[i] += (t - 1.0f) * 4.0f;
		}
	}
}

//////////////////////////////////////////////////////////////////////
// SquareFunc
void SquareFunc(float *pbuf, int len, int partial) {
	int half = len >> 1;
	for (int i = 0; i < len; i++)
	{
		pbuf[i] += (i < half ? 1.0f : -1.0f);
	}
}

//////////////////////////////////////////////////////////////////////
// SawtoothFunc
void SawtoothFunc(float *pbuf, int len, int partial) {
	for (int i = 0; i < len; i++)
	{
		pbuf[i] += ((float) (i << 1) / (float) len) - 1.0f;
	}
}

//////////////////////////////////////////////////////////////////////
// RevSawtoothFunc
void RevSawtoothFunc(float *pbuf, int len, int partial) {
	for (int i = 0; i < len; i++)
	{
		pbuf[i] += 1.0f - ((float) (i << 1) / (float) len);
	}
}

//////////////////////////////////////////////////////////////////////
// WAVESPEC type
struct WAVESPEC {
	char const *pname;
	bool bandlimited;
	void (*func) (float *pbuf, int len, int partial);
	float *pdata;
	int ninstances;
};

//////////////////////////////////////////////////////////////////////
// WAVESPEC list
WAVESPEC wavespecs[] = {
	{ "Blank", false, BlankFunc, 0, 0 },
	{ "Sine", false, SineFunc, 0, 0 },
	{ "BL-Triangle", true, BLTriangleFunc, 0, 0 },
	{ "BL-Square", true, BLSquareFunc, 0, 0 },
	{ "BL-Sawtooth", true, BLSawtoothFunc, 0, 0 },
	{ "BL-Parabola", true, BLParabolaFunc, 0, 0 },
	{ "Triangle", false, TriangleFunc, 0, 0 },
	{ "Square", false, SquareFunc, 0, 0 },
	{ "Sawtooth", false, SawtoothFunc, 0, 0 },
	{ "Rev. Sawtooth", false, RevSawtoothFunc, 0, 0 }
};



//////////////////////////////////////////////////////////////////////
// InitLibrary
void InitLibrary() {
	int i;
	//Initialize environment
	samplingrate = 0;
	totalpartials = 0;
	
	// Create buffers
	psinetable = new float [WAVESIZE];
	preverse[0] = new int[MAX_FREQUENCY];
	preverse[1] = new int[MAX_FREQUENCY];
	pfmtable = new float[MAX_FREQUENCY];
	ppmtable = new float[MAX_FREQUENCY];
	// Create sine table
	for (i = 0; i < WAVESIZE; i++) {
		psinetable[i] = (float) sin((double) i * PI2 / (double) WAVESIZE);
	}
	// Initialize reverse lookup table 0
	for (i = 0; i < MAX_FREQUENCY; i++) {
		preverse[0][i] = 0;
	}

}

//////////////////////////////////////////////////////////////////////
// CreateWaveform
bool CreateWaveform(WAVESPEC *pspec) {
	// Variables
	int a;
	int b;
	int n;
	float freq;
	int numpartials;
	int lastnumpartials;
	int partialindex;
	float *pin;
	float *pout;
	float max;

	// Bandlimit?
	if (pspec->bandlimited == false) {
		pspec->pdata = new float[WAVESIZE];
		if (pspec->pdata == 0) {
			return false;
		}
		for (b = 0; b < WAVESIZE; b++) {
			pspec->pdata[b] = 0.0f;
		}
		(pspec->func)(pspec->pdata, WAVESIZE, 0);
		return true;
	}

	// Fast bandlimit creation
	pspec->pdata = new float[totalpartials * WAVESIZE];
	if (pspec->pdata == 0) {
		return false;
	}
	partialindex = totalpartials;
	lastnumpartials = 0;
	pin = 0;
	pout = 0;
	//For each note...
	for (n = 119; n >= 0; n--) {
		freq = 440.0f * (float) pow(2.0, (double) (n - 69) / 12.0);
		numpartials = (int) (0.5f * (float) samplingrate / freq);
		if (lastnumpartials < numpartials) {
			--partialindex;
			pout = &pspec->pdata[partialindex * WAVESIZE];
			if (pin) {
				for (b = 0; b < WAVESIZE; b++) {
					pout[b] = pin[b];
				}
			} else {
				for (b = 0; b < WAVESIZE; b++) {
					pout[b] = 0.0f;
				}
			}
			pin = pout;
			for (a = lastnumpartials+1; a <= numpartials; a++) {
				(pspec->func)(pout, WAVESIZE, a);
			}
			lastnumpartials = numpartials;
		}
	}
	
	// Normalize
	max = 0.0f;
	for (b = 0; b < WAVESIZE; b++) {
		if ((float) fabs(pout[b]) > max) {
			max = (float) fabs(pout[b]);
		} 
	}
	max = 1.0f/max;
	for (b = WAVESIZE * totalpartials; --b >= 0; ) {
		pspec->pdata[b] *= max;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////
// DeleteWaveforms
void DeleteWaveforms() {
	for (int i = 0; i < WF_NUM_WAVEFORMS; i++) {
		if (wavespecs[i].pdata != 0) {
			delete[] wavespecs[i].pdata;
			wavespecs[i].pdata = 0;
		}
	}
}

//////////////////////////////////////////////////////////////////////
// CloseLibrary
void CloseLibrary() {
	// Delete wave form data
	DeleteWaveforms();

	// Clear instances
	for (int i = 0; i < WF_NUM_WAVEFORMS; i++) {
		wavespecs[i].ninstances = 0;
	}

	// Delete buffers
	delete[] preverse[1];
	delete[] preverse[0];
	delete[] ppmtable;
	delete[] pfmtable;
	delete[] psinetable;
}

//////////////////////////////////////////////////////////////////////
// GetWaveInfo
bool GetWaveInfo(WAVEFORM* pwave) {
	if (!pwave) {
		return false;
	}
	if ((pwave->index < 0) || (pwave->index >= WF_NUM_WAVEFORMS)) {
		return false;
	}
	pwave->pname = wavespecs[pwave->index].pname;
	return true;
}

//////////////////////////////////////////////////////////////////////
// EnableWaveform
bool EnableWaveform(wf_type index) {
	if ((index < 0) || (index >= WF_NUM_WAVEFORMS)) {
		return false;
	}
	if (wavespecs[index].ninstances == 0) {
		CreateWaveform(&wavespecs[index]);
	}
	wavespecs[index].ninstances++;
	return true;
}

//////////////////////////////////////////////////////////////////////
// DisableWaveform
bool DisableWaveform(wf_type index) {
	if ((index < 0) || (index >= WF_NUM_WAVEFORMS)) {
		return false;
	}
	if (wavespecs[index].ninstances > 0) {
		wavespecs[index].ninstances--;
		if (wavespecs[index].ninstances == 0) {
			if (wavespecs[index].pdata != 0) {
				delete[] wavespecs[index].pdata;
				wavespecs[index].pdata = 0;
			}
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////
//GetFMTable
float* GetFMTable() {
	return pfmtable;
}

//////////////////////////////////////////////////////////////////////
// GetPMTable
float* GetPMTable() {
	return ppmtable;
}

//////////////////////////////////////////////////////////////////////
// GetWaveform
bool GetWaveform(wf_type index, WAVEFORM *pwave) {
	if (!pwave) {
		return false;
	}
	if ((index < 0) || (index >= WF_NUM_WAVEFORMS)) {
		pwave->index = WF_INIT_STRUCT;
		pwave->pname = 0;
		pwave->pdata = 0;
		pwave->preverse = preverse[0];
		return false;
	}
	EnableWaveform(index);
	pwave->index = index;
	pwave->pname = wavespecs[index].pname;
	pwave->pdata = wavespecs[index].pdata;
	pwave->preverse = (wavespecs[index].bandlimited ? preverse[1] : preverse[0]);
	return true;
}

//////////////////////////////////////////////////////////////////////
// UpdateWaveforms
bool UpdateWaveforms(int sr) {
	int a;
	int b;
	int n;
	float freq;
	int numpartials;
	int lastnumpartials;
	int reverseindex;
	
	// Check if new sampling rate
	if (samplingrate == sr) {
		return true;
	}
	samplingrate = sr;
	incr2freq = 2.0f * (float) sr / WAVEFSIZE;

	
	//Remove all waveform data
	DeleteWaveforms();
	
	// Recompute metric data
	lastnumpartials = 0;
	totalpartials = 0;
	reverseindex = 0;
	for (n = 0; n < 120; ++n) {
		freq = 440.0f * (float) pow(2.0, (double) (n - 69) / 12.0);
		numpartials = (int) (0.5f * (float) samplingrate / freq);
		if (lastnumpartials != numpartials) {
			a = (int) (2.0f * freq);
			for (b = reverseindex; b <= a; ++b) {
				preverse[1][b] = totalpartials;
			}
			reverseindex = a + 1;
			lastnumpartials = numpartials;
			totalpartials++;
		}
	}
	for (b = reverseindex; b < MAX_FREQUENCY; ++b) {
		preverse[1][b] = totalpartials - 1;
	}				
	
	// Recreate waveform data
	for (n = 0; n < WF_NUM_WAVEFORMS; ++n) {
		if (wavespecs[n].ninstances > 0) {
			if (!CreateWaveform(&wavespecs[n])) {
				wavespecs[n].pdata = 0;
				wavespecs[n].ninstances = 0;
			}
		}
	}

	// Setup amp table
	a = samplingrate >> 1;
	for (b = 0; b < a; ++b) {
		pfmtable[b] = 1.0f - (float) sqrt((float) (b + 1) / (float) a);
		ppmtable[b] = pfmtable[b] * WAVEFSIZE;
	}
	for (b = a; b < MAX_FREQUENCY; ++b) {
		pfmtable[b] = 0.0f;
		ppmtable[b] = 0.0f;
	}

	// Done
	return true;
}

/*
#if defined DIVERSALIS__COMPILER__GNU
	namespace init {
		void constructor() UNIVERSALIS__COMPILER__ATTRIBUTE(constructor) UNIVERSALIS__COMPILER__DYN_LINK__HIDDEN;
		void constructor() {
			InitLibrary();
			UpdateWaveforms(44100);
		}
		
		void destructor() UNIVERSALIS__COMPILER__ATTRIBUTE(destructor) UNIVERSALIS__COMPILER__DYN_LINK__HIDDEN;
		void destructor() {
			CloseLibrary();
		}
	}
#elif defined DIVERSALIS__OS__MICROSOFT && defined DIVERSALIS__COMPILER__MICROSOFT
	#include <universalis/os/include_windows_without_crap.hpp>
	// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dllproc/base/dynamic_link_library_functions.asp
	::BOOL APIENTRY DllMain(::HMODULE module, ::DWORD reason_for_call, ::LPVOID) {
		bool result(true);
		switch(reason_for_call) {
			case DLL_PROCESS_ATTACH:
				InitLibrary();
				UpdateWaveforms(44100);
			case DLL_THREAD_ATTACH:
				break;
			case DLL_THREAD_DETACH:
				break;
			case DLL_PROCESS_DETACH:
				CloseLibrary();
				break;
			default:
				result = false;
				break;
		}
		return result;
	}
#else
	#error unsupported platform
#endif
*/
}