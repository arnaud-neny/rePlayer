// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2013 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include <universalis/detail/project.hpp>

#ifdef DIVERSALIS__COMPILER__MICROSOFT
	#if DIVERSALIS__COMPILER__VERSION__MAJOR >= 15
		#define UNIVERSALIS__COMPILER__PRAGMA(x) __pragma(x)
	#else
		#define UNIVERSALIS__COMPILER__PRAGMA(x)
	#endif
#else
	/// ISO C-1999 _Pragma operator
	#define UNIVERSALIS__COMPILER__PRAGMA(x) _Pragma(x)
#endif
