#include "RevModel.hpp"
namespace psycle::plugins::yezar_freeverb {
// Reverb model implementation
//
// Originally Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain

revmodel::revmodel()
: gain(0), roomsize(0), roomsize1(0), damp(0), damp1(0)
, wet(0), wet1(0), wet2(0), dry(0), width(0), mode(0)
{
}
void revmodel::recalculatebuffers(int samplerate)
{
	float srfactor = samplerate/44100.0f;
	// Set up the buffer sizes
	combL[0].setbuffer(combtuningL1*srfactor);
	combR[0].setbuffer(combtuningR1*srfactor);
	combL[1].setbuffer(combtuningL2*srfactor);
	combR[1].setbuffer(combtuningR2*srfactor);
	combL[2].setbuffer(combtuningL3*srfactor);
	combR[2].setbuffer(combtuningR3*srfactor);
	combL[3].setbuffer(combtuningL4*srfactor);
	combR[3].setbuffer(combtuningR4*srfactor);
	combL[4].setbuffer(combtuningL5*srfactor);
	combR[4].setbuffer(combtuningR5*srfactor);
	combL[5].setbuffer(combtuningL6*srfactor);
	combR[5].setbuffer(combtuningR6*srfactor);
	combL[6].setbuffer(combtuningL7*srfactor);
	combR[6].setbuffer(combtuningR7*srfactor);
	combL[7].setbuffer(combtuningL8*srfactor);
	combR[7].setbuffer(combtuningR8*srfactor);
	allpassL[0].setbuffer(allpasstuningL1*srfactor);
	allpassR[0].setbuffer(allpasstuningR1*srfactor);
	allpassL[1].setbuffer(allpasstuningL2*srfactor);
	allpassR[1].setbuffer(allpasstuningR2*srfactor);
	allpassL[2].setbuffer(allpasstuningL3*srfactor);
	allpassR[2].setbuffer(allpasstuningR3*srfactor);
	allpassL[3].setbuffer(allpasstuningL4*srfactor);
	allpassR[3].setbuffer(allpasstuningR4*srfactor);
	// Buffer will be full of rubbish - so we MUST mute them
	mute();
}
void revmodel::setdefaultvalues(int samplerate)
{
	recalculatebuffers(samplerate);
	// Set default values
	allpassL[0].setfeedback(0.5f);
	allpassR[0].setfeedback(0.5f);
	allpassL[1].setfeedback(0.5f);
	allpassR[1].setfeedback(0.5f);
	allpassL[2].setfeedback(0.5f);
	allpassR[2].setfeedback(0.5f);
	allpassL[3].setfeedback(0.5f);
	allpassR[3].setfeedback(0.5f);

	setroomsize(initialroom, false);
	setdamp(initialdamp, false);
	setwet(initialwet, false);
	setdry(initialdry);
	setwidth(initialwidth, false);
	setmode(initialmode, false);

	update();
}

void revmodel::mute()
{
	if (getmode() >= freezemode)
		return;

	for (int i=0;i<numcombs;i++)
	{
		combL[i].mute();
		combR[i].mute();
	}
	for (int i=0;i<numallpasses;i++)
	{
		allpassL[i].mute();
		allpassR[i].mute();
	}
}

void revmodel::processreplace(float *inputL, float *inputR, float *outputL, float *outputR, long numsamples, int skip)
{
	float outL,outR,input;

	while(numsamples-- > 0)
	{
		outL = outR = 0;
		input = (*inputL + *inputR) * gain;

		// Accumulate comb filters in parallel
		for(int i=0; i<numcombs; i++)
		{
			outL += combL[i].process(input);
			outR += combR[i].process(input);
		}

		// Feed through allpasses in series
		for(int i=0; i<numallpasses; i++)
		{
			outL = allpassL[i].process(outL);
			outR = allpassR[i].process(outR);
		}

		// Calculate output REPLACING anything already there
		*outputL = outL*wet1 + outR*wet2 + *inputL*dry;
		*outputR = outR*wet1 + outL*wet2 + *inputR*dry;

		// Increment sample pointers, allowing for interleave (if any)
		inputL += skip;
		inputR += skip;
		outputL += skip;
		outputR += skip;
	}
}

void revmodel::processmix(float *inputL, float *inputR, float *outputL, float *outputR, long numsamples, int skip)
{
	float outL,outR,input;

	while(numsamples-- > 0)
	{
		outL = outR = 0;
		input = (*inputL + *inputR) * gain;

		// Accumulate comb filters in parallel
		for(int i=0; i<numcombs; i++)
		{
			outL += combL[i].process(input);
			outR += combR[i].process(input);
		}

		// Feed through allpasses in series
		for(int i=0; i<numallpasses; i++)
		{
			outL = allpassL[i].process(outL);
			outR = allpassR[i].process(outR);
		}

		// Calculate output MIXING with anything already there
		*outputL += outL*wet1 + outR*wet2 + *inputL*dry;
		*outputR += outR*wet1 + outL*wet2 + *inputR*dry;

		// Increment sample pointers, allowing for interleave (if any)
		inputL += skip;
		inputR += skip;
		outputL += skip;
		outputR += skip;
	}
}

void revmodel::update()
{
// Recalculate internal values after parameter change

	int i;

	wet1 = wet*(width/2.0f + 0.5f);
	wet2 = wet*((1.0f-width)/2.0f);

	if (mode >= freezemode)
	{
		roomsize1 = 1.0f;
		damp1 = 0.0f;
		gain = muted;
	}
	else
	{
		roomsize1 = roomsize;
		damp1 = damp;
		gain = fixedgain;
	}

	for(i=0; i<numcombs; i++)
	{
		combL[i].setfeedback(roomsize1);
		combR[i].setfeedback(roomsize1);
		combL[i].setdamp(damp1);
		combR[i].setdamp(damp1);
	}
}

// The following get/set functions are not inlined, because
// speed is never an issue when calling them, and also
// because as you develop the reverb model, you may
// wish to take dynamic action when they are called.

void revmodel::setroomsize(float value, float doupdate)
{
	roomsize = (value*scaleroom) + offsetroom;
	if (doupdate) update();
}

float revmodel::getroomsize()
{
	return (roomsize-offsetroom)/scaleroom;
}

void revmodel::setdamp(float value, float doupdate)
{
	damp = value*scaledamp;
	if (doupdate) update();
}

float revmodel::getdamp()
{
	return damp/scaledamp;
}

void revmodel::setwet(float value, float doupdate)
{
	wet = value*scalewet;
	if(doupdate) update();
}

float revmodel::getwet()
{
	return wet/scalewet;
}

void revmodel::setdry(float value)
{
	dry = value*scaledry;
}

float revmodel::getdry()
{
	return dry/scaledry;
}

void revmodel::setwidth(float value, float doupdate)
{
	width = value;
	if(doupdate) update();
}

float revmodel::getwidth()
{
	return width;
}

void revmodel::setmode(float value, float doupdate)
{
	mode = value;
	if(doupdate) update();
}

float revmodel::getmode()
{
	if (mode >= freezemode)
		return 1;
	else
		return 0;
}
}