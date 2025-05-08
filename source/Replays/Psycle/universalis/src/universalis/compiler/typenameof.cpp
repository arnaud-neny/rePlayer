// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\implementation universalis::compiler::typenameof

#include <universalis/detail/project.private.hpp>
#include "typenameof.hpp"

#if defined DIVERSALIS__COMPILER__GNU
	#include <cxxabi.h>
	#include <cstdlib>
#endif

namespace universalis { namespace compiler { namespace detail {

std::string demangle(std::string const & mangled_symbol) {
	#if defined DIVERSALIS__COMPILER__GNU
		int status;
		char * c(abi::__cxa_demangle(mangled_symbol.c_str(), 0, 0, &status));
		std::string s(c);
		std::free(c);
		return s;
	#else
		return mangled_symbol;
	#endif
}

}}}
