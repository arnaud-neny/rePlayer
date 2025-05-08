///\file
///\brief wave tables
#pragma once
#include <algorithm>
#include <psycle/helpers/math.hpp>

namespace psycle::plugins::druttis {
using namespace psycle::helpers::math;

/// wave size (2^n)
unsigned int const WAVESIZE = 4096;
/// wave size (2^n - 1)
unsigned int const WAVEMASK = 4095;

/// afloat structure
struct afloat {
	int inertiaSamples;
	float inertiaAmount;
	float target;
	float current;
};

/// SetAFloat
inline void SetAFloat(afloat& afloat, float value, int inertiaSamples) {
	if (inertiaSamples > 0) {
		float const dist = value - afloat.current;
		afloat.inertiaAmount = dist/inertiaSamples;
		afloat.inertiaSamples = inertiaSamples;
	}
	else {
		afloat.current = value;
		afloat.inertiaSamples = 0;
	}
	afloat.target = value;
}
/// AnimateAFloat
inline void AnimateAFloat(afloat& p) {
	if(p.inertiaSamples-- > 0) p.current += p.inertiaAmount;
	else p.current = p.target;
}

/// Get wavetable sample
inline float GetWTSample(float *wavetable, float phase) {
	return wavetable[rint<int>(phase) & WAVEMASK];
}

/// Get wavetable sample with linear interpolation
inline float GetWTSampleLinear(float *wavetable, float phase) {
	int const pos = rint<int>(phase);
	float const fraction = phase - (float) pos;
	float const out = wavetable[pos & WAVEMASK];
	return out + fraction * (wavetable[(pos + 1) & WAVEMASK] - out);
}

}