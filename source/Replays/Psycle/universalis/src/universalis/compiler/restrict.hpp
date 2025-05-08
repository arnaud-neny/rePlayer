// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include <universalis/detail/project.hpp>

///\file
/// the restrict keyword has been introduced in the ISO C 1999 standard.
/// it is a hint for the compiler telling it several references or pointers will *always* holds different memory addresses.
/// hence it can optimize more, knowing that writting to memory via one reference cannot alias another.
/// example:
	/// void f(int & restrict r1, int & restrict r2, int * restrict p1, int restrict p2[]);
	/// here the compiler is told that &r1 != &r2 != p1 != p2

///\todo test __STDC_VERSION
#if defined DIVERSALIS__COMPILER__FEATURE__NOT_CONCRETE
	// not sure netbeans, eclipse and doxygen handle the keyword
	#define UNIVERSALIS__COMPILER__RESTRICT
	#define UNIVERSALIS__COMPILER__RESTRICT_REF
#elif defined DIVERSALIS__COMPILER__GNU
	#define UNIVERSALIS__COMPILER__RESTRICT __restrict__
	#define UNIVERSALIS__COMPILER__RESTRICT_REF __restrict__
#elif defined DIVERSALIS__COMPILER__MICROSOFT
	#define UNIVERSALIS__COMPILER__RESTRICT __restrict
	#define UNIVERSALIS__COMPILER__RESTRICT_REF // With msvc, the __restrict keyword works on pointers, but not on C++ references :-(
#else
	#define UNIVERSALIS__COMPILER__RESTRICT
	#define UNIVERSALIS__COMPILER__RESTRICT_REF
#endif
