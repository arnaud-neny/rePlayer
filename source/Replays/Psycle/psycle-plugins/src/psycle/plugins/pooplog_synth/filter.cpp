// filter.cpp: implementation of the filter class.
//
//////////////////////////////////////////////////////////////////////
#include "filter.h"
#ifdef SYNTH_LIGHT
#ifdef SYNTH_ULTRALIGHT
namespace psycle::plugins::pooplog_synth_ultralight {
#else
namespace psycle::plugins::pooplog_synth_light {
#endif
#else
namespace psycle::plugins::pooplog_synth {
#endif
#define INTERPOLATE(pos,start,end) ((start)+(pos)*((end)-(start)))

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

filter::filter()
{
	denormal = (float)1.0E-18;
}

filter::~filter()
{

}

void filter::SetFilter_4PoleLP(float cf, int Resonance)
{
	cf=CUTOFFCONV(cf);
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

	ncf=(float)(1.0/tan(3.1415926*(cf*0.7)/(double)sr));
	A=fA*ncf;      // denormalizacja i uwzglêdnienie czêstotliwoœci próbkowania
	B=fB*ncf*ncf;
	a0=float(1/(1+A+B));
	Biquad2.m_b1=2*(Biquad2.m_b2=Biquad2.m_b0=0.35f*a0);// obliczenie wspó³czynników filtru cyfrowego (przekszta³cenie dwuliniowe)
	Biquad2.m_a1=a0*(2-B-B);
	Biquad2.m_a2=a0*(1-A+B);
}

void filter::SetFilter_4PoleEQ1(float cf, int Resonance)
{
	cf=CUTOFFCONV(cf);
	//float ScaleResonance=1.0;
	//float fQ=(float)(1.01+30*Resonance*ScaleResonance/240.0);
	Biquad.SetParametricEQ(cf,(float)(1.0+Resonance/12.0),float(6+Resonance/30.0),sr,0.4f/(1+(240-Resonance)/120.0f));
	Biquad2.SetParametricEQ(float(cf/(1+Resonance/240.0)),float(1.0+Resonance/12.0),float(6+Resonance/30.0),sr,0.4f);
}

void filter::SetFilter_4PoleEQ2(float cf, int Resonance)
{
	cf=CUTOFFCONV(cf);
	//float ScaleResonance=1.0;
	//float fQ=(float)(1.01+30*Resonance*ScaleResonance/240.0);
	Biquad.SetParametricEQ(cf,8.0f,9.0f,sr,0.5f);
	Biquad2.SetParametricEQ(float(cf/(3.5-2*Resonance/240.0)),8.0f,9.0f,sr,0.4f);
}

void filter::SetFilter_Vocal1(float cf, int Resonance)
{
	float Cutoff1=THREESEL(cf,270,400,800);
	float Cutoff2=THREESEL(cf,2140,800,1150);
	Biquad.SetParametricEQ(Cutoff1,2.0f+Resonance/48.0f,6.0f+Resonance/24.0f,sr,0.3f);
	Biquad2.SetParametricEQ(Cutoff2,2.0f+Resonance/48.0f,6.0f+Resonance/24.0f,sr,0.3f);
}

void filter::SetFilter_Vocal2(float cf, int Resonance)
{
	float Cutoff1=THREESEL(cf,270,400,650);
	float Cutoff2=THREESEL(cf,2140,1700,1080);
	Biquad.SetParametricEQ(Cutoff1,2.0f+Resonance/56.0f,6.0f+Resonance/16.0f,sr,0.3f);
	Biquad2.SetParametricEQ(Cutoff2,2.0f+Resonance/56.0f,6.0f+Resonance/16.0f,sr,0.3f);
}
}