#pragma once
#include <psycle/helpers/math/erase_denormals.hpp>
namespace psycle::plugins::yezar_freeverb {
// Comb filter class declaration
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain

class comb
{
public:
					comb();
	virtual			~comb();
			void	setbuffer(int samples);
	inline  float	process(float inp);
			void	mute();
			void	setdamp(float val);
			float	getdamp();
			void	setfeedback(float val);
			float	getfeedback();
private:
	void deletebuffer();

	float			*buffer;
	float			feedback;
	float			filterstore;
	float			damp1;
	float			damp2;
	int				bufsize;
	int				bufidx;
};

// Big to inline - but crucial for speed

inline float comb::process(float input)
{
	float bufin;
	float output = buffer[bufidx];

	filterstore = (output*damp2) + (filterstore*damp1);
	psycle::helpers::math::fast_erase_denormals_inplace(filterstore);

	bufin = input + (filterstore*feedback);

	psycle::helpers::math::fast_erase_denormals_inplace(bufin);
	buffer[bufidx] = bufin;

	if(++bufidx>=bufsize) bufidx = 0;

	return output;
}
}