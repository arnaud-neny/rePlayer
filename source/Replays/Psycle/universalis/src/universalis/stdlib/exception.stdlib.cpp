// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#include <universalis/detail/project.private.hpp>
#include "exception.hpp"
#include <universalis/stdlib/mutex.hpp>
#include <cstring> // iso std::strerror
#include <sstream>
namespace universalis { namespace stdlib {

char const * exception::what() const throw() {
	if(!what_) what_ = new std::string(exceptions::desc(code()));
	return what_->c_str();
}

namespace exceptions {
	std::string desc(int code) throw() {
		std::ostringstream s;
		s << "standard error: " << code << " 0x" << std::hex << code << ": ";
		{
			static mutex m;
			unique_lock<mutex> l(m);
			s << std::strerror(code);
		}
		return s.str();
	}
}
}}
