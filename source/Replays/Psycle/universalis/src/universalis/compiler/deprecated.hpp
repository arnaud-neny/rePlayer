// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include "attribute.hpp"

/// declares a symbol as deprecated
#if defined DIVERSALIS__COMPILER__GNU
	#define UNIVERSALIS__COMPILER__DEPRECATED(message) UNIVERSALIS__COMPILER__ATTRIBUTE(__deprecated__)
#elif defined DIVERSALIS__COMPILER__MICROSOFT
	#if DIVERSALIS__COMPILER__VERSION__FULL < 140050320
		#define UNIVERSALIS__COMPILER__DEPRECATED(message) UNIVERSALIS__COMPILER__ATTRIBUTE(deprecated)
	#else
		#define UNIVERSALIS__COMPILER__DEPRECATED(message) UNIVERSALIS__COMPILER__ATTRIBUTE(deprecated(message))
	#endif
#else
	#define UNIVERSALIS__COMPILER__DEPRECATED(message)
#endif

#include "pragma.hpp"

/// declares a symbol as poisonous (stops compilation and errors out).
#if defined DIVERSALIS__COMPILER__GNU
	#define UNIVERSALIS__COMPILER__POISON(x) UNIVERSALIS__COMPILER__PRAGMA("GCC poison " #x)
#elif defined DIVERSALIS__COMPILER__MICROSOFT
	#define UNIVERSALIS__COMPILER__POISON(x) UNIVERSALIS__COMPILER__PRAGMA("deprecated(\"#x\")")
#else
	#define UNIVERSALIS__COMPILER__POISON(x)
#endif
