// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2009-2011 members of the psycle project http://psycle.sourceforge.net

///\file
///\brief public project-wide definitions. file included first by every header.
#pragma once
#include "config.hpp"
#include <universalis.hpp>

#ifndef DIVERSALIS__COMPILER__RESOURCE
	namespace psycle { namespace core {

		using namespace universalis::stdlib;
		namespace loggers = universalis::os::loggers;

		#ifdef PSYCLE__CORE__SHARED
			#ifdef PSYCLE__CORE__SOURCE
				#define PSYCLE__CORE__DECL UNIVERSALIS__COMPILER__DYN_LINK__EXPORT
			#else
				#define PSYCLE__CORE__DECL UNIVERSALIS__COMPILER__DYN_LINK__IMPORT
			#endif
		#else
			#define PSYCLE__CORE__DECL //UNIVERSALIS__COMPILER__DYN_LINK__HIDDEN
		#endif

		#if !defined PSYCLE__CORE__SOURCE && defined DIVERSALIS__COMPILER__FEATURE__AUTO_LINK
			#pragma comment(lib, "psycle-core")
		#endif
	}}
#endif
