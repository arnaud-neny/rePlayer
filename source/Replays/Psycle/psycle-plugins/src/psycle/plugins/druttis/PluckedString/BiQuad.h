//============================================================================
//
//				BiQuad (2 pole, 2 zero) filter
//
//	BUGGY CODE. TAKE THE ONE FROM DSP FOR ANYTHING OTHER THAN THIS PLUGIN
//
//============================================================================
#pragma once

#include <psycle/helpers/math/erase_all_nans_infinities_and_denormals.hpp>
namespace psycle::plugins::druttis::plucked_string {
using namespace psycle::helpers::math;

class BiQuad
{
protected:
	float				inputs[2];
	float				gain;
	float				lastOutput;
	float				poleCoeffs[2];
	float				zeroCoeffs[2];
public:
	BiQuad();
	~BiQuad();
	void Init();
	void Clear();
	void SetRawCoeffs(float a1,float a2,float b1,float b2);
	void SetGain(float newGain);
	//------------------------------------------------------------------------
	//				tick
	//	This method is buggy. Instead of keeping a buffer of inputs and one
	//  of outputs outputs, it keeps a buffer of half-processed inputs
	//------------------------------------------------------------------------
	inline float Tick(float sample)
	{
		erase_all_nans_infinities_and_denormals(sample);
		float temp;
		//
		//
		temp = sample * gain;
		temp += inputs[0] * poleCoeffs[0];
		temp += inputs[1] * poleCoeffs[1];
		//
		//
		erase_all_nans_infinities_and_denormals(temp);

		lastOutput = temp;
		lastOutput += (inputs[0] * zeroCoeffs[0]);
		lastOutput += (inputs[1] * zeroCoeffs[1]);
		inputs[1] = inputs[0];
		inputs[0] = temp;
		erase_all_nans_infinities_and_denormals(lastOutput);
		//
		//
		return lastOutput;
	}
};
}