// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2011-2012 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include <universalis/detail/project.hpp>
#if __cplusplus >= 201103L
	#include <ratio>
	namespace universalis { namespace stdlib {
		using std::intmax_t;
		using std::ratio;
		using std::nano;
		using std::micro;
		using std::milli;
	}}
#else
	#include "cstdint.hpp"
	namespace universalis { namespace stdlib {
	
		template<intmax_t Num, intmax_t Den = 1>
		struct ratio {
			static const intmax_t num = Num;
			static const intmax_t den = Den;
		};
		
		typedef ratio<1, 1000000000> nano;
		typedef ratio<1,    1000000> micro;
		typedef ratio<1,       1000> milli;
	}}
#endif
