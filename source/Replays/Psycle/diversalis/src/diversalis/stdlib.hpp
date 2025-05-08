// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2012 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\file
///\brief compiler-independant meta-information about the standard/runtime library.
#pragma once
#include "compiler.hpp"
#include "os.hpp"

#if __cplusplus >= 201103L || __STDC__VERSION__ >= 199901L
//	#define DIVERSALIS__STDLIB__MATH 199901L // rePlayer
#endif

#if defined DIVERSALIS__COMPILER__MICROSOFT
	#ifdef _MT // defined when -MD or -MDd (multithreaded dll) or -MT or -MTd (multithreaded static) is specified.
		#define DIVERSALIS__STDLIB__RUNTIME__MULTI_THREADED
	#endif
	#ifdef _DLL // defined when -MD or -MDd (multithread dll) is specified.
		#define DIVERSALIS__STDLIB__RUNTIME__DYN_LINK
	#endif
	#ifdef _DEBUG // defined when compiling with -LDd, -MDd, or -MTd.
		#define DIVERSALIS__STDLIB__RUNTIME__DEBUG
	#endif
#elif defined _REENTRANT
		#define DIVERSALIS__STDLIB__RUNTIME__MULTI_THREADED
#endif
