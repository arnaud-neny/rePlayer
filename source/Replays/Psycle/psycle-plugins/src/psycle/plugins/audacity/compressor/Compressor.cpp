/**********************************************************************

Audacity: A Digital Audio Editor

Compressor.cpp

Dominic Mazzoni
Martyn Shaw
Steve Jolly

Based on svn revision 10189
*******************************************************************//**

\class EffectCompressor
\brief An Effect derived from EffectTwoPassSimpleMono

- Martyn Shaw made it inherit from EffectTwoPassSimpleMono 10/2005.
- Steve Jolly made it inherit from EffectSimpleMono.
- GUI added and implementation improved by Dominic Mazzoni, 5/11/2003.

*//****************************************************************/


#include "Compressor.h"
#include <psycle/helpers/math/erase_all_nans_infinities_and_denormals.hpp>
#include <cmath>

namespace psycle::plugins::audacity_compressor {

EffectCompressor::EffectCompressor():
	mCurRate(44100.0),
	mNormalize(true),
	mUsePeak(),
	mCircle(),
	mFollow1(),
	mFollowLen()
{
	setAttackSec(0.2);
	setDecaySec(1.0);
	setRatio(2.0);
	setThresholddB(-12.0);
	setNoiseFloordB(-40.0);
}

EffectCompressor::~EffectCompressor() {
	delete[] mCircle;
	delete[] mFollow1;
}

void EffectCompressor::Init(int max_samples) {
	InitPass1(max_samples);
	NewTrackPass1();
}

bool EffectCompressor::NewTrackPass1() {
	mNoiseCounter = 100;
	mLastLevel = mThreshold;
	delete[] mCircle;
	mCircleSize = mCurRate * 0.05;
	mCircle = new double[mCircleSize];
	for(int i = 0; i < mCircleSize; ++i) mCircle[i] = 0;
	mCirclePos = 0;
	mRMSSum = 0.0;
	return true;
}

bool EffectCompressor::InitPass1(int max_samples) {
	delete[] mFollow1;
	mFollow1 = new float[max_samples];
	mFollowLen = max_samples;
	return true;
}

void EffectCompressor::setSampleRate(double rate) {
	mCurRate = rate;
	this->setAttackFactor();
	this->setDecayFactor();
}

void EffectCompressor::setThresholddB(double threshold) {
	mThresholdDB = threshold;
	mThreshold = std::pow(10.0, mThresholdDB/20); // factor of 20 because it's amplitude
	setGain(mNormalize);
}

void EffectCompressor::setNoiseFloordB(double noisefloor) {
	mNoiseFloorDB = noisefloor;
	mNoiseFloor = std::pow(10.0, mNoiseFloorDB/20); // factor of 20 because it's amplitude
}

void EffectCompressor::setRatio(double ratio) {
	mRatio = ratio;
	if(mRatio > 1)
		mCompression = 1.0 - 1.0 / mRatio;
	else
		mCompression = 0.0;
	setGain(mNormalize);
}

void EffectCompressor::setAttackSec(double attack) {
	mAttackTime = attack;
	this->setAttackFactor();
}

inline void EffectCompressor::setAttackFactor() {
	mAttackInverseFactor = std::exp(log(mThreshold) / (mCurRate * mAttackTime + 0.5));
	mAttackFactor = 1.0 / mAttackInverseFactor;
}

void EffectCompressor::setDecaySec(double decay) {
	mDecayTime = decay;
	this->setDecayFactor();
}

inline void EffectCompressor::setDecayFactor() {
	mDecayFactor = std::exp(log(mThreshold) / (mCurRate * mDecayTime + 0.5));
}

void EffectCompressor::setNormalize(bool normalize) {
	setGain(normalize);
}

void EffectCompressor::setPeakAnalisys(bool peak) {
	mUsePeak = peak;
}

void EffectCompressor::setGain(bool gain) {
	// Using the old gain method, since the two pass method cannot be used in Psycle.
	mNormalize = gain;
	double mGaindB = ((mThresholdDB*-0.7) * (1 - 1/mRatio));
	if(mGaindB < 0) mGaindB = 0;
	if(gain)
		mGain = std::pow(10.0, mGaindB/20); // factor of 20 because it's amplitude
	else
		mGain = 1.0;
}

void EffectCompressor::Process(float *samples, int num) {
	// Adapting from psycle's range
	float *buffer = samples;
	for(int i = 0; i < num; ++i) {
		*buffer = *buffer * 0.000030517578125f; // -1 < buffer[i] < 1
		++buffer;
	}
	Follow(samples, mFollow1, num, 0, 0);
	for(int i = 0; i < num; ++i) samples[i] = DoCompression(samples[i], mFollow1[i]);
	ProcessPass2(samples, num);
}

bool EffectCompressor::ProcessPass2(float *buffer, int len) {
	// Adding gain and adapting to psycle's range
	float invMax = mGain * 32768.0f;
	for(int i = 0; i < len; ++i) buffer[i] *= invMax;
	return true;
}

void EffectCompressor::FreshenCircle() {
	// Recompute the RMS sum periodically to prevent accumulation of rounding errors
	// during long waveforms
	mRMSSum = 0;
	for(int i = 0; i < mCircleSize; ++i) mRMSSum += mCircle[i];
}

float EffectCompressor::AvgCircle(float value) {
	// Calculate current level from root-mean-squared of
	// circular buffer ("RMS")
	mRMSSum -= mCircle[mCirclePos];
	mCircle[mCirclePos] = value * value;
	mRMSSum += mCircle[mCirclePos];
	float level = std::sqrt(mRMSSum / mCircleSize);
	psycle::helpers::math::erase_all_nans_infinities_and_denormals(level);
	mCirclePos = (mCirclePos+1)%mCircleSize;
	return level;
}
float EffectCompressor::lastLevel() {
	return mLastLevel;
}
void EffectCompressor::Follow(float *buffer, float *env, int len, float *previous, int previous_len) {
	/*
		"Follow"ing algorithm by Roger B. Dannenberg, taken from
		Nyquist.  His description follows.  -DMM

		Description: this is a sophisticated envelope follower.
		The input is an envelope, e.g. something produced with
		the AVG function. The purpose of this function is to
		generate a smooth envelope that is generally not less
		than the input signal. In other words, we want to "ride"
		the peaks of the signal with a smooth function. The
		algorithm is as follows: keep a current output value
		(called the "value"). The value is allowed to increase
		by at most rise_factor and decrease by at most fall_factor.
		Therefore, the next value should be between
		value * rise_factor and value * fall_factor. If the input
		is in this range, then the next value is simply the input.
		If the input is less than value * fall_factor, then the
		next value is just value * fall_factor, which will be greater
		than the input signal. If the input is greater than value *
		rise_factor, then we compute a rising envelope that meets
		the input value by working bacwards in time, changing the
		previous values to input / rise_factor, input / rise_factor^2,
		input / rise_factor^3, etc. until this new envelope intersects
		the previously computed values. There is only a limited buffer
		in which we can work backwards, so if the new envelope does not
		intersect the old one, then make yet another pass, this time
		from the oldest buffered value forward, increasing on each
		sample by rise_factor to produce a maximal envelope. This will
		still be less than the input.

		The value has a lower limit of floor to make sure value has a
		reasonable positive value from which to begin an attack.
	*/

	if(!mUsePeak) {
		// Update RMS sum directly from the circle buffer
		// to avoid accumulation of rounding errors
		FreshenCircle();
	}
	// First apply a peak detect with the requested decay rate
	double last = mLastLevel;
	for(int i = 0; i < len; ++i) {
		double level;
		if(mUsePeak || mNormalize)
			level = std::fabs(buffer[i]);
		else // use RMS
			level = AvgCircle(buffer[i]);
		// Don't increase gain when signal is continuously below the noise floor
		if(level < mNoiseFloor)
			++mNoiseCounter;
		else
			mNoiseCounter = 0;
		if(mNoiseCounter < 100) {
			last *= mDecayFactor;
			if(last < mThreshold) last = mThreshold;
			if(level > last) last = level;
		}
		env[i] = last;
	}
	mLastLevel = last;

	// Next do the same process in reverse direction to get the requested attack rate
	last = mLastLevel;
	for(int i = len - 1; i >= 0; --i) {
		last *= mAttackInverseFactor;
		if(last < mThreshold) last = mThreshold;
		if(env[i] < last)
			env[i] = last;
		else
			last = env[i];
	}
}

float EffectCompressor::DoCompression(float value, double env) {
	float out;
	if(mUsePeak) {
		// Peak values map mThreshold to 1.0 - 'upward' compression
		out = value * std::pow(1.0/env, mCompression);
	} else {
		// With RMS-based compression don't change values below mThreshold - 'downward' compression
		out = value * std::pow(mThreshold/env, mCompression);
	}
	return out;
}
}