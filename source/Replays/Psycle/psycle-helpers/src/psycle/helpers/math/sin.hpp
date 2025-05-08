// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net and of the music-dsp mailing list http://www.music.columbia.edu/cmc/music-dsp

#ifndef PSYCLE__HELPERS__MATH__SIN__INCLUDED
#define PSYCLE__HELPERS__MATH__SIN__INCLUDED
#pragma once

#include "constants.hpp"
#include <universalis.hpp>
#include <cmath>
#ifdef BOOST_AUTO_TEST_CASE
	#include <universalis/os/clocks.hpp>
	#include <sstream>
#endif

namespace psycle { namespace helpers { namespace math {

/// polynomial approximation of the sin function.
///
/// two variants: 2nd degree positive polynomial (parabola), 4th degree polynomial (square of the same parabola).
/// input range: [-pi, pi]
/// output range: [0, 1]
/// constraints applied: sin(0) = 0, sin(pi / 2) = 1, sin(pi) = 0
/// THD = 3.8% with only odd harmonics (in the accurate variant THD is only 0.078% with q = 0.775, p = 0.225)
///
/// worth reading:
/// http://www.audiomulch.com/~rossb/code/sinusoids/
/// http://www.devmaster.net/forums/showthread.php?t=5784
/// 
/// WARNING!!!!! This does not work for a sinc(). The 2nd degree is completely out of the question (makes like a top-cut triangle), 
/// and the 4th degree pushes the top of the sinc down. (rebounds). 
///	In this case, the problematic range is 	-0.46 .. 0.46, and only for that first lob.
template<unsigned int Polynomial_Degree, typename Real> UNIVERSALIS__COMPILER__CONST_FUNCTION
Real inline fast_sin(Real radians) {
	//assert(-pi <= radians && radians <= pi);
	// we solve:
	// y(x) = a + b x + c x^2
	// with the constraints: y(0) = 0, y(pi / 2) = 1, y(pi) = 0
	// this gives, a = 0, b = 4 / pi, c = -4 / pi^2
	Real static constexpr b(4 / pi);
	Real static constexpr c(-b / pi); // pi^2 = 9.86960440108935861883449099987615114
	// we use absolute values to mirror the parabola around the orign 
	Real y(b * radians + c * radians * std::abs(radians));
	if(Polynomial_Degree > 2) {
		// q + p = 1
		// q = 0.775991821224, p = 0.224008178776 for minimal absolute error (0.0919%)
		// q = 0.782, p = 0.218 for minimal relative error
		// q = ?, p = ? for minimal THD error
		#if 1
			Real static constexpr p(Real(0.224008178776));
			y = p * (y * std::abs(y) - y) + y;
		#else
			Real static constexpr q(Real(0.775991821224));
			y = q * y + p * y * std::abs(y);
		#endif
	}
	//assert(-1 <= y && y <= 1);
	return y;
}


/******************************************************************************************/
#ifdef BOOST_AUTO_TEST_CASE
	namespace test {
		template<unsigned int Polynomial_Degree, typename Real>
		void fast_sin_test_template() {
			typedef universalis::os::clocks::hires_thread_or_fallback clock;
			int const iterations(1000000);
			Real const step(pi / 1000);
			clock::time_point const t1(clock::now());
			Real f1(1), ff1(0);
			for(int i(0); i < iterations; ++i) {
				f1 += fast_sin<Polynomial_Degree>(ff1);
				ff1 += step;
				if(ff1 > pi) ff1 -= 2 * pi; //too slow: ff1 = std::fmod(ff1 + pi, 2 * pi) - pi;
			}
			clock::time_point const t2(clock::now());
			Real f2(1), ff2(0);
			for(int i(0); i < iterations; ++i) {
				f2 += std::sin(ff2);
				ff2 += step;
			}
			clock::time_point const t3(clock::now());
			{
				std::ostringstream s; s << "fast_sin<Polynomial_Degree = " << Polynomial_Degree << ", Real = " << universalis::compiler::typenameof(f1) << ">: " << f1;
				BOOST_MESSAGE(s.str());
			}
			{
				std::ostringstream s; s << "std::sin: " << f2;
				BOOST_MESSAGE(s.str());
			}
			{
				std::ostringstream s;
				using universalis::stdlib::chrono::nanoseconds;
				s << nanoseconds(t2 - t1).count() * 1e-9 << "s < " << nanoseconds(t3 - t2).count() * 1e-9 << "s";
				BOOST_MESSAGE(s.str());
			}
			BOOST_CHECK(t2 - t1 < t3 - t2);
		}

		BOOST_AUTO_TEST_CASE(fast_sin_test) {
			fast_sin_test_template<2, float>();
			fast_sin_test_template<4, float>();
			fast_sin_test_template<2, double>();
			fast_sin_test_template<4, double>();
		}
	}
#endif

}}}

#endif
