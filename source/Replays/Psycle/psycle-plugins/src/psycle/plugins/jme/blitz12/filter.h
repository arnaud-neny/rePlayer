// filter.h: interface for the filter class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include <cmath>
#include <psycle/plugin_interface.hpp>
namespace psycle::plugins::jme::blitz12 {
typedef float SIG;
double const two_pi = 2 * psycle::plugin_interface::pi;

class CBiquad
{
public:
	float m_a1, m_a2, m_b0, m_b1, m_b2;
	float m_x1, m_x2, m_y1, m_y2;
	CBiquad() { m_x1=0.0; m_y1=0.0; m_x2=0.0; m_y2=0.0; }
	inline SIG ProcessSample(SIG dSmp) { 
		SIG dOut=m_b0*dSmp+m_b1*m_x1+m_b2*m_x2-m_a1*m_y1-m_a2*m_y2;
	if (dOut>=-0.00001 && dOut<=0.00001) dOut=0.0;
	if (dOut>900000) dOut=900000.0;
		if (dOut<-900000) dOut=-900000.0;
		m_x2=m_x1;
		m_x1=dSmp;
		m_y2=m_y1;
		m_y1=dOut;
		return dOut;
	}
	float PreWarp(float dCutoff, float dSampleRate)
	{
		if (dCutoff>dSampleRate*0.4) dCutoff=(float)(dSampleRate*0.4);
		//return (float)(/*1.0/*/tan(3.1415926/2.0-3.1415926*dCutoff/dSampleRate));
		return (float)(tan(3.1415926*dCutoff/dSampleRate));
	}
	float PreWarp2(float dCutoff, float dSampleRate)
	{
		if (dCutoff>dSampleRate*0.4) dCutoff=(float)(dSampleRate*0.4);
		//return (float)(/*1.0/*/tan(3.1415926/2.0-3.1415926*dCutoff/dSampleRate));
		return (float)(tan(3.1415926/2.0-3.1415926*dCutoff/dSampleRate));
	}
	void SetBilinear(float B0, float B1, float B2, float A0, float A1, float A2)
	{
		float q=(float)(1.0/(A0+A1+A2));
		m_b0=(B0+B1+B2)*q;
		m_b1=2*(B0-B2)*q;
		m_b2=(B0-B1+B2)*q;
		m_a1=2*(A0-A2)*q;
		m_a2=(A0-A1+A2)*q;
	}
	// Zoelzer's Parmentric Equalizer Filters - rodem z Csound'a
	void SetLowShelf(float fc, float q, float v, float esr)
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
	void SetHighShelf(float fc, float q, float v, float esr)
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
	void SetParametricEQ(float fc, float q, float v, float esr, float gain=1.0f)
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
	void SetLowpass1(float dCutoff, float dSampleRate)
	{
		float a=PreWarp(dCutoff, dSampleRate);
		SetBilinear(a, 0, 0, a, 1, 0);
	}
	void SetHighpass1(float dCutoff, float dSampleRate)
	{
		float a=PreWarp(dCutoff, dSampleRate);
		SetBilinear(0, 1, 0, a, 1, 0);
	}
	void SetIntegHighpass1(float dCutoff, float dSampleRate)
	{
		float a=PreWarp(dCutoff, dSampleRate);
		SetBilinear(0, 1, 1, 0, 2*a, 2);
	}
	void SetAllpass1(float dCutoff, float dSampleRate)
	{
		float a=PreWarp(dCutoff, dSampleRate);
		SetBilinear(1, -a, 0, 1, a, 0);
	}
	void SetAllpass2(float dCutoff, float fPoleR, float dSampleRate)
	{
		float a=PreWarp(dCutoff, dSampleRate);
		float q=fPoleR;
		SetBilinear(a*a+q*q, -2.0f*a, 1, a*a+q*q, 2.0f*a, 1);
	}
	void SetBandpass(float dCutoff, float dBandwith, float dSampleRate)
	{
		float b=(float)(2.0*psycle::plugin_interface::pi*dBandwith/dSampleRate);
		float a=(float)(PreWarp2(dCutoff, dSampleRate));
		SetBilinear(0, b*a, 0, 1, b*a, a*a);
	}
	void SetBandreject(float dCutoff, float dSampleRate)
	{
		float a=(float)PreWarp(dCutoff, dSampleRate);
		SetBilinear(1, 0, 1, 1, a, 1);
	}
	void SetIntegrator()
	{
		m_b0=1.0;
		m_a1=-1.0;
		m_a2=0.0;
		m_b1=0.0;
		m_b2=0.0;
	}
	void SetResonantLP(float dCutoff, float Q, float dSampleRate)
	{
		float a=(float)PreWarp2(dCutoff, dSampleRate);
		// wsp�czynniki filtru analogowego
		float B=(float)(sqrt(Q*Q-1)/Q);
		float A=(float)(2*B*(1-B));
		SetBilinear(1, 0, 0, 1, A*a, B*a*a);
	}
	void SetResonantHP(float dCutoff, float Q, float dSampleRate)
	{
		float a=(float)PreWarp2((dSampleRate/2)/dCutoff, dSampleRate);
		// wsp�czynniki filtru analogowego
		float B=(float)(sqrt(Q*Q-1)/Q);
		float A=(float)(2*B*(1-B));
		SetBilinear(0, 0, 1, B*a*a, A*a, 1);
	}
	void Reset()
	{
	m_x1=m_y1=m_x2=m_y2=0.0f;
	}
};


class filter  
{
public:
	filter();
	virtual ~filter();
	void init(int s) 
	{
		sr=s;
		invert=false;
	}
	float res(float in)
	{
		float out=Biquad.ProcessSample(in);
		if (invert) return in-out;
		return out;
	}
	void reset()
	{
		Biquad.Reset();
	}
	void SetFilter_4PoleLP(int CurCutoff, int Resonance);
	void SetFilter_4PoleEQ1(int CurCutoff, int Resonance); 
	void SetFilter_4PoleEQ2(int CurCutoff, int Resonance); 
	void SetFilter_Vocal1(int CurCutoff, int Resonance);
	void SetFilter_Vocal2(int CurCutoff, int Resonance);
	
	void setfilter(int type, int c,int r)

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
	


	CBiquad Biquad;
		
private: 
	int type;
	bool invert;
	int sr;
};
}