#pragma once

namespace psycle::plugins::arguru_reverb {

class CCombFilter  
{
public:
	CCombFilter();
	virtual ~CCombFilter() throw();
	void Clear();
	void Initialize(int new_rate, int time, int stph);
	void SetDelay(int time, int stph);
	inline void Work(float l_input,float r_input);
	inline float left_output() { return leftBuffer[l_delayedCounter]; }
	inline float right_output() { return rightBuffer[r_delayedCounter]; }

private:
	void DeleteBuffer();

	float *leftBuffer;
	float *rightBuffer;
	int l_delayedCounter;
	int r_delayedCounter;
	int Counter;
	int bufferSize;

};

inline void CCombFilter::Work(float l_input,float r_input)
{
	//This isn't a combfilter. This is only a time delay with different
	//delay between left and right.
	leftBuffer[Counter]=l_input;
	rightBuffer[Counter]=r_input;

	if(++Counter>=bufferSize)Counter=0;
	if(++l_delayedCounter>=bufferSize)l_delayedCounter=0;
	if(++r_delayedCounter>=bufferSize)r_delayedCounter=0;
}
}