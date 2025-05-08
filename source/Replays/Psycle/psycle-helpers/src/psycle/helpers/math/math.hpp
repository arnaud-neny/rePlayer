// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2011 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__HELPERS__MATH__MATH__INCLUDED
#define PSYCLE__HELPERS__MATH__MATH__INCLUDED
#pragma once

#include "constants.hpp"
#include "log.hpp"
#include "sin.hpp"
#include "sinseq.hpp"
#include "clip.hpp"
#include "erase_all_nans_infinities_and_denormals.hpp"
#include "erase_denormals.hpp"
#include <universalis.hpp>
#include <universalis/stdlib/cmath.hpp>

namespace psycle { namespace helpers { namespace math {

using universalis::stdlib::asinh;
using universalis::stdlib::sincos;
using universalis::stdlib::rint;
using universalis::stdlib::round;

/// compares two floating point numbers for rough equality (difference less than epsilon by default).
template<typename Real> UNIVERSALIS__COMPILER__CONST_FUNCTION
bool inline roughly_equals(Real a, Real b, Real tolerance = std::numeric_limits<Real>::epsilon()) {
	return std::abs(a - b) < tolerance;
}

/// compile-time factorial.
template<unsigned int i>
struct compile_time_factorial {
	unsigned int const static value = i * compile_time_factorial<i - 1>::value;
	BOOST_STATIC_ASSERT(value > 0); // makes constant overflows errors, not just warnings
};
///\internal template specialisation for compile-time factorial of 0.
template<> struct compile_time_factorial<0u> { unsigned int const static value = 1; };

template<typename Real> UNIVERSALIS__COMPILER__CONST_FUNCTION
Real inline deci_bell_to_linear(Real deci_bell) {
	///\todo merge with psycle::helpers::dsp::dB2Amp
	return std::pow(10u, deci_bell / Real(20));
}

template<typename Real> UNIVERSALIS__COMPILER__CONST_FUNCTION
Real inline linear_to_deci_bell(Real linear) {
	///\todo merge with psycle::helpers::dsp::dB
	return Real(20) * std::log10(linear);
}

}}}

#endif
