// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include "attribute.hpp"

#if defined DIVERSALIS__OS__MICROSOFT
	#define UNIVERSALIS__COMPILER__DYN_LINK__EXPORT UNIVERSALIS__COMPILER__ATTRIBUTE(dllexport)
	#define UNIVERSALIS__COMPILER__DYN_LINK__IMPORT UNIVERSALIS__COMPILER__ATTRIBUTE(dllimport)
	#define UNIVERSALIS__COMPILER__DYN_LINK__HIDDEN
#elif defined DIVERSALIS__COMPILER__GNU && DIVERSALIS__COMPILER__VERSION__MAJOR >= 4
	#define UNIVERSALIS__COMPILER__DYN_LINK__EXPORT UNIVERSALIS__COMPILER__ATTRIBUTE(visibility("default"))
	#define UNIVERSALIS__COMPILER__DYN_LINK__IMPORT UNIVERSALIS__COMPILER__ATTRIBUTE(visibility("default"))
	#define UNIVERSALIS__COMPILER__DYN_LINK__HIDDEN UNIVERSALIS__COMPILER__ATTRIBUTE(visibility("hidden"))
#else
	#define UNIVERSALIS__COMPILER__DYN_LINK__EXPORT
	#define UNIVERSALIS__COMPILER__DYN_LINK__IMPORT
	#define UNIVERSALIS__COMPILER__DYN_LINK__HIDDEN
#endif
