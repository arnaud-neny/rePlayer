//////////////////////////////////////////////////////////////////////
//
//				Dsp.h
//
//				druttis@darkface.pp.se
//
//////////////////////////////////////////////////////////////////////
#pragma once
#include "DspMath.h"
#include "Envelope.h"
#include "Filter.h"
#include "Formant.h"
#include "Inertia.h"

namespace psycle::plugins::druttis {

//////////////////////////////////////////////////////////////////////
//
//				Initialization & Destruction functions
//
//////////////////////////////////////////////////////////////////////

#if defined _MSC_VER
	#define DRUTTIS__DSP__CALLING_CONVENTION__C __cdecl
	#define DRUTTIS__DSP__CALLING_CONVENTION__FAST __fastcall
#else
	#define DRUTTIS__DSP__CALLING_CONVENTION__C
	#define DRUTTIS__DSP__CALLING_CONVENTION__FAST
#endif

void DRUTTIS__DSP__CALLING_CONVENTION__C InitializeDSP();
void DRUTTIS__DSP__CALLING_CONVENTION__C DestroyDSP();
void DRUTTIS__DSP__CALLING_CONVENTION__FAST Fill(float *pbuf, float value, int nsamples);
void DRUTTIS__DSP__CALLING_CONVENTION__FAST Copy(float *pbuf1, float *pbuf2, int nsamples);
void DRUTTIS__DSP__CALLING_CONVENTION__FAST Add(float *pbuf1, float *pbuf2, int nsamples);
void DRUTTIS__DSP__CALLING_CONVENTION__FAST Sub(float *pbuf1, float *pbuf2, int nsamples);
void DRUTTIS__DSP__CALLING_CONVENTION__FAST Mul(float *pbuf1, float *pbuf2, int nsamples);
}