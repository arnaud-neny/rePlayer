// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2013-2013 members of the psycle project http://psycle.pastnotecut.org ; johan boule <bohan@jabber.org>

#pragma once
#include "pragma.hpp"
#include "stringize.hpp"

// see http://stackoverflow.com/questions/471935/user-warnings-on-msvc-and-gcc

#ifdef DIVERSALIS__COMPILER__MICROSOFT
	#define UNIVERSALIS__COMPILER__MESSAGE(str) \
		UNIVERSALIS__COMPILER__PRAGMA(message(__FILE__ "(" UNIVERSALIS__COMPILER__STRINGIZE(__LINE__) ") : " str))
#else
	#define UNIVERSALIS__COMPILER__MESSAGE(str) \
		UNIVERSALIS__COMPILER__PRAGMA(UNIVERSALIS__COMPILER__STRINGIZE(message str))
#endif

#define UNIVERSALIS__COMPILER__WARNING(str) UNIVERSALIS__COMPILER__MESSAGE("warning: " str)
