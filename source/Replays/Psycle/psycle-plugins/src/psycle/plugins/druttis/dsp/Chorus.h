//////////////////////////////////////////////////////////////////////
//
//				Chorus.h
//
//				druttis@darkface.pp.se
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "DspAlgs.h"

namespace psycle::plugins::druttis {

//////////////////////////////////////////////////////////////////////
//				Chorus class
//////////////////////////////////////////////////////////////////////

class Chorus
{
	//////////////////////////////////////////////////////////////////
	//				Variables
	//////////////////////////////////////////////////////////////////

private:

	float*				inputs;
	int								mask;
	int								inpos;
	int								outpos;
	float				frac;
	float				zm;
	float				fb;
	float				mix;
	float				nmix;

	//////////////////////////////////////////////////////////////////
	//				Methods
	//////////////////////////////////////////////////////////////////

public:

	Chorus()
	{
		inputs = 0;
		Init(1024);
		SetFeedback(0.0f);
		SetMix(0.5f);
		SetDelay(112.0f);
	}

	virtual ~Chorus()
	{
		delete[] inputs;
	}

	inline void Init(int maxDelay)
	{
		mask = v2m(maxDelay);
		delete[] inputs;
		inputs = new float[mask + 1];
		inpos = 0;
		Reset();
	}

	inline void Reset()
	{
		zm = 0.0f;
		for (int i = mask; i >= 0; i--)
			inputs[i] = 0.0f;
	}

	void SetMix(float value)
	{
		mix = value;
		nmix = 1.0f - mix;
	}

	void SetFeedback(float value)
	{
		fb = value;
	}

	inline void SetDelay(float delay)
	{
		float tmp = (float) (inpos - delay);
		tmp += (float) mask + 1.f;
		outpos = f2i(tmp);
		frac = tmp - (float) outpos;
		outpos &= mask;
	}
	
	inline float Tick(float in)
	{
		inputs[inpos++] = in - zm * fb;
		inpos &= mask;
		float out = inputs[outpos++];
		outpos &= mask;
		out += frac * (inputs[outpos] - out);
		zm = out;
		return in * nmix + out * mix;
	}

};
}