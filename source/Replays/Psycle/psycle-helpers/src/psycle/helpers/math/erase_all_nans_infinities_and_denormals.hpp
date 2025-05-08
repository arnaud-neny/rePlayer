// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__HELPERS__MATH__ERASE_ALL_NANS_INFINITES_AND_DENORMALS__INCLUDED
#define PSYCLE__HELPERS__MATH__ERASE_ALL_NANS_INFINITES_AND_DENORMALS__INCLUDED
#pragma once

#include <universalis.hpp>

namespace psycle { namespace helpers { namespace math {

using namespace universalis::stdlib;

/// Cure for malicious samples
/// Type : Filters Denormals, NaNs, Infinities
/// References : Posted by urs[AT]u-he[DOT]com
void inline erase_all_nans_infinities_and_denormals(float & sample) {
	#if !defined DIVERSALIS__CPU__X86
		// just do nothing.. not crucial for other archs ?
	#else
		union {
			float sample;
			uint32_t bits;
		} u;
		u.sample = sample;

		uint32_t const exponent_mask(0x7f800000);
		uint32_t const exponent(u.bits & exponent_mask);

		// exponent < exponent_mask is 0 if NaN or Infinity, otherwise 1
		uint32_t const not_nan_nor_infinity(exponent < exponent_mask);

		// exponent > 0 is 0 if denormalized, otherwise 1
		uint32_t const not_denormal(exponent > 0);

		u.bits *= not_nan_nor_infinity & not_denormal;
		sample = u.sample;
	#endif
}

///\todo This works, but uses too much CPU probably. (at least on 32bit processors)
void inline erase_all_nans_infinities_and_denormals(double & sample) {
	#if !defined DIVERSALIS__CPU__X86
		// just do nothing.. not crucial for other archs ?
	#else
		union {
			double sample;
			uint64_t bits;
		} u;
		u.sample = sample;

		uint64_t const exponent_mask(0x7f80000000000000ULL);
		uint64_t const exponent(u.bits & exponent_mask);

		// exponent < exponent_mask is 0 if NaN or Infinity, otherwise 1
		uint64_t const not_nan_nor_infinity(exponent < exponent_mask);

		// exponent > 0 is 0 if denormalized, otherwise 1
		uint64_t const not_denormal(exponent > 0);

		u.bits *= not_nan_nor_infinity & not_denormal;
		sample = u.sample;
	#endif
}

void inline erase_all_nans_infinities_and_denormals(float samples[], unsigned int const sample_count) {
	for(unsigned int i(0); i < sample_count; ++i) erase_all_nans_infinities_and_denormals(samples[i]);
}

void inline erase_all_nans_infinities_and_denormals(double samples[], unsigned int const sample_count) {
	for(unsigned int i(0); i < sample_count; ++i) erase_all_nans_infinities_and_denormals(samples[i]);
}

}}}

#endif
