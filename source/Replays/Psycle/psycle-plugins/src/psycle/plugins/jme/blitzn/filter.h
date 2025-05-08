// filter.h: interface for the filter class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include <cmath>
namespace psycle::plugins::jme::blitzn {
typedef float SIG;

class CBiquad
{
public:
	CBiquad() { m_x1=0.0; m_y1=0.0; m_x2=0.0; m_y2=0.0; }

	float PreWarp(float dCutoff, float dSampleRate);
	float PreWarp2(float dCutoff, float dSampleRate);
	void SetBilinear(float B0, float B1, float B2, float A0, float A1, float A2);
	// Zoelzer's Parmentric Equalizer Filters - rodem z Csound'a
	void SetLowShelf(float fc, float q, float v, float esr);
	void SetHighShelf(float fc, float q, float v, float esr);
	void SetParametricEQ(float fc, float q, float v, float esr, float gain=1.0f);
	void SetLowpass1(float dCutoff, float dSampleRate);
	void SetHighpass1(float dCutoff, float dSampleRate);
	void SetIntegHighpass1(float dCutoff, float dSampleRate);
	void SetAllpass1(float dCutoff, float dSampleRate);
	void SetAllpass2(float dCutoff, float fPoleR, float dSampleRate);
	void SetBandpass(float dCutoff, float dBandwith, float dSampleRate);
	void SetBandreject(float dCutoff, float dSampleRate);
	void SetIntegrator();
	void SetResonantLP(float dCutoff, float Q, float dSampleRate);
	void SetResonantHP(float dCutoff, float Q, float dSampleRate);
	void Reset();

	inline SIG ProcessSample(SIG dSmp);

	float m_a1, m_a2, m_b0, m_b1, m_b2;
	float m_x1, m_x2, m_y1, m_y2;
};


class filter  
{
public:
	filter();
	virtual ~filter();
	void Init(int s);
	void reset();
	void SetFilter_4PoleLP(int CurCutoff, int Resonance);
	void SetFilter_4PoleEQ1(int CurCutoff, int Resonance); 
	void SetFilter_4PoleEQ2(int CurCutoff, int Resonance); 
	void SetFilter_Vocal1(int CurCutoff, int Resonance);
	void SetFilter_Vocal2(int CurCutoff, int Resonance);
	void setfilter(int type, int c,int r);

	inline float res(float in);

	CBiquad Biquad;

private: 
	int type;
	bool invert;
	int sr;
};

inline SIG CBiquad::ProcessSample(SIG dSmp) { 
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

inline float filter::res(float in)
	{
		float out=Biquad.ProcessSample(in);
		if (invert) return in-out;
		return out;
	}
}