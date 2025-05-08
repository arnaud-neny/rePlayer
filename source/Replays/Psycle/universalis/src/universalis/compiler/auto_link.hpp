// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include "pragma.hpp"

#if defined DIVERSALIS__COMPILER__FEATURE__AUTO_LINK
	#define UNIVERSALIS__COMPILER__AUTO_LINK(library) UNIVERSALIS__COMPILER__PRAGMA("comment(lib, \"" #library "\"")
#else
	#define UNIVERSALIS__COMPILER__AUTO_LINK(library)
#endif
