// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2012 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include "detail/chrono/duration_and_time_point.hpp"
#if __cplusplus >= 201103L
	#include <chrono>
	namespace universalis { namespace stdlib { namespace chrono {
		using std::chrono::system_clock;
		using std::chrono::steady_clock;
		using std::chrono::high_resolution_clock;
	}}}
#else
	#include <universalis/os/clocks.hpp>
	namespace universalis { namespace stdlib { namespace chrono {
		typedef os::clocks::utc_since_epoch system_clock;
		typedef os::clocks::steady steady_clock;
		typedef system_clock high_resolution_clock; // as done in gcc 4.4
	}}}
#endif

/******************************************************************************************/
#ifdef BOOST_AUTO_TEST_CASE
	#include "detail/chrono/measure_clock.hpp"
	#include <typeinfo>
	namespace universalis { namespace stdlib { namespace chrono { namespace test {
		BOOST_AUTO_TEST_CASE(measure_clocks_stdlib_test) {
			measure_clock_against_sleep<system_clock>();
			measure_clock_resolution<system_clock>(100);
			if(typeid(steady_clock) != typeid(system_clock)) {
				measure_clock_against_sleep<steady_clock>();
				measure_clock_resolution<steady_clock>(100);
			}
			if(
				typeid(high_resolution_clock) != typeid(system_clock)  &&
				typeid(high_resolution_clock) != typeid(steady_clock)
			) {
				measure_clock_against_sleep<high_resolution_clock>();
				measure_clock_resolution<high_resolution_clock>(100);
			}
		}

	}}}}
#endif
