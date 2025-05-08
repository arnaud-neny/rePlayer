// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2014-2014 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include <universalis/detail/project.hpp>

#if __cplusplus >= 201103L
	// noexcept is a keyword
#else
	#define noexcept throw() // not strictly the same, but close
#endif
