#pragma once

#include <psycle/helpers/math/erase_all_nans_infinities_and_denormals.hpp>

namespace psycle::plugins::arguru_reverb {

class CAllPass
{
public:
	CAllPass();
	virtual ~CAllPass() throw();
	void Clear();
	void Initialize(int max_size, int time, int spth);
	void SetDelay(int time, int stph);
	inline void Work(float l_input,float r_input,float g);
	inline float left_output() const { return left_output_; }
	inline float right_output() const { return right_output_; }

private:
	void DeleteBuffer();

	float left_output_;
	float right_output_;
	float *leftBuffer;
	float *rightBuffer;
	int l_delayedCounter;
	int r_delayedCounter;
	int Counter;
	int bufferSize;
};

inline void CAllPass::Work(float l_input,float r_input,float g)
{
	///This implementation is strange: at 0 feedback, it acts as a time delay.
	//Increasing the feedback, increases the gain of the direct output path while
	//also increasing the feedback of the total output into the delayed buffer.
	left_output_=(l_input*-g)+leftBuffer[l_delayedCounter];
	right_output_=(r_input*-g)+rightBuffer[r_delayedCounter];

	float tmpleft = l_input+left_output_*g;
	float tmpright = r_input+right_output_*g;

	psycle::helpers::math::erase_all_nans_infinities_and_denormals(tmpleft);
	psycle::helpers::math::erase_all_nans_infinities_and_denormals(tmpright);
	
	leftBuffer[Counter] = tmpleft;
	rightBuffer[Counter] = tmpright;

	if(++Counter>=bufferSize) Counter = 0;
	if(++l_delayedCounter>=bufferSize) l_delayedCounter = 0;
	if(++r_delayedCounter>=bufferSize) r_delayedCounter = 0;
}
}