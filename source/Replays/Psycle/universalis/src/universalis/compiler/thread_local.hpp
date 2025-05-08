// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2013 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include <universalis/detail/project.hpp>

///\file
///\brief thread_local keyword - thread local storage

#if \
	__cplusplus >= 201103L && ( \
		/* A standard-conformant compiler shall only define __cplusplus >= 201103L once it implements *all* of the standard. */ \
		/* However gcc 4.7 claims to be fully conformant with the C++11 standard since it defines __cplusplus to 201103L, */ \
		/* but has no thread_local keyword! That keyword is only implemented since version 4.8. */ \
		!defined DIVERSALIS__COMPILER__GNU || \
		DIVERSALIS__COMPILER__VERSION >= 408000 \
	)
	// thread_local is a keyword
#elif \
	defined DIVERSALIS__COMPILER__GNU && ( \
		/* TLS support added on windows only in version 4.3.0 */ \
		!defined DIVERSALIS__OS__MICROSOFT || \
		DIVERSALIS__COMPILER__VERSION >= 40300 \
	)

	#define thread_local __thread
#elif defined DIVERSALIS__COMPILER__MICROSOFT
	#include "attribute.hpp"
	#define thread_local UNIVERSALIS__COMPILER__ATTRIBUTE(thread)
#else
	/// variable stored in a per thread local storage.
	#define thread_local

	#ifndef DIVERSALIS__COMPILER__FEATURE__NOT_CONCRETE
		UNIVERSALIS__COMPILER__WARNING("Cannot generate thread-safe code for this compiler ; please add support for thread-local storage for your compiler in the file where this warning is triggered.")
	#endif
#endif
