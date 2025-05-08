// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#ifndef PSYCLE__HELPERS__MATH__SINSEQ__INCLUDED
#define PSYCLE__HELPERS__MATH__SINSEQ__INCLUDED
#pragma once

#include "clip.hpp"
#ifdef BOOST_AUTO_TEST_CASE
	#include <universalis/os/clocks.hpp>
	#include <sstream>
	#include <cmath>
#endif

namespace psycle { namespace helpers { namespace math {

/// optimised sine computation when dealing with constant phase increment.
template<bool Clip>
class sinseq {
	public:
		/// use 64-bit floating point numbers or else accuracy is not sufficient
		typedef double real;

		/// constructor
		sinseq() : index_(0) {}

		/// changes the phase and increment
		void operator()(real phase, real radians_per_sample) {
			step_ = 2 * std::cos(radians_per_sample);
			sequence_[0] = std::sin(phase - radians_per_sample);
			sequence_[1] = std::sin(phase - 2 * radians_per_sample);
			index_ = 0;
		}

		/// compute the next sine according to last phase and increment
		real operator()() {
			int const swapped_index(index_ ^ 1);
			real sin;
			if(Clip) sin = clip(real(-1), sequence_[index_] * step_ - sequence_[swapped_index], real(+1));
			else sin = sequence_[index_] * step_ - sequence_[swapped_index];
			index_ = swapped_index;
			return sequence_[swapped_index] = sin;
		}

	private:
		real step_, sequence_[2];
		int index_;
};

/******************************************************************************************/
#ifdef BOOST_AUTO_TEST_CASE
	namespace test {
		template<bool Clip>
		void sinseq_test_template() {
			typedef typename sinseq<Clip>::real real;
			sinseq<Clip> sin;
			{ // speed test
				typedef universalis::os::clocks::hires_thread_or_fallback clock;
				int const iterations(1000000);
				real const step(pi / 1000);
				sin(0, step);
				clock::time_point const t1(clock::now());
				real f1(1);
				for(int i(0); i < iterations; ++i) f1 += sin();
				clock::time_point const t2(clock::now());
				real f2(1), ff2(0);
				for(int i(0); i < iterations; ++i) {
					f2 += std::sin(ff2);
					ff2 += step;
				}
				clock::time_point const t3(clock::now());
				{
					std::ostringstream s; s << "sinseq<Clip = " << Clip << ">: " << f1;
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
			{ // bounds test
				real min(+2), max(-2);
				real step(pi);
				for(int i(0); i < 1000; ++i) {
					for(int j(0); j < 1000; ++j) {
						real const s(sin());
						if(s < min) min = s;
						if(s > max) max = s;
					}
					step *= 0.999;
					sin(0, step);
				}
				{
					std::ostringstream s;
					s << "sinseq<Clip = " << Clip << ">: min + 1: " << min + 1 << ", max - 1: " << max - 1;
					BOOST_MESSAGE(s.str());
				}
				if(Clip) BOOST_CHECK(-1 <= min && max <= 1);
			}
		}
		BOOST_AUTO_TEST_CASE(sinseq_test) {
			sinseq_test_template<false>();
			sinseq_test_template<true>();
		}
	}
#endif

}}}

#endif
