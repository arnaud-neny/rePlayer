#include "AllPass.hpp"
#include <universalis/os/aligned_alloc.hpp>
#include <psycle/helpers/dsp.hpp>
namespace psycle::plugins::yezar_freeverb {
// Allpass filter implementation
//
// Originally Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain

allpass::allpass()
: buffer(0), feedback(0), bufsize(0), bufidx(0)
{}

allpass::~allpass()
{
	deletebuffer();
}

void allpass::setbuffer(int samples)
{
	if (bufsize) {
		deletebuffer();
	}
	bufsize = samples;
	universalis::os::aligned_memory_alloc(16, buffer, (bufsize+3)&0xFFFFFFFC);
	if(bufidx>=bufsize) bufidx = 0;
}

void allpass::mute()
{
	psycle::helpers::dsp::Clear(buffer, bufsize);
}

void allpass::setfeedback(float val) 
{
	feedback = val;
}

float allpass::getfeedback() 
{
	return feedback;
}

void allpass::deletebuffer() {
	if (bufsize) {
		universalis::os::aligned_memory_dealloc(buffer);
	}
}
}