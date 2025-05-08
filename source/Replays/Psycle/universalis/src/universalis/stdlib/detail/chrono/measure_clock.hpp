// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#ifdef BOOST_AUTO_TEST_CASE
	#include "duration_and_time_point.hpp"
	#include <universalis/compiler/typenameof.hpp>
	#include <universalis/stdlib/thread.hpp>
	#include <sstream>
	#include <iomanip>
	namespace universalis { namespace stdlib { namespace chrono { namespace test {
		template<typename Clock>
		void measure_clock_against_sleep_template(bool absolute, typename Clock::duration const sleep_duration) {
			typename Clock::time_point const t0(Clock::now());
			if(absolute)
				this_thread::sleep_until(t0 + sleep_duration);
			else
				this_thread::sleep_for(sleep_duration);
			typename Clock::time_point const t1(Clock::now());
			typename Clock::duration const d(t1 - t0);
			double const ratio(double(d.count()) / sleep_duration.count());
			{
				std::ostringstream s; s << "clock: " << std::setw(60) << compiler::typenameof<Clock>() << ": absolute: " << absolute << ", ratio: " << ratio;
				BOOST_MESSAGE(s.str());
			}
			milliseconds const tolerance(50);
			BOOST_CHECK(sleep_duration - tolerance < d && d < sleep_duration + tolerance);
		}
		
		template<typename Clock>
		void measure_clock_against_sleep(typename Clock::duration const sleep_duration = milliseconds(250)) {
			measure_clock_against_sleep_template<Clock>(false, sleep_duration);
			measure_clock_against_sleep_template<Clock>(true, sleep_duration);
		}
		
		/// measures the resolution of a clock and displays the result
		template<typename Clock>
		void measure_clock_resolution(unsigned int const count = 1000000) {
			using namespace stdlib::chrono;
			nanoseconds min(hours(1)), max, sum;
			for(unsigned int i(0); i < count; ++i) {
				uintmax_t timeout(0);
				typename Clock::time_point t1;
				typename Clock::time_point const t0(Clock::now());
				do { t1 = Clock::now(); ++timeout; } while(t1 == t0 && timeout < 1000 * 1000 * 100);
				if(Clock::is_steady) BOOST_CHECK(t1 >= t0);
				if(t1 == t0) max = hours(1); // reports the timeout as a bogus big value
				nanoseconds const d(t1 - t0);
				if(d < min) min = d;
				if(d > max) max = d;
				sum += d;
			}
			std::ostringstream s;
			s
				<< "clock: " << std::setw(60) << compiler::typenameof<Clock>()
				<< ": min: " << std::setw(10) << min.count() * 1e-9 << 's'
				<< ", avg: " << std::setw(10) << sum.count() * 1e-9 / count << 's'
				<< ", max: " << std::setw(10) << max.count() * 1e-9 << 's';
			BOOST_MESSAGE(s.str());
		}
	}}}}
#endif
