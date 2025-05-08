#include "envelope.hpp"
namespace psycle::plugins::zephod_super_fm {
envelope::envelope()
{
	envcoef=0;
	suscounter=0;
}

envelope::~envelope()
{
}

void envelope::reset()
{
	if(envstate==ENV_NONE)
	{
		envstate=ENV_ATT;
		envcoef=1.0f/(float)a;
	}
	else
	{
		envstate=ENV_ANTICLICK;
		envcoef=envvol/32.0f;
	}
}

void envelope::attack(int newv)
{
	a=newv;
}
void envelope::decay(int newv)
{
	d=newv;
}
void envelope::sustain(int newv)
{
	s=newv;
}
void envelope::release(int newv)
{
	r=newv;
}
void envelope::sustainv(float newv)
{
	susvol=newv;
}
void envelope::stop()
{
	envvol=0;
	envstate=ENV_NONE;
}

void envelope::noteoff()
{
	if (envstate!=ENV_NONE)
	{
		envstate=ENV_REL;
		envcoef=envvol/(float)r;
	}
}
}