//////////////////////////////////////////////////////////////////////
//
//				Dsp.cpp
//
//				druttis@darkface.pp.se
//
//////////////////////////////////////////////////////////////////////
#include "Dsp.h"

namespace psycle::plugins::druttis {

//////////////////////////////////////////////////////////////////////
// Variables


//////////////////////////////////////////////////////////////////////
// InitializeDSP
void DRUTTIS__DSP__CALLING_CONVENTION__C InitializeDSP() {
}

//////////////////////////////////////////////////////////////////////
// DestroyDSP
void DRUTTIS__DSP__CALLING_CONVENTION__C DestroyDSP() {
}

//////////////////////////////////////////////////////////////////////
// Fill
void DRUTTIS__DSP__CALLING_CONVENTION__FAST Fill(float *pbuf, float value, int nsamples) {
	--pbuf;
	do
	{
		*++pbuf = value;
	}
	while (--nsamples);
}

//////////////////////////////////////////////////////////////////////
// Copy
void DRUTTIS__DSP__CALLING_CONVENTION__FAST Copy(float *pbuf1, float *pbuf2, int nsamples) {
	--pbuf1;
	--pbuf2;
	do
	{
		*++pbuf1 = *++pbuf2;
	}
	while (--nsamples);
}

//////////////////////////////////////////////////////////////////////
// Add
void DRUTTIS__DSP__CALLING_CONVENTION__FAST Add(float *pbuf1, float *pbuf2, int nsamples) {
	--pbuf1;
	--pbuf2;
	do
	{
		*++pbuf1 += *++pbuf2;
	}
	while (--nsamples);
}

//////////////////////////////////////////////////////////////////////
// Sub
void DRUTTIS__DSP__CALLING_CONVENTION__FAST Sub(float *pbuf1, float *pbuf2, int nsamples) {
	--pbuf1;
	--pbuf2;
	do {
		*++pbuf1 -= *++pbuf2;
	}
	while(--nsamples);
}

//////////////////////////////////////////////////////////////////////
// Mul
void DRUTTIS__DSP__CALLING_CONVENTION__FAST Mul(float *pbuf1, float *pbuf2, int nsamples) {
	--pbuf1;
	--pbuf2;
	do {
		*++pbuf1 *= *++pbuf2;
	}
	while(--nsamples);
}
}