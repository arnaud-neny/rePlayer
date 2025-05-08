#include "AllPass.hpp"
#include <universalis/os/aligned_alloc.hpp>
#include <psycle/helpers/dsp.hpp>

namespace psycle::plugins::arguru_reverb {

CAllPass::CAllPass(): l_delayedCounter(0), r_delayedCounter(0),Counter(0),
left_output_(0.0f), right_output_(0.0f), bufferSize(0) {
}

CAllPass::~CAllPass() throw() {
	DeleteBuffer();
}

void CAllPass::Clear() {
	psycle::helpers::dsp::Clear(leftBuffer, bufferSize);
	psycle::helpers::dsp::Clear(rightBuffer, bufferSize);
}

void CAllPass::Initialize(int max_size, int time, int stph) {
	DeleteBuffer();
	//properly aligned.
	bufferSize = (max_size+3)&0xFFFFFF00;
	universalis::os::aligned_memory_alloc(16, leftBuffer, bufferSize);
	universalis::os::aligned_memory_alloc(16, rightBuffer, bufferSize);
	Clear();
	if(Counter>=bufferSize) Counter = 0;
	SetDelay(time,stph);
}

void CAllPass::SetDelay(int time, int stph) {
	l_delayedCounter = Counter - std::min(time, bufferSize);
	r_delayedCounter = l_delayedCounter - std::min(stph, bufferSize);
	if(l_delayedCounter < 0) l_delayedCounter += bufferSize;
	if(r_delayedCounter < 0) r_delayedCounter += bufferSize;
}

void CAllPass::DeleteBuffer() {
	if(bufferSize) {
		universalis::os::aligned_memory_dealloc(leftBuffer);
		universalis::os::aligned_memory_dealloc(rightBuffer);
	}
}
}