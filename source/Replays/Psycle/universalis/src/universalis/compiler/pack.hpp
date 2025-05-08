// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include <universalis/detail/project.hpp>

#if defined DIVERSALIS__COMPILER__GNU || defined DIVERSALIS__COMPILER__MICROSOFT
	#define UNIVERSALIS__COMPILER__PACK__PUSH(x) UNIVERSALIS__COMPILER__PRAGMA("pack(push, " #x ")")
	#define UNIVERSALIS__COMPILER__PACK(x)       UNIVERSALIS__COMPILER__PRAGMA("pack(" #x ")")
	#define UNIVERSALIS__COMPILER__PACK__POP()   UNIVERSALIS__COMPILER__PRAGMA("pack(pop)")
#else
	#define UNIVERSALIS__COMPILER__PACK__PUSH(x)
	#define UNIVERSALIS__COMPILER__PACK(x)
	#define UNIVERSALIS__COMPILER__PACK__POP()
#endif
