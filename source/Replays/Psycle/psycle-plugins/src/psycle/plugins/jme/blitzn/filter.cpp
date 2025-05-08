// filter.cpp: implementation of the filter class.
//
//////////////////////////////////////////////////////////////////////

#include "filter.h"
#include <psycle/plugin_interface.hpp>
namespace psycle::plugins::jme::blitzn {
double const two_pi = 2 * psycle::plugin_interface::pi;
#define INTERPOLATE(pos,start,end) ((start)+(pos)*((end)-(start)))

float CBiquad::PreWarp(float dCutoff, float dSampleRate)
{
	if (dCutoff>dSampleRate*0.4) dCutoff=(float)(dSampleRate*0.4);
	//return (float)(/*1.0/*/tan(3.1415926/2.0-3.1415926*dCutoff/dSampleRate));
	return (float)(tan(3.1415926*dCutoff/dSampleRate));
}
float CBiquad::PreWarp2(float dCutoff, float dSampleRate)
{
	if (dCutoff>dSampleRate*0.4) dCutoff=(float)(dSampleRate*0.4);
	//return (float)(/*1.0/*/tan(3.1415926/2.0-3.1415926*dCutoff/dSampleRate));
	return (float)(tan(3.1415926/2.0-3.1415926*dCutoff/dSampleRate));
}
void CBiquad::SetBilinear(float B0, float B1, float B2, float A0, float A1, float A2)
{
	float q=(float)(1.0/(A0+A1+A2));
	m_b0=(B0+B1+B2)*q;
	m_b1=2*(B0-B2)*q;
	m_b2=(B0-B1+B2)*q;
	m_a1=2*(A0-A2)*q;
	m_a2=(A0-A1+A2)*q;
}
// Zoelzer's Parmentric Equalizer Filters - rodem z Csound'a
void CBiquad::SetLowShelf(float fc, float q, float v, float esr)
{
	float sq = (float)sqrt(2.0*(double)v);
	float omega = two_pi*fc/esr;
	float k = (float) tan((double)omega*0.5);
	float kk = k*k;
	float vkk = v*kk;
	float oda0 =  1.0f/(1.0f + k/q +kk);
	m_b0 =  oda0*(1.0f + sq*k + vkk);
	m_b1 =  oda0*(2.0f*(vkk - 1.0f));
	m_b2 =  oda0*(1.0f - sq*k + vkk);
	m_a1 =  oda0*(2.0f*(kk - 1.0f));
	m_a2 =  oda0*(1.0f - k/q + kk);
}
void CBiquad::SetHighShelf(float fc, float q, float v, float esr)
{
	float sq = (float)sqrt(2.0*(double)v);
	float omega = two_pi*fc/esr;
	float k = (float) tan((psycle::plugin_interface::pi - (double)omega)*0.5);
	float kk = k*k;
	float vkk = v*kk;
	float oda0 = 1.0f/( 1.0f + k/q +kk);
	m_b0 = oda0*( 1.0f + sq*k + vkk);
	m_b1 = oda0*(-2.0f*(vkk - 1.0f));
	m_b2 = oda0*( 1.0f - sq*k + vkk);
	m_a1 = oda0*(-2.0f*(kk - 1.0f));
	m_a2 = oda0*( 1.0f - k/q + kk);
}
void CBiquad::SetParametricEQ(float fc, float q, float v, float esr, float gain)
{
	//float sq = (float)sqrt(2.0*(double)v);
	float omega = two_pi*fc/esr;
	float k = (float) tan((double)omega*0.5);
	float kk = k*k;
	float vk = v*k;
	float vkdq = vk/q;
	float oda0 =  1.0f/(1.0f + k/q +kk);
	m_b0 =  gain*oda0*(1.0f + vkdq + kk);
	m_b1 =  gain*oda0*(2.0f*(kk - 1.0f));
	m_b2 =  gain*oda0*(1.0f - vkdq + kk);
	m_a1 =  oda0*(2.0f*(kk - 1.0f));
	m_a2 =  oda0*(1.0f - k/q + kk);
}
void CBiquad::SetLowpass1(float dCutoff, float dSampleRate)
{
	float a=PreWarp(dCutoff, dSampleRate);
	SetBilinear(a, 0, 0, a, 1, 0);
}
void CBiquad::SetHighpass1(float dCutoff, float dSampleRate)
{
	float a=PreWarp(dCutoff, dSampleRate);
	SetBilinear(0, 1, 0, a, 1, 0);
}
void CBiquad::SetIntegHighpass1(float dCutoff, float dSampleRate)
{
	float a=PreWarp(dCutoff, dSampleRate);
	SetBilinear(0, 1, 1, 0, 2*a, 2);
}
void CBiquad::SetAllpass1(float dCutoff, float dSampleRate)
{
	float a=PreWarp(dCutoff, dSampleRate);
	SetBilinear(1, -a, 0, 1, a, 0);
}
void CBiquad::SetAllpass2(float dCutoff, float fPoleR, float dSampleRate)
{
	float a=PreWarp(dCutoff, dSampleRate);
	float q=fPoleR;
	SetBilinear(a*a+q*q, -2.0f*a, 1, a*a+q*q, 2.0f*a, 1);
}
void CBiquad::SetBandpass(float dCutoff, float dBandwith, float dSampleRate)
{
	float b=(float)(2.0*psycle::plugin_interface::pi*dBandwith/dSampleRate);
	float a=(float)(PreWarp2(dCutoff, dSampleRate));
	SetBilinear(0, b*a, 0, 1, b*a, a*a);
}
void CBiquad::SetBandreject(float dCutoff, float dSampleRate)
{
	float a=(float)PreWarp(dCutoff, dSampleRate);
	SetBilinear(1, 0, 1, 1, a, 1);
}
void CBiquad::SetIntegrator()
{
	m_b0=1.0;
	m_a1=-1.0;
	m_a2=0.0;
	m_b1=0.0;
	m_b2=0.0;
}
void CBiquad::SetResonantLP(float dCutoff, float Q, float dSampleRate)
{
	float a=(float)PreWarp2(dCutoff, dSampleRate);
	// wspó³czynniki filtru analogowego
	float B=(float)(sqrt(Q*Q-1)/Q);
	float A=(float)(2*B*(1-B));
	SetBilinear(1, 0, 0, 1, A*a, B*a*a);
}
void CBiquad::SetResonantHP(float dCutoff, float Q, float dSampleRate)
{
	float a=(float)PreWarp2((dSampleRate/2)/dCutoff, dSampleRate);
	// wspó³czynniki filtru analogowego
	float B=(float)(sqrt(Q*Q-1)/Q);
	float A=(float)(2*B*(1-B));
	SetBilinear(0, 0, 1, B*a*a, A*a, 1);
}
void CBiquad::Reset()
{
	m_x1=m_y1=m_x2=m_y2=0.0f;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

filter::filter()
{
}

filter::~filter()
{
}
void filter::Init(int s) 
{
	sr=s;
	invert=false;
}
void filter::reset()
{
	Biquad.Reset();
}
void filter::setfilter(int type, int c,int r)
{
	switch(type)
	{
	case 0: break;
	case 1: SetFilter_4PoleLP(c,r); invert=false; break;
	case 2: SetFilter_4PoleLP(c,r); invert=true; break;
	case 3: SetFilter_4PoleEQ1(c,r); invert=false; break;
	case 4: SetFilter_4PoleEQ1(c,r); invert=true; break;
	case 5: SetFilter_4PoleEQ2(c,r); invert=false; break;
	case 6: SetFilter_4PoleEQ2(c,r); invert=true; break;
	case 7: SetFilter_Vocal1(c,r);invert=false; break;
	case 8: SetFilter_Vocal1(c,r);invert=true; break;
	case 9: SetFilter_Vocal2(c,r);invert=false; break;
	case 10: SetFilter_Vocal2(c,r);invert=true; break;
	}
}

void filter::SetFilter_4PoleLP(int CurCutoff, int Resonance)
{
	float CutoffFreq=(float)(564*std::pow(32.,CurCutoff/240.0));
	float cf=(float)CutoffFreq;
	if (cf>=sr/2) cf=sr/2; // próba wprowadzenia nieliniowoœci przy koñcu charakterystyki
	if (cf<11) cf=(float)(11.0);
	float ScaleResonance=1.0;
	float fQ=(float)sqrt(1.01+14*Resonance*ScaleResonance/240.0);

	float fB=(float)sqrt(fQ*fQ-1)/fQ;
	float fA=(float)(2*fB*(1-fB));

	float A,B;

	float ncf=(float)(1.0/tan(3.1415926*cf/(double)sr));
	A=fA*ncf;      // denormalizacja i uwzglêdnienie czêstotliwoœci próbkowania
	B=fB*ncf*ncf;
	float a0=float(1/(1+A+B));
	Biquad.m_b1=2*(Biquad.m_b2=Biquad.m_b0=a0);// obliczenie wspó³czynników filtru cyfrowego (przekszta³cenie dwuliniowe)
	Biquad.m_a1=a0*(2-B-B);
	Biquad.m_a2=a0*(1-A+B);
}

void filter::SetFilter_4PoleEQ1(int CurCutoff, int Resonance)
{
	float CutoffFreq=(float)(564*std::pow(32.,CurCutoff/240.0));
	float cf=(float)CutoffFreq;
	if (cf>=sr/2) cf=sr/2; // próba wprowadzenia nieliniowoœci przy koñcu charakterystyki
	if (cf<33) cf=(float)(33.0);
	//float ScaleResonance=1.0;
	//float fQ=(float)(1.01+30*Resonance*ScaleResonance/240.0);
	Biquad.SetParametricEQ(cf,(float)(1.0+Resonance/12.0),float(6+Resonance/30.0),sr,0.4f/(1+(240-Resonance)/120.0f));
}

void filter::SetFilter_4PoleEQ2(int CurCutoff, int Resonance)
{
	float CutoffFreq=(float)(564*std::pow(32.,CurCutoff/240.0));
	float cf=(float)CutoffFreq;
	if (cf>=sr/2) cf=sr/2; // próba wprowadzenia nieliniowoœci przy koñcu charakterystyki
	if (cf<33) cf=(float)(33.0);
	//float ScaleResonance=1.0;
	//float fQ=(float)(1.01+30*Resonance*ScaleResonance/240.0);
	Biquad.SetParametricEQ(cf,8.0f,9.0f,sr,0.5f);
}

#define THREESEL(sel,a,b,c) ((sel)<120)?((a)+((b)-(a))*(sel)/120):((b)+((c)-(b))*((sel)-120)/120)

void filter::SetFilter_Vocal1(int CurCutoff, int Resonance)
{
	float CutoffFreq=CurCutoff;
	float Cutoff1=THREESEL(CutoffFreq,270,400,800);
	//float Cutoff2=THREESEL(CutoffFreq,2140,800,1150);
	Biquad.SetParametricEQ(Cutoff1,2.0f+Resonance/48.0f,6.0f+Resonance/24.0f,sr,0.3f);
}

void filter::SetFilter_Vocal2(int CurCutoff, int Resonance)
{
	float CutoffFreq=CurCutoff;
	float Cutoff1=THREESEL(CutoffFreq,270,400,650);
	Biquad.SetParametricEQ(Cutoff1,2.0f+Resonance/56.0f,6.0f+Resonance/16.0f,sr,0.3f);
}
/*
void filter::SetFilter_ResonantHiPass(int CurCutoff, int Resonance)
{
Biquad.SetResonantHP(float dCutoff, float Q, float dSampleRate)

}
*/
}