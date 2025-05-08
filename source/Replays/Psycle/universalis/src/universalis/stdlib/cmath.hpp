// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2014 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\file \brief cmath standard header

#pragma once
#include <universalis/detail/project.hpp>
#include <universalis/compiler/const_function.hpp>
#include <universalis/compiler/restrict.hpp>
#include <universalis/compiler/noexcept.hpp>
#include <boost/static_assert.hpp>
#include <cmath>
#include <limits>
#include "cstdint.hpp"

#ifdef BOOST_AUTO_TEST_CASE
	#include <universalis/compiler/typenameof.hpp>
	#include <sstream>
	#if DIVERSALIS__STDLIB__MATH >= 199901 // some test using C1999's features
		#include <fenv.h>
	#endif
#endif

#if __cplusplus >= 201103L
	// TODO Check whether Microsoft Visual C++ brings what it promises: C99's math functions being part of C++11, might we finally use those ?
#else
	// Microsoft Visual C++ never supported C99's math functions, at least not before they claimed C++11 compliance.
#endif

namespace universalis { namespace stdlib {

/******************************************************************************************/
// Constants
// Only POSIX defines these constants, they're deceptively not part of C++11

/// john napier's log(2) aka ln(2) constant
double constexpr log2 = /* std::log(2) */ .69314718055994530941723212145817656807550013436025/*...*/;
/// apery's zeta(3) constant
double constexpr zeta3 = 1.20205690315959428539973816151144999076498629234049/*...*/;
/// leibniz's and euler's e constant
double constexpr e = /* std::exp(1) */ 2.71828182845904523536028747135266249775724709369995/*...*/;
/// pythagoreans' nu2 constant
double constexpr nu2 = 1.41421356237309504880168872420969807856967187537694/*...*/;
/// leonhard euler's gamma constant
double constexpr gamma = .57721566490153286060651209008240243104215933593992/*...*/;
/// archimedes' pi constant
double constexpr pi = /* std::acos(-1) */ 3.1415926535897932384626433832795028841971693993751/*...*/;



/******************************************************************************************/
/// asinh
#if DIVERSALIS__STDLIB__MATH >= 199901L
	using std::asinh;
#else
	template<typename Real> UNIVERSALIS__COMPILER__CONST_FUNCTION
	Real inline asinh(Real x) noexcept {
		return
			x > 0 ?
			+std::log(+x + std::sqrt(Real(1) + x * x)) :
			-std::log(-x + std::sqrt(Real(1) + x * x));
	}
#endif



/******************************************************************************************/
/// sincos - computes both the sine and the cosine at the same time
template<typename Real>
void inline sincos (
	Real x,
	Real & UNIVERSALIS__COMPILER__RESTRICT_REF sine,
	Real & UNIVERSALIS__COMPILER__RESTRICT_REF cosine
) noexcept {
	// some compilers are able to optimise those two calls into one.
	sine = std::sin(x);
	cosine = std::cos(x);
}

#if DIVERSALIS__STDLIB__MATH >= 199901 // or _GNU_SOURCE actually, it seems this is some GNU extension: http://refspecs.linux-foundation.org/LSB_4.0.0/LSB-Core-generic/LSB-Core-generic/baselib-sincosl.html
	template<>
	void inline sincos<>(
		long double const x,
		long double & UNIVERSALIS__COMPILER__RESTRICT_REF sine,
		long double & UNIVERSALIS__COMPILER__RESTRICT_REF cosine
	) noexcept {
		::sincosl(x, &sine, &cosine);
	}

	template<>
	void inline sincos<>(
		double const x,
		double & UNIVERSALIS__COMPILER__RESTRICT_REF sine,
		double & UNIVERSALIS__COMPILER__RESTRICT_REF cosine
	) noexcept {
		::sincos(x, &sine, &cosine);
	}

	template<>
	void inline sincos<>(
		float const x,
		float & UNIVERSALIS__COMPILER__RESTRICT_REF sine,
		float & UNIVERSALIS__COMPILER__RESTRICT_REF cosine
	) noexcept {
		::sincosf(x, &sine, &cosine);
	}
#endif

#ifdef BOOST_AUTO_TEST_CASE
	namespace test {
		template<typename Real>
		void sincos_test_template() {
			Real const tolerance = Real(1e-7);
			Real const input_values[] = { 0, Real(pi) / 4, Real(pi) / 3, Real(pi) / 2, Real(pi) };
			for(unsigned int i(0); i < sizeof input_values / sizeof *input_values; ++i) {
				Real const x(input_values[i]);
				Real s, c;
				sincos(x, s, c);
				std::ostringstream oss;
				oss << "sin_cos<" << universalis::compiler::typenameof(x) << ">(" << x << "): sin = " << s << ", cos = " << c;
				BOOST_MESSAGE(oss.str());
				BOOST_CHECK(std::abs(s - std::sin(x)) < tolerance);
				BOOST_CHECK(std::abs(c - std::cos(x)) < tolerance);
			}
		}
		BOOST_AUTO_TEST_CASE(sincos_test) {
			sincos_test_template<float>();
			sincos_test_template<double>();
			#if DIVERSALIS__STDLIB__MATH >= 199901
				sincos_test_template<long double>();
			#endif
		}
	}
#endif



/*************************************************************************************************
Summary of some of the C99 functions that do floating point rounding and conversions to integers:
	"(int)" means it returns an integral number.
	"(float)" means it returns a floating point number.

	round to integer, using the current rounding direction (fesetround FE_TONEAREST, FE_TOWARDZERO, FE_DOWNWARD, FE_UPWARD):
		(float) rint, rintf, rintl - raise the inexact exception when the result differs in value from the argument
		(float) nearbyint, nearbyintf, nearbyintl - don't raise the inexact exception
		(int) lrint, lrintf, lrintl, llrint, llrintf, llrintl
	round to nearest integer, away from zero for halfway cases (fesetround FE_TONEAREST):
		(float) round, roundf, roundl
		(int) lround, lroundf, lroundl, llround, llroundf, llroundl
	round to integer, towards zero (fesetround FE_TOWARDZERO):
		(float) trunc, truncf, truncl
		(int) static_cast, c-style cast, constructor-style cast
	round to integer, towards -inf (fesetround FE_DOWNWARD):
		(float) floor, floorf, floorl
	round to integer, towards +inf (fesetround FE_UPWARD):
		(float) ceil, ceilf, ceill
	remainder/modulo:
		(float) fmod, fmodf, fmodl - quotient rounded towards zero to an integer
		(float) remainder, remainderf, remainderl - quotient rounded to the nearest integer.
*************************************************************************************************/



/******************************************************************************************/
/// C1999 *rint* - converts a floating point number to an integer by rounding in an unspecified way.
/// This function has the same semantic as C1999's *rint* series of functions,
/// but with C++ overload support, we don't need different names for each type.
/// On C1999, the rounding mode may be set with fesetround, but msvc does not support it, so the mode is unspecified.
/// Because the rounding mode is left as is, this is faster than using static_cast (trunc), floor, ceil, or round, which need to change it temporarily.
template<typename IntegralResult, typename Real> UNIVERSALIS__COMPILER__CONST_FUNCTION
IntegralResult inline rint(Real x) noexcept {
	// check that the Result type is an integral number
	BOOST_STATIC_ASSERT((std::numeric_limits<IntegralResult>::is_integer));

	return x;
}

#if DIVERSALIS__STDLIB__MATH >= 199901

	// signed long long int

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed long long int inline rint<>(long double ld) noexcept { return ::llrintl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed long long int inline rint<>(double d) noexcept { return ::llrint(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed long long int inline rint<>(float f) noexcept { return ::llrintf(f); }

	// unsigned long long int

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned long long int inline rint<>(long double ld) noexcept { return ::llrintl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned long long int inline rint<>(double d) noexcept { return ::llrint(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned long long int inline rint<>(float f) noexcept { return ::llrintf(f); }

	// signed long int

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed long int inline rint<>(long double ld) noexcept { return ::lrintl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed long int inline rint<>(double d) noexcept { return ::lrint(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed long int inline rint<>(float f) noexcept { return ::lrintf(f); }
	
	// unsigned long int

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned long int inline rint<>(long double ld) noexcept { return ::lrintl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned long int inline rint<>(double d) noexcept { return ::lrint(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned long int inline rint<>(float f) noexcept { return ::lrintf(f); }

	// signed int

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed int inline rint<>(long double ld) noexcept { return ::lrintl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed int inline rint<>(double d) noexcept { return ::lrint(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed int inline rint<>(float f) noexcept { return ::lrintf(f); }

	// unsigned int

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned int inline rint<>(long double ld) noexcept { return ::lrintl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned int inline rint<>(double d) noexcept { return ::lrint(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned int inline rint<>(float f) noexcept { return ::lrintf(f); }

	// signed short

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed short inline rint<>(long double ld) noexcept { return ::lrintl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed short inline rint<>(double d) noexcept { return ::lrint(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed short inline rint<>(float f) noexcept { return ::lrintf(f); }

	// unsigned short

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned short inline rint<>(long double ld) noexcept { return ::lrintl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned short inline rint<>(double d) noexcept { return ::lrint(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned short inline rint<>(float f) noexcept { return ::lrintf(f); }

	// signed char

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed char inline rint<>(long double ld) noexcept { return ::lrintl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed char inline rint<>(double d) noexcept { return ::lrint(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed char inline rint<>(float f) noexcept { return ::lrintf(f); }

	// unsigned char

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned char inline rint<>(long double ld) noexcept { return ::lrintl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned char inline rint<>(double d) noexcept { return ::lrint(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned char inline rint<>(float f) noexcept { return ::lrintf(f); }

	// char

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION char inline rint<>(long double ld) noexcept { return ::lrintl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION char inline rint<>(double d) noexcept { return ::lrint(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION char inline rint<>(float f) noexcept { return ::lrintf(f); }
#else

	// int32_t
	
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION
	int32_t inline rint<>(double d) noexcept {
		BOOST_STATIC_ASSERT((sizeof d == 8));
		union result_union
		{
			double d;
			int32_t i;
		} result;
		result.d = d + 6755399441055744.0; // 2^51 + 2^52
		return result.i;
	}

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION
	int32_t inline rint<>(float f) noexcept {
		#if defined DIVERSALIS__CPU__X86 && defined DIVERSALIS__COMPILER__MICROSOFT && defined DIVERSALIS__COMPILER__ASSEMBLER__INTEL// also intel's compiler?
			///\todo not always the fastest when using sse(2)
			///\todo the double "2^51 + 2^52" version might be faster.
			int32_t i;
			__asm
			{ 
				fld f;
				fistp i;
			}
			return i;
		#else
			return static_cast<int32_t>(f);
		#endif
	}

	// uint32_t

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION uint32_t inline rint<>(double d) noexcept { return rint<int32_t>(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION uint32_t inline rint<>(float f) noexcept { return rint<int32_t>(f); }

	// int16_t

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION int16_t inline rint<>(double d) noexcept { return rint<int32_t>(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION int16_t inline rint<>(float f) noexcept { return rint<int32_t>(f); }

	// uint16_t

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION uint16_t inline rint<>(double d) noexcept { return rint<int32_t>(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION uint16_t inline rint<>(float f) noexcept { return rint<int32_t>(f); }

	// int8_t

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION int8_t inline rint<>(double d) noexcept { return rint<int32_t>(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION int8_t inline rint<>(float f) noexcept { return rint<int32_t>(f); }

	// uint8_t

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION uint8_t inline rint<>(double d) noexcept { return rint<int32_t>(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION uint8_t inline rint<>(float f) noexcept { return rint<int32_t>(f); }

#endif

#ifdef BOOST_AUTO_TEST_CASE
	namespace test {
		template<typename Real>
		void rint_test_template() {
			Real r(-1e6);
			while(r < Real(1e6)) {
				int32_t i = rint<int32_t>(r);
				int32_t si = static_cast<int32_t>(r);
				BOOST_CHECK(i == si || i == si + (r > 0 ? +1 : -1));
				r += Real(100.1);
			}
		}
		BOOST_AUTO_TEST_CASE(rint_test) {
			rint_test_template<float>();
			rint_test_template<double>();
			#if DIVERSALIS__STDLIB__MATH >= 199901
				rint_test_template<long double>();
			#endif
		}
	
		#if DIVERSALIS__STDLIB__MATH >= 199901 // some test using C1999's features in <fenv.h>
			BOOST_AUTO_TEST_CASE(rint_c1999_test) {
				int const initial_feround(fegetround());
				try {
					fesetround(FE_TONEAREST);
					BOOST_CHECK(rint<long int>(+1.6) == +2);
					BOOST_CHECK(rint<long int>(+1.4) == +1);
					BOOST_CHECK(rint<long int>(-1.6) == -2);
					BOOST_CHECK(rint<long int>(-1.4) == -1);
					fesetround(FE_TOWARDZERO);
					BOOST_CHECK(rint<long int>(+1.6) == +1);
					BOOST_CHECK(rint<long int>(+1.4) == +1);
					BOOST_CHECK(rint<long int>(-1.6) == -1);
					BOOST_CHECK(rint<long int>(-1.4) == -1);
					fesetround(FE_DOWNWARD);
					BOOST_CHECK(rint<long int>(+1.6) == +1);
					BOOST_CHECK(rint<long int>(+1.4) == +1);
					BOOST_CHECK(rint<long int>(-1.6) == -2);
					BOOST_CHECK(rint<long int>(-1.4) == -2);
					fesetround(FE_UPWARD);
					BOOST_CHECK(rint<long int>(+1.6) == +2);
					BOOST_CHECK(rint<long int>(+1.4) == +2);
					BOOST_CHECK(rint<long int>(-1.6) == -1);
					BOOST_CHECK(rint<long int>(-1.4) == -1);
				} catch(...) {
					fesetround(initial_feround);
					throw;
				}
				fesetround(initial_feround);
			}
		#endif
	}
#endif



/******************************************************************************************/
/// C1999 *round* - converts a floating point number to an integer by rounding to the nearest integer.
/// This function has the same semantic as C1999's *round* series of functions,
/// but with C++ overload support, we don't need different names for each type.
/// note: it is unspecified whether rounding x.5 rounds up, down or towards the even integer.
template<typename IntegralResult, typename Real> UNIVERSALIS__COMPILER__CONST_FUNCTION
IntegralResult inline round(Real x) noexcept {
	// check that the Result type is an integral number
	BOOST_STATIC_ASSERT((std::numeric_limits<IntegralResult>::is_integer));

	return x > 0 ? std::floor(x + Real(0.5)) : std::ceil(x - Real(0.5));
}

#if DIVERSALIS__STDLIB__MATH >= 199901
	
	// signed long long int

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed long long int inline round<>(long double ld) noexcept { return ::llroundl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed long long int inline round<>(double d) noexcept { return ::llround(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed long long int inline round<>(float f) noexcept { return ::llroundf(f); }

	// unsigned long long int

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned long long int inline round<>(long double ld) noexcept { return ::llroundl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned long long int inline round<>(double d) noexcept { return ::llround(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned long long int inline round<>(float f) noexcept { return ::llroundf(f); }

	// signed long int

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed long int inline round<>(long double ld) noexcept { return ::lroundl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed long int inline round<>(double d) noexcept { return ::lround(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed long int inline round<>(float f) noexcept { return ::lroundf(f); }

	// unsigned long int

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned long int inline round<>(long double ld) noexcept { return ::lroundl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned long int inline round<>(double d) noexcept { return ::lround(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned long int inline round<>(float f) noexcept { return ::lroundf(f); }

	// signed int

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed int inline round<>(long double ld) noexcept { return ::lroundl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed int inline round<>(double d) noexcept { return ::lround(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed int inline round<>(float f) noexcept { return ::lroundf(f); }

	// unsigned int

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned int inline round<>(long double ld) noexcept { return ::lroundl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned int inline round<>(double d) noexcept { return ::lround(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned int inline round<>(float f) noexcept { return ::lroundf(f); }

	// signed short

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed short inline round<>(long double ld) noexcept { return ::lroundl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed short inline round<>(double d) noexcept { return ::lround(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed short inline round<>(float f) noexcept { return ::lroundf(f); }

	// unsigned short

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned short inline round<>(long double ld) noexcept { return ::lroundl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned short inline round<>(double d) noexcept { return ::lround(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned short inline round<>(float f) noexcept { return ::lroundf(f); }

	// signed char

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed char inline round<>(long double ld) noexcept { return ::lroundl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed char inline round<>(double d) noexcept { return ::lround(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION signed char inline round<>(float f) noexcept { return ::lroundf(f); }

	// unsigned char

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned char inline round<>(long double ld) noexcept { return ::lroundl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned char inline round<>(double d) noexcept { return ::lround(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION unsigned char inline round<>(float f) noexcept { return ::lroundf(f); }

	// char

	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION char inline round<>(long double ld) noexcept { return ::lroundl(ld); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION char inline round<>(double d) noexcept { return ::lround(d); }
	template<> UNIVERSALIS__COMPILER__CONST_FUNCTION char inline round<>(float f) noexcept { return ::lroundf(f); }

#endif

#ifdef BOOST_AUTO_TEST_CASE
	namespace test {
		BOOST_AUTO_TEST_CASE(round_test) {
			BOOST_CHECK(round<long int>(+2.6) == +3);
			BOOST_CHECK(round<long int>(+1.4) == +1);
			BOOST_CHECK(round<long int>(-2.6) == -3);
			BOOST_CHECK(round<long int>(-1.4) == -1);
			BOOST_CHECK(round<long int>(+2.6f) == +3);
			BOOST_CHECK(round<long int>(+1.4f) == +1);
			BOOST_CHECK(round<long int>(-2.6f) == -3);
			BOOST_CHECK(round<long int>(-1.4f) == -1);
		}
	}
#endif

}}
