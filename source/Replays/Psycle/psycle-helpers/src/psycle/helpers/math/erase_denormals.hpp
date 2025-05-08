// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__HELPERS__MATH__ERASE_DENORMALS__INCLUDED
#define PSYCLE__HELPERS__MATH__ERASE_DENORMALS__INCLUDED
#pragma once

#include <universalis.hpp>

namespace psycle { namespace helpers { namespace math {

float inline UNIVERSALIS__COMPILER__CONST_FUNCTION erase_denormals(float x) {
	x += 1.0e-30f;
	x -= 1.0e-30f;
	return x;
}

double inline UNIVERSALIS__COMPILER__CONST_FUNCTION erase_denormals(double x) {
	x += 1.0e-291;
	x -= 1.0e-291;
	return x;
}

float inline erase_denormals_inplace(float & x) {
	return x = erase_denormals(x);
}

double inline erase_denormals_inplace(double & x) {
	return x = erase_denormals(x);
}

float inline UNIVERSALIS__COMPILER__CONST_FUNCTION fast_erase_denormals(float x) {
	return x + 1.0e-30f;
}

double inline UNIVERSALIS__COMPILER__CONST_FUNCTION fast_erase_denormals(double x) {
	return x + 1.0e-291;
}

float inline fast_erase_denormals_inplace(float & x) {
	return x += 1.0e-30f;
}

double inline fast_erase_denormals_inplace(double & x) {
	return x += 1.0e-291;
}

}}}

#endif
