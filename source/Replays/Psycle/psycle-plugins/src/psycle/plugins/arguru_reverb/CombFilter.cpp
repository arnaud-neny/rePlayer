#include "CombFilter.hpp"
#include <universalis/os/aligned_alloc.hpp>
#include <psycle/helpers/dsp.hpp>

namespace psycle::plugins::arguru_reverb {

CCombFilter::CCombFilter()
:
	leftBuffer(0), rightBuffer(0),
	l_delayedCounter(0), r_delayedCounter(0),
	Counter(0), bufferSize(0)
{}

CCombFilter::~CCombFilter() throw() {
	DeleteBuffer();
}

void CCombFilter::Clear() {
	psycle::helpers::dsp::Clear(leftBuffer, bufferSize);
	psycle::helpers::dsp::Clear(rightBuffer, bufferSize);
}

void CCombFilter::SetDelay(int time, int stph) {
	l_delayedCounter = Counter - std::min(time, bufferSize);
	r_delayedCounter = l_delayedCounter - std::min(stph, bufferSize);
	if(l_delayedCounter < 0) l_delayedCounter += bufferSize;
	if(r_delayedCounter < 0) r_delayedCounter += bufferSize;
}

void CCombFilter::Initialize(int new_rate, int time, int stph){
	DeleteBuffer();
	//Allow up to 0.75 seconds of delay, properly aligned.
	bufferSize = (int(new_rate*0.75)+3)&0xFFFFFF00;
	universalis::os::aligned_memory_alloc(16, leftBuffer, bufferSize);
	universalis::os::aligned_memory_alloc(16, rightBuffer, bufferSize);
	Clear();
	if(Counter>=bufferSize)Counter=0;
	SetDelay(time, stph);
}

void CCombFilter::DeleteBuffer() {
	if (bufferSize) {
		universalis::os::aligned_memory_dealloc(leftBuffer);
		universalis::os::aligned_memory_dealloc(rightBuffer);
	}
}
}