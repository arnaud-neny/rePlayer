// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2013-2013 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include "attribute.hpp"

#if defined DIVERSALIS__COMPILER__GNU
	#define UNIVERSALIS__COMPILER__PURE_FUNCTION UNIVERSALIS__COMPILER__ATTRIBUTE(__pure__)
#elif defined DIVERSALIS__COMPILER__INTEL
	///\todo supported.. check the doc
	#define UNIVERSALIS__COMPILER__PURE_FUNCTION
#else
	#define UNIVERSALIS__COMPILER__PURE_FUNCTION
#endif
