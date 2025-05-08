// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2013 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include "attribute.hpp"

///\file
///\brief stuff related with virtual member functions

/// pure virtual classes
#if \
	defined DIVERSALIS__COMPILER__GNU || \
	defined DIVERSALIS__COMPILER__MICROSOFT
	#define UNIVERSALIS__COMPILER__PURE_VIRTUAL UNIVERSALIS__COMPILER__ATTRIBUTE(novtable)
#else
	#define UNIVERSALIS__COMPILER__PURE_VIRTUAL
#endif

/// override keyword - overridding of virtual member function
#if __cplusplus >= 201103L
	// override is a keyword
#else
	#define override
#endif
