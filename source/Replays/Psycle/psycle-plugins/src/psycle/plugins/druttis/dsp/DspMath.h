// DspMath.h
// druttis@darkface.pp.se

#pragma once
#include <psycle/helpers/math.hpp>

namespace psycle::plugins::druttis {
using namespace psycle::helpers::math;

// PI constants
#define PI 3.1415926536f
#define PI2 6.2831853072f
#define HALFPI 1.5707963268f

//////////////////////////////////////////////////////////////////////
// floor2int : fast floor thing
UNIVERSALIS__COMPILER__DEPRECATED("portability: someone come back and centralise this")
inline int floor2i(float x) {
	#if defined _MSC_VER && defined _M_IX86 && _MSC_VER < 1400 ///\todo [bohan] i'm disabling this on msvc 8 because all of druttis plugins have shown weird behavior when built with this compiler
		__asm
		{
			FLD								DWORD PTR [x]
			FIST				DWORD PTR [ESP-8]
			SUB								ESP, 8
			FISUB				DWORD PTR [ESP]
			NOP
			FSTP				DWORD PTR [ESP+4]
			POP								EAX
			POP								EDX
			ADD								EDX, 7FFFFFFFH
			SBB								EAX, 0
		}
	#else
		return static_cast<int>(std::floor(x));
	#endif
}

//////////////////////////////////////////////////////////////////////
// fand : like fmod(x, m - 1) but faster and handles neg. x
inline float fand(float val, int mask) {
	const int index = floor2i(val);
	return (float) (index & mask) + (val - (float) index);
}

//////////////////////////////////////////////////////////////////////
/// Converts milliseconds to samples
inline int millis2samples(int ms, int samplerate) {
	return ms * samplerate / 1000;
}

//////////////////////////////////////////////////////////////////////
// Converts a note to a phase increment
inline float note2incr(int size, float note, int samplerate) {
	return (float) size * 440.0f * (float) pow(2.0, (note - 69.0) / 12.0) / (float) samplerate;
}

//////////////////////////////////////////////////////////////////////
// Converts a note to a value in Hz.
inline float note2freq(float note) {
	return (float) 440.0f * (float) pow(2.0, (note - 69.0) / 12.0);
}

//////////////////////////////////////////////////////////////////////
// Get random number
inline unsigned long rnd_number() { 
	static unsigned long randSeed = 22222; 
	randSeed = (randSeed * 196314165) + 907633515; 
	return randSeed; 
}

//////////////////////////////////////////////////////////////////////
// Get random signal
inline float rnd_signal() {
	return (float) ((int) rnd_number()  & 0xffff) * 0.000030517578125f - 1.0f;
}
}