// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.pastnotecut.org : johan boule <bohan@jabber.org>

#include <universalis/detail/project.private.hpp>
#include "exception.hpp"
#include <sstream>
#if defined DIVERSALIS__COMPILER__GNU
	#include <cxxabi.h>
	#include <typeinfo>
	#include "typenameof.hpp"
#endif

namespace universalis { namespace compiler { namespace exceptions {

std::string ellipsis_desc() {
	#if defined DIVERSALIS__COMPILER__GNU
		std::type_info * type_info(abi::__cxa_current_exception_type());
		std::ostringstream s;
		s << "ellipsis: " << (type_info ? typenameof(*type_info) : "unknown type");
		return s.str();
	#elif defined DIVERSALIS__COMPILER__BORLAND
		std::ostringstream s;
		s << "ellipsis: " << __ThrowExceptionName() << " was thrown by source file " << __ThrowFileName() << "#" << __ThrowLineNumber();
		return s.str();
	#else
		return "ellipsis: unknown type";
	#endif
}

}}}
