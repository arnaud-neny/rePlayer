//============================================================================
//
//				DLineN (Non-Interpolating Delay Line)
//
//				druttis@darkface.pp.se
//
//============================================================================
#pragma once
//============================================================================
//				Class
//============================================================================
#include <psycle/helpers/math/erase_all_nans_infinities_and_denormals.hpp>

namespace psycle::plugins::druttis {

using namespace psycle::helpers::math;
class DLineN
{
private:
	float				*inputs;
	int								inPoint;
	int								outPoint;
	int								length;
	float				delay;
	float				lastOutput;
public:
	DLineN();
	~DLineN();
	void Init();
	void Init(int maxLength);
	void Clear();
	void SetDelay(float lag);
	float GetDelay();
	//------------------------------------------------------------------------
	//				GetLastOutput
	//------------------------------------------------------------------------
	inline float GetLastOutput()
	{
		return lastOutput;
	}
	//------------------------------------------------------------------------
	//				Tick
	//------------------------------------------------------------------------
	inline float Tick(float sample)
	{
		erase_all_nans_infinities_and_denormals(sample);
		inputs[inPoint++] = sample;
		while (inPoint >= length)
			inPoint -= length;
		lastOutput = inputs[outPoint++];
		while (outPoint >= length)
			outPoint -= length;
		return lastOutput;
	}
};
}