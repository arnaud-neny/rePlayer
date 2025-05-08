// blwtbl.h
// druttis@darkface.pp.se
#pragma once
#include <universalis.hpp>

namespace psycle::plugins::druttis {
// Constants
#define WAVESIBI 8
#define WAVESIZE 256
#define WAVEMASK 255
#define WAVEFSIZE 256.0f

// Waveform constants
typedef enum {
	WF_INIT_STRUCT=-1,
	WF_BLANK=0,
	WF_SINE,
	WF_BLTRIANGLE,
	WF_BLSQUARE,
	WF_BLSAWTOOTH,
	WF_BLPARABOLA,
	WF_TRIANGLE,
	WF_SQUARE,
	WF_SAWTOOTH,
	WF_REVSAWTOOTH,


	WF_NUM_WAVEFORMS
} wf_type;

/// WAVEFORM type
struct WAVEFORM {
	wf_type index; // Index of waveform (wavenumber)
	//int count; // How many shares this wave now.
	char const *pname; // Name
	float *pdata; // Data, partial or non partial
	int *preverse; // Lookup table to find pdata offset
};

/*
#ifdef DRUTTIS__BAND_LIMITED_WAVES_TABLES__SOURCE
	#define DRUTTIS__BAND_LIMITED_WAVES_TABLES__DECL UNIVERSALIS__COMPILER__DYN_LINK__EXPORT
#else
	#define DRUTTIS__BAND_LIMITED_WAVES_TABLES__DECL UNIVERSALIS__COMPILER__DYN_LINK__IMPORT
#endif

#if !defined DRUTTIS__BAND_LIMITED_WAVES_TABLES__SOURCE && defined DIVERSALIS__COMPILER__FEATURE__AUTO_LINK
	#pragma comment(lib, "blwtbl")
#endif
*/
#define DRUTTIS__BAND_LIMITED_WAVES_TABLES__DECL

DRUTTIS__BAND_LIMITED_WAVES_TABLES__DECL extern float incr2freq;

DRUTTIS__BAND_LIMITED_WAVES_TABLES__DECL bool EnableWaveform(wf_type index);
DRUTTIS__BAND_LIMITED_WAVES_TABLES__DECL bool DisableWaveform(wf_type index);
DRUTTIS__BAND_LIMITED_WAVES_TABLES__DECL bool GetWaveform(wf_type wavenum, WAVEFORM *pwave);
DRUTTIS__BAND_LIMITED_WAVES_TABLES__DECL bool UpdateWaveforms(int samplingrate);
DRUTTIS__BAND_LIMITED_WAVES_TABLES__DECL bool GetWaveInfo(WAVEFORM* pwave);
DRUTTIS__BAND_LIMITED_WAVES_TABLES__DECL float* GetFMTable();
DRUTTIS__BAND_LIMITED_WAVES_TABLES__DECL float* GetPMTable();

void InitLibrary();
bool UpdateWaveforms(int sr);
void CloseLibrary();
}