#include <cassert>
#include "oscillator.hpp"
namespace psycle::plugins::jm_drums {
Coscillator::Coscillator()
{
	setSampleRate(44100);
	incremspeed = 0.015;
	envpos = 0;
}

Coscillator::~Coscillator()
{

}

void Coscillator::setEnvPos(float epos)
{
	assert(epos<=MAX_ENVPOS);
	envpos=epos;
	
}
void Coscillator::setSampleRate(int nsamplerate)
{
	hz=nsamplerate;
	hztosamples=(float)MAX_ENVPOS/nsamplerate;
}
}