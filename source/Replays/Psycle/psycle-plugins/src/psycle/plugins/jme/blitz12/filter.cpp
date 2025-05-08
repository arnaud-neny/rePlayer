// filter.cpp: implementation of the filter class.
//
//////////////////////////////////////////////////////////////////////

#include "filter.h"
namespace psycle::plugins::jme::blitz12 {
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

filter::filter()
{

}

filter::~filter()
{

}
#define INTERPOLATE(pos,start,end) ((start)+(pos)*((end)-(start)))

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