// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include "attribute.hpp"

#if defined DIVERSALIS__COMPILER__GNU
	#define UNIVERSALIS__COMPILER__ALIGN(bytes) UNIVERSALIS__COMPILER__ATTRIBUTE(aligned(bytes))
	// note: a bit field is packed to 1 bit, not one byte.
	#define UNIVERSALIS__COMPILER__DOALIGN(bytes, what) what UNIVERSALIS__COMPILER__ATTRIBUTE(aligned(bytes))
#elif defined DIVERSALIS__COMPILER__MICROSOFT
	#define UNIVERSALIS__COMPILER__ALIGN(bytes) UNIVERSALIS__COMPILER__ATTRIBUTE(align(bytes))
	// see also: #pragma pack(x) in the optimization section
	#define UNIVERSALIS__COMPILER__DOALIGN(bytes, what) UNIVERSALIS__COMPILER__ATTRIBUTE(align(bytes)) what
#else
	/// memory alignment.
	///\see packed
	#define UNIVERSALIS__COMPILER__ALIGN(bytes)
	#define UNIVERSALIS__COMPILER__DOALIGN(bytes, what)
#endif
