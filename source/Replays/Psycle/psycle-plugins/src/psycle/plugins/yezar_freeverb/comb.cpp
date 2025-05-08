#include "comb.hpp"
#include <universalis/os/aligned_alloc.hpp>
#include <psycle/helpers/dsp.hpp>
namespace psycle::plugins::yezar_freeverb {
// Comb filter implementation
//
// Originally Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain

comb::comb()
: buffer(0), feedback(0), filterstore(0), damp1(0), damp2(0),
	bufsize(0), bufidx(0)
{}
comb::~comb()
{
	deletebuffer();
}

void comb::setbuffer(int samples)
{
	if (bufsize) {
		deletebuffer();
	}
	bufsize = samples;
	universalis::os::aligned_memory_alloc(16, buffer, (bufsize+3)&0xFFFFFFFC);
	if(bufidx>=bufsize) bufidx = 0;
}

void comb::mute()
{
	psycle::helpers::dsp::Clear(buffer, bufsize);
}

void comb::setdamp(float val) 
{
	//todo: this will need some work for multiple sampling rates.
	damp1 = val; 
	damp2 = 1-val;
}

float comb::getdamp() 
{
	return damp1;
}

void comb::setfeedback(float val) 
{
	feedback = val;
}

float comb::getfeedback() 
{
	return feedback;
}
void comb::deletebuffer() {
	if (bufsize) {
		universalis::os::aligned_memory_dealloc(buffer);
	}
}
}