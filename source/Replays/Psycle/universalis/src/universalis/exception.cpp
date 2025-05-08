// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#include <universalis/detail/project.private.hpp>
#include "exception.hpp"
#include "os/loggers.hpp"
namespace universalis { namespace exceptions {

runtime_error::runtime_error(std::string const & what, compiler::location const & location, void const * cause) throw()
: std::runtime_error(what), locatable(location), nested(cause)
{
	if(os::loggers::trace()()) {
		std::ostringstream s;
		s  << "exception: " << this->what();
		os::loggers::exception()(s.str(), location);
	}
}

}}
